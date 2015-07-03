#include <QDebug>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <linux/ioctl.h>

#include <stdexcept>
#include <stdint.h>

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

// EGL variables
EGLNativeWindowType m_window;
EGLDisplay m_display;
EGLConfig m_config;
EGLSurface m_surface;
EGLContext m_context;

// Buffers
GLuint m_vertices;

GLuint m_uVertexShader, m_uFragmentShader;
GLuint m_program;

// Shader variables
GLint m_attributeVertex;
GLint m_uniformValue;

QString glErrorString()
{
    return QString("%1").arg(glGetError(), 0, 16);
}

QString eglErrorString()
{
    return QString("%1").arg(eglGetError(), 0, 16);
}

#define GL_ASSERT() Q_ASSERT_X(glGetError() == GL_NO_ERROR, Q_FUNC_INFO, qPrintable(glErrorString()))
#define EGL_ASSERT() Q_ASSERT_X(eglGetError() == EGL_SUCCESS, Q_FUNC_INFO, qPrintable(eglErrorString()))
#define EGL_ASSERT_X(rc) Q_ASSERT_X(rc, Q_FUNC_INFO, qPrintable(eglErrorString()))

void quit()
{
    eglDestroySurface(m_display, m_surface);
    EGL_ASSERT();

    eglDestroyContext(m_display, m_context);
    EGL_ASSERT();

    eglTerminate(m_display);
    EGL_ASSERT();
}

GLuint createShader(GLenum shaderType, const GLchar *shaderSource)
{
    GLuint shader = glCreateShader(shaderType);
    GL_ASSERT();

    glShaderSource(shader, 1, &shaderSource, NULL);
    GL_ASSERT();

    glCompileShader(shader);
    GL_ASSERT();

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    GL_ASSERT();
    if (compiled == GL_FALSE) {
        GLchar info[1024];
        glGetShaderInfoLog(shader, 1024, NULL, info);
        GL_ASSERT();
        qWarning() << info;
        Q_ASSERT(false);
    }

    return shader;
}

void initShaders()
{
    const GLchar *vertexShaderSource =
            "attribute lowp vec4 vertex;\n"
            "varying lowp vec2 texcoord;\n"
            "\n"
            "void main(void) {\n"
            "    texcoord = vertex.xy * 0.5+0.5;\n"
            "    texcoord.y = 1.0 - texcoord.y;"
            "    gl_Position = vertex;\n"
            "}\n";

    m_uVertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSource);

    const char *fragmentShaderSource =
            "uniform lowp float val;\n"
            "varying lowp vec2 texcoord;\n"

            "void main(void) {\n"
            "    gl_FragColor = texcoord.x < val ? vec4(1.0, 1.0, 1.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
            "}\n";

    m_uFragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    m_program = glCreateProgram();

    glAttachShader(m_program, m_uVertexShader);
    GL_ASSERT();

    glAttachShader(m_program, m_uFragmentShader);
    GL_ASSERT();

    glLinkProgram(m_program);
    GL_ASSERT();

    GLint linked;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        char info[1024];
        glGetProgramInfoLog(m_program, 1024, NULL, info);
        GL_ASSERT();
        qWarning() << info;
        Q_ASSERT(false);
    }

    m_attributeVertex = glGetAttribLocation(m_program, "vertex");
    GL_ASSERT();

    m_uniformValue = glGetUniformLocation(m_program, "val");
    GL_ASSERT();

    glBindBuffer(GL_ARRAY_BUFFER, m_vertices);
    GL_ASSERT();

    glVertexAttribPointer(m_attributeVertex, 4, GL_FLOAT, GL_FALSE, 16, 0);
    GL_ASSERT();

    glEnableVertexAttribArray(m_attributeVertex);
    GL_ASSERT();
}

GLuint initBuffers(GLfloat *src, GLuint length)
{
    GLuint id;

    glGenBuffers(1, &id);
    GL_ASSERT();

    glBindBuffer(GL_ARRAY_BUFFER, id);
    GL_ASSERT();

    glBufferData(GL_ARRAY_BUFFER, length, src, GL_STATIC_DRAW);
    GL_ASSERT();

    return id;
}

void initBuffers()
{
    GLfloat verticesFullScreen[] = {
        -1.0, -1.0, 1.0, 1.0,
        -1.0, 1.0, 1.0, 1.0,
        1.0, 1.0, 1.0, 1.0,
        1.0, -1.0, 1.0, 1.0,
    };

    m_vertices = initBuffers(verticesFullScreen, sizeof(verticesFullScreen));
}

void init()
{
    setenv("EGL_PLATFORM", "fbdev", 0);

    m_window = 0;

    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGL_ASSERT_X(m_display != EGL_NO_DISPLAY);

    EGLBoolean rc;

    rc = eglInitialize(m_display, NULL, NULL);
    EGL_ASSERT_X(rc);

    static const EGLint attributes[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,
        EGL_NATIVE_RENDERABLE, EGL_TRUE,
        EGL_NONE
    };

    EGLint numConfig;
    rc = eglChooseConfig(m_display, attributes, &m_config, 1, &numConfig);
    EGL_ASSERT_X(rc);

    rc = eglBindAPI(EGL_OPENGL_ES_API);
    EGL_ASSERT_X(rc);

    const EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT, contextAttributes);
    EGL_ASSERT_X(m_context != EGL_NO_CONTEXT);

    m_surface = eglCreateWindowSurface(m_display, m_config, m_window, NULL);
    EGL_ASSERT_X(m_surface != EGL_NO_SURFACE);

    rc = eglMakeCurrent(m_display, m_surface, m_surface, m_context);
    EGL_ASSERT_X(rc);

    rc = eglSwapInterval(m_display, 1);
    EGL_ASSERT_X(rc);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GL_ASSERT();

    initBuffers();
    initShaders();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GL_ASSERT();

    glUseProgram(m_program);
    GL_ASSERT();
}

void draw()
{
    static int frames = 0;
    float c = (frames % 120) / 120.0f;
    frames++;

    glUniform1f(m_uniformValue, c);
    GL_ASSERT();

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    GL_ASSERT();

    glFinish();
    GL_ASSERT();

    eglSwapBuffers(m_display, m_surface);
    EGL_ASSERT();
}

int main(int argc, char *argv[])
{
    init();
    for (;;)
        draw();

    return 0;
}
