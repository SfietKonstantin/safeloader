TEMPLATE = lib
CONFIG += static
TARGET = safeloader

CONFIG += c++11
QT += quick

HEADERS += \
    qobjectptr.h \
    safeloader.h

SOURCES += \
    safeloader.cpp
