#include "HUDWidget.h"
#include "PomodoroTimer.h"
#include "PomodoroConfigDialog.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QTime>
#include <QtMath>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

HUDWidget::HUDWidget(QWidget *parent)
    : BaseFloatingWidget(parent)
{
    resize(280, 80);
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sr = screen->geometry();
        move((sr.width() - width()) / 2, 0);
    }

    // 时钟更新
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, [this]() {
        update();
    });
    m_clockTimer->start(1000);

    // 番茄钟逻辑
    m_pomodoro = new PomodoroTimer(this);
    connect(m_pomodoro, &PomodoroTimer::tick, this, &HUDWidget::onPomodoroTick);
    connect(m_pomodoro, &PomodoroTimer::stateChanged, this,
            [this](PomodoroTimer::State state) {
                m_pomodoroActive = (state != PomodoroTimer::Idle);
                if (!m_pomodoroActive) {
                    m_currentTaskTitle.clear();
                }
                #ifdef Q_OS_WIN
                if (state == PomodoroTimer::Working) {
                    Beep(880, 150);   // 短促高音
                } else if (state == PomodoroTimer::Resting) {
                    Beep(440, 200);   // 低沉提示音
                }
                #endif
                update();
            });

    // 番茄钟完成提示定时器
    m_completedTimer = new QTimer(this);
    connect(m_completedTimer, &QTimer::timeout, this, &HUDWidget::onCompletedTimer);
}

HUDWidget::~HUDWidget() {}

void HUDWidget::playStartupAnimation()
{
    m_startupFinished = true;
    emit startupAnimationFinished();
}

void HUDWidget::onPomodoroTick()
{
    int secs = m_pomodoro->remainingSeconds();
    int min = secs / 60;
    int sec = secs % 60;
    m_pomodoroDisplay = QString("%1:%2").arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    update();
}

void HUDWidget::startPomodoro(int workMin, int restMin, int cycles)
{
    m_pomodoro->start(workMin, restMin, cycles);
    m_pomodoroActive = true;
    update();
}

void HUDWidget::onCompletedTimer()
{
    m_completedFrame--;
    if (m_completedFrame <= 0) {
        m_showCompleted = false;
        m_completedTimer->stop();
    }
    update();
}

void HUDWidget::drawPomodoroStatus(QPainter &painter)
{
    if (!m_pomodoroActive) return;

    // 任务名称（放在番茄钟上方，亮青色，稍小字体）
    if (!m_currentTaskTitle.isEmpty()) {
        QFont taskFont("Courier New", 8, QFont::Bold);
        painter.setFont(taskFont);
        painter.setPen(QColor(0, 255, 255));
        // 距离顶部 15px，高度 16px，避免与顶部边框重叠
        QRectF taskRect(0, 25, width(), 16);
        painter.drawText(taskRect, Qt::AlignCenter, m_currentTaskTitle);
    }

    // 番茄钟倒计时（放在任务名称下方）
    QFont timeFont("Courier New", 10, QFont::Bold);
    painter.setFont(timeFont);
    painter.setPen(QColor(255, 0, 150));
    QString text = m_pomodoro->stateText() + " " + m_pomodoroDisplay;
    if (m_pomodoro->totalCycles() > 1) {
        text += QString(" [%1/%2]").arg(m_pomodoro->currentCycle() + 1).arg(m_pomodoro->totalCycles());
    }
    // 距离顶部 35px，高度 20px，在任务名称下方
    QRectF pomoRect(0, 55, width(), 20);
    painter.drawText(pomoRect, Qt::AlignCenter, text);
}

void HUDWidget::drawNormalContent(QPainter &painter)
{
    int w = width(), h = height();
    int margin = 15;

    // 梯形路径
    QPolygonF trapezoid;
    trapezoid << QPointF(0, 0) << QPointF(w, 0)
              << QPointF(w - margin, h) << QPointF(margin, h);
    QPainterPath path;
    path.addPolygon(trapezoid);
    painter.setClipPath(path);

    // 背景
    painter.fillRect(rect(), QColor(20, 10, 30, 220));
    painter.setPen(QPen(QColor(0, 255, 200), 2));
    painter.drawPolygon(trapezoid);

    // 背景方格线（四周向中心渐隐）
    int spacing = 8;
    for (int x = spacing; x < w; x += spacing) {
        for (int y = spacing; y < h; y += spacing) {
            float dx = (x - w / 2.0f) / (w / 2.0f);
            float dy = (y - h / 2.0f) / (h / 2.0f);
            float dist = std::sqrt(dx * dx + dy * dy);
            int alpha = 30 + (int)(40 * dist);
            painter.setPen(QColor(0, 255, 200, qBound(20, alpha, 70)));
            painter.drawPoint(x, y);
        }
    }

    // 时间显示：HH:MM 居中 + ss 右下角
    QTime now = QTime::currentTime();
    QString hm = now.toString("hh:mm");
    QString ss = now.toString("ss");

    QFont timeFont("Courier New", 32, QFont::Bold);
    QFont secFont("Courier New", 8, QFont::Bold);

    // 绘制 HH:MM 并获取其实际矩形
    painter.setFont(timeFont);
    painter.setPen(QColor(0, 255, 200));
    QRectF usedRect;
    painter.drawText(QRectF(0, 0, w, h), Qt::AlignCenter, hm, &usedRect);

    // 绘制秒数字在 usedRect 右下角
    painter.setFont(secFont);
    // 你可根据自己的视觉偏好微调坐标
    QRectF ssRect(usedRect.right(), usedRect.bottom() - 15, 20, 10);
    painter.drawText(ssRect, Qt::AlignRight | Qt::AlignBottom, ss);

    // 番茄钟状态
    if (m_pomodoroActive) {
        drawPomodoroStatus(painter);
    }

    // 番茄钟完成提示
    if (m_showCompleted) {
        int alpha = 255;
        if (m_completedFrame < 20) {
            alpha = m_completedFrame * 255 / 20; // 淡出
        } else if (m_completedFrame > 70) {
            alpha = (90 - m_completedFrame) * 255 / 20; // 淡入
        }
        // 闪烁：偶数帧用青色，奇数帧用洋红
        QColor textColor = (m_completedFrame / 10) % 2 == 0
                               ? QColor(0, 255, 200, alpha)
                               : QColor(255, 0, 150, alpha);
        QFont promptFont("Courier New", 9, QFont::Bold);
        painter.setFont(promptFont);
        painter.setPen(textColor);
        QRectF promptRect(0, height() - 18, width(), 16);
        painter.drawText(promptRect, Qt::AlignCenter, "COMPLETE");
    }
}

void HUDWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    // 非模态方式复用第一版 UI，父对象为 nullptr 避免 Tool 标志冲突
    PomodoroConfigDialog *dlg = new PomodoroConfigDialog(nullptr);
    dlg->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    dlg->setAttribute(Qt::WA_ShowWithoutActivating, true);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // 居中显示
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect r = screen->geometry();
        dlg->move(r.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
    }

    connect(dlg, &PomodoroConfigDialog::startRequested, this,
            [this](int work, int rest, int cycles) {
                startPomodoro(work, rest, cycles);
            });

    dlg->show();
}

void HUDWidget::startPomodoroForTask(const QString &taskTitle)
{
    m_currentTaskTitle = taskTitle;

    // 弹出配置对话框
    PomodoroConfigDialog *dlg = new PomodoroConfigDialog(nullptr);
    dlg->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    dlg->setAttribute(Qt::WA_ShowWithoutActivating, true);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect r = screen->geometry();
        dlg->move(r.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
    }

    connect(dlg, &PomodoroConfigDialog::startRequested, this,
            [this](int work, int rest, int cycles) {
                startPomodoro(work, rest, cycles);
                update();
            });

    dlg->show();
}