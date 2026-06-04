#include "desktoppet.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    DesktopPet pet;
    pet.show();
    return app.exec();
}
