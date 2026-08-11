#include "GridBackground.h"
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QtMath>

GridBackground::GridBackground(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) setGeometry(screen->geometry());

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GridBackground::updatePhase);
    m_timer->start(30);
}

GridBackground::~GridBackground() {}

void GridBackground::updatePhase()
{
    m_lightPhase += 0.005f;
    if (m_lightPhase >= 1.0f) m_lightPhase -= 1.0f;

    m_runnerPhase += 2.0f;   // 控制灯线滑动速度，越大越快
    if (m_runnerPhase >= 10000.0f) m_runnerPhase -= 10000.0f;

    update();
}

void GridBackground::drawCircleWithLights(QPainter &painter, const QPointF &center, float radius, float phase)
{
    painter.setPen(QPen(QColor(0, 160, 255, 200), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);

    float outerRadius = radius + 6;
    float lightLen = 0.25f;
    int segments = 12;
    for (int i = 0; i < 3; ++i) {
        float startAngle = (phase + i / 3.0f) * 2 * M_PI;
        float endAngle = startAngle + lightLen * 2 * M_PI;
        float step = (endAngle - startAngle) / segments;
        QColor lightColor(0, 180, 255, 220);
        painter.setPen(QPen(lightColor, 3));
        for (int s = 0; s < segments; ++s) {
            float a1 = startAngle + s * step;
            float a2 = a1 + step;
            QPointF p1(center.x() + outerRadius * cos(a1), center.y() + outerRadius * sin(a1));
            QPointF p2(center.x() + outerRadius * cos(a2), center.y() + outerRadius * sin(a2));
            painter.drawLine(p1, p2);
        }
    }
}

void GridBackground::drawLineWithDotRunner(QPainter &painter, const QPointF &start, const QPointF &end)
{
    QPointF dir = end - start;
    float length = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
    if (length < 2.0f) return;
    QPointF unit = dir / length;

    float dotSpacing = 1.0f;        // 更密集的点
    int dotCount = (int)(length / dotSpacing);
    if (dotCount < 2) return;

    // 1. 静态底色：亮度提高
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 80, 160, 80));
    for (int i = 0; i < dotCount; ++i) {
        QPointF pos = start + unit * (i * dotSpacing);
        painter.drawRect(QRectF(pos.x(), pos.y(), 3, 3));
    }

    // 2. 灯线部分：更亮、更宽
    int lightSpan = qMin(10, dotCount / 2);        // 灯线点数增加到10个
    int startIndex = (int)(m_runnerPhase) % dotCount;

    // 灯线从亮到暗拖尾，产生流动感
    for (int i = 0; i < lightSpan; ++i) {
        int idx = (startIndex - i + dotCount) % dotCount; // 向后拖尾
        int alpha = 255 - i * 20;                         // 逐渐变暗
        alpha = qMax(80, alpha);
        painter.setBrush(QColor(80, 200, 255, 255));   // 亮青色
        QPointF pos = start + unit * (idx * dotSpacing);
        painter.drawRect(QRectF(pos.x(), pos.y(), 3, 3));
    }
}

void GridBackground::drawPolylineWithRunner(QPainter &painter, const QVector<QPointF> &points)
{
    if (points.size() < 2) return;

    // 1. 静态轨迹（深暗）
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 80, 160, 80));
    int dotSize = 3;
    float dotSpacing = 3.0f;
    for (int i = 0; i < points.size() - 1; ++i) {
        QPointF start = points[i];
        QPointF end = points[i + 1];
        QPointF dir = end - start;
        float length = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
        if (length < 1.0f) continue;
        QPointF unit = dir / length;
        for (float d = 0; d < length; d += dotSpacing) {
            QPointF pos = start + unit * d;
            painter.drawRect(QRectF(pos.x(), pos.y(), dotSize, dotSize));
        }
    }

    // 2. 计算总长度
    QVector<float> segLengths;
    float totalLength = 0.0f;
    for (int i = 0; i < points.size() - 1; ++i) {
        float segLen = std::sqrt((points[i + 1].x() - points[i].x()) * (points[i + 1].x() - points[i].x()) +
                                 (points[i + 1].y() - points[i].y()) * (points[i + 1].y() - points[i].y()));
        segLengths.append(segLen);
        totalLength += segLen;
    }
    if (totalLength < 2.0f) return;

    // 3. 灯线（亮蓝色，连贯滑动）
    int lightSpan = 10;  // 灯线点数
    float lightLength = lightSpan * dotSpacing;
    float startDist = std::fmod(m_runnerPhase * 0.5f, totalLength + lightLength) - lightLength;

    // 获取路径上距离起点的位置
    auto pointAtDist = [&](float dist) -> QPointF {
        if (dist < 0) dist += totalLength;
        if (dist >= totalLength) dist -= totalLength;
        float accum = 0.0f;
        for (int i = 0; i < segLengths.size(); ++i) {
            if (accum + segLengths[i] >= dist) {
                float t = (dist - accum) / segLengths[i];
                return points[i] + t * (points[i + 1] - points[i]);
            }
            accum += segLengths[i];
        }
        return points.last();
    };

    // 绘制灯线点（连续）
    painter.setBrush(QColor(80, 200, 255, 255));
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < lightSpan; ++i) {
        float dist = startDist + i * dotSpacing;
        QPointF pos = pointAtDist(dist);
        painter.drawRect(QRectF(pos.x(), pos.y(), dotSize, dotSize));
    }
}

void GridBackground::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    QRect r = rect();

    const float radius = 30.0f;
    const float startDist = radius + 6;
    const float horizontalLength = r.width() / 4.0f;
    const QPointF center(r.center());

    // ============= 左下角圆 =============
    QPointF leftBottom(60, r.height() - 60);
    drawCircleWithLights(painter, leftBottom, radius, m_lightPhase);

    float startAngle = 225.0f * M_PI / 180.0f;
    QPointF diagStart(leftBottom.x() - startDist * cos(startAngle),
                      leftBottom.y() - startDist * sin(startAngle));
    QPointF diagEnd = diagStart + QPointF(10, 10);
    QPointF firstEnd = diagEnd + QPointF(horizontalLength, 0);

    // 折线：斜线 + 第一条水平线（共享连贯灯线）
    drawPolylineWithRunner(painter, {diagStart, diagEnd, firstEnd});

    // 第二条水平线（独立）
    float midX = (diagEnd.x() + firstEnd.x()) / 2.0f;
    float midY = diagEnd.y() + 10;
    QPointF secondStart(midX, midY);
    QPointF secondEnd = secondStart + QPointF(horizontalLength / 2 + 30, 0);
    drawLineWithDotRunner(painter, secondStart, secondEnd);

    // ============= 右上角圆（180°旋转对称） =============
    // 计算左下角各点绕屏幕中心旋转180°后的坐标
    auto rotate180 = [&center](const QPointF &p) {
        return QPointF(2 * center.x() - p.x(), 2 * center.y() - p.y());
    };

    QPointF rightTop = rotate180(leftBottom);
    drawCircleWithLights(painter, rightTop, radius, m_lightPhase);

    QPointF diagStart2 = rotate180(diagStart);
    QPointF diagEnd2 = rotate180(diagEnd);
    QPointF firstEnd2 = rotate180(firstEnd);
    drawPolylineWithRunner(painter, {diagStart2, diagEnd2, firstEnd2});

    QPointF secondStart2 = rotate180(secondStart);
    QPointF secondEnd2 = rotate180(secondEnd);
    drawLineWithDotRunner(painter, secondStart2, secondEnd2);
}