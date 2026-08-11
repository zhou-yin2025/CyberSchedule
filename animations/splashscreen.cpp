#include "SplashScreen.h"
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QtMath>

SplashScreen::SplashScreen(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) setGeometry(screen->geometry());

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SplashScreen::updateAnimation);
    m_timer->start(16);
}

SplashScreen::~SplashScreen() {}

void SplashScreen::updateAnimation()
{
    m_frame++;

    // 阶段 [冲刷] 0 -> 1
    if (m_washPhase == 0) {
        // 每帧生成新粒子 (从左向右冲刷)
        for (int i = 0; i < 30; ++i) {
            WashParticle p;
            p.x = rect().width() * (1.0f - m_washProgress) + (rand() % 20 - 10);
            p.y = rand() % rect().height();
            p.speed = 0.5f + (rand() % 2);
            p.size = 6 + (rand() % 7);
            p.maxLife = 1 + (rand() % 15);
            p.life = p.maxLife;
            p.alpha = 140 + (rand() % 70);
            m_washParticles.push_back(p);
        }
        // 移动粒子并销毁死亡的粒子
        for (int i = 0; i < m_washParticles.size(); ++i) {
            m_washParticles[i].x -= m_washParticles[i].speed;
            m_washParticles[i].life--;
        }
        m_washParticles.erase(std::remove_if(m_washParticles.begin(), m_washParticles.end(),
                                             [](const WashParticle &p) { return p.life <= 0; }), m_washParticles.end());

        m_washProgress += 0.035f;
        if (m_washProgress >= 1.0f) {
            m_washProgress = 1.0f;
            m_washPhase = 1; // 冲刷完成，进入Logo阶段
            m_frame = 0;
            m_washParticles.clear();
        }
        update();
        return; // 冲刷阶段不执行后续Logo逻辑
    }
    // 原有的 m_phase 逻辑放在 else if(m_phase == ...) 中保持不变

    // 阶段0：粒子汇聚 (0~65帧) → 简化为短暂停留
    if (m_phase == 0 && m_frame > 40) { m_phase = 1; m_frame = 0; }
    // 阶段1：Logo停留 (65~95帧) → 缩短
    if (m_phase == 1 && m_frame > 20) { m_phase = 2; m_frame = 0; }
    // 阶段2：Logo闪烁 (95~143帧) → 短暂闪烁
    if (m_phase == 2 && m_frame > 30) { m_phase = 3; m_frame = 0; }

    // 进度条加载
    if (m_phase >= 3 && !m_loadFinished) {
        m_loadProgress += 0.015f;
        if (m_loadProgress >= 1.0f) {
            m_loadProgress = 1.0f;
            m_loadFinished = true;
            // 延迟后淡出
            QTimer::singleShot(300, this, [this]() {
                m_fadeOut = true;
                m_fadeOutAlpha = 255;
                m_fadeOutTimer = new QTimer(this);
                connect(m_fadeOutTimer, &QTimer::timeout, this, &SplashScreen::onFadeOut);
                m_fadeOutTimer->start(16);
            });
        }
    }

    update();
}

void SplashScreen::onFadeOut()
{
    m_fadeOutAlpha -= 8;
    if (m_fadeOutAlpha <= 0) {
        m_fadeOutAlpha = 0;
        m_fadeOutTimer->stop();
        emit finished();
        close();
        return;
    }
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    QRect r = rect();

    if (m_fadeOut) painter.setOpacity(m_fadeOutAlpha / 255.0);

    // 纯黑背景
    painter.fillRect(r, QColor(10, 10, 15));

    // 冲刷阶段的粒子（从左向右）
    if (m_washPhase == 0) {
        painter.setPen(Qt::NoPen);
        for (const auto &p : m_washParticles) {
            painter.setBrush(QColor(0, 160, 255, (int)(p.alpha * 0.8)));
            painter.drawRect(p.x, p.y, p.size, p.size);
        }
    }

    // Logo (phase >= 1)
    if (m_phase >= 1) {
        QFont logoFont("Courier New", 52, QFont::Bold);
        painter.setFont(logoFont);
        QString text = "> CYBER_SCHEDULE";

        // 霓虹光晕
        for (int i = 4; i >= 1; --i) {
            painter.setPen(QPen(QColor(0, 255, 200, 40 - i * 10), 2 * i));
            painter.drawText(r, Qt::AlignCenter, text);
        }
        // 黑色描边
        painter.setPen(QPen(QColor(0, 0, 0), 3));
        painter.drawText(r.adjusted(2, 2, 2, 2), Qt::AlignCenter, text);
        painter.drawText(r.adjusted(-2, -2, -2, -2), Qt::AlignCenter, text);

        // 前景色，阶段2闪烁洋红
        if (m_phase == 2 && (m_frame / 10) % 2 == 0)
            painter.setPen(QColor(255, 0, 150));
        else
            painter.setPen(QColor(0, 255, 200));
        painter.drawText(r, Qt::AlignCenter, text);

        // 下方基线
        int baseY = r.center().y() + 60;
        int baseL = qMax(0, r.center().x() - 200);
        int baseR = qMin(r.width(), r.center().x() + 200);
        painter.setPen(QPen(QColor(0, 255, 200), 2));
        painter.drawLine(baseL, qBound(0, baseY, r.height() - 1), baseR, qBound(0, baseY, r.height() - 1));
    }

    // 进度条 (phase >= 3)
    if (m_phase >= 3) {
        int barW = 300, barH = 12;
        int barX = r.center().x() - barW / 2;
        int barY = r.center().y() + 100;

        // 蓝色阴影
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 80, 255, 40));
        painter.drawRect(barX + 7, barY + 7, barW, barH);

        // 边框
        painter.setPen(QPen(QColor(0, 255, 200), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(barX, barY, barW, barH);

        // 像素块填充
        int fillW = (int)(barW * m_loadProgress);
        int blockSize = 7, blockSpacing = 9;
        for (int bx = barX + 2; bx < barX + fillW - blockSize; bx += blockSpacing) {
            painter.fillRect(bx, barY + 2, blockSize, barH - 4, QColor(0, 255, 200, 220));
        }

        // 百分比数字（带阴影）
        int percent = (int)(m_loadProgress * 100);
        QFont loadFont("Courier New", 10, QFont::Bold);
        painter.setFont(loadFont);
        QRect percentRect(barX, barY + barH + 5, barW, 20);
        painter.setPen(QColor(0, 60, 80, 150));
        painter.drawText(percentRect.translated(1, 1), Qt::AlignCenter, QString("%1%").arg(percent));
        painter.setPen(QColor(0, 255, 200));
        painter.drawText(percentRect, Qt::AlignCenter, QString("%1%").arg(percent));
    }
}