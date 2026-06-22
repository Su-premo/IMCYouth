#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "changepassworddialog.h"
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

#include <QKeyEvent>

#include <QInputDialog>
#include <QDateTime>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initDatabase();

    initAttendanceTab();
    connect(ui->btnSearchAt, &QPushButton::clicked, this, &MainWindow::loadAttendance);
    connect(ui->btnSaveAt, &QPushButton::clicked, this, &MainWindow::saveAttendance);

    // ---- 공통 링크형 버튼 스타일 ----
    const QString linkButtonStyle =
        "QPushButton { "
        "background: transparent; border: none; "
        "color: blue; padding: 3px 4px; font-weight: 700; "
        "min-height: 0px; }"
        "QPushButton:hover { color: darkblue; }";

    // 출석 탭의 전체현황 버튼에 적용
    ui->btnOverviewAt->setStyleSheet("");  // 기존 스타일 삭제
    ui->btnOverviewAt->setStyleSheet(linkButtonStyle);
    ui->btnOverviewAt->setCursor(Qt::PointingHandCursor);
    ui->btnOverviewAt->setFlat(true);
    ui->btnOverviewAt->setMaximumWidth(80);

    // 출석 탭 선택시에만 전체현황 버튼 표시 / 회계 탭 선택시에만 break 버튼 표시
    // cornerWidget에 전체현황 + break 버튼 함께 배치
    QPushButton *btnBreak = new QPushButton("BREAK IN");
    btnBreak->setStyleSheet(linkButtonStyle);
    btnBreak->setCursor(Qt::PointingHandCursor);
    btnBreak->setFlat(true);
    btnBreak->setMaximumWidth(100);         // break 글자 길이에 맞게
    btnBreak->setVisible(false);

    QWidget *cornerWidget = new QWidget();
    QHBoxLayout *cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 5, 0, 0);
    cornerLayout->addWidget(ui->btnOverviewAt);
    cornerLayout->addWidget(btnBreak);
    ui->tabWidget->setCornerWidget(cornerWidget, Qt::TopRightCorner);

    // 탭 변경에 따라 corner 버튼 표시 제어
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this, btnBreak](int index){
        ui->btnOverviewAt->setVisible(index == 0);          // 출석탭(0)일때
        btnBreak->setVisible(index == 1);                   // 회계탭(1)일때
    });

    // 출석 전체현황 버튼 동작
    connect(ui->btnOverviewAt, &QPushButton::clicked, this, [this](){
        AttendanceOverview *overview = new AttendanceOverview();
        int year = ui->spinYearAt->value();
        int month = ui->comboMonthAt->currentData().toInt();
        overview->loadOverview(year);
        overview->setAttribute(Qt::WA_DeleteOnClose);           // 닫힐때 메모리 해제 - 프로그램 창 닫히면 전체현황 창도 닫히게 함
        overview->show();
    });

    // 회계 BREAK 버튼 동작
    connect(btnBreak, &QPushButton::clicked, this, [this, btnBreak](){
        if (!isBreakMode) {
            bool ok;
            QString code = QInputDialog::getText(this, "누구요?", "",
                QLineEdit::Password, "", &ok);
            if (!ok) return;

            QSqlQuery query(db);
            query.prepare("SELECT id FROM users WHERE officerCode = ?");
            query.addBindValue(code);
            query.exec();

            if (!query.next()) {
                QMessageBox::warning(this, "", "");
                return;
            }

            currentOfficerId = query.value(0).toInt();

            // 진입 로그 기록
            QSqlQuery logQuery(db);
            logQuery.prepare("INSERT INTO audit_log (userId, action, description, timestamp) VALUES (?,?,?,?)");
            logQuery.addBindValue(currentOfficerId);
            logQuery.addBindValue("BREAK_LOGIN");
            logQuery.addBindValue("블랙 진입");
            logQuery.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
            logQuery.exec();

            isBreakMode = true;
            btnBreak->setText("BREAK OUT");
            ui->btnExportAc->setVisible(false);
            ui->tabWidget->setCurrentIndex(1);
            loadBreak();
        } else {
            isBreakMode = false;
            currentOfficerId = -1;
            btnBreak->setText("BREAK IN");          // 회계 페이지로 진입
            ui->btnExportAc->setVisible(true);
            loadAccount();
        }
    });

    initAccountTab();
    ui->tableAc->installEventFilter(this);
    connect(ui->btnAddAc, &QPushButton::clicked, this, [this](){
        if (isBreakMode) addBreakRow();    // 브레이크머니 모드면 break 추가
        else addAccountRow();              // 일반 모드면 회계 추가
    });
    connect(ui->btnDeleteAc, &QPushButton::clicked, this, [this](){
        if (isBreakMode) deleteBreakRow(); // 브레이크머니 모드면 break 삭제
        else deleteAccountRow();           // 일반 모드면 회계 삭제
    });
    connect(ui->btnSaveAc, &QPushButton::clicked, this, [this](){
        if (isBreakMode) saveBreak();      // 브레이크머니 모드면 break 저장
        else saveAccount();                // 일반 모드면 회계 저장
    });
    connect(ui->btnSearchAc, &QPushButton::clicked, this, [this](){
        if (isBreakMode) loadBreak();      // 브레이크머니 모드면 break 조회
        else loadAccount();                // 일반 모드면 회계 조회
    });
    // 출납대장 내보내기 버튼
    connect(ui->btnExportAc, &QPushButton::clicked, this, &MainWindow::exportAccountToExcel);


    // 회계 페이지 수정 버튼 클릭시 전체 테이블 편집 가능으로 전환 + 수정 모드 표시
    connect(ui->btnEditAc, &QPushButton::clicked, this, [this](){
        ui->tableAc->setEditTriggers(QAbstractItemView::DoubleClicked);
        // 수정 모드 표시 (포인트 컬러 적용)
        ui->btnEditAc->setStyleSheet(
            QString("background-color: %1;").arg(CHART_PastelYellow[2].name())
        );
    });

    // 편집 시작시 편집기에 이벤트 필터 설치
    connect(ui->tableAc, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item){
        if (item->column() != 4) return;
        // 편집기 생성 타이밍 대기
        QTimer::singleShot(0, this, [this](){
            QWidget *editor = ui->tableAc->focusWidget();
            if (editor && qobject_cast<QLineEdit*>(editor)) {
                editor->installEventFilter(this);
            }
        });
    });
    // 수입(5) 또는 지출(6) 컬럼 편집 완료 후 콤마 포맷
    connect(ui->tableAc, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item){
        if (item->column() != 5 && item->column() != 6) return; // 4 → 5,6
        QString text = item->text().remove(",");
        bool ok;
        int value = text.toInt(&ok);
        if (!ok) return;
        QString formatted = QLocale(QLocale::Korean).toString(value);
        if (item->text() != formatted) {
            ui->tableAc->blockSignals(true);
            item->setText(formatted);
            ui->tableAc->blockSignals(false);
        }
    });

    initMembersTab();
    connect(ui->btnSearchMember, &QPushButton::clicked, this, &MainWindow::searchMemberByName);
    connect(ui->searchMember, &QLineEdit::returnPressed, this, &MainWindow::searchMemberByName);
    connect(ui->btnAddMember, &QPushButton::clicked, this, &MainWindow::addMemberRow);
    connect(ui->btnDeleteMember, &QPushButton::clicked, this, &MainWindow::deleteMember);
    connect(ui->btnSaveMember, &QPushButton::clicked, this, &MainWindow::saveMember);

    // 멤버 페이지 수정 버튼 클릭시 전체 테이블 편집 가능으로 전환 + 수정 모드 표시
    connect(ui->btnEditMember, &QPushButton::clicked, this, [this](){
        ui->tableMembers->setEditTriggers(QAbstractItemView::DoubleClicked);
        ui->tableMembersInactive->setEditTriggers(QAbstractItemView::DoubleClicked);
        // 수정 모드 표시 (포인트 컬러 적용)
        ui->btnEditMember->setStyleSheet(
            QString("background-color: %1;").arg(CHART_PastelYellow[2].name())
        );
    });

    initSettingsTab();
}

QString MainWindow::getCode(const QString &name)
{
    QSqlQuery q(db);
    q.prepare("SELECT value FROM codes WHERE name = ?");
    q.addBindValue(name);
    q.exec();
    if (q.next()) return q.value(0).toString();
    return "";
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

MainWindow::~MainWindow()
{
    delete ui;
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

    // 현재 월로 기본 설정
    ui->comboMonthAt->setCurrentIndex(QDate::currentDate().month() - 1);

    // 월 콤보박스 가운데 정렬
    ui->comboMonthAt->setEditable(true);
    ui->comboMonthAt->lineEdit()->setAlignment(Qt::AlignCenter);
    ui->comboMonthAt->setEditable(false);

    // 프로그램 실행 시 현재 월에 해당하는 출석부 자동으로 로드
    loadAttendance();
}

void MainWindow::initAccountTab()
{
    // 년도+월 형식으로 설정
    ui->dateAc->setDisplayFormat("yyyy-MM");
    ui->dateAc->setDate(QDate::currentDate());

    // 테이블 컬럼 설정
    ui->tableAc->setColumnCount(10);
    ui->tableAc->setHorizontalHeaderLabels({"#", "날짜", "계정과목", "적요", "거래처", "수입", "지출", "잔액", "비고", "영수증"});
    ui->tableAc->verticalHeader()->hide();

    // ui->tableAc->horizontalHeader()->setStretchLastSection(false);
    ui->tableAc->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(0, 30);   // #
    ui->tableAc->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(1, 100);  // Date
    ui->tableAc->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(2, 110);  // Category
    ui->tableAc->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); // Description 자동
    ui->tableAc->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(4, 90);   // 거래처
    ui->tableAc->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(5, 90);   // 수입
    ui->tableAc->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(6, 90);   // 지출
    ui->tableAc->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(7, 100);   // 잔액
    ui->tableAc->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch); // 비고 자동
    ui->tableAc->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Fixed);
    ui->tableAc->setColumnWidth(9, 70);   // Receipt

    // 기본 편집 불가
    ui->tableAc->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 컬럼 오토포커스 제거
    ui->tableAc->setFocusPolicy(Qt::NoFocus);
    ui->dateAc->setFocusPolicy(Qt::NoFocus);

    loadAccount();
}

void MainWindow::initMembersTab()
{
    auto setupTable = [](QTableWidget *table) {
        table->setColumnCount(7);
        table->setHorizontalHeaderLabels({"#", "이름", "생년월일", "연락처", "주소", "소속", "비고"});
        table->verticalHeader()->hide();
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);                  // 기본 편집 불가
        // # 컬럼만 고정, 나머지 자동 분배
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->setColumnWidth(0, 35);
        table->setColumnWidth(1, 80);  // Name
        table->setColumnWidth(2, 100);  // BirthDate
        table->setColumnWidth(3, 130);  // Phone
        table->setColumnWidth(4, 335);
        table->setColumnWidth(5, 220);
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
//    ui->tableMembers->setWordWrap(true);                // 내용 감싸기
    ui->tableMembers->setMinimumHeight(330);
    ui->tableMembersInactive->setMaximumHeight(230);

    // 오토포커스 제거
    ui->tableMembers->setFocusPolicy(Qt::NoFocus);
    ui->tableMembersInactive->setFocusPolicy(Qt::NoFocus);

    ui->labelInactive->setText("⚠️ 3개월 이상 미출석 멤버");
    loadMembers();
}

void MainWindow::initDatabase()
{

    qDebug() << "BASE_PATH:" << BASE_PATH();
    qDebug() << "DB path:" << BASE_PATH() + "/imcyouth.db";

    // 폴더 생성
    QDir().mkpath(BASE_PATH());
    QDir().mkpath(BASE_PATH() + "/receipts");

    db = QSqlDatabase::addDatabase("QSQLITE", "imcyouth");
    db.setDatabaseName(BASE_PATH() + "/imcyouth.db");

    if (!db.open()) {
        QMessageBox::critical(this, "오류", "DB 연결에 실패했습니다.\n" + db.lastError().text());
        return;
    }

    QSqlQuery query(db);

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
            isBlack INTEGER DEFAULT 0,  -- 0: 일반회계, 1: 블랙
            receiptImage TEXT
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS audit_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            userId INTEGER,
            action TEXT,
            description TEXT,
            timestamp TEXT,
            FOREIGN KEY (userId) REFERENCES users(id)
        )
    )");

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS codes (
            name TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )");

    // 기존 컬럼 없으면 추가 (이미 있으면 조용히 실패 — 정상)
    query.exec("ALTER TABLE transactions ADD COLUMN counterparty TEXT");
    query.exec("ALTER TABLE transactions ADD COLUMN notes TEXT");
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
    headers << "이름";
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
    QSqlQuery memberQuery(db);
    memberQuery.exec("SELECT id, name FROM members ORDER BY id");

    int row = 0;
    while (memberQuery.next()) {
        int memberId = memberQuery.value(0).toInt();
        QString name = memberQuery.value(1).toString();

        ui->tableAt->insertRow(row);

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

            QSqlQuery attendQuery(db);
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
    db.transaction();           // 트랜잭션 시작

    for (int row = 0; row < ui->tableAt->rowCount(); row++) {
        int memberId = ui->tableAt->item(row, 1)->data(Qt::UserRole).toInt();

        for (int col = 0; col < currentColumns.size(); col++) {
            QDate date = currentColumns[col].first;
            QString service = currentColumns[col].second;
            QString dateStr = date.toString("yyyy-MM-dd");

            QWidget *checkWidget = ui->tableAt->cellWidget(row, col + 2);
            QCheckBox *checkBox = checkWidget->findChild<QCheckBox*>();
            bool isPresent = checkBox->isChecked();

            QSqlQuery upsertQuery(db);
            upsertQuery.prepare(
                "INSERT OR REPLACE INTO attendance (memberId, date, service, isPresent) "
                "VALUES (?, ?, ?, ?)"
                );
            upsertQuery.addBindValue(memberId);
            upsertQuery.addBindValue(dateStr);
            upsertQuery.addBindValue(service);
            upsertQuery.addBindValue(isPresent ? 1 : 0);
            upsertQuery.exec();
        }
    }

    db.commit();                // 트랜잭션 완료
    QMessageBox::information(this, "저장 완료", "출석이 저장되었습니다!");
    updateCharts();
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

    // 토요일 / 1st / 2nd 출석률 계산 (C안: 저장된 기록이 있는 세션만 평균에 포함)
    auto calcRate = [&](QString service) -> QPair<int,int> {
        int totalPresent = 0;
        int sessionCount = 0;
        int daysInMonth = QDate(year, month, 1).daysInMonth();

        for (int day = 1; day <= daysInMonth; day++) {
            QDate date(year, month, day);
            bool isSat = (date.dayOfWeek() == 6 && service == "");
            bool isSun = (date.dayOfWeek() == 7 && service != "");
            if (!isSat && !isSun) continue;

            QSqlQuery q(db);
            q.prepare("SELECT SUM(isPresent) FROM attendance "
                      "WHERE date = ? AND service = ? AND isPresent = 1");
            q.addBindValue(date.toString("yyyy-MM-dd"));
            q.addBindValue(service);
            q.exec();

            if (q.next()) {
                int present = q.value(0).toInt();
                if (present == 0) continue;   // 출석자 없는 주는 제외
                sessionCount++;
                totalPresent += present;
            }
        }

        if (sessionCount == 0 || totalMembers == 0)
            return {0, totalMembers > 0 ? totalMembers : 1};

        // ✅ 핵심 수정: 세션별 평균 출석자 수를 반올림하지 않고
        //    "전체 출석 합계 / 세션 수" 를 present로, totalMembers를 absent 기준으로 사용
        int avgPresent = qRound((double)totalPresent / sessionCount);
        // totalMembers 기준으로 absent 계산 (avgPresent가 totalMembers 초과 방지)
        avgPresent = qMin(avgPresent, totalMembers);
        return {avgPresent, totalMembers};
    };


    auto makeChart = [&](QWidget *container, QString title, QPair<int,int> data) {
        QPieSeries *series = new QPieSeries();
        int present = data.first;
        int absent = data.second - present;
        if (data.second == 0) {
            series->append("No Data", 1);
            series->slices().at(0)->setColor(CHART_PastelYellow[0]);
        } else {
            series->append("출석 " + QString::number(present), present);
            series->append("결석 " + QString::number(absent), absent);
            series->slices().at(0)->setColor(CHART_PastelYellow[2]); // 출석
            series->slices().at(1)->setColor(CHART_PastelYellow[0]); // 결석
        }

        QChart *chart = new QChart();
        chart->addSeries(series);
        chart->setTitle(title);
        chart->legend()->show();

        // 원형 그래프 마진 최소화
        chart->setMargins(QMargins(5, 20, 5, 5));

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
    barSet->setColor(CHART_PastelYellow[1]);          // 막대 색상
    barSet->setBorderColor(CHART_PastelYellow[2]);    // 막대 테두리 색상
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

    // ---- 연간 막대그래프 세팅 파트 ----
    QBarSeries *barSeries = new QBarSeries();
    barSeries->append(barSet);

    QChart *barChart = new QChart();
    barChart->addSeries(barSeries);
    barChart->setTitle(QString::number(year) + "년 월별 출석");

    // X축 (월별 레이블) 설정
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(monthLabels);
//    axisX->setLabelsAngle(-15);  // 레이블 45도 기울이기
    axisX->setLabelsVisible(true);
    axisX->setLabelsFont(QFont("kartrider", 8));
    barChart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);
    // barChart->setMargins(QMargins(0, 10, 10, 10)); // 하단 여백 추가

    // 차트 플롯 영역 축소해서 레이블 공간 확보
    barChart->legend()->setAlignment(Qt::AlignTop);

    // 연도 월별 막대 그래프
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 100);            // 전체 인원수 기준(totalMembers) -> 월 평균(100)으로 변경

    axisY->setTickCount(6);
    axisY->setLabelFormat("%d%%");        // 전체 인원수 정수만 표시(%d) -> 평균률(%d%%)로 변경
    barChart->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);

    // 마진 미세 조율
    // 아래쪽(bottom) 마진을 16으로 당겨서 글자와 상자 하단 경계면 사이의 간격을 좁히고,
    // 위쪽(top) 마진을 5로 조여서 격자 자체를 위로
    barChart->setMargins(QMargins(22, 5, 15, 16)); // (왼쪽, 위, 오른쪽, 아래)

    QChartView *barView = new QChartView(barChart);
    barView->setRenderHint(QPainter::Antialiasing);

    // QChartView 자체의 불필요한 테두리 공백을 0으로
    barView->setContentsMargins(0, 0, 0, 0);

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

    // 외부 컨테이너 박스의 마진을 완전히 제로로 밀어 그래프가 레이아웃 안에서 최대로 커지게
    barLayout->setContentsMargins(0,0,0,0);
}

// ---------------------------------------------- Account --------------------------------------------------------

void MainWindow::loadAccount()
{
    QString yearMonth = ui->dateAc->date().toString("yyyy-MM");

    QSqlQuery query(db);
    query.prepare("SELECT id, date, category, description, counterparty, amount, type, notes, receiptImage FROM transactions WHERE date LIKE ? AND isBlack=0 ORDER BY date ASC");
    query.addBindValue(yearMonth + "-%");
    query.exec();

    ui->tableAc->setRowCount(0);

    // 가운데 정렬
    auto makeItem = [](const QString &text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        return item;
    };

    int index = 1;
    int runningBalance = 0;

    while (query.next()) {
        int row = ui->tableAc->rowCount();
        ui->tableAc->insertRow(row);

        int     id           = query.value(0).toInt();
        QString date         = query.value(1).toString();
        QString category     = query.value(2).toString();
        QString description  = query.value(3).toString();
        QString counterparty = query.value(4).toString();
        int     amount       = query.value(5).toInt();
        QString type         = query.value(6).toString();
        QString notes        = query.value(7).toString();
        QString receiptPath  = query.value(8).toString();

        int income  = (type == "수입") ? amount : 0;
        int expense = (type == "지출") ? amount : 0;
        runningBalance += income - expense;

        // col 0: 인덱스 (수정불가)
        QTableWidgetItem *indexItem = makeItem(QString::number(index++));
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        ui->tableAc->setItem(row, 0, indexItem);

        // col 1: 날짜 (id 저장)
        ui->tableAc->setItem(row, 1, makeItem(date));
        ui->tableAc->item(row, 1)->setData(Qt::UserRole, id);

        ui->tableAc->setItem(row, 2, makeItem(category));    // 계정과목
        ui->tableAc->setItem(row, 3, makeItem(description)); // 적요
        ui->tableAc->setItem(row, 4, makeItem(counterparty));// 거래처

        // col 5: 수입
        QString incomeStr = (income > 0) ? QLocale(QLocale::Korean).toString(income) : "";
        ui->tableAc->setItem(row, 5, makeItem(incomeStr));

        // col 6: 지출
        QString expenseStr = (expense > 0) ? QLocale(QLocale::Korean).toString(expense) : "";
        ui->tableAc->setItem(row, 6, makeItem(expenseStr));

        // col 7: 잔액 (수정불가, 마이너스 빨간색)
        QString balStr = QLocale(QLocale::Korean).toString(runningBalance);
        QTableWidgetItem *balItem = makeItem(balStr);
        balItem->setFlags(balItem->flags() & ~Qt::ItemIsEditable);
        if (runningBalance < 0) balItem->setForeground(Qt::red);
        ui->tableAc->setItem(row, 7, balItem);

        ui->tableAc->setItem(row, 8, makeItem(notes)); // 비고

        // col 9: 영수증 버튼
        QPushButton *receiptBtn = new QPushButton();
        if (receiptPath.isEmpty()) {
            receiptBtn->setText("첨부");
            receiptBtn->setStyleSheet("background-color: gray; color: white;");
        } else {
            receiptBtn->setText("보기");
            receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
        }
        receiptBtn->setProperty("receiptPath", receiptPath);

        connect(receiptBtn, &QPushButton::clicked, this, [this, row, receiptBtn]() {
            QString path = receiptBtn->property("receiptPath").toString();
            if (path.isEmpty()) {
                // 항목 채워졌는지 확인
                QString date     = ui->tableAc->item(row, 1) ? ui->tableAc->item(row, 1)->text() : "";
                QString category = ui->tableAc->item(row, 2) ? ui->tableAc->item(row, 2)->text() : "";
                QString desc     = ui->tableAc->item(row, 3) ? ui->tableAc->item(row, 3)->text() : "";
                if (date.isEmpty() || category.isEmpty() || desc.isEmpty()) {
                    QMessageBox::warning(this, "입력 오류", "날짜, 계정과목, 적요를 먼저 입력해주세요!");
                    return;
                }
                QString filePath = QFileDialog::getOpenFileName(this, "영수증 선택", "", "Images (*.png *.jpg *.jpeg)");
                if (!filePath.isEmpty()) {
                    QString destDir  = BASE_PATH() + "/receipts/";
                    QDir().mkpath(destDir);
                    QString idx      = ui->tableAc->item(row, 0)->text();
                    QString ext      = QFileInfo(filePath).suffix();
                    QString fileName = date + "_" + category + "_" + idx + "." + ext;
                    QString destPath = destDir + fileName;
                    QFile::copy(filePath, destPath);
                    receiptBtn->setText("보기");
                    receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
                    receiptBtn->setProperty("receiptPath", destPath);
                    int id = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();
                    if (id > 0) {
                        QSqlQuery q(db);
                        q.prepare("UPDATE transactions SET receiptImage = ? WHERE id = ?");
                        q.addBindValue(destPath);
                        q.addBindValue(id);
                        q.exec();

                    // // DB 업데이트
                    // if (id > 0) {
                    //     QSqlQuery updateQuery(db);
                    //     updateQuery.prepare("UPDATE transactions SET receiptImage = ? WHERE id = ?");
                    //     updateQuery.addBindValue(destPath);
                    //     updateQuery.addBindValue(id);
                    //     updateQuery.exec();
                    // }
                    }
                }
            } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
        });
        ui->tableAc->setCellWidget(row, 9, receiptBtn);
    }

    // 로드/저장 후 편집 불가 + 수정 버튼 색상 복귀
    ui->tableAc->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->btnEditAc->setStyleSheet("");

    updateAccountCharts();
}

void MainWindow::addAccountRow()
{
    int row = ui->tableAc->rowCount();
    ui->tableAc->insertRow(row);

    // 추가시 편집 가능으로 전환 + 수정 모드 표시
    ui->tableAc->setEditTriggers(QAbstractItemView::DoubleClicked);
    ui->btnEditAc->setStyleSheet(
        QString("background-color: %1; color: white;").arg(CHART_PastelYellow[2].name())
    );

    // 가운데 정렬
    auto makeItem = [](QString text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        return item;
    };

    QString today = QDate::currentDate().toString("yyyy-MM-dd");

    QTableWidgetItem *indexItem = makeItem(QString::number(row + 1));
    indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
    ui->tableAc->setItem(row, 0, indexItem);

    ui->tableAc->setItem(row, 1, makeItem(today));
    ui->tableAc->item(row, 1)->setData(Qt::UserRole, 0);
    ui->tableAc->setItem(row, 2, makeItem(""));  // 계정과목
    ui->tableAc->setItem(row, 3, makeItem(""));  // 적요
    ui->tableAc->setItem(row, 4, makeItem(""));  // 거래처
    ui->tableAc->setItem(row, 5, makeItem(""));  // 수입
    ui->tableAc->setItem(row, 6, makeItem("0")); // 지출 (기본값)

    // 잔액: 수정불가
    QTableWidgetItem *balItem = makeItem("-");
    balItem->setFlags(balItem->flags() & ~Qt::ItemIsEditable);
    ui->tableAc->setItem(row, 7, balItem);

    ui->tableAc->setItem(row, 8, makeItem("")); // 비고

    QPushButton *receiptBtn = new QPushButton("첨부");
    receiptBtn->setStyleSheet("background-color: gray; color: white;");
    connect(receiptBtn, &QPushButton::clicked, this, [this, row, receiptBtn]() {
        QString path = receiptBtn->property("receiptPath").toString();
        if (path.isEmpty()) {
            QString date     = ui->tableAc->item(row, 1) ? ui->tableAc->item(row, 1)->text() : "";
            QString category = ui->tableAc->item(row, 2) ? ui->tableAc->item(row, 2)->text() : "";
            QString desc     = ui->tableAc->item(row, 3) ? ui->tableAc->item(row, 3)->text() : "";
            if (date.isEmpty() || category.isEmpty() || desc.isEmpty()) {
                QMessageBox::warning(this, "입력 오류", "날짜, 계정과목, 적요를 먼저 입력해주세요!");
                return;
            }
            QString filePath = QFileDialog::getOpenFileName(this, "영수증 선택", "", "Images (*.png *.jpg *.jpeg)");
            if (!filePath.isEmpty()) {
                QString destDir  = BASE_PATH() + "/receipts/";
                QDir().mkpath(destDir);
                QString idx      = ui->tableAc->item(row, 0)->text();
                QString ext      = QFileInfo(filePath).suffix();
                QString fileName = date + "_" + category + "_" + idx + "." + ext;
                QString destPath = destDir + fileName;
                QFile::copy(filePath, destPath);
                receiptBtn->setText("보기");
                receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
                receiptBtn->setProperty("receiptPath", destPath);
                int id = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();
                if (id > 0) {
                    QSqlQuery q(db);
                    q.prepare("UPDATE transactions SET receiptImage = ? WHERE id = ?");
                    q.addBindValue(destPath);
                    q.addBindValue(id);
                    q.exec();
                }
            }
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });
    ui->tableAc->setCellWidget(row, 9, receiptBtn);
    ui->tableAc->editItem(ui->tableAc->item(row, 1));
}

void MainWindow::deleteAccountRow()
{
    int row = ui->tableAc->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "선택 오류", "삭제할 항목을 선택해주세요!");
        return;
    }
    auto reply = QMessageBox::question(this, "삭제 확인",
                                       "정말 삭제하시겠습니까?", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    int id = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt(); // 0→1 버그수정
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
        int     id           = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();
        QString date         = ui->tableAc->item(row, 1)->text();
        QString category     = ui->tableAc->item(row, 2) ? ui->tableAc->item(row, 2)->text() : "";
        QString description  = ui->tableAc->item(row, 3) ? ui->tableAc->item(row, 3)->text() : "";
        QString counterparty = ui->tableAc->item(row, 4) ? ui->tableAc->item(row, 4)->text() : "";
        QString notes        = ui->tableAc->item(row, 8) ? ui->tableAc->item(row, 8)->text() : "";

        int incomeVal  = ui->tableAc->item(row, 5) ? ui->tableAc->item(row, 5)->text().remove(",").toInt() : 0;
        int expenseVal = ui->tableAc->item(row, 6) ? ui->tableAc->item(row, 6)->text().remove(",").toInt() : 0;
        int amount = (incomeVal > 0) ? incomeVal : expenseVal;
        QString type = (incomeVal > 0) ? "수입" : "지출";

        if (id > 0) {
            QSqlQuery query(db);
            query.prepare("UPDATE transactions SET date=?, category=?, description=?, counterparty=?, amount=?, type=?, notes=? WHERE id=?");
            query.addBindValue(date);
            query.addBindValue(category);
            query.addBindValue(description);
            query.addBindValue(counterparty);
            query.addBindValue(amount);
            query.addBindValue(type);
            query.addBindValue(notes);
            query.addBindValue(id);
            query.exec();
        } else {
            QSqlQuery query(db);
            query.prepare("INSERT INTO transactions (date, category, description, counterparty, amount, type, notes, isBlack) VALUES (?,?,?,?,?,?,?,0)");
            query.addBindValue(date);
            query.addBindValue(category);
            query.addBindValue(description);
            query.addBindValue(counterparty);
            query.addBindValue(amount);
            query.addBindValue(type);
            query.addBindValue(notes);
            query.exec();
        }
    }
    QMessageBox::information(this, "저장 완료", "저장되었습니다!");
    // 로드/저장 후 편집 불가 + 수정 버튼 색상 복귀
    ui->tableAc->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->btnEditAc->setStyleSheet("");

    loadAccount();
    updateAccountCharts();
}

void MainWindow::exportAccountToExcel()
{
    // ── 다이얼로그 ──
    QDialog dlg(this);
    dlg.setWindowTitle("출납대장 내보내기");
    dlg.setFixedWidth(360);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(0);

    // 서브 타이틀
    QLabel *labelSub = new QLabel("EXPORT");
    labelSub->setStyleSheet(
        "font-size: 11px; font-weight: 600; letter-spacing: 2px; color: #b8b84f;"
        );
    mainLayout->addWidget(labelSub);
    mainLayout->addSpacing(4);

    // 메인 타이틀
    QLabel *labelTitle = new QLabel("출납대장 내보내기");
    labelTitle->setStyleSheet(
        "font-size: 18px; font-weight: 600; color: #222;"
        "padding-bottom: 16px; border-bottom: 1.5px solid #f0ead8;"
        );
    mainLayout->addWidget(labelTitle);
    mainLayout->addSpacing(20);

    // 공통 스타일
    const QString lineEditStyle =
        "QLineEdit { height: 42px; border: 1.5px solid #e0d8c0; border-radius: 10px;"
        " background: #faf8f2; padding: 0 14px; }"
        "QLineEdit:focus { border-color: #d1d066; background: white; }";
    const QString comboStyle =
        "QComboBox { height: 42px; border: 1.5px solid #e0d8c0; border-radius: 10px;"
        " background: #faf8f2; padding: 0 14px; text-align: center; }"
        "QComboBox:focus { border-color: #d1d066; }"
        "QComboBox::drop-down { border: none; width: 30px; }"
        "QComboBox::down-arrow { image: none; width: 0; height: 0;"
        " border-left: 5px solid transparent;"
        " border-right: 5px solid transparent;"
        " border-top: 6px solid #b8b84f; }";
    const QString labelStyle =
        "font-size: 11px; font-weight: 600; color: #888; margin-bottom: 4px;";

    // 연도 (DB에 데이터 있는 연도만)
    QLabel *lYear = new QLabel("연도");
    lYear->setStyleSheet(labelStyle);
    QComboBox *comboYear = new QComboBox();
    comboYear->setStyleSheet(comboStyle);

    QSqlQuery yearQuery(db);
    yearQuery.exec("SELECT DISTINCT strftime('%Y', date) as year FROM transactions WHERE isBlack=0 ORDER BY year DESC");
    while (yearQuery.next()) {
        comboYear->addItem(yearQuery.value(0).toString() + "년", yearQuery.value(0).toInt());
    }
    if (comboYear->count() == 0) {
        int cur = QDate::currentDate().year();
        comboYear->addItem(QString::number(cur) + "년", cur);
    }
    mainLayout->addWidget(lYear);
    mainLayout->addWidget(comboYear);
    mainLayout->addSpacing(12);

    // 기간
    QLabel *lPeriod = new QLabel("기간");
    lPeriod->setStyleSheet(labelStyle);
    QComboBox *comboPeriod = new QComboBox();
    comboPeriod->addItem("상반기 (1~6월)",  QVariantList({1, 6}));
    comboPeriod->addItem("하반기 (7~12월)", QVariantList({7, 12}));
    comboPeriod->addItem("전체 (1~12월)",   QVariantList({1, 12}));
    comboPeriod->setStyleSheet(comboStyle);
    mainLayout->addWidget(lPeriod);
    mainLayout->addWidget(comboPeriod);
    mainLayout->addSpacing(12);

    // 부서명 (수정불가 라벨)
    QLabel *lDept = new QLabel("부서명");
    lDept->setStyleSheet(labelStyle);
    QLabel *deptLabel = new QLabel("청년부");
    deptLabel->setStyleSheet(
        "height: 42px; border: 1.5px solid #e0d8c0; border-radius: 10px;"
        " background: #f0ede3; padding: 0 14px; color: #888;"
        );
    deptLabel->setFixedHeight(42);
    mainLayout->addWidget(lDept);
    mainLayout->addWidget(deptLabel);
    mainLayout->addSpacing(12);

    // 이월금액
    QLabel *lCarry = new QLabel("이월금액");
    lCarry->setStyleSheet(labelStyle);
    QLineEdit *editCarryOver = new QLineEdit("0");
    editCarryOver->setStyleSheet(lineEditStyle);
    mainLayout->addWidget(lCarry);
    mainLayout->addWidget(editCarryOver);
    mainLayout->addSpacing(24);

    // 가운데 정렬
    labelSub->setAlignment(Qt::AlignCenter);
    labelTitle->setAlignment(Qt::AlignCenter);
    lYear->setAlignment(Qt::AlignCenter);
    lPeriod->setAlignment(Qt::AlignCenter);
    lDept->setAlignment(Qt::AlignCenter);
    lCarry->setAlignment(Qt::AlignCenter);
    deptLabel->setAlignment(Qt::AlignCenter);

    comboYear->setEditable(true);
    comboYear->lineEdit()->setAlignment(Qt::AlignCenter);
    comboYear->setEditable(false);
    comboPeriod->setEditable(true);
    comboPeriod->lineEdit()->setAlignment(Qt::AlignCenter);
    comboPeriod->setEditable(false);

    // 버튼
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    QPushButton *btnCancel = new QPushButton("취소");
    btnCancel->setFixedHeight(46);
    btnCancel->setAutoDefault(false);  // 엔터키 반응 방지
    btnCancel->setDefault(false);
    btnCancel->setStyleSheet(
        "QPushButton { background: transparent; border: 1.5px solid #d1d066;"
        " border-radius: 10px; color: #b8b84f; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #f2f1ac; }"
        );

    QPushButton *btnConfirm = new QPushButton("내보내기");
    btnConfirm->setFixedHeight(46);
    btnConfirm->setAutoDefault(false);  // 엔터키 반응 방지
    btnConfirm->setDefault(false);
    btnConfirm->setStyleSheet(
        "QPushButton { background: #d1d066; color: white; border: none;"
        " border-radius: 10px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #b8b84f; }"
        );

    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnConfirm);
    mainLayout->addLayout(btnLayout);

    connect(btnCancel,  &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnConfirm, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    // ── 값 가져오기 ──
    int     year       = comboYear->currentData().toInt();
    QVariantList per   = comboPeriod->currentData().toList();
    int     startMonth = per[0].toInt();
    int     endMonth   = per[1].toInt();
    // QString dept       = editDept->text();
    QString dept       = "청년부";
    int     carryOver  = editCarryOver->text().remove(",").toInt();

    // ── DB 조회 ──
    QString startDate = QString("%1-%2-01").arg(year).arg(startMonth, 2, 10, QChar('0'));
    QString endDate   = QString("%1-%2-31").arg(year).arg(endMonth,   2, 10, QChar('0'));

    QSqlQuery query(db);
    query.prepare(
        "SELECT date, category, description, counterparty, amount, type, notes "
        "FROM transactions WHERE date >= ? AND date <= ? AND isBlack=0 ORDER BY date ASC"
        );
    query.addBindValue(startDate);
    query.addBindValue(endDate);
    query.exec();

    // ── QXlsx 포맷 정의 ──
    QXlsx::Document xlsx;

    auto makeFmt = [](bool bold=false, QColor bg=Qt::white,
                      QXlsx::Format::HorizontalAlignment ha=QXlsx::Format::AlignHCenter,
                      bool border=true, QColor fontColor=Qt::black) {
        QXlsx::Format f;
        f.setFontBold(bold);
        f.setPatternBackgroundColor(bg);
        f.setHorizontalAlignment(ha);
        f.setVerticalAlignment(QXlsx::Format::AlignVCenter);
        if (border) f.setBorderStyle(QXlsx::Format::BorderThin);
        f.setFontColor(fontColor);
        return f;
    };

    QColor blueHeader("#BDD7EE");
    QXlsx::Format titleFmt  = makeFmt(true, Qt::white, QXlsx::Format::AlignHCenter, false);
    titleFmt.setFontSize(14);
    QXlsx::Format lblFmt    = makeFmt(true,  blueHeader);
    QXlsx::Format cellFmt   = makeFmt(false, Qt::white);
    QXlsx::Format colHdrFmt = makeFmt(true,  blueHeader);
    QXlsx::Format numFmt    = makeFmt(false, Qt::white, QXlsx::Format::AlignRight);
    numFmt.setNumberFormat("#,##0");
    QXlsx::Format negFmt    = makeFmt(false, Qt::white, QXlsx::Format::AlignRight, true, Qt::red);
    negFmt.setNumberFormat("#,##0");

    // ── Row 1: 제목 ──
    QString title = QString("%1년 %2월 ~ %3년 %4월  금 전 출 납 대 장")
                        .arg(year).arg(startMonth).arg(year).arg(endMonth);
    xlsx.mergeCells("A1:H1");
    xlsx.write("A1", title, titleFmt);
    xlsx.setRowHeight(1, 28);

    // ── Row 2: 부서명 ──
    xlsx.write(2, 1, "부서명", lblFmt);
    xlsx.mergeCells(QXlsx::CellRange(2, 2, 2, 4));
    xlsx.write(2, 2, dept,    cellFmt);
    xlsx.write(2, 5, "부장",  lblFmt);
    xlsx.mergeCells(QXlsx::CellRange(2, 6, 2, 8));
    xlsx.write(2, 6, "",      cellFmt);

    // ── Row 3: 이월금액 ──
    xlsx.write(3, 1, "이월금액", lblFmt);
    xlsx.mergeCells(QXlsx::CellRange(3, 2, 3, 4));
    QString carryStr = (carryOver == 0) ? "-" : QLocale(QLocale::Korean).toString(carryOver);
    xlsx.write(3, 2, carryStr,  cellFmt);
    xlsx.write(3, 5, "총무",    lblFmt);
    xlsx.mergeCells(QXlsx::CellRange(3, 6, 3, 8));
    xlsx.write(3, 6, "",        cellFmt);

    // ── Row 4: 컬럼 헤더 ──
    QStringList headers = {"날짜", "계정과목", "적  요", "거래처", "수  입", "지출", "잔액", "비고"};
    for (int c = 0; c < headers.size(); c++)
        xlsx.write(4, c + 1, headers[c], colHdrFmt);
    xlsx.setRowHeight(4, 20);

    // ── 컬럼 너비 ──
    xlsx.setColumnWidth(1, 11);  // 날짜
    xlsx.setColumnWidth(2, 14);  // 계정과목
    xlsx.setColumnWidth(3, 32);  // 적요
    xlsx.setColumnWidth(4, 14);  // 거래처
    xlsx.setColumnWidth(5, 13);  // 수입
    xlsx.setColumnWidth(6, 13);  // 지출
    xlsx.setColumnWidth(7, 13);  // 잔액
    xlsx.setColumnWidth(8, 8);   // 비고

    // ── 데이터 행 ──
    int dataRow = 5;
    int balance = carryOver;

    while (query.next()) {
        QString date         = query.value(0).toString();
        QString category     = query.value(1).toString();
        QString description  = query.value(2).toString();
        QString counterparty = query.value(3).toString();
        int     amount       = query.value(4).toInt();
        QString type         = query.value(5).toString();
        QString notes        = query.value(6).toString();

        int income  = (type == "수입") ? amount : 0;
        int expense = (type == "지출") ? amount : 0;
        balance += income - expense;

        QDate d = QDate::fromString(date, "yyyy-MM-dd");
        QString dateStr = d.toString("yy.MM.dd");

        xlsx.write(dataRow, 1, dateStr,      cellFmt);
        xlsx.write(dataRow, 2, category,     cellFmt);
        xlsx.write(dataRow, 3, description,  cellFmt);
        xlsx.write(dataRow, 4, counterparty, cellFmt);

        if (income > 0)  xlsx.write(dataRow, 5, income,  numFmt);
        else             xlsx.write(dataRow, 5, "",       cellFmt);

        if (expense > 0) xlsx.write(dataRow, 6, expense, numFmt);
        else             xlsx.write(dataRow, 6, "",       cellFmt);

        xlsx.write(dataRow, 7, balance, (balance < 0) ? negFmt : numFmt);
        xlsx.write(dataRow, 8, notes,   cellFmt);

        dataRow++;
    }

    // // ── 저장 ──
    // QString defaultName = QString("%1_%2년_%3_출납대장.xlsx")
    //                           .arg(dept).arg(year)
    //                           .arg(startMonth <= 6 ? "상반기" : "하반기");
    // QString fileName = QFileDialog::getSaveFileName(
    //     this, "저장 위치 선택", defaultName, "Excel Files (*.xlsx)"
    //     );
    // if (fileName.isEmpty()) return;

    // if (xlsx.saveAs(fileName)) {
    //     QMessageBox::information(this, "완료", "출납대장이 저장되었습니다!\n" + fileName);
    //     QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
    // } else {
    //     QMessageBox::warning(this, "오류", "파일 저장에 실패했습니다.");
    // }

    // ── 자동 저장 ──
    QString exportDir = BASE_PATH() + "/Exported/";
    QDir().mkpath(exportDir);

    QString period = (startMonth <= 6) ? "상반기" : (endMonth >= 12 && startMonth <= 1) ? "전체" : "하반기";
    QString fileName = exportDir + QString("청년부_%1년_%2_출납대장.xlsx")
                                       .arg(year).arg(period);

    if (xlsx.saveAs(fileName)) {
        QMessageBox::information(this, "완료",
                                 "출납대장이 저장되었습니다!\n\n" + fileName);
        QDesktopServices::openUrl(QUrl::fromLocalFile(exportDir)); // 폴더 열기
    } else {
        QMessageBox::warning(this, "오류", "파일 저장에 실패했습니다.");
    }
}

void MainWindow::updateAccountCharts()
{
    QString yearMonth = ui->dateAc->date().toString("yyyy-MM");

    // 수입/지출 합계 계산
    QSqlQuery query(db);
    query.prepare("SELECT type, SUM(amount) FROM transactions WHERE date LIKE ? AND isBlack=0 GROUP BY type");
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
        pieSeries->slices().at(0)->setColor(CHART_PastelYellow[2]);
    } else {
        pieSeries->append("수입 " + QString::number(income), income);
        pieSeries->append("지출 " + QString::number(expense), expense);
        pieSeries->slices().at(0)->setColor(CHART_PastelYellow[1]); // 수입
        pieSeries->slices().at(1)->setColor(CHART_PastelYellow[2]); // 지출
    }

    QChart *pieChart = new QChart();
    pieChart->addSeries(pieSeries);
    pieChart->setTitle("수입 / 지출");
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
    incomeSet->setColor(CHART_PastelYellow[1]);       // 수입 막대 색상
    expenseSet->setColor(CHART_PastelYellow[2]);      // 지출 막대 색상
    incomeSet->setBorderColor(CHART_PastelYellow[1]);
    expenseSet->setBorderColor(CHART_PastelYellow[2]);
    QStringList monthLabels;

    int year = ui->dateAc->date().year();
    for (int m = 1; m <= 12; m++) {
        QSqlQuery q(db);
        q.prepare("SELECT type, SUM(amount) FROM transactions WHERE date LIKE ? AND isBlack=0 GROUP BY type");
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
    barChart->setTitle(QString::number(year) + "년 월별 수입 / 지출");

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(monthLabels);
    axisX->setLabelsFont(QFont("kartrider", 10));
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

// ---------------------------------------------- break --------------------------------------------------------

void MainWindow::loadBreak()
{
    // 브레이크머니 모드: isBlack=1 항목만 조회
    QString yearMonth = ui->dateAc->date().toString("yyyy-MM");

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, date, category, description, amount, type, receiptImage "
        "FROM transactions WHERE date LIKE ? AND isBlack=1 ORDER BY date ASC"
        );
    query.addBindValue(yearMonth + "-%");
    query.exec();

    ui->tableAc->setRowCount(0);

    // 가운데 정렬 아이템 생성 람다
    auto makeItem = [](QString text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        return item;
    };

    int index = 1;
    int runningBalance = 0;

    while (query.next()) {
        int row = ui->tableAc->rowCount();
        ui->tableAc->insertRow(row);

        int     id          = query.value(0).toInt();
        QString date        = query.value(1).toString();
        QString category    = query.value(2).toString();
        QString description = query.value(3).toString();
        int     amount      = query.value(4).toInt();
        QString type        = query.value(5).toString();
        QString receiptPath = query.value(6).toString();

        int income  = (type == "수입") ? amount : 0;
        int expense = (type == "지출") ? amount : 0;
        runningBalance += income - expense;

        // col 0: 인덱스 (수정불가)
        QTableWidgetItem *indexItem = makeItem(QString::number(index++));
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        ui->tableAc->setItem(row, 0, indexItem);

        // col 1: 날짜 (id 저장)
        ui->tableAc->setItem(row, 1, makeItem(date));
        ui->tableAc->item(row, 1)->setData(Qt::UserRole, id);

        ui->tableAc->setItem(row, 2, makeItem(category));    // 계정과목
        ui->tableAc->setItem(row, 3, makeItem(description)); // 적요

        // col 4: 거래처 (break는 미사용 — 빈칸)
        QTableWidgetItem *cpItem = makeItem("");
        cpItem->setFlags(cpItem->flags() & ~Qt::ItemIsEditable);
        ui->tableAc->setItem(row, 4, cpItem);

        // col 5: 수입
        QString incomeStr = (income > 0) ? QLocale(QLocale::Korean).toString(income) : "";
        ui->tableAc->setItem(row, 5, makeItem(incomeStr));

        // col 6: 지출
        QString expenseStr = (expense > 0) ? QLocale(QLocale::Korean).toString(expense) : "";
        ui->tableAc->setItem(row, 6, makeItem(expenseStr));

        // col 7: 잔액 (수정불가, 마이너스 빨간색)
        QTableWidgetItem *balItem = makeItem(QLocale(QLocale::Korean).toString(runningBalance));
        balItem->setFlags(balItem->flags() & ~Qt::ItemIsEditable);
        if (runningBalance < 0) balItem->setForeground(Qt::red);
        ui->tableAc->setItem(row, 7, balItem);

        // col 8: 비고 (break는 미사용 — 빈칸)
        QTableWidgetItem *notesItem = makeItem("");
        notesItem->setFlags(notesItem->flags() & ~Qt::ItemIsEditable);
        ui->tableAc->setItem(row, 8, notesItem);

        // col 9: 영수증 버튼
        QPushButton *receiptBtn = new QPushButton();
        if (receiptPath.isEmpty()) {
            receiptBtn->setText("첨부");
            receiptBtn->setStyleSheet("background-color: gray; color: white;");
        } else {
            receiptBtn->setText("보기");
            receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
        }
        receiptBtn->setProperty("receiptPath", receiptPath);

        connect(receiptBtn, &QPushButton::clicked, this, [this, row, receiptBtn]() {
            QString path = receiptBtn->property("receiptPath").toString();
            if (path.isEmpty()) {
                QString date     = ui->tableAc->item(row, 1) ? ui->tableAc->item(row, 1)->text() : "";
                QString category = ui->tableAc->item(row, 2) ? ui->tableAc->item(row, 2)->text() : "";
                QString desc     = ui->tableAc->item(row, 3) ? ui->tableAc->item(row, 3)->text() : "";
                if (date.isEmpty() || category.isEmpty() || desc.isEmpty()) {
                    QMessageBox::warning(this, "입력 오류", "날짜, 계정과목, 적요를 먼저 입력해주세요!");
                    return;
                }
                QString filePath = QFileDialog::getOpenFileName(this, "영수증 선택", "", "Images (*.png *.jpg *.jpeg)");
                if (!filePath.isEmpty()) {
                    QString destDir  = BASE_PATH() + "/receipts/";
                    QDir().mkpath(destDir);
                    QString idx      = ui->tableAc->item(row, 0)->text();
                    QString ext      = QFileInfo(filePath).suffix();
                    QString fileName = date + "_" + category + "_" + idx + "." + ext;
                    QString destPath = destDir + fileName;
                    QFile::copy(filePath, destPath);
                    receiptBtn->setText("보기");
                    receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
                    receiptBtn->setProperty("receiptPath", destPath);
                    int id = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();
                    if (id > 0) {
                        QSqlQuery q(db);
                        q.prepare("UPDATE transactions SET receiptImage = ? WHERE id = ?");
                        q.addBindValue(destPath);
                        q.addBindValue(id);
                        q.exec();
                    }
                }
            } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
        });
        ui->tableAc->setCellWidget(row, 9, receiptBtn);
    }

    // 로드/저장 후 편집 불가 + 수정 버튼 색상 복귀
    ui->tableAc->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->btnEditAc->setStyleSheet("");

    updateBreakCharts();
}

void MainWindow::addBreakRow()
{
    int row = ui->tableAc->rowCount();
    ui->tableAc->insertRow(row);

    // 추가시 편집 가능으로 전환 + 수정 모드 표시
    ui->tableAc->setEditTriggers(QAbstractItemView::DoubleClicked);
    ui->btnEditAc->setStyleSheet(
        QString("background-color: %1; color: white;").arg(CHART_PastelYellow[2].name())
    );

    auto makeItem = [](QString text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        return item;
    };

    QString today = QDate::currentDate().toString("yyyy-MM-dd");

    // col 0: 인덱스 (수정불가)
    QTableWidgetItem *indexItem = makeItem(QString::number(row + 1));
    indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
    ui->tableAc->setItem(row, 0, indexItem);

    ui->tableAc->setItem(row, 1, makeItem(today));
    ui->tableAc->item(row, 1)->setData(Qt::UserRole, 0);
    ui->tableAc->setItem(row, 2, makeItem(""));  // 계정과목
    ui->tableAc->setItem(row, 3, makeItem(""));  // 적요

    // col 4: 거래처 (break 미사용 — 수정불가)
    QTableWidgetItem *cpItem = makeItem("");
    cpItem->setFlags(cpItem->flags() & ~Qt::ItemIsEditable);
    ui->tableAc->setItem(row, 4, cpItem);

    ui->tableAc->setItem(row, 5, makeItem(""));  // 수입
    ui->tableAc->setItem(row, 6, makeItem("0")); // 지출

    // col 7: 잔액 (수정불가)
    QTableWidgetItem *balItem = makeItem("-");
    balItem->setFlags(balItem->flags() & ~Qt::ItemIsEditable);
    ui->tableAc->setItem(row, 7, balItem);

    // col 8: 비고 (break 미사용 — 수정불가)
    QTableWidgetItem *notesItem = makeItem("");
    notesItem->setFlags(notesItem->flags() & ~Qt::ItemIsEditable);
    ui->tableAc->setItem(row, 8, notesItem);

    // col 9: 영수증 버튼
    QPushButton *receiptBtn = new QPushButton("첨부");
    receiptBtn->setStyleSheet("background-color: gray; color: white;");
    connect(receiptBtn, &QPushButton::clicked, this, [this, row, receiptBtn]() {
        QString path = receiptBtn->property("receiptPath").toString();
        if (path.isEmpty()) {
            QString date     = ui->tableAc->item(row, 1) ? ui->tableAc->item(row, 1)->text() : "";
            QString category = ui->tableAc->item(row, 2) ? ui->tableAc->item(row, 2)->text() : "";
            QString desc     = ui->tableAc->item(row, 3) ? ui->tableAc->item(row, 3)->text() : "";
            if (date.isEmpty() || category.isEmpty() || desc.isEmpty()) {
                QMessageBox::warning(this, "입력 오류", "날짜, 계정과목, 적요를 먼저 입력해주세요!");
                return;
            }
            QString filePath = QFileDialog::getOpenFileName(this, "영수증 선택", "", "Images (*.png *.jpg *.jpeg)");
            if (!filePath.isEmpty()) {
                QString destDir  = BASE_PATH() + "/receipts/";
                QDir().mkpath(destDir);
                QString idx      = ui->tableAc->item(row, 0)->text();
                QString ext      = QFileInfo(filePath).suffix();
                QString fileName = date + "_" + category + "_" + idx + "." + ext;
                QString destPath = destDir + fileName;
                QFile::copy(filePath, destPath);
                receiptBtn->setText("보기");
                receiptBtn->setStyleSheet("background-color: #2196F3; color: white;");
                receiptBtn->setProperty("receiptPath", destPath);
                int id = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();
                if (id > 0) {
                    QSqlQuery q(db);
                    q.prepare("UPDATE transactions SET receiptImage = ? WHERE id = ?");
                    q.addBindValue(destPath);
                    q.addBindValue(id);
                    q.exec();
                }
            }
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });
    ui->tableAc->setCellWidget(row, 9, receiptBtn);
    ui->tableAc->editItem(ui->tableAc->item(row, 1));
}

void MainWindow::deleteBreakRow()
{
    QList<QTableWidgetItem*> selected = ui->tableAc->selectedItems();
    if (selected.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(this, "삭제 확인",
        "정말 삭제하시겠습니까?\n(저장하기 전까지 복구가 가능합니다.)",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    int row = ui->tableAc->row(selected.first());
    int id = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();

    // DB에 저장된 항목이면 삭제
    if (id > 0) {
        QSqlQuery query(db);
        query.prepare("DELETE FROM transactions WHERE id = ?");
        query.addBindValue(id);
        query.exec();
    }
    ui->tableAc->removeRow(row);
}

void MainWindow::saveBreak()
{
    // 저장 로그 기록
    if (currentOfficerId > 0) {
        QSqlQuery logQuery(db);
        logQuery.prepare("INSERT INTO audit_log (userId, action, description, timestamp) VALUES (?,?,?,?)");
        logQuery.addBindValue(currentOfficerId);
        logQuery.addBindValue("BREAK_SAVE");
        logQuery.addBindValue(QString("블랙 저장 (%1건)").arg(ui->tableAc->rowCount()));
        logQuery.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        logQuery.exec();
    }

    for (int row = 0; row < ui->tableAc->rowCount(); row++) {
        int     id          = ui->tableAc->item(row, 1)->data(Qt::UserRole).toInt();
        QString date        = ui->tableAc->item(row, 1)->text();
        QString category    = ui->tableAc->item(row, 2) ? ui->tableAc->item(row, 2)->text() : "";
        QString description = ui->tableAc->item(row, 3) ? ui->tableAc->item(row, 3)->text() : "";

        int incomeVal  = ui->tableAc->item(row, 5) ? ui->tableAc->item(row, 5)->text().remove(",").toInt() : 0;
        int expenseVal = ui->tableAc->item(row, 6) ? ui->tableAc->item(row, 6)->text().remove(",").toInt() : 0;
        int amount  = (incomeVal > 0) ? incomeVal : expenseVal;
        QString type = (incomeVal > 0) ? "수입" : "지출";

        if (id > 0) {
            // 기존 항목 업데이트
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
            // 새 항목은 isBlack=1 (블랙)으로 저장
            QSqlQuery query(db);
            query.prepare("INSERT INTO transactions (date, category, description, amount, type, isBlack) VALUES (?,?,?,?,?,1)");
            query.addBindValue(date);
            query.addBindValue(category);
            query.addBindValue(description);
            query.addBindValue(amount);
            query.addBindValue(type);
            query.exec();
        }
    }
    QMessageBox::information(this, "저장 완료", "저장되었습니다!");
    // 로드/저장 후 편집 불가 + 수정 버튼 색상 복귀
    ui->tableAc->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->btnEditAc->setStyleSheet("");

    loadBreak();
    updateBreakCharts();
}

void MainWindow::updateBreakCharts()
{
    QString yearMonth = ui->dateAc->date().toString("yyyy-MM");

    // 그래프1: 공식회계 vs 브레이크머니 비율 (파이차트)
    // isBlack=0: 공식회계, isBlack=1: 브레이크머니 지출 합계 비교
    QSqlQuery q1(db);
    q1.prepare("SELECT SUM(amount), isBlack FROM transactions WHERE date LIKE ? AND type='지출' GROUP BY isBlack");
    q1.addBindValue(yearMonth + "-%");
    q1.exec();

    QPieSeries *pieSeries = new QPieSeries();
    int sliceIndex = 0;
    while (q1.next()) {
        int amount = q1.value(0).toInt();
        bool isBlack = q1.value(1).toBool();
        pieSeries->append(isBlack ? "break" : "공식", amount);
        pieSeries->slices().at(sliceIndex)->setColor(isBlack ? CHART_PastelBlue[2] : CHART_PastelBlue[0]);
        sliceIndex++;
    }
    if (pieSeries->count() == 0) {
        pieSeries->append("No Data", 1);
        pieSeries->slices().at(0)->setColor(CHART_PastelBlue[2]);
    }

    QChart *pieChart = new QChart();
    pieChart->addSeries(pieSeries);
    pieChart->setTitle("IN / OUT");
    pieChart->legend()->show();

    QChartView *pieView = new QChartView(pieChart);
    pieView->setRenderHint(QPainter::Antialiasing);

    // 기존 차트 제거 후 새 차트 삽입
    QLayout *old1 = ui->chartPieAc->layout();
    if (old1) { QLayoutItem *item; while ((item = old1->takeAt(0))) { delete item->widget(); delete item; } delete old1; }
    QVBoxLayout *l1 = new QVBoxLayout(ui->chartPieAc);
    l1->addWidget(pieView);
    l1->setContentsMargins(0,0,0,0);

    // 그래프2: 브레이크머니 카테고리별 비율 (파이차트)
    // isBlack=1 항목들 중 카테고리별 지출 금액 비율
    QSqlQuery q2(db);
    q2.prepare("SELECT category, SUM(amount) FROM transactions WHERE date LIKE ? AND isBlack=1 GROUP BY category");
    q2.addBindValue(yearMonth + "-%");
    q2.exec();

    QPieSeries *pieSeries2 = new QPieSeries();
    while (q2.next()) {
        pieSeries2->append(q2.value(0).toString(), q2.value(1).toInt());
    }
    if (pieSeries2->count() == 0) pieSeries2->append("No Data", 1); // 데이터 없을때 빈 차트 방지

    // 모든 슬라이스 추가 후 색상 지정
    for (int i = 0; i < pieSeries2->slices().size(); i++) {
        pieSeries2->slices().at(i)->setColor(CHART_PastelBlue[i % 3]);
    }

    QChart *pieChart2 = new QChart();
    pieChart2->addSeries(pieSeries2);
    pieChart2->setTitle("카테고리별");
    pieChart2->legend()->show();

    QChartView *pieView2 = new QChartView(pieChart2);
    pieView2->setRenderHint(QPainter::Antialiasing);

    // 기존 차트 제거 후 새 차트 삽입
    QLayout *old2 = ui->chartBarAc->layout();
    if (old2) { QLayoutItem *item; while ((item = old2->takeAt(0))) { delete item->widget(); delete item; } delete old2; }
    QVBoxLayout *l2 = new QVBoxLayout(ui->chartBarAc);
    l2->addWidget(pieView2);
    l2->setContentsMargins(0,0,0,0);
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

    // 저장/로드 후 편집 불가 + 수정 버튼 색상 복귀
    ui->tableMembers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableMembersInactive->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->btnEditMember->setStyleSheet("");
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

    // 추가시 편집 가능으로 전환 + 수정 모드 표시
    ui->tableMembers->setEditTriggers(QAbstractItemView::DoubleClicked);
    ui->tableMembersInactive->setEditTriggers(QAbstractItemView::DoubleClicked);
    ui->btnEditMember->setStyleSheet(
        QString("background-color: %1; color: white;").arg(CHART_PastelYellow[2].name())
    );

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
    QMessageBox::StandardButton reply = QMessageBox::question(this, "삭제 확인",
        "정말 삭제하시겠습니까?\n(저장하기 전까지 복구가 가능합니다.)",
        QMessageBox::Yes | QMessageBox::No);
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
    // 저장/로드 후 편집 불가 + 수정 버튼 색상 복귀
    ui->tableMembers->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableMembersInactive->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->btnEditMember->setStyleSheet("");

    loadMembers();
}

// ---------------------------------------------- Settings --------------------------------------------------------
void MainWindow::initSettingsTab() {
    // 아이템 추가
    ui->settingsMenu->addItem("시스템 로그");
    ui->settingsMenu->addItem("암호 설정");

    // 사이드바 스타일
    ui->settingsMenu->setStyleSheet(
        "QListWidget {"
        "  background-color: #f7f3e8;"
        "  border: none;"
        "  border-right: 2px solid #d1d066;"
        "  outline: none;"
        "  padding: 8px 6px;"
        "}"
        "QListWidget::item {"
        "  height: 60px;"
        "  border-radius: 15px;"
        "  padding-left: 16px;"
        "  margin: 4px 6px;"
        "  color: #333333;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #d1d066;"
        "  color: white;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #f7f3e8;"
        "  color: #333333;"
        "  border: 2px solid #d1d066;"
        "}"
        );

    ui->labelLogSub->setStyleSheet(
        "font-size: 11px; font-weight: 600; letter-spacing: 2px; color: #b8b84f;"
        );
    ui->labelLog->setStyleSheet(
        "font-size: 18px; font-weight: 600; color: #222;"
        "padding-bottom: 16px; border-bottom: 1.5px solid #f0ead8;"
        );

    // tableAuditLog 기본 설정
    ui->tableAuditLog->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui->tableAuditLog->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableAuditLog->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableAuditLog->setAlternatingRowColors(true);
    ui->tableAuditLog->verticalHeader()->setVisible(false);
    ui->tableAuditLog->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // 컬럼 비율: ID 10%, 나머지 4개 균등
    auto resizeAuditCols = [this]() {
        int total = ui->tableAuditLog->viewport()->width();
        int idWidth = total * 0.10;
        int rest = (total - idWidth) / 4;
        ui->tableAuditLog->setColumnWidth(0, idWidth);
        ui->tableAuditLog->setColumnWidth(1, rest);
        ui->tableAuditLog->setColumnWidth(2, rest);
        ui->tableAuditLog->setColumnWidth(3, rest);
        ui->tableAuditLog->setColumnWidth(4, rest);
    };

    connect(ui->tableAuditLog->horizontalHeader(), &QHeaderView::geometriesChanged,
            this, resizeAuditCols);
    resizeAuditCols();

    connect(ui->btnRefreshLog, &QPushButton::clicked,
            this, &MainWindow::loadAuditLog);

    // 메뉴 선택 시그널
    // initSettingsTab()의 시그널 연결 부분
    connect(ui->settingsMenu, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (m_settingsChangingRow) return;
        int index = ui->settingsMenu->row(item);
        if (index == 1) {
            ui->stackedWidget->setCurrentIndex(1);
            ChangePasswordDialog dlg(db, this);
            dlg.exec();
            m_settingsChangingRow = true;
            ui->settingsMenu->setCurrentItem(ui->settingsMenu->item(0));
            m_settingsChangingRow = false;
            ui->stackedWidget->setCurrentIndex(0);
        } else {
            ui->stackedWidget->setCurrentIndex(index);
        }
    });

    // 초기 선택
    ui->settingsMenu->setCurrentRow(0);
    loadAuditLog();
}

void MainWindow::loadAuditLog() {
    QSqlQuery query(db);
    query.prepare(
        "SELECT al.id, u.username, al.action, al.description, al.timestamp "
        "FROM audit_log al "
        "LEFT JOIN users u ON al.userId = u.id "
        "ORDER BY al.timestamp DESC"
    );

    if (!query.exec()) {
        qDebug() << "loadAuditLog error:" << query.lastError().text();
        return;
    }

    ui->tableAuditLog->setRowCount(0);
    while (query.next()) {
        int row = ui->tableAuditLog->rowCount();
        ui->tableAuditLog->insertRow(row);
        for (int col = 0; col < 5; col++) {
            QTableWidgetItem *item = new QTableWidgetItem(query.value(col).toString());
            item->setTextAlignment(Qt::AlignCenter);
            ui->tableAuditLog->setItem(row, col, item);
        }
    }
}

// ---------------------------------------------- Event --------------------------------------------------------

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // + 키 ×1000
        if (keyEvent->key() == Qt::Key_Plus) {
            QTableWidgetItem *item = ui->tableAc->currentItem();
            if (item && (item->column() == 5 || item->column() == 6)) {
                QLineEdit *editor = qobject_cast<QLineEdit*>(obj);
                QString text = editor ? editor->text().remove(",") : item->text().remove(",");
                bool ok;
                long long value = text.toLongLong(&ok);
                if (ok) {
                    QString formatted = QLocale(QLocale::Korean).toString(value * 1000);
                    if (editor) editor->setText(formatted);
                    else {
                        ui->tableAc->blockSignals(true);
                        item->setText(formatted);
                        ui->tableAc->blockSignals(false);
                    }
                }
                return true;
            }
        }

        // 숫자 입력시 실시간 콤마
        if (keyEvent->key() >= Qt::Key_0 && keyEvent->key() <= Qt::Key_9) {
            QLineEdit *editor = qobject_cast<QLineEdit*>(obj);
            if (editor) {
                QTableWidgetItem *item = ui->tableAc->currentItem();
                if (item && (item->column() == 5 || item->column() == 6)) {
                    QString current = editor->text().remove(",") + keyEvent->text();
                    bool ok;
                    long long value = current.toLongLong(&ok);
                    if (ok) {
                        editor->setText(QLocale(QLocale::Korean).toString(value));
                        return true;
                    }
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
