TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    helloW.c

INCLUDEPATH += ../../../spinnaker_tools_pm/pmapps/include

DISTFILES += \
    Makefile \
    README
