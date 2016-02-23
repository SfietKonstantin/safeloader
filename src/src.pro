TEMPLATE = app
TARGET = safeloader

CONFIG += c++11
QT += quick quick-private qml-private gui-private core-private

SOURCES += \
    main.cpp \
    safeloader.cpp

HEADERS += \
    qobjectptr.h \
    safeloader.h

OTHER_FILES += \
    main.qml \
    test.qml

RESOURCES += \
    res.qrc


