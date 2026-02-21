#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QMessageBox>
#include <QDate>
#include <QCloseEvent>
#include <QTimer>
#include <QResizeEvent>
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

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    QList<QPair<QDate, QString>> currentColumns;
    void initDatabase();
    void initAttendanceTab();
    void initAccountTab();
    void initMembersTab();

    void loadAttendance();
    void saveAttendance();
    void updateCharts();            // 출석 그래프

    void loadAccount();
    void saveAccount();
    void addAccountRow();
    void deleteAccountRow();
    void updateAccountCharts();      // 회계 그래프

    void loadMembers();
    void addMemberRow();
    void deleteMember();
    void saveMember();
    void searchMemberByName();
};

#endif // MAINWINDOW_H
