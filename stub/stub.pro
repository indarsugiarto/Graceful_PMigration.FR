TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    stub.c

INCLUDEPATH += /opt/spinnaker_tools_134/include

DISTFILES += \
    stub.lnk \
    Makefile \
    sark.lnk
