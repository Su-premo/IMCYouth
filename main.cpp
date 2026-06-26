#include "mainwindow.h"
#include "logindialog.h"
#include "config.h"

#include <QApplication>
#include <QFontDatabase>
#include <QSqlDatabase>
#include <QSqlQuery>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ===== 폰트 지정 =====
    // 폰트 등록
    QFontDatabase::addApplicationFont(":/fonts/Cafe24Behappy.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Cafe24OhsquareAir-v2.0.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Cafe24Supermagic-Regular-v1.0.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Cafe24Supermagic-Regular-v1.0.ttf");
    QFontDatabase::addApplicationFont(":/fonts/NEXON-Kart-Gothic-Medium.ttf");
    QFontDatabase::addApplicationFont(":/fonts/NEXON-Kart-Gothic-Bold.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Danjo-bold-Regular.otf");

    // 폰트 지정 (하나 선택)
    // Cafe24 Behappy / Cafe24 Ohsquare air / Cafe24 Supermagic / Kartrider / Danjo-bold
    QFont font("Kartrider", 11);
    font.setBold(false);
    a.setFont(font);

    // 아이콘 지정
    a.setWindowIcon(QIcon(":/icons/youth.png"));

    // ===== 윈도우 생성 (DB 초기화가 MainWindow 생성자에서 실행됨) =====
    MainWindow w;

    // ===== 프로그램 로그인 =====
    QSqlDatabase db = QSqlDatabase::database("imcyouth");
    LoginDialog login(db);
    if (login.exec() != QDialog::Accepted) {
        return 0;                       // 로그인 취소시 종료
    }

    w.setWindowTitle("권라면과 안성탕면");

    // ===== IMCYouth UI 테마 =====
    // 베이스: #f7f3e8(연베이지) | 미듐: #f2f1ac(연노랑) | 포인트: #d1d066(겨자)

    QString globalStyle =
        "QMainWindow, QWidget { background-color: #f7f3e8; }"
        "QLineEdit, QSpinBox, QDateEdit { qproperty-alignment: AlignCenter; }"
        "QComboBox { qproperty-alignment: AlignCenter; color: black; }"

        /* 탭 바(Tab Bar) 스타일 */
        "QTabBar::tab { min-width: 333px; min-height: 50px; font-size: 12pt; font-weight: 600; border: 1px solid #c0c0c0; border-bottom: none; }"
        "QTabBar::tab:selected { background-color: #d1d066; color: white; border: 1px solid #d1d066; }"
        "QTabBar::tab:!selected { background-color: #f7f3e8; }"
        "QTabWidget::pane { border: 1px solid #c0c0c0; background-color: #f7f3e8; }"

        /* 테이블 헤더 */
        "QHeaderView::section { padding: 6px; background-color: #f2f1ac; font-weight: bold; }"

        /* 체크박스 버그 해결 (Windows 시인성 확보) */
        "QCheckBox { background: transparent; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #d1d066; background: white; border-radius: 3px; }"
        "QCheckBox::indicator:checked { background: #d1d066; }"; // 체크 시 포인트 컬러로 채움

    a.setStyleSheet(globalStyle);

    // 프로그램 창 사이즈 지정 및 포인트 스타일
    w.resize(1500, 1000);
    w.findChild<QTabWidget*>()->setStyleSheet(
//        "QMenuBar { background-color: #d1d066; }"
        "QTabBar::tab { min-width: 333px; min-height: 50px; padding: 6px; "
        "font-size: 12pt; font-weight: 600; "
        "border: 1px solid #c0c0c0; border-bottom: none; }"
        "QTabBar::tab:selected { font-weight: 610; background-color: #d1d066; "
        "border: 1px solid #d1d066; border-bottom: none; color: white; }"
        "QTabBar::tab:!selected { background-color: #f7f3e8; }"
        "QTabWidget::pane { border: 1px solid #c0c0c0; background-color: #f7f3e8; }"
        "QHeaderView::section { padding-top: 6px; padding-bottom: 6px; background-color: #f2f1ac; }"
        "QLineEdit { qproperty-alignment: AlignCenter; }"
        "QComboBox { qproperty-alignment: AlignCenter; color: black; }"
        "QSpinBox { qproperty-alignment: AlignCenter; }"
        "QDateEdit { qproperty-alignment: AlignCenter; }"
    );

    // 프로그램 시작시 출석 페이지로 로드
    w.findChild<QTabWidget*>()->setCurrentIndex(0);
    w.show();
    return a.exec();
}
