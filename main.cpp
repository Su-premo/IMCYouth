#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.resize(1500, 1000);            // 프로그램 창 사이즈 지정
    w.show();
    return a.exec();
}
