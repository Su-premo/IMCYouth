#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    w.setWindowTitle("권라면");

    // 폰트 지정
    #ifdef Q_OS_WIN
        QFont font("Malgun Gothic", 10); // Windows: 맑은 고딕
    #else
        QFont font("Noto Sans KR", 10);  // Linux: Noto
    #endif
    font.setBold(false);
    a.setFont(font);

    // 프로그램 창 사이즈 지정 및 포인트 스타일
    // ------> QTabBar selected: #d0e8ff(하늘색) / QTabBar !selected: #f0f0f0(흰색) / pane: #c0c0c0(회색)
    w.resize(1500, 1000);
    w.findChild<QTabWidget*>()->setStyleSheet(
        "QTabBar::tab { min-width: 333px; min-height: 50px; padding: 6px; "
        "font-family: 'Noto Sans KR'; font-size: 10pt; "
        "border: 1px solid #c0c0c0; border-bottom: none; }"
        "QTabBar::tab:selected { font-weight: 610; background-color: #d0e8ff; "
        "border: 1px solid #a0a0a0; border-bottom: none; }"
        "QTabBar::tab:!selected { background-color: #f0f0f0; }"
        "QTabWidget::pane { border: 1px solid #c0c0c0; }"
        "QHeaderView::section { padding-top: 6px; padding-bottom: 6px; }"
        "QLineEdit { qproperty-alignment: AlignCenter; }"
        "QComboBox { qproperty-alignment: AlignCenter; }"
        "QSpinBox { qproperty-alignment: AlignCenter; }"
        "QDateEdit { qproperty-alignment: AlignCenter; }"
    );
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
