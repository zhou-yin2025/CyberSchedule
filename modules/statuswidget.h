#ifndef STATUSWIDGET_H
#define STATUSWIDGET_H

#include "BaseFloatingWidget.h"
#include <QTimer>
#include <QPixmap>

class DatabaseManager;

class StatusWidget : public BaseFloatingWidget
{
    Q_OBJECT
    Q_PROPERTY(float expAnimValue READ expAnimValue WRITE setExpAnimValue)
    Q_PROPERTY(float hydAnimValue READ hydAnimValue WRITE setHydAnimValue)
    Q_PROPERTY(float loadAnimValue READ loadAnimValue WRITE setLoadAnimValue)

public:
    explicit StatusWidget(DatabaseManager *db, QWidget *parent = nullptr);
    ~StatusWidget();

    void playStartupAnimation();
    void setHydration(int value, int max);
    void setSystemLoad(int value);
    void addExp(int amount);

    float expAnimValue() const { return m_expAnimValue; }
    void setExpAnimValue(float v) { m_expAnimValue = v; update(); }
    float hydAnimValue() const { return m_hydAnimValue; }
    void setHydAnimValue(float v) { m_hydAnimValue = v; update(); }
    float loadAnimValue() const { return m_loadAnimValue; }
    void setLoadAnimValue(float v) { m_loadAnimValue = v; update(); }

public slots:
    void reloadSettings();

signals:
    void startupAnimationFinished();

protected:
    void drawNormalContent(QPainter &painter) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onHydrationTimer();
    void onLoadTimer();
    void updateStartupAnimation();
    void onAlertTimer();

private:
    void loadFromDatabase();
    void refreshFromTotalExp();

    void drawHexagon(QPainter &painter);
    void drawLightLines(QPainter &painter, const QPolygonF &hex, qreal highlightPhase = -1);
    void drawAvatar(QPainter &painter);
    void drawName(QPainter &painter);
    void drawProgressBars(QPainter &painter);
    void drawExpBar(QPainter &painter, int barX, int barWidth, int barHeight, int y);
    void drawHydrationBar(QPainter &painter, int barX, int barWidth, int barHeight, int y);
    void drawLoadBar(QPainter &painter, int barX, int barWidth, int barHeight, int y);

    QRect nameRect() const;
    QRect avatarRect() const;
    QRect hydrationButtonRect() const;
    QRect loadButtonRect() const;

    DatabaseManager *m_db;

    // 启动动画相关
    QTimer *m_startupTimer = nullptr;
    int m_startupFrame = 0;
    bool m_startupFinished = false;
    float m_lightPhase = 0.0f;
    float m_lightFastPhase = -1.0f;
    float m_innerAlpha = 0.0f;
    float m_avatarScale = 0.8f;
    float m_expProgress = 0.0f;
    float m_hydProgress = 0.0f;
    float m_loadProgress = 0.0f;
    int m_typingIndex = 0;

    QTimer *m_hydrationTimer;
    QTimer *m_loadTimer;
    QTimer *m_lightTimer;
    QTimer *m_alertTimer;               // 口渴提示定时器
    QTimer *m_animTimer;                // 升级动画定时器

    QString m_playerName;
    int m_totalExp = 0;
    int m_expCurrent = 0;
    int m_expMax = 100;
    int m_expLevel = 1;
    int m_hydration = 100;
    int m_hydrationMax = 100;
    int m_systemLoad = 25;

    QPixmap m_avatarPixmap;
    QString m_avatarPath;

    QString m_floatingExpText;
    int m_floatingExpTimer = 0;

    // 可配置参数
    int m_hydrationRate = 1;
    int m_loadIntervalSec = 8;
    int m_alertThreshold = 30;
    float m_lightSpeed = 0.005f;
    int m_lightDirection = -1;

    // 口渴值提示动画
    bool m_hydrationAlertActive = false;
    int m_hydrationAlertTimer = 0;

    // 升级弹跳动画
    bool m_levelUpAnim = false;
    int m_levelUpAnimTimer = 0;

    // 进度条平滑动画
    QPropertyAnimation *m_expAnim;
    QPropertyAnimation *m_hydAnim;
    QPropertyAnimation *m_loadAnim;

    // 动画中间值（0~1，表示动画进度）
    float m_expAnimValue = 1.0f;
    float m_hydAnimValue = 1.0f;
    float m_loadAnimValue = 1.0f;

    // 动画前的旧值（用于绘制淡色预填充条）
    int m_expOldValue = 0;
    int m_hydOldValue = 100;
    int m_loadOldValue = 0;

};

#endif