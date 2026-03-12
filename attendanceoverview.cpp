#include "attendanceoverview.h"
#include "ui_attendanceoverview.h"
#include <QSqlQuery>
#include <QTableWidgetItem>
#include <QDate>
#include <QHeaderView>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QShowEvent>

AttendanceOverview::AttendanceOverview(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AttendanceOverview)
{
    ui->setupUi(this);
    setWindowTitle("전체 출석 현황");
    resize(1000, 700);
    ui->tableWidget->hide(); // 기존 tableWidget 숨기기
}

AttendanceOverview::~AttendanceOverview()
{
    delete ui;
}

void AttendanceOverview::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}

void AttendanceOverview::loadOverview(int year)
{
    // 기존 스크롤 영역 제거 후 재생성
    QScrollArea *oldScroll = findChild<QScrollArea*>("overviewScrollArea");
    if (oldScroll) delete oldScroll;

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("overviewScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setGeometry(0, 0, this->width(), this->height());

    QWidget *container = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 멤버 목록 미리 로드
    QSqlQuery memberQuery;
    memberQuery.exec("SELECT id, name FROM members ORDER BY id");
    QList<QPair<int, QString>> members;
    while (memberQuery.next()) {
        members.append({memberQuery.value(0).toInt(), memberQuery.value(1).toString()});
    }

    for (int month = 1; month <= 12; month++) {
        // 월 라벨
        QLabel *monthLabel = new QLabel(QString("%1년 %2월").arg(year).arg(month));
        QFont labelFont = monthLabel->font();
        labelFont.setBold(true);
        labelFont.setPointSize(11);
        monthLabel->setFont(labelFont);
        mainLayout->addWidget(monthLabel);

        // 해당 월 토/일 계산
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

        // 헤더
        QStringList headers;
        headers << "#" << "Name";
        for (auto &col : columns) {
            QDate date = col.first;
            QString service = col.second;
            if (service.isEmpty()) headers << date.toString("M/d\n(토)");
            else if (service == "1st") headers << date.toString("M/d\n(일)1부");
            else headers << date.toString("M/d\n(일)2부");
        }

        // 테이블 생성
        QTableWidget *table = new QTableWidget();
        table->setColumnCount(headers.size());
        table->setHorizontalHeaderLabels(headers);
        table->setRowCount(members.size());
        table->verticalHeader()->hide();
        table->setFocusPolicy(Qt::NoFocus);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setStretchLastSection(false);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->setColumnWidth(0, 40);  // #
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        table->setColumnWidth(1, 100); // Name
        for (int i = 2; i < headers.size(); i++) {
            table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
        }

        // 테이블 높이 고정 (행 수에 맞게)
        int rowHeight = 25;
        table->setFixedHeight(rowHeight * members.size() + 35);

        // 데이터 채우기
        for (int r = 0; r < members.size(); r++) {
            int memberId = members[r].first;
            QString name = members[r].second;

            table->setRowHeight(r, rowHeight);

            // 인덱스
            QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(r + 1));
            indexItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(r, 0, indexItem);

            // 이름
            QTableWidgetItem *nameItem = new QTableWidgetItem(name);
            nameItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(r, 1, nameItem);

            for (int col = 0; col < columns.size(); col++) {
                QDate date = columns[col].first;
                QString service = columns[col].second;

                QSqlQuery attendQuery;
                attendQuery.prepare("SELECT isPresent FROM attendance WHERE memberId = ? AND date = ? AND service = ?");
                attendQuery.addBindValue(memberId);
                attendQuery.addBindValue(date.toString("yyyy-MM-dd"));
                attendQuery.addBindValue(service);
                attendQuery.exec();

                QString text = "-";
                if (attendQuery.next()) {
                    text = attendQuery.value(0).toBool() ? "O" : "-";
                }
                QTableWidgetItem *item = new QTableWidgetItem(text);
                item->setTextAlignment(Qt::AlignCenter);
                table->setItem(r, col + 2, item);
            }
        }
        mainLayout->addWidget(table);
        mainLayout->addSpacing(5); // 월별 마진
    }

    mainLayout->addStretch();
    scrollArea->setWidget(container);
    scrollArea->show();
}
