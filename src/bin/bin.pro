TEMPLATE = app
TARGET = safeloadertest

CONFIG += c++11
QT += quick

INCLUDEPATH += ../lib
LIBS += -L../lib -lsafeloader
SOURCES += main.cpp

OTHER_FILES += \
    main.qml \
    test.qml \
    test2.qml

RESOURCES += \
    res.qrc

