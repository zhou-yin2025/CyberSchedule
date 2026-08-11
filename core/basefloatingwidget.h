#ifndef BASEFLOATINGWIDGET_H
#define BASEFLOATINGWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <cstdlib>
#include <algorithm>

// 冲刷粒子结构
struct WashParticle {
    float x, y, speed, alpha;
    int size, life, maxLife;
    QVector<QPointF> trail;
    int maxTrailLength = 6;

    void updateTrail() {
        trail.prepend(QPointF(x, y));
        while (trail.size() > maxTrailLength) trail.pop_back();
    }

    void draw(QPainter &painter) const {
        for (int i = 1; i < trail.size(); ++i) {
            float ratio = 1.0f - (float)i / maxTrailLength;
            QColor trailColor(0, 120, 220, (int)(alpha * ratio * 0.25));
            painter.setBrush(trailColor);
            painter.setPen(Qt::NoPen);
            float rsize = size * ratio;
            painter.drawRect(QRectF(trail[i].x(), trail[i].y(), rsize, rsize));
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 80, 180, (int)(alpha * 0.8)));
        painter.drawRect(QRectF(x + 1, y + 1, size, size));
        painter.setBrush(QColor(0, 160, 255, (int)(alpha * 0.8)));
        painter.drawRect(QRectF(x, y, size, size));
    }
};

class BaseFloatingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BaseFloatingWidget(QWidget *parent = nullptr);
    virtual ~BaseFloatingWidget();

    void setMouseTransparent(bool transparent);
    void setAutoHideEnabled(bool enabled, double inactiveOpacity = 0.3, int delayMs = 1000);

    enum WashDirection { WashRightToLeft, WashLeftToRight };

    // 外部可调用的公共冲刷接口
    void startLeftToRightWash() { startPixelWash(WashLeftToRight); }
    void startRightToLeftWash() { startPixelWash(WashRightToLeft); }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void drawNormalContent(QPainter &painter);

    void startPixelWash(WashDirection direction = WashRightToLeft);
    virtual void drawWashParticles(QPainter &painter);

private slots:
    void updatePixelWash();

private:
    bool m_isDragging = false;
    QPoint m_dragStartPos;
    QTimer *m_autoHideTimer;
    bool m_autoHideEnabled = false;
    double m_inactiveOpacity = 0.3;
    int m_autoHideDelayMs = 1000;

    QTimer *m_washTimer = nullptr;
    QVector<WashParticle> m_washParticles;
    float m_washProgress = 0.0f;
    bool m_washAnimating = false;
    WashDirection m_washDirection = WashRightToLeft;
};

#endif