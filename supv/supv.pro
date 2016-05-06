TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    supv.c

INCLUDEPATH += ../../spinnaker_tools_pm/ori_134/include

DISTFILES += \
    Makefile \
    README
