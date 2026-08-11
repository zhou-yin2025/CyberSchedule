#include "CalendarWidget.h"
#include "DatabaseManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <cstdlib>
#include <algorithm>

CalendarWidget::CalendarWidget(DatabaseManager *db, QWidget *parent)
    : BaseFloatingWidget(parent), m_db(db)
{
    m_today = QDate::currentDate();
    m_selectedDate = m_today;
    m_currentMonth = QDate(m_today.year(), m_today.month(), 1);
    loadMonthTasks();

    resize(120, 60);
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sr = screen->geometry();
        move(sr.width() - width() - 20, sr.height() - height() - 40);
    }

    m_resizeAnim = new QPropertyAnimation(this, "geometry");
    m_resizeAnim->setDuration(300);
    m_resizeAnim->setEasingCurve(QEasingCurve::InOutQuad);
}

CalendarWidget::~CalendarWidget() {}

void CalendarWidget::loadMonthTasks() {
    if (!m_db) return;
    m_priorityMap.clear();
    QVector<TaskData> tasks = m_db->getAllTasks("deadline");
    for (const auto &task : tasks) {
        QDate date = task.deadline.date();
        if (date.year() == m_currentMonth.year() && date.month() == m_currentMonth.month()) {
            if (!m_priorityMap.contains(date) || m_priorityMap[date] < task.priority)
                m_priorityMap[date] = task.priority;
        }
    }
}

QColor CalendarWidget::priorityColor(int prio) const {
    switch (prio) {
    case 3: return QColor(255, 50, 50, 120);
    case 2: return QColor(255, 200, 50, 120);
    case 1: return QColor(50, 200, 255, 120);
    case 0: return QColor(200, 200, 200, 80);
    default: return QColor(0, 0, 0, 0);
    }
}

QRect CalendarWidget::calendarGridRect() const { return QRect(5, 35, width() - 10, height() - 40); }

QRect CalendarWidget::cellRect(int weekDay, int row) const {
    QRect grid = calendarGridRect();
    int cellW = grid.width() / 7;
    int cellH = (grid.height() - 20) / 6;
    return QRect(grid.left() + weekDay * cellW, grid.top() + 20 + row * cellH, cellW, cellH);
}

QDate CalendarWidget::dateAtCell(int weekDay, int row) const {
    QDate firstDay(m_currentMonth.year(), m_currentMonth.month(), 1);
    int firstDayOfWeek = firstDay.dayOfWeek() % 7;
    int day = row * 7 + weekDay - firstDayOfWeek + 1;
    return firstDay.addDays(day - 1);
}

void CalendarWidget::goToNextMonth() { m_currentMonth = m_currentMonth.addMonths(1); loadMonthTasks(); update(); }
void CalendarWidget::goToPrevMonth() { m_currentMonth = m_currentMonth.addMonths(-1); loadMonthTasks(); update(); }
void CalendarWidget::goToToday() {
    m_currentMonth = QDate(m_today.year(), m_today.month(), 1);
    m_selectedDate = m_today;
    loadMonthTasks();
    emit dateClicked(QDate());
    update();
}
void CalendarWidget::reloadAndUpdate() { loadMonthTasks(); update(); }

// ---- 冲刷动画 ----
void CalendarWidget::startWashToExpanded() {
    QScreen *sc = QGuiApplication::primaryScreen();
    if (sc) {
        QRect sr = sc->geometry();
        resize(350, 280);
        move(sr.width() - width() - 20, sr.height() - height() - 40);
    }
    startPixelWash(WashRightToLeft);
    QTimer::singleShot(800, this, [this]() {
        m_mode = Expanded;
        update();
    });
}

void CalendarWidget::drawExpandedContent(QPainter &painter) {
    QRect r = rect();
    QFont navFont("Courier New", 10, QFont::Bold);
    painter.setFont(navFont);
    painter.setPen(QColor(0, 255, 200));
    painter.drawText(QRectF(5, 5, 20, 20), Qt::AlignCenter, "<");
    painter.drawText(QRectF(r.width() - 25, 5, 20, 20), Qt::AlignCenter, ">");
    painter.drawText(QRectF(30, 5, r.width() - 60, 20), Qt::AlignCenter, m_currentMonth.toString("yyyy-MM"));
    painter.setPen(QColor(255, 0, 150));
    painter.drawText(QRectF(r.width() - 80, 5, 50, 20), Qt::AlignCenter, "今");

    QStringList weekDays = {"日", "一", "二", "三", "四", "五", "六"};
    QFont weekFont("Courier New", 8);
    painter.setFont(weekFont);
    QRect grid = calendarGridRect();
    int cellW = grid.width() / 7;
    for (int i = 0; i < 7; ++i) {
        painter.setPen(QColor(255, 0, 150));
        painter.drawText(QRectF(grid.left() + i * cellW, grid.top(), cellW, 18), Qt::AlignCenter, weekDays[i]);
    }

    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            QDate date = dateAtCell(col, row);
            QRect cell = cellRect(col, row);
            if (date.month() != m_currentMonth.month()) {
                painter.setPen(QColor(100, 100, 100));
                painter.drawText(QRectF(cell), Qt::AlignCenter, QString::number(date.day()));
                continue;
            }
            if (m_priorityMap.contains(date))
                painter.fillRect(cell, priorityColor(m_priorityMap[date]));
            painter.setPen(QColor(0, 255, 200));
            if (date == m_selectedDate) {
                painter.setPen(QPen(QColor(0, 255, 200), 2));
                painter.drawRect(cell);
            }
            if (date == m_today) painter.setPen(QColor(255, 0, 150));
            painter.drawText(QRectF(cell), Qt::AlignCenter, QString::number(date.day()));
        }
    }
}

void CalendarWidget::drawNormalContent(QPainter &painter) {
    QRect r = rect();
    painter.fillRect(r, QColor(20, 10, 30, 240));
    painter.setPen(QPen(QColor(0, 255, 200), 2));
    painter.drawRect(1, 1, r.width() - 2, r.height() - 2);
    if (m_mode == Collapsed) {
        QFont bigFont("Courier New", 20, QFont::Bold);
        painter.setFont(bigFont);
        painter.setPen(QColor(0, 255, 200));
        painter.drawText(QRectF(r), Qt::AlignCenter, m_today.toString("dd"));
        QFont smallFont("Courier New", 8);
        painter.setFont(smallFont);
        painter.setPen(QColor(255, 0, 150));
        painter.drawText(QRectF(0, r.height() - 15, r.width(), 12), Qt::AlignCenter, m_today.toString("ddd"));
        return;
    }
    drawExpandedContent(painter);
}

void CalendarWidget::mousePressEvent(QMouseEvent *event) {
    if (m_washAnimating) return;
    QPoint pos = event->pos();
    if (m_mode == Collapsed) {
        m_mode = Expanded;
        QRect geo = geometry();
        QSize targetSize(350, 280);
        QPoint newTopLeft = geo.bottomRight() - QPoint(targetSize.width(), targetSize.height());
        QRect targetRect(newTopLeft, targetSize);
        m_resizeAnim->stop();
        m_resizeAnim->setStartValue(geo);
        m_resizeAnim->setEndValue(targetRect);
        m_resizeAnim->start();
        update();
        return;
    }
    if (QRect(5, 5, 20, 20).contains(pos)) { goToPrevMonth(); return; }
    if (QRect(width() - 25, 5, 20, 20).contains(pos)) { goToNextMonth(); return; }
    if (QRect(width() - 80, 5, 50, 20).contains(pos)) { goToToday(); return; }
    for (int r = 0; r < 6; ++r) for (int c = 0; c < 7; ++c) {
            if (cellRect(c, r).contains(pos)) {
                m_selectedDate = dateAtCell(c, r);
                update();
                emit dateClicked(m_selectedDate);
                return;
            }
        }
    m_mode = Collapsed;
    QRect geo = geometry();
    QSize targetSize(120, 60);
    QPoint newTopLeft = geo.bottomRight() - QPoint(targetSize.width(), targetSize.height());
    m_resizeAnim->stop();
    m_resizeAnim->setStartValue(geo);
    m_resizeAnim->setEndValue(QRect(newTopLeft, targetSize));
    m_resizeAnim->start();
    update();
}