#include "Pro_Seis_MASW.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Pro_Seis_MASW window;
    window.show();
    return app.exec();
}
