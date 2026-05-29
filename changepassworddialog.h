#ifndef CHANGEPASSWORDDIALOG_H
#define CHANGEPASSWORDDIALOG_H

#include <QDialog>
#include <QSqlDatabase>

namespace Ui {
class ChangePasswordDialog;
}

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangePasswordDialog(QSqlDatabase db, QWidget *parent = nullptr);
    ~ChangePasswordDialog();

private slots:
    void onConfirm();

private:
    Ui::ChangePasswordDialog *ui;
    QSqlDatabase db;
    void applyStyles();
};

#endif
