#ifndef CALENDARWIDGET_H
#define CALENDARWIDGET_H

#include "BaseFloatingWidget.h"
#include <QDate>
#include <QMap>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVector>
#include <QPainter>

class DatabaseManager;

class CalendarWidget : public BaseFloatingWidget
{
    Q_OBJECT
public:
    explicit CalendarWidget(DatabaseManager *db = nullptr, QWidget *parent = nullptr);
    ~CalendarWidget();

public slots:
    void reloadAndUpdate();

signals:
    void dateClicked(const QDate &date);

protected:
    void drawNormalContent(QPainter &painter) override;
    void mousePressEvent(QMouseEvent *event) override;       // 必须声明

private:
    void loadMonthTasks();
    QColor priorityColor(int prio) const;
    void startWashToExpanded();
    void drawExpandedContent(QPainter &painter);

    enum ViewMode { Collapsed, Expanded };
    ViewMode m_mode = Collapsed;
    QDate m_today;
    QDate m_selectedDate;
    QDate m_currentMonth;

    DatabaseManager *m_db;
    QMap<QDate, int> m_priorityMap;

    QPropertyAnimation *m_resizeAnim;
    QTimer *m_washTimer;
    float m_washProgress = 0.0f;
    bool m_washAnimating = false;
    bool m_pendingExpanded = false;

    QVector<WashParticle> m_washParticles;

    QRect calendarGridRect() const;
    QRect cellRect(int weekDay, int row) const;
    QDate dateAtCell(int weekDay, int row) const;
    void goToNextMonth();
    void goToPrevMonth();
    void goToToday();
};

#endif