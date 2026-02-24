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

#include <QCoreApplication>

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

#include <QLineEdit>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initDatabase();

    initAttendanceTab();
    connect(ui->btnSearchAt, &QPushButton::clicked, this, &MainWindow::loadAttendance);
    connect(ui->btnSaveAt, &QPushButton::clicked, this, &MainWindow::saveAttendance);

    // 출석 탭 선택시에만 전체현황 버튼 표시 / 회계 탭 선택시에만 break 버튼 표시
    // cornerWidget에 전체현황 + break 버튼 함께 배치
    QPushButton *btnBreak = new QPushButton();
    btnBreak->setText("break");
    btnBreak->setFlat(true);
    btnBreak->setCursor(Qt::PointingHandCursor);
    btnBreak->setStyleSheet(
        "QPushButton { background: transparent; border: none; "
        "color: gray; font-weight: bold; "
        "text-decoration: underline; }"
        "QPushButton:hover { color: black; }"
    );
    btnBreak->setVisible(false);

    QWidget *cornerWidget = new QWidget();
    QHBoxLayout *cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 0, 0);
    cornerLayout->addWidget(ui->btnOverviewAt);
    cornerLayout->addWidget(btnBreak);
    ui->tabWidget->setCornerWidget(cornerWidget, Qt::TopRightCorner);

    // tab_4 인덱스 동적으로 계산
    int breakTabIndex = ui->tabWidget->count() - 1;
    ui->tabWidget->tabBar()->setTabVisible(breakTabIndex, false);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this, btnBreak, breakTabIndex](int index){
        ui->btnOverviewAt->setVisible(index == 0);          // 출석탭(0)일때
        btnBreak->setVisible(index == 1);                   // 회계탭(1)일때
        if (index != breakTabIndex) {
            ui->tabWidget->tabBar()->setTabVisible(breakTabIndex, false); // break탭 항상 숨김
        }
    });

    connect(ui->btnOverviewAt, &QPushButton::clicked, this, [this](){
        AttendanceOverview *overview = new AttendanceOverview();
        int year = ui->spinYearAt->value();
        int month = ui->comboMonthAt->currentData().toInt();
        overview->loadOverview(year);
        overview->setAttribute(Qt::WA_DeleteOnClose);           // 닫힐때 메모리 해제 - 프로그램 창 닫히면 전체현황 창도 닫히게 함
        overview->show();
    });

    connect(btnBreak, &QPushButton::clicked, this, [this, breakTabIndex](){
        ui->tabWidget->tabBar()->setTabVisible(breakTabIndex, true);
        ui->tabWidget->setCurrentIndex(breakTabIndex);
    });

    initAccountTab();
    connect(ui->btnSearchAc, &QPushButton::clicked, this, &MainWindow::loadAccount);
    connect(ui->btnSaveAc, &QPushButton::clicked, this, &MainWindow::saveAccount);
    connect(ui->btnAddAc, &QPushButton::clicked, this, &MainWindow::addAccountRow);
    connect(ui->btnDeleteAc, &QPushButton::clicked, this, &MainWindow::deleteAccountRow);

    initMembersTab();
    connect(ui->btnSearchMember, &QPushButton::clicked, this, &MainWindow::searchMemberByName);
    connect(ui->searchMember, &QLineEdit::returnPressed, this, &MainWindow::searchMemberByName);
    connect(ui->btnAddMember, &QPushButton::clicked, this, &MainWindow::addMemberRow);
    connect(ui->btnDeleteMember, &QPushButton::clicked, this, &MainWindow::deleteMember);
    connect(ui->btnSaveMember, &QPushButton::clicked, this, &MainWindow::saveMember);

    initBreakTab();
    connect(ui->btnSearchBreak, &QPushButton::clicked, this, &MainWindow::loadBreak);
    connect(ui->btnSaveBreak, &QPushButton::clicked, this, &MainWindow::saveBreak);
    connect(ui->btnAddBreak, &QPushButton::clicked, this, &MainWindow::addBreakRow);
    connect(ui->btnDeleteBreak, &QPushButton::clicked, this, &MainWindow::deleteBreakRow);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 자식 위젯 모두 닫기
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget != this) {
            widget->close();
        }
    }
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // 출석 테이블 날짜 컬럼 너비 재계산
    int dateColCount = ui->tableAt->columnCount() - 2;
    if (dateColCount <= 0) return;

    int availableWidth = ui->tableAt->width() - 40 - 100 - 20;
    int dateColWidth = availableWidth / dateColCount;

    for (int i = 2; i < ui->tableAt->columnCount(); i++) {
        ui->tableAt->setColumnWidth(i, dateColWidth);
    }
}

// ---------------------------------------------- init --------------------------------------------------------

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

    // 전체 현황 버튼
    ui->btnOverviewAt->setStyleSheet(
        "QPushButton { background: transparent; border: none; "
        "color: blue; padding: 3px 4px; font-weight: 700; "
        "text-decoration: underline; }"
        "QPushButton:hover { color: darkblue; }"
    );
    ui->btnOverviewAt->setCursor(Qt::PointingHandCursor);
    ui->btnOverviewAt->setMaximumWidth(80);
    ui->btnOverviewAt->setMaximumHeight(22);
    ui->btnOverviewAt->setContentsMargins(0, 5, 0, 0); // 위쪽 마진 5px


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
    ui->tableAc->setColumnCount(7);
    ui->tableAc->setHorizontalHeaderLabels({"#", "Date", "Category", "Description", "Amount", "Type", "Receipt"});
    ui->tableAc->verticalHeader()->hide();
    ui->tableAc->horizontalHeader()->setStretchLastSection(false);
    ui->tableAc->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(0, 30);   // #
    ui->tableAc->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(1, 120);  // Date
    ui->tableAc->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(2, 130);  // Category
    ui->tableAc->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); // Description 자동
    ui->tableAc->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(4, 100);   // Amount
    ui->tableAc->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(5, 100);   // Type
    ui->tableAc->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(6, 120);   // Receipt
//    ui->tableAc->setHorizontalHeaderLabels({"#", "Date", "Category", "Description", "Amount", "Type", "Receipt"});
//    ui->tableAc->verticalHeader()->hide();
//    ui->tableAc->setColumnWidth(0, 40);  // #
//    ui->tableAc->setColumnWidth(1, 100);  // Date
//    ui->tableAc->setColumnWidth(2, 120);  // Category
////    ui->tableAc->setColumnWidth(3, 180);  // Description
//    ui->tableAc->setColumnWidth(4, 80);   // Amount
//    ui->tableAc->setColumnWidth(5, 80);   // Type
//    ui->tableAc->setColumnWidth(6, 80);   // Receipt
////    ui->tableAc->horizontalHeader()->setStretchLastSection(true);
//    ui->tableAc->horizontalHeader()->setStretchLastSection(false);
//    ui->tableAc->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); // Description만 늘어나게


    // 컬럼 오토포커스 제거
    ui->tableAc->setFocusPolicy(Qt::NoFocus);
    ui->dateAc->setFocusPolicy(Qt::NoFocus);

    loadAccount();
}

void MainWindow::initMembersTab()
{
    auto setupTable = [](QTableWidget *table) {
        table->setColumnCount(7);
        table->setHorizontalHeaderLabels({"#", "Name", "BirthDate", "Phone", "Address", "Workplace", "Notes"});
        table->verticalHeader()->hide();
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::DoubleClicked);
        // # 컬럼만 고정, 나머지 자동 분배
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->setColumnWidth(0, 40);
        table->setColumnWidth(1, 100);  // Name
        table->setColumnWidth(2, 170);  // BirthDate
        table->setColumnWidth(3, 170);  // Phone
//        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);  // Name
//        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);  // BirthDate
//        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);  // Phone
        table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);  // Address
        table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);  // Workplace
        table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);  // Notes
    };

    setupTable(ui->tableMembers);
    setupTable(ui->tableMembersInactive);

    // 단일 선택 연동
    connect(ui->tableMembers, &QTableWidget::itemSelectionChanged, this, [this](){
        if (!ui->tableMembers->selectedItems().isEmpty()) {
            ui->tableMembersInactive->clearSelection();
        }
    });
    connect(ui->tableMembersInactive, &QTableWidget::itemSelectionChanged, this, [this](){
        if (!ui->tableMembersInactive->selectedItems().isEmpty()) {
            ui->tableMembers->clearSelection();
        }
    });

    // 일반 테이블 3 : 미출석 테이블 1 비율
    ui->tableMembers->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->tableMembersInactive->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->tableMembers->setMinimumHeight(330);
    ui->tableMembersInactive->setMaximumHeight(230);

    // 오토포커스 제거
    ui->tableMembers->setFocusPolicy(Qt::NoFocus);
    ui->tableMembersInactive->setFocusPolicy(Qt::NoFocus);

    // 텍스트에 따라 컬럼 가로길이 조정
//    ui->tableMembers->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->labelInactive->setText("⚠️ 3개월 이상 미출석 멤버");
    loadMembers();
}

void MainWindow::initDatabase()
{
    // 폴더 생성
    QDir().mkpath(BASE_PATH);
    QDir().mkpath(BASE_PATH + "/receipts");

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(BASE_PATH + "/imcyouth.db");

    if (!db.open()) {
        qDebug() << "DB 연결 실패:" << db.lastError().text();
        return;
    }

    qDebug() << "DB 연결 성공! 경로:" << BASE_PATH + "/imcyouth.db";

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
            isBlack INTEGER DEFAULT 0,  -- 0: 일반회계, 1: 브레이크머니
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
    headers << "#";
    headers << "Name";
    QList<QPair<QDate, QString>> columns; // 날짜, service

    // 날짜 순서대로 토/일 합쳐서 정렬
    QList<QDate> allDates;
    for (QDate d : satDates) allDates.append(d);
    for (QDate d : sunDates) allDates.append(d);
    std::sort(allDates.begin(), allDates.end());

    for (QDate d : allDates) {
        if (d.dayOfWeek() == 6) {
            headers << d.toString("M/d (토)\n");
            columns.append({d, ""});
        } else {
            headers << d.toString("M/d (일)\n1부");
            headers << d.toString("M/d (일)\n2부");
            columns.append({d, "1st"});
            columns.append({d, "2nd"});
        }
    }

    ui->tableAt->clear();
    ui->tableAt->setRowCount(0);
    ui->tableAt->setColumnCount(headers.size());
    ui->tableAt->setHorizontalHeaderLabels(headers);
    ui->tableAt->verticalHeader()->hide();

/*
    // 컬럼 가로 크기 설정 (# 고정, Name 고정, 날짜 컬럼 화면에 맞게 자동 계산)
    ui->tableAt->horizontalHeader()->setStretchLastSection(false);
    ui->tableAt->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableAt->setColumnWidth(0, 40);   // #
    ui->tableAt->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->tableAt->setColumnWidth(1, 100);  // Name
    int dateColCount = headers.size() - 2;  // # 과 Name 제외
    int availableWidth = ui->tableAt->width() - 40 - 100 - 20;  // 스크롤바 여백 20 (오른쪽 공백 생겼음) -> 2로 수정
    int dateColWidth = (dateColCount > 0) ? availableWidth / dateColCount : 90;
    for (int i = 2; i < headers.size(); i++) {
        ui->tableAt->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
        ui->tableAt->setColumnWidth(i, dateColWidth);  // 자동 계산된 너비
    }
*/
    // 컬럼 가로 크기 설정 - Stretch로 자동 분배
    ui->tableAt->horizontalHeader()->setStretchLastSection(false);
    ui->tableAt->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableAt->setColumnWidth(0, 40);   // #
    ui->tableAt->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->tableAt->setColumnWidth(1, 100);  // Name
    for (int i = 2; i < headers.size(); i++) {
        ui->tableAt->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);  // 날짜 컬럼 자동 균등분배
    }

    // 멤버 불러오기
    QSqlQuery memberQuery;
    memberQuery.exec("SELECT id, name FROM members ORDER BY id");

    int row = 0;
    while (memberQuery.next()) {
        int memberId = memberQuery.value(0).toInt();
        QString name = memberQuery.value(1).toString();

        ui->tableAt->insertRow(row);
//        ui->tableAt->setItem(row, 0, new QTableWidgetItem(name));
//        ui->tableAt->item(row, 0)->setData(Qt::UserRole, memberId);

        // 인덱스
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        ui->tableAt->setItem(row, 0, indexItem);

        // 이름
        QTableWidgetItem *nameItem = new QTableWidgetItem(name);
        nameItem->setTextAlignment(Qt::AlignCenter);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        ui->tableAt->setItem(row, 1, nameItem);
        ui->tableAt->item(row, 1)->setData(Qt::UserRole, memberId);

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

            ui->tableAt->setCellWidget(row, col + 2, checkWidget);
        }
        ui->tableAt->setRowHeight(row, 25);             // 테이블 컬럼 세로 길이 맞춤
        row++;
    }

    // columns 저장 (저장할 때 사용)
    currentColumns = columns;
    updateCharts();

    // 컬럼 너비 재계산 (타이밍 문제 방지)
    QTimer::singleShot(0, this, [this](){
        resizeEvent(nullptr);
    });
}

void MainWindow::saveAttendance()
{
    qDebug() << "저장 시작, 행 수:" << ui->tableAt->rowCount() << "컬럼 수:" << currentColumns.size();

    for (int row = 0; row < ui->tableAt->rowCount(); row++) {
        int memberId = ui->tableAt->item(row, 1)->data(Qt::UserRole).toInt();

        for (int col = 0; col < currentColumns.size(); col++) {
            QDate date = currentColumns[col].first;
            QString service = currentColumns[col].second;
            QString dateStr = date.toString("yyyy-MM-dd");

            QWidget *checkWidget = ui->tableAt->cellWidget(row, col + 2);
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
    axisX->setLabelsVisible(true);
    barChart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);
    barChart->setMargins(QMargins(0, 0, 0, 20)); // 하단 여백 추가

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

        // 가운데 정렬
        auto makeItem = [](QString text) {
            QTableWidgetItem *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            return item;
        };

        // 인덱스 추가
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        ui->tableAc->setItem(row, 0, indexItem);

        ui->tableAc->setItem(row, 1, makeItem(query.value(1).toString())); // date
        ui->tableAc->setItem(row, 2, makeItem(query.value(2).toString())); // category
        ui->tableAc->setItem(row, 3, makeItem(query.value(3).toString())); // description
        ui->tableAc->setItem(row, 4, makeItem(query.value(4).toString())); // amount
        ui->tableAc->item(row, 1)->setData(Qt::UserRole, query.value(0).toInt());

        // 수입/지출 콤보박스
        QComboBox *typeCombo = new QComboBox();
        typeCombo->addItems(ACCOUNT_TYPES);
        typeCombo->setCurrentText(query.value(5).toString());
        typeCombo->setStyleSheet("QComboBox { qproperty-alignment: AlignCenter; }");
        ui->tableAc->setCellWidget(row, 5, typeCombo);

        // 영수증 버튼
        QString receiptPath = query.value(6).toString();
        QPushButton *receiptBtn = new QPushButton();
        if (receiptPath.isEmpty()) {
            receiptBtn->setText("첨부");
            receiptBtn->setStyleSheet("background-color: gray; color: white;");
        } else {
            receiptBtn->setText("보기");
            receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
        }
        receiptBtn->setProperty("receiptPath", receiptPath);

        connect(receiptBtn, &QPushButton::clicked, this, [this, row, receiptBtn](){
            QString path = receiptBtn->property("receiptPath").toString();
            if (path.isEmpty()) {
                QString filePath = QFileDialog::getOpenFileName(this, "영수증 선택", "", "Images (*.png *.jpg *.jpeg)");
                if (!filePath.isEmpty()) {
                    QString destDir = BASE_PATH + "/receipts/";
                    QDir().mkpath(destDir);
                    QString fileName = QFileInfo(filePath).fileName();
                    QString destPath = destDir + fileName;
                    QFile::copy(filePath, destPath);
                    receiptBtn->setText("보기");
                    receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
                    receiptBtn->setProperty("receiptPath", destPath);
                    int id = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();

                    // DB 업데이트
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
        ui->tableAc->setCellWidget(row, 6, receiptBtn);
    }
    updateAccountCharts();
}

void MainWindow::addAccountRow()
{
    int row = ui->tableAc->rowCount();
    ui->tableAc->insertRow(row);

    // 가운데 정렬
    auto makeItem = [](QString text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        return item;
    };

    QString date = QDate::currentDate().toString("yyyy-MM-dd");

    QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
    indexItem->setTextAlignment(Qt::AlignCenter);
    indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
    ui->tableAc->setItem(row, 0, indexItem);

    ui->tableAc->setItem(row, 1, makeItem(date));
    ui->tableAc->setItem(row, 2, makeItem(""));
    ui->tableAc->setItem(row, 3, makeItem(""));
    ui->tableAc->setItem(row, 4, makeItem("0"));
    ui->tableAc->item(row, 1)->setData(Qt::UserRole, 0);

    // 수입/지출 콤보박스 (디폴트: 지출)
    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItems(ACCOUNT_TYPES);
    typeCombo->setCurrentText("지출");
    typeCombo->setStyleSheet("QComboBox { qproperty-alignment: AlignCenter; }");
    ui->tableAc->setCellWidget(row, 5, typeCombo);

    QPushButton *receiptBtn = new QPushButton("첨부");
    receiptBtn->setStyleSheet("background-color: gray; color: white;");
    connect(receiptBtn, &QPushButton::clicked, this, [this, row, receiptBtn](){
        QString receiptPath = receiptBtn->property("receiptPath").toString();
        if (receiptPath.isEmpty()) {
            QString filePath = QFileDialog::getOpenFileName(this, "영수증 선택", "", "Images (*.png *.jpg *.jpeg)");
            if (!filePath.isEmpty()) {
                QString destDir = BASE_PATH + "/receipts/";
                QDir().mkpath(destDir);
                QString fileName = QFileInfo(filePath).fileName();
                QString destPath = destDir + fileName;
                QFile::copy(filePath, destPath);
                receiptBtn->setText("보기");
                receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
                receiptBtn->setProperty("receiptPath", destPath);

                // DB에 저장된 행이면 바로 업데이트
                int id = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();
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

    ui->tableAc->setCellWidget(row, 6, receiptBtn);
    ui->tableAc->editItem(ui->tableAc->item(row, 1));
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
    for (int row = 0; row < ui->tableAc->rowCount(); row++) {
        int id = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();
        QString date = ui->tableAc->item(row, 1)->text();
        QString category = ui->tableAc->item(row, 2) ? ui->tableAc->item(row, 2)->text() : "";
        QString description = ui->tableAc->item(row, 3) ? ui->tableAc->item(row, 3)->text() : "";
        int amount = ui->tableAc->item(row, 4) ? ui->tableAc->item(row, 4)->text().toInt() : 0;
        QString type = ui->tableAc->item(row, 5) ? ui->tableAc->item(row, 5)->text() : "";

        if (id > 0) {
            QSqlQuery query(db);
            query.prepare("UPDATE transactions SET date=?, category=?, description=?, amount=?, type=? WHERE id=?");
            query.addBindValue(date);
            query.addBindValue(category);
            query.addBindValue(description);
            query.addBindValue(amount);
            query.addBindValue(type);
            query.addBindValue(id);
            query.exec();
        } else {
            QSqlQuery query(db);
            query.prepare("INSERT INTO transactions (date, category, description, amount, type) VALUES (?,?,?,?,?)");
            query.addBindValue(date);
            query.addBindValue(category);
            query.addBindValue(description);
            query.addBindValue(amount);
            query.addBindValue(type);
            query.exec();
        }
    }
    QMessageBox::information(this, "저장 완료", "저장되었습니다!");
}

void MainWindow::updateAccountCharts()
{
    QString yearMonth = ui->dateAc->date().toString("yyyy-MM");

    // 수입/지출 합계 계산
    QSqlQuery query(db);
    query.prepare("SELECT type, SUM(amount) FROM transactions WHERE date LIKE ? GROUP BY type");
    query.addBindValue(yearMonth + "-%");
    query.exec();

    int income = 0, expense = 0;
    while (query.next()) {
        QString type = query.value(0).toString();
        int amount = query.value(1).toInt();
        if (type == "수입") income = amount;
        else if (type == "지출") expense = amount;
    }

    // 원그래프 (수입 vs 지출)
    QPieSeries *pieSeries = new QPieSeries();
    if (income == 0 && expense == 0) {
        pieSeries->append("No Data", 1);
    } else {
        pieSeries->append("수입 " + QString::number(income), income);
        pieSeries->append("지출 " + QString::number(expense), expense);
    }

    QChart *pieChart = new QChart();
    pieChart->addSeries(pieSeries);
    pieChart->setTitle("수입 / 지출 비율");
    pieChart->legend()->show();

    QChartView *pieView = new QChartView(pieChart);
    pieView->setRenderHint(QPainter::Antialiasing);

    QLayout *oldPie = ui->chartPieAc->layout();
    if (oldPie) {
        QLayoutItem *item;
        while ((item = oldPie->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldPie;
    }
    QVBoxLayout *pieLayout = new QVBoxLayout(ui->chartPieAc);
    pieLayout->addWidget(pieView);
    pieLayout->setContentsMargins(0,0,0,0);

    // 막대그래프 (월별 수입/지출)
    QBarSet *incomeSet = new QBarSet("수입");
    QBarSet *expenseSet = new QBarSet("지출");
    QStringList monthLabels;

    int year = ui->dateAc->date().year();
    for (int m = 1; m <= 12; m++) {
        QSqlQuery q(db);
        q.prepare("SELECT type, SUM(amount) FROM transactions WHERE date LIKE ? GROUP BY type");
        q.addBindValue(QString::number(year) + "-" + QString("%1").arg(m, 2, 10, QChar('0')) + "-%");
        q.exec();

        int inc = 0, exp = 0;
        while (q.next()) {
            QString type = q.value(0).toString();
            int amount = q.value(1).toInt();
            if (type == "수입") inc = amount;
            else if (type == "지출") exp = amount;
        }
        *incomeSet << inc;
        *expenseSet << exp;
        monthLabels << QString::number(m) + "월";
    }

    QBarSeries *barSeries = new QBarSeries();
    barSeries->append(incomeSet);
    barSeries->append(expenseSet);

    QChart *barChart = new QChart();
    barChart->addSeries(barSeries);
    barChart->setTitle(QString::number(year) + "년 월별 수입/지출");

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(monthLabels);
    axisX->setLabelsAngle(-45);
    barChart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setLabelFormat("%d");
    barChart->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);

    QChartView *barView = new QChartView(barChart);
    barView->setRenderHint(QPainter::Antialiasing);

    QLayout *oldBar = ui->chartBarAc->layout();
    if (oldBar) {
        QLayoutItem *item;
        while ((item = oldBar->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldBar;
    }
    QVBoxLayout *barLayout = new QVBoxLayout(ui->chartBarAc);
    barLayout->addWidget(barView);
    barLayout->setContentsMargins(0,0,0,0);
}

// ---------------------------------------------- Members --------------------------------------------------------

void MainWindow::loadMembers()
{
    ui->tableMembers->setRowCount(0);
    ui->tableMembersInactive->setRowCount(0);

    // 기준 변경 필요시 아래 숫자 수정 (현재: 84일 = 12주 = 3개월)
    QDate cutoffDate = QDate::currentDate().addDays(-84);
    QString cutoffStr = cutoffDate.toString("yyyy-MM-dd");

    auto addRowToTable = [](QTableWidget *table, int index, QVariant memberId,
                            QString name, QString birthDate, QString phone,
                            QString address, QString workplace, QString notes) {
        int row = table->rowCount();
        table->insertRow(row);

        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(index));
        indexItem->setTextAlignment(Qt::AlignCenter);
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 0, indexItem);

        auto makeItem = [](QString text) {
            QTableWidgetItem *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            return item;
        };

        table->setItem(row, 1, makeItem(name));
        table->setItem(row, 2, makeItem(birthDate));
        table->setItem(row, 3, makeItem(phone));
        table->setItem(row, 4, makeItem(address));
        table->setItem(row, 5, makeItem(workplace));
        // notes가 비어있으면 하이픈 표시
        table->setItem(row, 6, makeItem(notes.isEmpty() ? "-" : notes));
        table->item(row, 1)->setData(Qt::UserRole, memberId);
    };

    QSqlQuery query(db);
    query.exec("SELECT id, name, birthDate, phone, address, workplace, notes FROM members ORDER BY id");

    int activeIndex = 1, inactiveIndex = 1;
    while (query.next()) {
        int memberId = query.value(0).toInt();

        QSqlQuery attendQuery(db);
        attendQuery.prepare("SELECT MAX(date) FROM attendance WHERE memberId = ? AND isPresent = 1");
        attendQuery.addBindValue(memberId);
        attendQuery.exec();

        bool isInactive = false;
        if (attendQuery.next()) {
            QString lastAttendDate = attendQuery.value(0).toString();
            if (lastAttendDate.isEmpty() || lastAttendDate < cutoffStr) {
                isInactive = true;
            }
        } else {
            isInactive = true;
        }

        if (isInactive) {
            addRowToTable(ui->tableMembersInactive, inactiveIndex++, memberId,
                query.value(1).toString(), query.value(2).toString(),
                query.value(3).toString(), query.value(4).toString(),
                query.value(5).toString(), query.value(6).toString());
        } else {
            addRowToTable(ui->tableMembers, activeIndex++, memberId,
                query.value(1).toString(), query.value(2).toString(),
                query.value(3).toString(), query.value(4).toString(),
                query.value(5).toString(), query.value(6).toString());
        }
    }
}

void MainWindow::searchMemberByName()
{
    QString keyword = ui->searchMember->text().trimmed();
    if (keyword.isEmpty()) {
        loadMembers();
        return;
    }

    ui->tableMembers->setRowCount(0);
    ui->tableMembersInactive->setRowCount(0);

    QDate cutoffDate = QDate::currentDate().addDays(-84);
    QString cutoffStr = cutoffDate.toString("yyyy-MM-dd");

    auto addRowToTable = [](QTableWidget *table, int index, QVariant memberId,
                            QString name, QString birthDate, QString phone,
                            QString address, QString workplace, QString notes) {
        int row = table->rowCount();
        table->insertRow(row);

        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(index));
        indexItem->setTextAlignment(Qt::AlignCenter);
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 0, indexItem);

        auto makeItem = [](QString text) {
            QTableWidgetItem *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            return item;
        };

        table->setItem(row, 1, makeItem(name));
        table->setItem(row, 2, makeItem(birthDate));
        table->setItem(row, 3, makeItem(phone));
        table->setItem(row, 4, makeItem(address));
        table->setItem(row, 5, makeItem(workplace));
        table->setItem(row, 6, makeItem(notes));
        table->item(row, 1)->setData(Qt::UserRole, memberId);
    };

    QSqlQuery query(db);
    query.prepare("SELECT id, name, birthDate, phone, address, workplace, notes FROM members WHERE name LIKE ? ORDER BY id");
    query.addBindValue("%" + keyword + "%");
    query.exec();

    int activeIndex = 1, inactiveIndex = 1;
    while (query.next()) {
        int memberId = query.value(0).toInt();

        QSqlQuery attendQuery(db);
        attendQuery.prepare("SELECT MAX(date) FROM attendance WHERE memberId = ? AND isPresent = 1");
        attendQuery.addBindValue(memberId);
        attendQuery.exec();

        bool isInactive = false;
        if (attendQuery.next()) {
            QString lastAttendDate = attendQuery.value(0).toString();
            if (lastAttendDate.isEmpty() || lastAttendDate < cutoffStr) {
                isInactive = true;
            }
        } else {
            isInactive = true;
        }

        if (isInactive) {
            addRowToTable(ui->tableMembersInactive, inactiveIndex++, memberId,
                query.value(1).toString(), query.value(2).toString(),
                query.value(3).toString(), query.value(4).toString(),
                query.value(5).toString(), query.value(6).toString());
        } else {
            addRowToTable(ui->tableMembers, activeIndex++, memberId,
                query.value(1).toString(), query.value(2).toString(),
                query.value(3).toString(), query.value(4).toString(),
                query.value(5).toString(), query.value(6).toString());
        }
    }
}

void MainWindow::addMemberRow()
{
    int row = ui->tableMembers->rowCount();
    ui->tableMembers->insertRow(row);

    QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
    indexItem->setTextAlignment(Qt::AlignCenter);
    indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
    ui->tableMembers->setItem(row, 0, indexItem);

    for (int col = 1; col <= 6; col++) {
        QTableWidgetItem *item = new QTableWidgetItem("");
        item->setTextAlignment(Qt::AlignCenter);
        ui->tableMembers->setItem(row, col, item);
    }
    ui->tableMembers->item(row, 1)->setData(Qt::UserRole, 0);
    ui->tableMembers->editItem(ui->tableMembers->item(row, 1));
}

void MainWindow::deleteMember()
{
    QTableWidget *activeTable = nullptr;
    int row = -1;

    if (ui->tableMembers->currentRow() >= 0) {
        activeTable = ui->tableMembers;
        row = ui->tableMembers->currentRow();
    } else if (ui->tableMembersInactive->currentRow() >= 0) {
        activeTable = ui->tableMembersInactive;
        row = ui->tableMembersInactive->currentRow();
    }

    if (!activeTable || row < 0) {
        QMessageBox::warning(this, "선택 오류", "삭제할 멤버를 선택해주세요!");
        return;
    }

    int id = activeTable->item(row, 1)->data(Qt::UserRole).toInt();
    QMessageBox::StandardButton reply = QMessageBox::question(this, "삭제 확인", "정말 삭제하시겠습니까?", QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        if (id > 0) {
            QSqlQuery query(db);
            query.prepare("DELETE FROM members WHERE id = ?");
            query.addBindValue(id);
            query.exec();
        }
        activeTable->removeRow(row);
    }
}

void MainWindow::saveMember()
{
    auto saveTable = [&](QTableWidget *table) {
        for (int row = 0; row < table->rowCount(); row++) {
            int id = table->item(row, 1)->data(Qt::UserRole).toInt();
            QString name = table->item(row, 1)->text();
            QString birthDate = table->item(row, 2) ? table->item(row, 2)->text() : "";
            QString phone = table->item(row, 3) ? table->item(row, 3)->text() : "";
            QString address = table->item(row, 4) ? table->item(row, 4)->text() : "";
            QString workplace = table->item(row, 5) ? table->item(row, 5)->text() : "";
            QString notes = table->item(row, 6) ? table->item(row, 6)->text() : "";

            if (name.isEmpty()) continue;

            if (id > 0) {
                QSqlQuery query(db);
                query.prepare("UPDATE members SET name=?, birthDate=?, phone=?, address=?, workplace=?, notes=? WHERE id=?");
                query.addBindValue(name);
                query.addBindValue(birthDate);
                query.addBindValue(phone);
                query.addBindValue(address);
                query.addBindValue(workplace);
                query.addBindValue(notes);
                query.addBindValue(id);
                query.exec();
            } else {
                QSqlQuery query(db);
                query.prepare("INSERT INTO members (name, birthDate, phone, address, workplace, notes) VALUES (?,?,?,?,?,?)");
                query.addBindValue(name);
                query.addBindValue(birthDate);
                query.addBindValue(phone);
                query.addBindValue(address);
                query.addBindValue(workplace);
                query.addBindValue(notes);
                query.exec();
            }
        }
    };

    saveTable(ui->tableMembers);
    saveTable(ui->tableMembersInactive);

    QMessageBox::information(this, "저장 완료", "멤버 정보가 저장되었습니다!");
    loadMembers();
}

// ---------------------------------------------- break --------------------------------------------------------

void MainWindow::initBreakTab()
{
    // 브레이크머니 테이블 컬럼 설정
    ui->tableBreak->setColumnCount(7);
    ui->tableBreak->setHorizontalHeaderLabels({"#", "Date", "Category", "Description", "Amount", "Type", "Receipt"});
    ui->tableBreak->verticalHeader()->hide();
    ui->tableBreak->horizontalHeader()->setStretchLastSection(false);
    ui->tableBreak->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableBreak->setColumnWidth(0, 40);   // #
    ui->tableBreak->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->tableBreak->setColumnWidth(1, 100);  // Date
    ui->tableBreak->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->tableBreak->setColumnWidth(2, 120);  // Category
    ui->tableBreak->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); // Description
    ui->tableBreak->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    ui->tableBreak->setColumnWidth(4, 80);   // Amount
    ui->tableBreak->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    ui->tableBreak->setColumnWidth(5, 80);   // Type
    ui->tableBreak->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    ui->tableBreak->setColumnWidth(6, 80);   // Receipt
    ui->tableBreak->setFocusPolicy(Qt::NoFocus);

    // 날짜 기본값 현재 달
    ui->dateBreak->setDate(QDate::currentDate());

    loadBreak();
}

void MainWindow::loadBreak()
{
    QString yearMonth = ui->dateBreak->date().toString("yyyy-MM");

    // 전체 항목 조회 (isBlack=0 + isBlack=1)
    QSqlQuery query(db);
    query.prepare("SELECT id, date, category, description, amount, type, receiptImage, isBlack FROM transactions WHERE date LIKE ? ORDER BY date ASC");
    query.addBindValue(yearMonth + "-%");
    query.exec();

    ui->tableBreak->setRowCount(0);

    int index = 1;
    while (query.next()) {
        int row = ui->tableBreak->rowCount();
        ui->tableBreak->insertRow(row);

        auto makeItem = [](QString text) {
            QTableWidgetItem *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            return item;
        };

        bool isBlack = query.value(7).toBool();

        // 인덱스
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(index++));
        indexItem->setTextAlignment(Qt::AlignCenter);
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        ui->tableBreak->setItem(row, 0, indexItem);

        ui->tableBreak->setItem(row, 1, makeItem(query.value(1).toString())); // date
        ui->tableBreak->setItem(row, 2, makeItem(query.value(2).toString())); // category
        ui->tableBreak->setItem(row, 3, makeItem(query.value(3).toString())); // description
        ui->tableBreak->setItem(row, 4, makeItem(query.value(4).toString())); // amount
        ui->tableBreak->item(row, 1)->setData(Qt::UserRole, query.value(0).toInt());

        // 수입/지출 콤보박스
        QComboBox *typeCombo = new QComboBox();
        typeCombo->addItems(ACCOUNT_TYPES);
        typeCombo->setCurrentText(query.value(5).toString());
        ui->tableBreak->setCellWidget(row, 5, typeCombo);

        // 영수증 버튼
        QString receiptPath = query.value(6).toString();
        QPushButton *receiptBtn = new QPushButton();
        if (receiptPath.isEmpty()) {
            receiptBtn->setText("첨부");
            receiptBtn->setStyleSheet("background-color: gray; color: white;");
        } else {
            receiptBtn->setText("보기");
            receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
        }
        receiptBtn->setProperty("receiptPath", receiptPath);
        connect(receiptBtn, &QPushButton::clicked, this, [this, row, receiptBtn](){
            QString path = receiptBtn->property("receiptPath").toString();
            if (path.isEmpty()) {
                QString filePath = QFileDialog::getOpenFileName(this, "영수증 선택", "", "Images (*.png *.jpg *.jpeg)");
                if (!filePath.isEmpty()) {
                    QString destDir = BASE_PATH + "/receipts/";
                    QDir().mkpath(destDir);
                    QString fileName = QFileInfo(filePath).fileName();
                    QString destPath = destDir + fileName;
                    QFile::copy(filePath, destPath);
                    receiptBtn->setText("보기");
                    receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
                    receiptBtn->setProperty("receiptPath", destPath);
                    int id = ui->tableBreak->item(row, 1)->data(Qt::UserRole).toInt();
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
        ui->tableBreak->setCellWidget(row, 6, receiptBtn);

        // 브레이크머니 항목 배경색 표시
        if (isBlack) {
            for (int col = 0; col < 5; col++) {
                if (ui->tableBreak->item(row, col))
                    ui->tableBreak->item(row, col)->setBackground(QColor("#FFE0E0"));
            }
        }
    }
    updateBreakCharts();
}

void MainWindow::addBreakRow()
{
    int row = ui->tableBreak->rowCount();
    ui->tableBreak->insertRow(row);

    auto makeItem = [](QString text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        return item;
    };

    QString date = QDate::currentDate().toString("yyyy-MM-dd");

    QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
    indexItem->setTextAlignment(Qt::AlignCenter);
    indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
    ui->tableBreak->setItem(row, 0, indexItem);

    ui->tableBreak->setItem(row, 1, makeItem(date));
    ui->tableBreak->setItem(row, 2, makeItem(""));
    ui->tableBreak->setItem(row, 3, makeItem(""));
    ui->tableBreak->setItem(row, 4, makeItem("0"));
    ui->tableBreak->item(row, 1)->setData(Qt::UserRole, 0);

    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItems(ACCOUNT_TYPES);
    typeCombo->setCurrentText("지출");
    ui->tableBreak->setCellWidget(row, 5, typeCombo);

    QPushButton *receiptBtn = new QPushButton("첨부");
    receiptBtn->setStyleSheet("background-color: gray; color: white;");
    ui->tableBreak->setCellWidget(row, 6, receiptBtn);

    ui->tableBreak->editItem(ui->tableBreak->item(row, 1));
}

void MainWindow::deleteBreakRow()
{
    QList<QTableWidgetItem*> selected = ui->tableBreak->selectedItems();
    if (selected.isEmpty()) return;

    int row = ui->tableBreak->row(selected.first());
    int id = ui->tableBreak->item(row, 1)->data(Qt::UserRole).toInt();

    if (id > 0) {
        QSqlQuery query(db);
        query.prepare("DELETE FROM transactions WHERE id = ?");
        query.addBindValue(id);
        query.exec();
    }
    ui->tableBreak->removeRow(row);
}

void MainWindow::saveBreak()
{
    for (int row = 0; row < ui->tableBreak->rowCount(); row++) {
        int id = ui->tableBreak->item(row, 1)->data(Qt::UserRole).toInt();
        QString date = ui->tableBreak->item(row, 1)->text();
        QString category = ui->tableBreak->item(row, 2) ? ui->tableBreak->item(row, 2)->text() : "";
        QString description = ui->tableBreak->item(row, 3) ? ui->tableBreak->item(row, 3)->text() : "";
        int amount = ui->tableBreak->item(row, 4) ? ui->tableBreak->item(row, 4)->text().toInt() : 0;
        QComboBox *typeCombo = qobject_cast<QComboBox*>(ui->tableBreak->cellWidget(row, 5));
        QString type = typeCombo ? typeCombo->currentText() : "지출";

        if (id > 0) {
            QSqlQuery query(db);
            query.prepare("UPDATE transactions SET date=?, category=?, description=?, amount=?, type=? WHERE id=?");
            query.addBindValue(date);
            query.addBindValue(category);
            query.addBindValue(description);
            query.addBindValue(amount);
            query.addBindValue(type);
            query.addBindValue(id);
            query.exec();
        } else {
            // 새 항목은 isBlack=0 (일반회계)으로 저장
            QSqlQuery query(db);
            query.prepare("INSERT INTO transactions (date, category, description, amount, type, isBlack) VALUES (?,?,?,?,?,0)");
            query.addBindValue(date);
            query.addBindValue(category);
            query.addBindValue(description);
            query.addBindValue(amount);
            query.addBindValue(type);
            query.exec();
        }
    }
    QMessageBox::information(this, "저장 완료", "저장되었습니다!");
    loadBreak();
}

void MainWindow::updateBreakCharts()
{
    QString yearMonth = ui->dateBreak->date().toString("yyyy-MM");

    // 그래프1: 공식회계 vs 브레이크머니 비율 (파이차트)
    QSqlQuery q1(db);
    q1.prepare("SELECT SUM(amount), isBlack FROM transactions WHERE date LIKE ? AND type='지출' GROUP BY isBlack");
    q1.addBindValue(yearMonth + "-%");
    q1.exec();

    QPieSeries *pieSeries = new QPieSeries();
    while (q1.next()) {
        int amount = q1.value(0).toInt();
        bool isBlack = q1.value(1).toBool();
        pieSeries->append(isBlack ? "브레이크머니" : "공식회계", amount);
    }

    QChart *pieChart = new QChart();
    pieChart->addSeries(pieSeries);
    pieChart->setTitle("공식회계 vs 브레이크머니");
    pieChart->legend()->show();

    QChartView *pieView = new QChartView(pieChart);
    pieView->setRenderHint(QPainter::Antialiasing);

    QLayout *old1 = ui->chartPieBreak->layout();
    if (old1) { QLayoutItem *item; while ((item = old1->takeAt(0))) { delete item->widget(); delete item; } delete old1; }
    QVBoxLayout *l1 = new QVBoxLayout(ui->chartPieBreak);
    l1->addWidget(pieView);
    l1->setContentsMargins(0,0,0,0);

    // 그래프2: 브레이크머니 카테고리별 비율 (파이차트)
    QSqlQuery q2(db);
    q2.prepare("SELECT category, SUM(amount) FROM transactions WHERE date LIKE ? AND isBlack=1 GROUP BY category");
    q2.addBindValue(yearMonth + "-%");
    q2.exec();

    QPieSeries *pieSeries2 = new QPieSeries();
    while (q2.next()) {
        pieSeries2->append(q2.value(0).toString(), q2.value(1).toInt());
    }
    if (pieSeries2->count() == 0) pieSeries2->append("No Data", 1);

    QChart *pieChart2 = new QChart();
    pieChart2->addSeries(pieSeries2);
    pieChart2->setTitle("브레이크머니 카테고리별");
    pieChart2->legend()->show();

    QChartView *pieView2 = new QChartView(pieChart2);
    pieView2->setRenderHint(QPainter::Antialiasing);

    QLayout *old2 = ui->chartBarBreak->layout();
    if (old2) { QLayoutItem *item; while ((item = old2->takeAt(0))) { delete item->widget(); delete item; } delete old2; }
    QVBoxLayout *l2 = new QVBoxLayout(ui->chartBarBreak);
    l2->addWidget(pieView2);
    l2->setContentsMargins(0,0,0,0);
}
