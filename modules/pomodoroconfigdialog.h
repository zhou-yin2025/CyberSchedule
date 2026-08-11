#ifndef POMODOROCONFIGDIALOG_H
#define POMODOROCONFIGDIALOG_H

#include <QDialog>

namespace Ui { class PomodoroConfigDialog; }

class PomodoroConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PomodoroConfigDialog(QWidget *parent = nullptr);
    ~PomodoroConfigDialog();

    int workMinutes() const;
    int restMinutes() const;
    int totalCycles() const;

signals:
    void startRequested(int workMin, int restMin, int cycles);

private slots:
    void onPresetChanged(int index);
    void onStartClicked();

private:
    Ui::PomodoroConfigDialog *ui;
};

#endif // POMODOROCONFIGDIALOG_H