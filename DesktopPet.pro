QT += widgets gui-private

CONFIG += c++17
TEMPLATE = app
TARGET = QtCatPet

SOURCES += \
    src/main.cpp \
    src/desktoppet.cpp \
    src/novelmanager.cpp \
    src/bookdialog.cpp \
    src/ebookparser.cpp \
    src/catrunnerwidget.cpp

HEADERS += \
    src/desktoppet.h \
    src/novelmanager.h \
    src/bookdialog.h \
    src/ebookparser.h \
    src/catrunnerwidget.h

RESOURCES += \
    resources/resources.qrc

win32 {
    RC_FILE += resources/app.rc
}
