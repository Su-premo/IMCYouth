#include "changepassworddialog.h"
#include "ui_changepassworddialog.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QPushButton>

ChangePasswordDialog::ChangePasswordDialog(QSqlDatabase db, QWidget *parent)
    : QDialog(parent), ui(new Ui::ChangePasswordDialog), db(db)
{
    ui->setupUi(this);
    applyStyles();
    connect(ui->btnConfirm, &QPushButton::clicked, this, &ChangePasswordDialog::onConfirm);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

ChangePasswordDialog::~ChangePasswordDialog()
{
    delete ui;
}

void ChangePasswordDialog::applyStyles()
{
    ui->labelSub->setStyleSheet(
        "font-size: 11px; font-weight: 600; letter-spacing: 2px; color: #b8b84f;"
        );
    ui->labelTitle->setStyleSheet(
        "font-size: 18px; font-weight: 600; color: #222;"
        "padding-bottom: 16px; border-bottom: 1.5px solid #f0ead8;"
        );
    ui->leCurrentPw->setStyleSheet(
        "QLineEdit { height: 42px; border: 1.5px solid #e0d8c0; border-radius: 10px; background: #faf8f2; padding: 0 14px; }"
        "QLineEdit:focus { border-color: #d1d066; background: white; }"
        );
    ui->leNewPw->setStyleSheet(
        "QLineEdit { height: 42px; border: 1.5px solid #e0d8c0; border-radius: 10px; background: #faf8f2; padding: 0 14px; }"
        "QLineEdit:focus { border-color: #d1d066; background: white; }"
        );
    ui->leConfirmPw->setStyleSheet(
        "QLineEdit { height: 42px; border: 1.5px solid #e0d8c0; border-radius: 10px; background: #faf8f2; padding: 0 14px; }"
        "QLineEdit:focus { border-color: #d1d066; background: white; }"
        );
    ui->btnConfirm->setStyleSheet(
        "QPushButton { background: #d1d066; color: white; border: none; border-radius: 10px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #b8b84f; }"
        );
    ui->btnCancel->setStyleSheet(
        "QPushButton { background: transparent; border: 1.5px solid #d1d066; border-radius: 10px; color: #b8b84f; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #f2f1ac; }"
        );
}

void ChangePasswordDialog::onConfirm()
{
    QString currentPw = ui->leCurrentPw->text().trimmed();
    QString newPw     = ui->leNewPw->text().trimmed();
    QString confirmPw = ui->leConfirmPw->text().trimmed();

    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT value FROM codes WHERE name = 'app_password'");
    if (!checkQuery.exec() || !checkQuery.next()) {
        QMessageBox::warning(this, "오류", "현재 암호를 확인할 수 없습니다.");
        return;
    }
    if (checkQuery.value(0).toString() != currentPw) {
        QMessageBox::warning(this, "암호 불일치", "현재 암호가 올바르지 않습니다.");
        ui->leCurrentPw->clear();
        ui->leCurrentPw->setFocus();
        return;
    }
    if (newPw.isEmpty()) {
        QMessageBox::warning(this, "입력 오류", "새 암호를 입력하세요.");
        return;
    }
    if (newPw != confirmPw) {
        QMessageBox::warning(this, "암호 불일치", "새 암호와 확인 암호가 일치하지 않습니다.");
        ui->leNewPw->clear();
        ui->leConfirmPw->clear();
        ui->leNewPw->setFocus();
        return;
    }

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE codes SET value = :pw WHERE name = 'app_password'");
    updateQuery.bindValue(":pw", newPw);
    if (!updateQuery.exec()) {
        QMessageBox::critical(this, "오류", "암호 변경 실패:\n" + updateQuery.lastError().text());
        return;
    }

    ui->leCurrentPw->clear();
    ui->leNewPw->clear();
    ui->leConfirmPw->clear();
    QMessageBox::information(this, "완료", "암호가 변경되었습니다.");
    accept();
}
