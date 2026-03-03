#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    w.setWindowTitle("권라면");

    // ===== 폰트 지정 =====
    #ifdef Q_OS_WIN
        QFont font("Malgun Gothic", 10); // Windows: 맑은 고딕
    #else
        QFont font("Noto Sans KR", 10);  // Linux: Noto
    #endif
    font.setBold(false);
    a.setFont(font);

    // ===== 컬러 팔레트 =====
    // 베이스: #f7f3e8 / 미듐: #f2f1ac / 포인트: #d1d066
    // 전체 앱 배경
    a.setStyleSheet(
        "QMainWindow { background-color: #f7f3e8; }"
        "QWidget { background-color: #f7f3e8; }"
    );

    // 프로그램 창 사이즈 지정 및 포인트 스타일
    w.resize(1500, 1000);
    w.findChild<QTabWidget*>()->setStyleSheet(
//        "QMenuBar { background-color: #d1d066; }"
        "QTabBar::tab { min-width: 333px; min-height: 50px; padding: 6px; "
        "font-family: 'Noto Sans KR'; font-size: 10pt; "
        "border: 1px solid #c0c0c0; border-bottom: none; }"
        "QTabBar::tab:selected { font-weight: 610; background-color: #d1d066; "
        "border: 1px solid #d1d066; border-bottom: none; color: white; }"
        "QTabBar::tab:!selected { background-color: #f7f3e8; }"
        "QTabWidget::pane { border: 1px solid #c0c0c0; background-color: #f7f3e8; }"
        "QHeaderView::section { padding-top: 6px; padding-bottom: 6px; background-color: #f2f1ac; }"
        "QLineEdit { qproperty-alignment: AlignCenter; }"
        "QComboBox { qproperty-alignment: AlignCenter; }"
        "QSpinBox { qproperty-alignment: AlignCenter; }"
        "QDateEdit { qproperty-alignment: AlignCenter; }"
    );

    // ------> QTabBar selected: #d0e8ff(하늘색) / QTabBar !selected: #f0f0f0(흰색) / pane: #c0c0c0(회색)
//    w.findChild<QTabWidget*>()->setStyleSheet(
//        "QTabBar::tab { min-width: 333px; min-height: 50px; padding: 6px; "
//        "font-family: 'Noto Sans KR'; font-size: 10pt; "
//        "border: 1px solid #c0c0c0; border-bottom: none; }"
//        "QTabBar::tab:selected { font-weight: 610; background-color: #d0e8ff; "
//        "border: 1px solid #a0a0a0; border-bottom: none; }"
//        "QTabBar::tab:!selected { background-color: #f0f0f0; }"
//        "QTabWidget::pane { border: 1px solid #c0c0c0; }"
//        "QHeaderView::section { padding-top: 6px; padding-bottom: 6px; }"
//        "QLineEdit { qproperty-alignment: AlignCenter; }"
//        "QComboBox { qproperty-alignment: AlignCenter; }"
//        "QSpinBox { qproperty-alignment: AlignCenter; }"
//        "QDateEdit { qproperty-alignment: AlignCenter; }"
//    );
/*
    // 전체 앱 배경
    a.setStyleSheet(
        "QMainWindow { background-color: #FBFAF5; }"
        "QWidget { background-color: #FBFAF5; }"
    );

    w.findChild<QTabWidget*>()->setStyleSheet(
        "QTabBar::tab { min-width: 333px; min-height: 50px; padding: 6px; "
        "font-family: 'Noto Sans KR'; font-size: 10pt; "
        "border: 1px solid #E8E9E4; border-bottom: none; }"
        "QTabBar::tab:selected { font-weight: 610; background-color: #E4D6E5; "
        "border: 1px solid #E4D6E5; border-bottom: none; }"
        "QTabBar::tab:!selected { background-color: #F9EDED; }"
        "QTabWidget::pane { border: 1px solid #E8E9E4; background-color: #FBFAF5; }"
        "QHeaderView::section { padding-top: 6px; padding-bottom: 6px; background-color: #E8E9E4; }"
        "QLineEdit { qproperty-alignment: AlignCenter; }"
        "QComboBox { qproperty-alignment: AlignCenter; }"
        "QSpinBox { qproperty-alignment: AlignCenter; }"
        "QDateEdit { qproperty-alignment: AlignCenter; }"
    );
*/


    // 프로그램 시작시 출석 페이지로 로드
    w.findChild<QTabWidget*>()->setCurrentIndex(0);
    w.show();
    return a.exec();
}
