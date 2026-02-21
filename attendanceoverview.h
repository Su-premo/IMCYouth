#ifndef ATTENDANCEOVERVIEW_H
#define ATTENDANCEOVERVIEW_H

#include <QWidget>
#include <QSqlDatabase>

namespace Ui {
class AttendanceOverview;
}

class AttendanceOverview : public QWidget
{
    Q_OBJECT

public:
    explicit AttendanceOverview(QWidget *parent = nullptr);
    ~AttendanceOverview();
//    void loadOverview(int year, int month);
    void loadOverview(int year);

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::AttendanceOverview *ui;
    int savedYear;
    int savedMonth;
};

#endif // ATTENDANCEOVERVIEW_H
