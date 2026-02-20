#include "attendanceoverview.h"
#include "ui_attendanceoverview.h"
#include <QSqlQuery>
#include <QTableWidgetItem>
#include <QDate>

AttendanceOverview::AttendanceOverview(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AttendanceOverview)
{
    ui->setupUi(this);
    setWindowTitle("전체 출석 현황");
    resize(900, 500);
}

AttendanceOverview::~AttendanceOverview()
{
    delete ui;
}

void AttendanceOverview::loadOverview(int year, int month)
{
    // 해당 월의 토/일 날짜 계산
    QList<QPair<QDate, QString>> columns;
    int daysInMonth = QDate(year, month, 1).daysInMonth();

    for (int day = 1; day <= daysInMonth; day++) {
        QDate date(year, month, day);
        if (date.dayOfWeek() == 6) {
            columns.append({date, ""});
        } else if (date.dayOfWeek() == 7) {
            columns.append({date, "1st"});
            columns.append({date, "2nd"});
        }
    }

    // 헤더 설정
    QStringList headers;
    headers << "Name";
    for (auto &col : columns) {
        QDate date = col.first;
        QString service = col.second;
        if (service.isEmpty()) {
            headers << date.toString("M/d(토)");
        } else if (service == "1st") {
            headers << date.toString("M/d(일)\n1부");
        } else {
            headers << date.toString("M/d(일)\n2부");
        }
    }

    ui->tableWidget->clear();
    ui->tableWidget->setRowCount(0);
    ui->tableWidget->setColumnCount(headers.size());
    ui->tableWidget->setHorizontalHeaderLabels(headers);

    // 멤버 불러오기
    QSqlQuery memberQuery;
    memberQuery.exec("SELECT id, name FROM members ORDER BY id");

    int row = 0;
    while (memberQuery.next()) {
        int memberId = memberQuery.value(0).toInt();
        QString name = memberQuery.value(1).toString();

        ui->tableWidget->insertRow(row);
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(name));

        for (int col = 0; col < columns.size(); col++) {
            QDate date = columns[col].first;
            QString service = columns[col].second;

            QSqlQuery attendQuery;
            attendQuery.prepare("SELECT isPresent FROM attendance WHERE memberId = ? AND date = ? AND service = ?");
            attendQuery.addBindValue(memberId);
            attendQuery.addBindValue(date.toString("yyyy-MM-dd"));
            attendQuery.addBindValue(service);
            attendQuery.exec();

            QString text = "";
            if (attendQuery.next()) {
                text = attendQuery.value(0).toBool() ? "O" : "X";
            }
            ui->tableWidget->setItem(row, col + 1, new QTableWidgetItem(text));
        }
        row++;
    }

    ui->tableWidget->horizontalHeader()->setStretchLastSection(false);          // 마지막 날짜의 출석체크란만 가로로 늘어나지 않게 하려고 false로 변경
    ui->tableWidget->resizeColumnsToContents();
}
