TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    pmagent.c

INCLUDEPATH += ../../spinnaker_tools_pm/pmagents/include

DISTFILES += \
    pmagent.lnk \
    Makefile
