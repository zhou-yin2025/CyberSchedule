#include "pomodorotimer.h"

PomodoroTimer::PomodoroTimer(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1000); // 每秒触发
    connect(m_timer, &QTimer::timeout, this, &PomodoroTimer::onTick);
}

QString PomodoroTimer::stateText() const
{
    switch (m_state) {
    case Idle:    return QStringLiteral("就绪");
    case Working: return QStringLiteral("工作中");
    case Resting: return QStringLiteral("休息中");
    }
    return {};
}

void PomodoroTimer::start(int workMinutes, int restMinutes, int totalCycles)
{
    stop();
    m_workDuration = workMinutes * 60;
    m_restDuration = restMinutes * 60;
    m_totalCycles = totalCycles;
    m_currentCycle = 0;

    // 开始第一个工作周期
    m_state = Working;
    m_remainingSeconds = m_workDuration;
    m_timer->start();
    emit stateChanged(m_state);
    emit tick();
}

void PomodoroTimer::pause()
{
    m_timer->stop();
}

void PomodoroTimer::resume()
{
    if (m_state != Idle)
        m_timer->start();
}

void PomodoroTimer::stop()
{
    m_timer->stop();
    m_state = Idle;
    m_remainingSeconds = 0;
    m_currentCycle = 0;
    emit stateChanged(m_state);
}

void PomodoroTimer::onTick()
{
    m_remainingSeconds--;
    emit tick();

    if (m_remainingSeconds <= 0)
    {
        if (m_state == Working)
        {
            // 工作周期结束
            m_currentCycle++;
            emit cycleCompleted(m_currentCycle);

            if (m_currentCycle >= m_totalCycles)
            {
                // 全部完成
                stop();
                emit allCompleted();
                return;
            }

            // 进入休息
            m_state = Resting;
            m_remainingSeconds = m_restDuration;
            emit stateChanged(m_state);
        }
        else if (m_state == Resting)
        {
            // 休息结束，进入下一个工作周期
            m_state = Working;
            m_remainingSeconds = m_workDuration;
            emit stateChanged(m_state);
        }
    }
}