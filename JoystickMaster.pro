QT += core gui widgets serialport
CONFIG += c++11
TEMPLATE = app
TARGET = JoystickMaster
SOURCES += main.cpp mainwindow.cpp joystickwidget.cpp protocol.cpp serialmaster.cpp
HEADERS += mainwindow.h joystickwidget.h protocol.h serialmaster.h
FORMS += mainwindow.ui
RESOURCES += resources.qrc
win32: RC_ICONS =

