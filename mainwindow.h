#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "config.h"

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

#include <QDir>
#include <QCoreApplication>

#include <QListWidget>
#include <QStackedWidget>
#include <QFormLayout>
#include <QTableWidget>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>


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

    int currentOfficerId = -1;                      // 현재 로그인한 임원 id (-1: 미로그인)

    // 회계 타입 목록 - 나중에 여기서 추가/수정
    const QStringList ACCOUNT_TYPES = {"지출", "수입"};

//      // ===== break 비밀번호 (변경 시 여기만 수정) =====
//    const QString BREAK_PASSWORD = "0000";
    QString getCode(const QString &name);

    bool isBreakMode = false;

    // ===== 차트 색상 팔레트 (변경 시 여기만 수정) =====
    const QList<QColor> CHART_PastelBlue = {
        QColor("#BED0E8"),
        QColor("#CAE0EF"),
        QColor("#DBEEF2")
    };

    const QList<QColor> CHART_PastelYellow = {
        QColor("#F7F3E8"),
        QColor("#F2F1AC"),
        QColor("#D1D066")
    };
    // ==================================================

    bool eventFilter(QObject *obj, QEvent *event) override;

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

    void loadBreak();
    void saveBreak();
    void addBreakRow();
    void deleteBreakRow();
    void updateBreakCharts();

    void loadMembers();
    void addMemberRow();
    void deleteMember();
    void saveMember();
    void searchMemberByName();

    void initSettingsTab();         // 설정 페이지
    void loadAuditLog();
    void changeAppPassword();

//    QListWidget    *settingsMenu;
//    QStackedWidget *settingsStack;
//    QTableWidget   *auditLogTable;
//    QLineEdit      *leCurrentPw;
//    QLineEdit      *leNewPw;
//    QLineEdit      *leConfirmPw;

};

#endif // MAINWINDOW_H
