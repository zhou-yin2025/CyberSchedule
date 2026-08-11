#ifndef POMODOROTIMER_H
#define POMODOROTIMER_H

#include <QObject>
#include <QTimer>

class PomodoroTimer : public QObject
{
    Q_OBJECT

public:
    enum State { Idle, Working, Resting };
    Q_ENUM(State)

    explicit PomodoroTimer(QObject *parent = nullptr);

    // 启动番茄钟
    void start(int workMinutes, int restMinutes, int totalCycles);
    void pause();
    void resume();
    void stop();

    // 获取当前状态
    State state() const { return m_state; }
    int remainingSeconds() const { return m_remainingSeconds; }
    int currentCycle() const { return m_currentCycle; }
    int totalCycles() const { return m_totalCycles; }
    QString stateText() const;

signals:
    void stateChanged(State newState);
    void tick();                    // 每秒触发
    void cycleCompleted(int cycle); // 完成一个工作周期
    void allCompleted();            // 全部完成

private slots:
    void onTick();

private:
    QTimer *m_timer;
    State m_state = Idle;
    int m_remainingSeconds = 0;
    int m_workDuration = 0;
    int m_restDuration = 0;
    int m_currentCycle = 0;
    int m_totalCycles = 0;
};

#endif // POMODOROTIMER_H