#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QSqlDatabase>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QSqlDatabase db, QWidget *parent = nullptr);
    ~LoginDialog();

    int getLoggedInUserId() const { return loggedInUserId; }

private:
    Ui::LoginDialog *ui;
    QSqlDatabase db;
    int loggedInUserId = -1;

    void on_btnConfirm_clicked();
};

#endif // LOGINDIALOG_H
