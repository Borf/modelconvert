TEMPLATE = app
DESTDIR = .

QT -= gui core
CONFIG -= QT
CONFIG += c++11
QMAKE_CXXFLAGS += -Wall -std=c++11

INCLUDEPATH += ../blib/
INCLUDEPATH += ./assimp/
INCLUDEPATH += ../blib/externals

#HEADERS += ModelConvert.h

SOURCES += main.cpp
SOURCES += assimp.cpp
SOURCES += pmd.cpp

LIBS += -L../blib -lblib
LIBS += -lGL
LIBS += -lGLEW
LIBS += -lassimp
LIBS += -lX11
LIBS += -lXext
