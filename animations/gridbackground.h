#ifndef GRIDBACKGROUND_H
#define GRIDBACKGROUND_H

#include <QWidget>
#include <QTimer>

class GridBackground : public QWidget
{
    Q_OBJECT
public:
    explicit GridBackground(QWidget *parent = nullptr);
    ~GridBackground();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updatePhase();
    void drawCircleWithLights(QPainter &painter, const QPointF &center, float radius, float phase);
    void drawLineWithRunner(QPainter &painter, const QPointF &start, const QPointF &end);

    QTimer *m_timer;
    float m_lightPhase = 0.0f;      // 灯线旋转相位
    float m_runnerPhase = 0.0f;     // 线条上的灯线流动相位

    void drawPolylineWithRunner(QPainter &painter, const QVector<QPointF> &points);
    void drawLineWithDotRunner(QPainter &painter, const QPointF &start, const QPointF &end);
};

#endif