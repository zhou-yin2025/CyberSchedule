#include "BaseFloatingWidget.h"
#include <QPainterPath>
#include <QGraphicsOpacityEffect>

BaseFloatingWidget::BaseFloatingWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowWithoutActivating, true);   // 不影响焦点体验
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setWindowFlag(Qt::NoDropShadowWindowHint, true);
    setMouseTransparent(false);

    m_autoHideTimer = new QTimer(this);
    m_autoHideTimer->setSingleShot(true);
    connect(m_autoHideTimer, &QTimer::timeout, this, [this]() {
        if (!underMouse()) setWindowOpacity(m_inactiveOpacity);
    });
    setFocusPolicy(Qt::ClickFocus);   // 点击窗口时能获得键盘焦点
}

BaseFloatingWidget::~BaseFloatingWidget() {}

void BaseFloatingWidget::drawNormalContent(QPainter &painter)
{
    Q_UNUSED(painter);
    // 子类重写此函数来绘制各自的内容
}

void BaseFloatingWidget::setMouseTransparent(bool transparent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, transparent);
}

void BaseFloatingWidget::setAutoHideEnabled(bool enabled, double inactiveOpacity, int delayMs)
{
    m_autoHideEnabled = enabled;
    m_inactiveOpacity = inactiveOpacity;
    m_autoHideDelayMs = delayMs;
    if (!enabled) {
        m_autoHideTimer->stop();
        setWindowOpacity(1.0);
    }
}

void BaseFloatingWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragStartPos = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void BaseFloatingWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();                      // 按 Esc 关闭窗口
        return;
    }
    QWidget::keyPressEvent(event);    // 其他按键正常处理
}

void BaseFloatingWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragStartPos);
        event->accept();
    }
}

void BaseFloatingWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_isDragging = false;
}

void BaseFloatingWidget::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    if (m_autoHideEnabled) {
        m_autoHideTimer->stop();
        setWindowOpacity(1.0);
    }
}

void BaseFloatingWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    if (m_autoHideEnabled)
        m_autoHideTimer->start(m_autoHideDelayMs);
}

// ---- 冲刷动画 ----
void BaseFloatingWidget::startPixelWash(WashDirection direction)
{
    m_washDirection = direction;
    m_washAnimating = true;
    m_washProgress = 0.0f;
    m_washParticles.clear();
    if (!m_washTimer) {
        m_washTimer = new QTimer(this);
        connect(m_washTimer, &QTimer::timeout, this, &BaseFloatingWidget::updatePixelWash);
    }
    m_washTimer->start(16);
}

void BaseFloatingWidget::updatePixelWash()
{
    m_washProgress += 0.035f;
    if (m_washProgress >= 1.0f) {
        m_washProgress = 1.0f;
        m_washTimer->stop();
        m_washAnimating = false;
        m_washParticles.clear();
        update();
        return;
    }

    QRect r = rect();
    int threshold;
    if (m_washDirection == WashRightToLeft) {
        threshold = (int)(r.width() * (1.0f - m_washProgress));
    } else {
        threshold = (int)(r.width() * m_washProgress);
    }

    for (int i = 0; i < 30; ++i) {
        WashParticle p;
        if (m_washDirection == WashRightToLeft) {
            p.x = threshold + (std::rand() % 40) - 20;
            p.speed = 0.5f + (std::rand() % 2);
        } else {
            p.x = threshold + (std::rand() % 40) - 20;
            p.speed = -(0.5f + (std::rand() % 2));
        }
        p.y = std::rand() % r.height();
        p.size = 6 + (std::rand() % 7);
        p.maxLife = 1 + (std::rand() % 15);
        p.life = p.maxLife;
        p.alpha = 140 + (std::rand() % 70);
        m_washParticles.push_back(p);
    }
    for (int i = 0; i < m_washParticles.size(); ++i) {
        auto &p = m_washParticles[i];
        p.updateTrail();
        p.x -= p.speed;
        p.life--;
    }
    m_washParticles.erase(std::remove_if(m_washParticles.begin(), m_washParticles.end(),
                                         [](const WashParticle &p) { return p.life <= 0; }), m_washParticles.end());
    update();
}

void BaseFloatingWidget::drawWashParticles(QPainter &painter)
{
    painter.setPen(Qt::NoPen);
    for (const auto &p : m_washParticles)
        p.draw(painter);
}

void BaseFloatingWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    QRect r = rect();

    if (m_washAnimating) {
        int threshold;
        if (m_washDirection == WashRightToLeft) {
            threshold = (int)(r.width() * (1.0f - m_washProgress));
            QPainterPath clipPath;
            clipPath.addRect(threshold, 0, r.width() - threshold, r.height());
            painter.setClipPath(clipPath);
        } else {
            threshold = (int)(r.width() * m_washProgress);
            QPainterPath clipPath;
            clipPath.addRect(0, 0, threshold, r.height());
            painter.setClipPath(clipPath);
        }
        drawNormalContent(painter);
        painter.setClipping(false);
        drawWashParticles(painter);
    } else {
        drawNormalContent(painter);
    }
}