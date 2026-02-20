#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDir>
#include <QDate>
#include <QCheckBox>
#include <QWidget>
#include <QHBoxLayout>
#include <algorithm>

#include "QChartView"
#include "QPieSeries"
#include "QBarSeries"
#include "QBarSet"
#include "QChart"
#include "QBarCategoryAxis"
#include "QValueAxis"
#include <QVBoxLayout>
#include <QPainter>

#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initDatabase();
    initAttendanceTab();

    initAccountTab();

    connect(ui->btnSearchAt, &QPushButton::clicked, this, &MainWindow::loadAttendance);
    connect(ui->btnSaveAt, &QPushButton::clicked, this, &MainWindow::saveAttendance);
    connect(ui->btnOverviewAt, &QPushButton::clicked, this, [this](){
        AttendanceOverview *overview = new AttendanceOverview();
        int year = ui->spinYearAt->value();
        int month = ui->comboMonthAt->currentData().toInt();
        overview->loadOverview(year, month);
        overview->show();
    });

    connect(ui->btnSearchAc, &QPushButton::clicked, this, &MainWindow::loadAccount);
    connect(ui->btnSaveAc, &QPushButton::clicked, this, &MainWindow::saveAccount);
    connect(ui->btnAddAc, &QPushButton::clicked, this, &MainWindow::addAccountRow);
    connect(ui->btnDeleteAc, &QPushButton::clicked, this, &MainWindow::deleteAccountRow);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ---------------------------------------------- init --------------------------------------------------------

void MainWindow::initDatabase()
{
    QString dbPath = QDir::homePath() + "/IMCYouth/imcyouth.db";
    QDir().mkpath(QDir::homePath() + "/IMCYouth");

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qDebug() << "DB 연결 실패:" << db.lastError().text();
        return;
    }

    qDebug() << "DB 연결 성공! 경로:" << dbPath;

    QSqlQuery query;

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password TEXT NOT NULL,
            role TEXT
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS members (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            birthDate TEXT,
            phone TEXT,
            address TEXT,
            workplace TEXT,
            notes TEXT
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS attendance (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            memberId INTEGER,
            date TEXT NOT NULL,
            service TEXT,
            isPresent INTEGER DEFAULT 0,
            FOREIGN KEY (memberId) REFERENCES members(id)
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS transactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            year INTEGER,
            month INTEGER,
            week INTEGER,
            category TEXT,
            description TEXT,
            amount INTEGER,
            type TEXT,
            isBlack INTEGER DEFAULT 0,
            receiptImage TEXT
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS audit_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            userId INTEGER,
            action TEXT,
            targetTable TEXT,
            description TEXT,
            timestamp TEXT,
            FOREIGN KEY (userId) REFERENCES users(id)
        )
    )");

    qDebug() << "모든 테이블 생성 완료!";
}

void MainWindow::initAttendanceTab()
{
    // 년도 SpinBox 설정
    ui->spinYearAt->setRange(2020, 2100);
    ui->spinYearAt->setValue(QDate::currentDate().year());

    // 월 ComboBox 설정
    ui->comboMonthAt->addItem("1월", 1);
    ui->comboMonthAt->addItem("2월", 2);
    ui->comboMonthAt->addItem("3월", 3);
    ui->comboMonthAt->addItem("4월", 4);
    ui->comboMonthAt->addItem("5월", 5);
    ui->comboMonthAt->addItem("6월", 6);
    ui->comboMonthAt->addItem("7월", 7);
    ui->comboMonthAt->addItem("8월", 8);
    ui->comboMonthAt->addItem("9월", 9);
    ui->comboMonthAt->addItem("10월", 10);
    ui->comboMonthAt->addItem("11월", 11);
    ui->comboMonthAt->addItem("12월", 12);

    // 현재 월로 기본 설정
    ui->comboMonthAt->setCurrentIndex(QDate::currentDate().month() - 1);
    // 프로그램 실행 시 현재 월에 해당하는 출석부 자동으로 로드
    loadAttendance();
}

void MainWindow::initAccountTab()
{
    // 년도+월 형식으로 설정
    ui->dateAc->setDisplayFormat("yyyy-MM");
    ui->dateAc->setDate(QDate::currentDate());

    // 테이블 컬럼 설정
    ui->tableAc->setColumnCount(6);
    ui->tableAc->setHorizontalHeaderLabels({"Date", "Category", "Description", "Amount", "Type", "Receipt"});
    ui->tableAc->horizontalHeader()->setStretchLastSection(true);

    loadAccount();
}

// ---------------------------------------------- Attendance --------------------------------------------------------

void MainWindow::loadAttendance()
{
    int year = ui->spinYearAt->value();
    int month = ui->comboMonthAt->currentData().toInt();

    // 해당 월의 토/일 날짜 계산
    QList<QDate> satDates, sunDates;
    int daysInMonth = QDate(year, month, 1).daysInMonth();

    for (int day = 1; day <= daysInMonth; day++) {
        QDate date(year, month, day);
        if (date.dayOfWeek() == 6) satDates.append(date);
        else if (date.dayOfWeek() == 7) sunDates.append(date);
    }

    // 컬럼 구성 (이름 + 토요일 + 주일 1st/2nd)
    QStringList headers;
    headers << "Name";
    QList<QPair<QDate, QString>> columns; // 날짜, service

    // 날짜 순서대로 토/일 합쳐서 정렬
    QList<QDate> allDates;
    for (QDate d : satDates) allDates.append(d);
    for (QDate d : sunDates) allDates.append(d);
    std::sort(allDates.begin(), allDates.end());

    for (QDate d : allDates) {
        if (d.dayOfWeek() == 6) {
            headers << d.toString("M/d(토)");
            columns.append({d, ""});
        } else {
            headers << d.toString("M/d(일) 1부");
            headers << d.toString("M/d(일) 2부");
            columns.append({d, "1st"});
            columns.append({d, "2nd"});
        }
    }

    ui->tableAt->clear();
    ui->tableAt->setRowCount(0);
    ui->tableAt->setColumnCount(headers.size());
    ui->tableAt->setHorizontalHeaderLabels(headers);

    // 멤버 불러오기
    QSqlQuery memberQuery;
    memberQuery.exec("SELECT id, name FROM members ORDER BY id");

    int row = 0;
    while (memberQuery.next()) {
        int memberId = memberQuery.value(0).toInt();
        QString name = memberQuery.value(1).toString();

        ui->tableAt->insertRow(row);
        ui->tableAt->setItem(row, 0, new QTableWidgetItem(name));
        ui->tableAt->item(row, 0)->setData(Qt::UserRole, memberId);

        for (int col = 0; col < columns.size(); col++) {
            QDate date = columns[col].first;
            QString service = columns[col].second;

            QSqlQuery attendQuery;
            attendQuery.prepare("SELECT isPresent FROM attendance WHERE memberId = :memberId AND date = :date AND service = :service");
            attendQuery.bindValue(":memberId", memberId);
            attendQuery.bindValue(":date", date.toString("yyyy-MM-dd"));
            attendQuery.bindValue(":service", service);
            attendQuery.exec();

            QWidget *checkWidget = new QWidget();
            QCheckBox *checkBox = new QCheckBox();
            QHBoxLayout *layout = new QHBoxLayout(checkWidget);
            layout->addWidget(checkBox);
            layout->setAlignment(Qt::AlignCenter);
            layout->setContentsMargins(0, 0, 0, 0);

            if (attendQuery.next()) {
                checkBox->setChecked(attendQuery.value(0).toBool());
            }

            ui->tableAt->setCellWidget(row, col + 1, checkWidget);
        }
        row++;
    }

    ui->tableAt->horizontalHeader()->setStretchLastSection(true);

    // columns 저장 (저장할 때 사용)
    currentColumns = columns;
    updateCharts();
}


void MainWindow::saveAttendance()
{
    qDebug() << "저장 시작, 행 수:" << ui->tableAt->rowCount() << "컬럼 수:" << currentColumns.size();

    for (int row = 0; row < ui->tableAt->rowCount(); row++) {
        int memberId = ui->tableAt->item(row, 0)->data(Qt::UserRole).toInt();

        for (int col = 0; col < currentColumns.size(); col++) {
            QDate date = currentColumns[col].first;
            QString service = currentColumns[col].second;
            QString dateStr = date.toString("yyyy-MM-dd");

            QWidget *checkWidget = ui->tableAt->cellWidget(row, col + 1);
            QCheckBox *checkBox = checkWidget->findChild<QCheckBox*>();
            bool isPresent = checkBox->isChecked();

            // 기존 레코드 확인
            QSqlQuery checkQuery(db);
            checkQuery.prepare("SELECT id FROM attendance WHERE memberId = ? AND date = ? AND service = ?");
            checkQuery.addBindValue(memberId);
            checkQuery.addBindValue(dateStr);
            checkQuery.addBindValue(service);
            checkQuery.exec();

            if (checkQuery.next()) {
                QSqlQuery updateQuery(db);
                updateQuery.prepare("UPDATE attendance SET isPresent = ? WHERE memberId = ? AND date = ? AND service = ?");
                updateQuery.addBindValue(isPresent ? 1 : 0);
                updateQuery.addBindValue(memberId);
                updateQuery.addBindValue(dateStr);
                updateQuery.addBindValue(service);
                updateQuery.exec();
                qDebug() << "UPDATE 결과:" << updateQuery.lastError().text();
            } else {
                QSqlQuery insertQuery(db);
                insertQuery.prepare("INSERT INTO attendance (memberId, date, service, isPresent) VALUES (?, ?, ?, ?)");
                insertQuery.addBindValue(memberId);
                insertQuery.addBindValue(dateStr);
                insertQuery.addBindValue(service);
                insertQuery.addBindValue(isPresent ? 1 : 0);
                insertQuery.exec();
                qDebug() << "INSERT 결과:" << insertQuery.lastError().text();
            }
        }
    }

    QMessageBox::information(this, "저장 완료", "출석이 저장되었습니다!");
}

// ---------------------------------------------- Account --------------------------------------------------------

void MainWindow::loadAccount()
{
    QString yearMonth = ui->dateAc->date().toString("yyyy-MM");

    QSqlQuery query(db);
    query.prepare("SELECT id, date, category, description, amount, type, receiptImage FROM transactions WHERE date LIKE ? ORDER BY date ASC");
    query.addBindValue(yearMonth + "-%");
    query.exec();

    ui->tableAc->setRowCount(0);

    while (query.next()) {
        int row = ui->tableAc->rowCount();
        ui->tableAc->insertRow(row);

        ui->tableAc->setItem(row, 0, new QTableWidgetItem(query.value(1).toString()));
        ui->tableAc->setItem(row, 1, new QTableWidgetItem(query.value(2).toString()));
        ui->tableAc->setItem(row, 2, new QTableWidgetItem(query.value(3).toString()));
        ui->tableAc->setItem(row, 3, new QTableWidgetItem(query.value(4).toString()));
        ui->tableAc->setItem(row, 4, new QTableWidgetItem(query.value(5).toString()));
        ui->tableAc->item(row, 0)->setData(Qt::UserRole, query.value(0).toInt());

        QString receiptPath = query.value(6).toString();
        QPushButton *receiptBtn = new QPushButton();
        if (receiptPath.isEmpty()) {
            receiptBtn->setText("첨부");
            receiptBtn->setStyleSheet("background-color: gray; color: white;");
        } else {
            receiptBtn->setText("영수증 보기");
            receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
        }
        receiptBtn->setProperty("receiptPath", receiptPath);

        connect(receiptBtn, &QPushButton::clicked, this, [this, row, receiptBtn](){
            QString path = receiptBtn->property("receiptPath").toString();
            if (path.isEmpty()) {
                QString filePath = QFileDialog::getOpenFileName(this, "영수증 선택", "", "Images (*.png *.jpg *.jpeg)");
                if (!filePath.isEmpty()) {
                    QString destDir = QDir::homePath() + "/IMCYouth/receipts/";
                    QDir().mkpath(destDir);
                    QString fileName = QFileInfo(filePath).fileName();
                    QString destPath = destDir + fileName;
                    QFile::copy(filePath, destPath);
                    receiptBtn->setText("보기");
                    receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
                    receiptBtn->setProperty("receiptPath", destPath);

                    // DB 업데이트
                    int id = ui->tableAc->item(row, 0)->data(Qt::UserRole).toInt();
                    if (id > 0) {
                        QSqlQuery updateQuery(db);
                        updateQuery.prepare("UPDATE transactions SET receiptImage = ? WHERE id = ?");
                        updateQuery.addBindValue(destPath);
                        updateQuery.addBindValue(id);
                        updateQuery.exec();
                    }
                }
            } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
        });

        ui->tableAc->setCellWidget(row, 5, receiptBtn);
    }
}

void MainWindow::addAccountRow()
{
    int row = ui->tableAc->rowCount();
    ui->tableAc->insertRow(row);

    QString date = QDate::currentDate().toString("yyyy-MM-dd");

    ui->tableAc->setItem(row, 0, new QTableWidgetItem(date));
    ui->tableAc->setItem(row, 1, new QTableWidgetItem(""));
    ui->tableAc->setItem(row, 2, new QTableWidgetItem(""));
    ui->tableAc->setItem(row, 3, new QTableWidgetItem("0"));
    ui->tableAc->setItem(row, 4, new QTableWidgetItem("지출"));
    ui->tableAc->item(row, 0)->setData(Qt::UserRole, 0);

    QPushButton *receiptBtn = new QPushButton("첨부");
    receiptBtn->setStyleSheet("background-color: gray; color: white;");

    connect(receiptBtn, &QPushButton::clicked, this, [this, row, receiptBtn](){
        QString receiptPath = receiptBtn->property("receiptPath").toString();
        if (receiptPath.isEmpty()) {
            QString filePath = QFileDialog::getOpenFileName(this, "영수증 선택", "", "Images (*.png *.jpg *.jpeg)");
            if (!filePath.isEmpty()) {
                QString destDir = QDir::homePath() + "/IMCYouth/receipts/";
                QDir().mkpath(destDir);
                QString fileName = QFileInfo(filePath).fileName();
                QString destPath = destDir + fileName;
                QFile::copy(filePath, destPath);
                receiptBtn->setText("영수증 보기");
                receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
                receiptBtn->setProperty("receiptPath", destPath);

                // DB에 저장된 행이면 바로 업데이트
                int id = ui->tableAc->item(row, 0)->data(Qt::UserRole).toInt();
                if (id > 0) {
                    QSqlQuery updateQuery(db);
                    updateQuery.prepare("UPDATE transactions SET receiptImage = ? WHERE id = ?");
                    updateQuery.addBindValue(destPath);
                    updateQuery.addBindValue(id);
                    updateQuery.exec();
                }
            }
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(receiptPath));
        }
    });

    ui->tableAc->setCellWidget(row, 5, receiptBtn);
    ui->tableAc->editItem(ui->tableAc->item(row, 0));
}
void MainWindow::deleteAccountRow()
{
    int row = ui->tableAc->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "선택 오류", "삭제할 항목을 선택해주세요!");
        return;
    }

    int id = ui->tableAc->item(row, 0)->data(Qt::UserRole).toInt();
    if (id > 0) {
        QSqlQuery query(db);
        query.prepare("DELETE FROM transactions WHERE id = ?");
        query.addBindValue(id);
        query.exec();
    }
    ui->tableAc->removeRow(row);
}

void MainWindow::saveAccount()
{
    qDebug() << "저장 시작, 행 수:" << ui->tableAc->rowCount();

    for (int row = 0; row < ui->tableAc->rowCount(); row++) {
        int id = ui->tableAc->item(row, 0)->data(Qt::UserRole).toInt();
        QString date = ui->tableAc->item(row, 0)->text();
        QString category = ui->tableAc->item(row, 1) ? ui->tableAc->item(row, 1)->text() : "";
        QString description = ui->tableAc->item(row, 2) ? ui->tableAc->item(row, 2)->text() : "";
        int amount = ui->tableAc->item(row, 3) ? ui->tableAc->item(row, 3)->text().toInt() : 0;
        QString type = ui->tableAc->item(row, 4) ? ui->tableAc->item(row, 4)->text() : "";
        QString receipt = ui->tableAc->item(row, 5) ? ui->tableAc->item(row, 5)->text() : "";

        qDebug() << "row:" << row << "id:" << id << "date:" << date << "category:" << category;

        if (id > 0) {
            QSqlQuery query(db);
            query.prepare("UPDATE transactions SET date=?, category=?, description=?, amount=?, type=?, receiptImage=? WHERE id=?");
            query.addBindValue(date);
            query.addBindValue(category);
            query.addBindValue(description);
            query.addBindValue(amount);
            query.addBindValue(type);
            query.addBindValue(receipt);
            query.addBindValue(id);
            bool ok = query.exec();
            qDebug() << "UPDATE 결과:" << ok << query.lastError().text();
        } else {
            QSqlQuery query(db);
            query.prepare("INSERT INTO transactions (date, category, description, amount, type, receiptImage) VALUES (?,?,?,?,?,?)");
            query.addBindValue(date);
            query.addBindValue(category);
            query.addBindValue(description);
            query.addBindValue(amount);
            query.addBindValue(type);
            query.addBindValue(receipt);
            bool ok = query.exec();
            qDebug() << "INSERT 결과:" << ok << query.lastError().text();
        }
    }
    QMessageBox::information(this, "저장 완료", "저장되었습니다!");
}

void MainWindow::updateCharts()
{
    int year = ui->spinYearAt->value();
    int month = ui->comboMonthAt->currentData().toInt();
    int totalMembers = 0;

    QSqlQuery countQuery(db);
    countQuery.exec("SELECT COUNT(*) FROM members");
    if (countQuery.next()) totalMembers = countQuery.value(0).toInt();
    if (totalMembers == 0) return;

    // 토요일 / 1st / 2nd 출석률 계산
    auto calcRate = [&](QString service) -> QPair<int,int> {
        int present = 0, total = 0;
        int daysInMonth = QDate(year, month, 1).daysInMonth();
        for (int day = 1; day <= daysInMonth; day++) {
            QDate date(year, month, day);
            bool isSat = (date.dayOfWeek() == 6 && service == "");
            bool isSun = (date.dayOfWeek() == 7 && service != "");
            if (!isSat && !isSun) continue;
            QSqlQuery q(db);
            q.prepare("SELECT SUM(isPresent), COUNT(*) FROM attendance WHERE date = ? AND service = ?");
            q.addBindValue(date.toString("yyyy-MM-dd"));
            q.addBindValue(service);
            q.exec();
            if (q.next()) {
                present += q.value(0).toInt();
                total += q.value(1).toInt();
            }
        }
        return {present, total};
    };

    auto makeChart = [&](QWidget *container, QString title, QPair<int,int> data) {
        QPieSeries *series = new QPieSeries();
        int present = data.first;
        int absent = data.second - present;
        if (data.second == 0) {
            series->append("No Data", 1);
        } else {
            series->append("출석 " + QString::number(present), present);
            series->append("결석 " + QString::number(absent), absent);
        }

        QChart *chart = new QChart();
        chart->addSeries(series);
        chart->setTitle(title);
        chart->legend()->show();

        QChartView *chartView = new QChartView(chart);
        chartView->setRenderHint(QPainter::Antialiasing);

        QLayout *old = container->layout();
        if (old) {
            QLayoutItem *item;
            while ((item = old->takeAt(0)) != nullptr) {
                delete item->widget();
                delete item;
            }
            delete old;
        }

        QVBoxLayout *layout = new QVBoxLayout(container);
        layout->addWidget(chartView);
        layout->setContentsMargins(0,0,0,0);
    };

    makeChart(ui->chartSatAt, "토요일 평균 출석률", calcRate(""));
    makeChart(ui->chartSun1At, "주일 1부 평균 출석률", calcRate("1st"));
    makeChart(ui->chartSun2At, "주일 2부 평균 출석률", calcRate("2nd"));

    // 연간 막대그래프
    QBarSet *barSet = new QBarSet("평균 출석률(%)");
    QStringList monthLabels;
    for (int m = 1; m <= 12; m++) {
        QSqlQuery q(db);
        q.prepare("SELECT COUNT(DISTINCT date || service) FROM attendance WHERE date LIKE ? AND isPresent = 1");
        q.addBindValue(QString::number(year) + "-" + QString("%1").arg(m, 2, 10, QChar('0')) + "-%");
        q.exec();

        int presentCount = 0;
        if (q.next()) presentCount = q.value(0).toInt();

        // 해당 월의 토/일 횟수 계산
        int sessionCount = 0;
        int daysInMonth = QDate(year, m, 1).daysInMonth();
        for (int day = 1; day <= daysInMonth; day++) {
            QDate date(year, m, day);
            if (date.dayOfWeek() == 6) sessionCount += 1;      // 토요일
            else if (date.dayOfWeek() == 7) sessionCount += 2; // 주일 1st + 2nd
        }

        double rate = 0;
        if (sessionCount > 0 && totalMembers > 0) {
            rate = (double)presentCount / (sessionCount * totalMembers) * 100.0;
        }

        *barSet << qRound(rate);
        monthLabels << QString::number(m) + "월";
    }

    QBarSeries *barSeries = new QBarSeries();
    barSeries->append(barSet);

    QChart *barChart = new QChart();
    barChart->addSeries(barSeries);
    barChart->setTitle(QString::number(year) + "년 월별 출석");

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(monthLabels);
    axisX->setLabelsAngle(-45);  // 레이블 45도 기울이기
    barChart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);

    barChart->setMargins(QMargins(0, 0, 0, 0));
    // barChart->layout()->setContentsMargins(0, 0, 0, 0);

    // 연도 월별 막대 그래프
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 100);            // 전체 인원수 기준(totalMembers) -> 월 평균(100)으로 변경
    axisY->setTickCount(6);
    axisY->setLabelFormat("%d%%");        // 전체 인원수 정수만 표시(%d) -> 평균률(%d%%)로 변경
    barChart->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);

    QChartView *barView = new QChartView(barChart);
    barView->setRenderHint(QPainter::Antialiasing);

    QLayout *old = ui->chartBarAt->layout();
    if (old) {
        QLayoutItem *item;
        while ((item = old->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete old;
    }

    QVBoxLayout *barLayout = new QVBoxLayout(ui->chartBarAt);
    barLayout->addWidget(barView);
    barLayout->setContentsMargins(0,0,0,0);
}
