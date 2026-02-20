#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QMessageBox>
#include <QDate>
#include "attendanceoverview.h"
#include "QChartView"
#include "QPieSeries"
#include "QBarSeries"
#include "QChart"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    QList<QPair<QDate, QString>> currentColumns;
    void initDatabase();
    void initAttendanceTab();
    void loadAttendance();
    void saveAttendance();
    void updateCharts();        // 출석 그래프

    void initAccountTab();
    void loadAccount();
    void saveAccount();
    void addAccountRow();
    void deleteAccountRow();
};

#endif // MAINWINDOW_H
