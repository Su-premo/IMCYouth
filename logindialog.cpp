#include "logindialog.h"
#include "ui_logindialog.h"
#include <QSqlQuery>
#include <QMessageBox>
#include <QAction>

LoginDialog::LoginDialog(QSqlDatabase db, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , db(db)
{
    ui->setupUi(this);
    setWindowTitle("IMCYouth");

    setStyleSheet(R"(
    QDialog {
        background-color: #f7f3e8;
    }

    /* 입력창 */
    QLineEdit {
        border: 1px solid #e2dccf;
        border-radius: 12px;
        padding: 8px;
        font-size: 16px;
        min-height: 36px;
    }

    /* 입력창 포커스 */
    QLineEdit:focus {
        border: 2px solid #e8e39a;
    }

    /* 버튼 */
    QPushButton {
        background-color: #f2f1ac;
        border: none;
        border-radius: 14px;
        padding: 10px;
        font-size: 16px;
        font-weight: bold;
        color: #444;
        min-height: 38px;
    }

    /* 버튼 호버 */
    QPushButton:hover {
        background-color: #ecea8f;
    }

    /* 버튼 클릭 */
    QPushButton:pressed {
        background-color: #e3e07a;
    }

    /* 메시지박스 */
    QMessageBox {
        background-color: #f7f3e8;
    }

    /* 눈 아이콘 여백 */
    QLineEdit::add-action {
        margin-right: 6px;
    }
    )");

    // 가운데 정렬 + 비밀번호 모드
    ui->inputCode->setAlignment(Qt::AlignCenter);
    ui->inputCode->setEchoMode(QLineEdit::Password);
    ui->inputCode->setTextMargins(30, 0, 0, 0); // 왼쪽만 여백

    ui->inputCode->setFixedHeight(39);
    ui->btnConfirm->setFixedHeight(39);

    // 눈 아이콘 추가
    QAction *toggleAction = ui->inputCode->addAction(
        QIcon::fromTheme("view-hidden"),
        QLineEdit::TrailingPosition
    );
    connect(toggleAction, &QAction::triggered, this, [this, toggleAction](){
        if (ui->inputCode->echoMode() == QLineEdit::Password) {
            ui->inputCode->setEchoMode(QLineEdit::Normal);
            toggleAction->setIcon(QIcon::fromTheme("view-visible"));
        } else {
            ui->inputCode->setEchoMode(QLineEdit::Password);
            toggleAction->setIcon(QIcon::fromTheme("view-hidden"));
        }
    });

    connect(ui->inputCode, &QLineEdit::returnPressed, this, &LoginDialog::on_btnConfirm_clicked);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::on_btnConfirm_clicked()
{
    QString input = ui->inputCode->text();

    QSqlQuery q(db);
    q.prepare("SELECT value FROM codes WHERE name = 'app_password'");
    q.exec();

    if (q.next() && q.value(0).toString() == input) {
        accept();                   // 성공 → 메인창 오픈
    } else {
        QMessageBox::warning(this, "", "누구세요?");
        ui->inputCode->clear();
    }
}
