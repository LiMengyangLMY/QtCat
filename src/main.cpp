#include "desktoppet.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/pet/idle/idle.png")));
    DesktopPet pet;
    pet.show();
    return app.exec();
}
