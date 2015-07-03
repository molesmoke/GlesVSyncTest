#-------------------------------------------------
#
# Project created by QtCreator 2015-07-03T16:06:53
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = GlesVSyncTest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

LIBS += \
    -lGLESv2 \
    -lEGL

SOURCES += main.cpp
