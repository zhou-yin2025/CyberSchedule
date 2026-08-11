#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QPoint>

class SplashScreen : public QWidget
{
    Q_OBJECT
public:
    explicit SplashScreen(QWidget *parent = nullptr);
    ~SplashScreen();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updateAnimation();
    void onFadeOut();

private:
    void initDots();
    void initRisingPixels();

    struct Dot {
        QPoint pos;
        int alpha;
    };

    struct RisingPixel {
        float x;
        float y;
        float startY;
        int phase;
        float speed;
        int size;
    };

    // 冲刷动画相关（与 BaseFloatingWidget 中的定义相同，简化版，无复杂拖尾）
    struct WashParticle {
        float x, y, speed, alpha;
        int size, life, maxLife;
    };
    QVector<WashParticle> m_washParticles;

    int m_washPhase = 0;        // 0:冲刷->1:Logo
    float m_washProgress = 0.0f;
    QTimer m_washTimer;

    QTimer *m_timer;
    QTimer *m_fadeOutTimer = nullptr;
    int m_frame = 0;
    int m_phase = 0;              // 0粒子汇聚, 1Logo停留, 2Logo闪烁, 3进度条
    bool m_loadFinished = false;
    float m_loadProgress = 0.0f;
    bool m_fadeOut = false;
    int m_fadeOutAlpha = 255;

    QVector<Dot> m_dots;
    QVector<RisingPixel> m_risingPixels;
};

#endif