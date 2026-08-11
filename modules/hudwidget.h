#ifndef HUDWIDGET_H
#define HUDWIDGET_H

#include "BaseFloatingWidget.h"
#include <QTimer>
#include <QTime>
#include <QString>

class PomodoroTimer;

class HUDWidget : public BaseFloatingWidget
{
    Q_OBJECT

public:
    explicit HUDWidget(QWidget *parent = nullptr);
    ~HUDWidget();

    void playStartupAnimation();
    void startPomodoro(int workMin, int restMin, int cycles);
    PomodoroTimer *pomodoroTimer() const { return m_pomodoro; }

signals:
    void startupAnimationFinished();

protected:
    void drawNormalContent(QPainter &painter) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;  // 新增声明

public slots:
    void startPomodoroForTask(const QString &taskTitle);
    void onCompletedTimer();

private:
    void onPomodoroTick();
    void drawPomodoroStatus(QPainter &painter);

    QTimer *m_clockTimer;
    PomodoroTimer *m_pomodoro;
    bool m_pomodoroActive = false;
    QString m_pomodoroDisplay;

    bool m_startupFinished = true;
    float m_bgAlpha = 1.0f;
    QString m_currentTaskTitle;

    // 番茄钟完成提示
    bool m_showCompleted = false;
    int m_completedFrame = 0;   // 剩余显示帧数
    QTimer *m_completedTimer;
};

#endif