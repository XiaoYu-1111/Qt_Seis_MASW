#include "Pro_Seis_WASW.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Pro_Seis_WASW window;
    window.show();
    return app.exec();
}
