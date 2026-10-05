#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle(QStringLiteral("Diamond Detector Signal Generator"));
    w.resize(800, 600);
    w.show();
    return a.exec();
}
