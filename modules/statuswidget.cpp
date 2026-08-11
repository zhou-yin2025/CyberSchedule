#include "StatusWidget.h"
#include "DatabaseManager.h"
#include "SettingsWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QtMath>
#include <cstdlib>

static const QString AVATAR_DIR = "data/avatars";
static const QString DEFAULT_AVATAR_KEY = "default";

StatusWidget::StatusWidget(DatabaseManager *db, QWidget *parent)
    : BaseFloatingWidget(parent), m_db(db)
{
    resize(480, 270);
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        move(0, 0);
    }

    m_playerName = "V4L3R1E";
    loadFromDatabase();

    // 灯线旋转定时器
    m_lightTimer = new QTimer(this);
    connect(m_lightTimer, &QTimer::timeout, this, [this]() {
        m_lightPhase += m_lightSpeed * m_lightDirection;
        if (m_lightPhase >= 1.0f) m_lightPhase -= 1.0f;
        if (m_lightPhase < 0.0f) m_lightPhase += 1.0f;
        update();
    });

    // 口渴值定时器
    m_hydrationTimer = new QTimer(this);
    connect(m_hydrationTimer, &QTimer::timeout, this, &StatusWidget::onHydrationTimer);

    // 负载定时器
    m_loadTimer = new QTimer(this);
    connect(m_loadTimer, &QTimer::timeout, this, &StatusWidget::onLoadTimer);

    // 口渴提示动画定时器
    m_alertTimer = new QTimer(this);
    connect(m_alertTimer, &QTimer::timeout, this, &StatusWidget::onAlertTimer);

    // 升级弹跳动画定时器
    m_animTimer = new QTimer(this);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        if (m_levelUpAnimTimer > 0) {
            m_levelUpAnimTimer--;
            update();
        } else {
            m_levelUpAnim = false;
            m_animTimer->stop();
        }
    });

    // 进度条平滑动画
    m_expAnim = new QPropertyAnimation(this);
    m_expAnim->setDuration(400);
    m_expAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_hydAnim = new QPropertyAnimation(this);
    m_hydAnim->setDuration(400);
    m_hydAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_loadAnim = new QPropertyAnimation(this);
    m_loadAnim->setDuration(400);
    m_loadAnim->setEasingCurve(QEasingCurve::OutCubic);

    // 启动正常运转
    m_startupFinished = false;
    m_lightTimer->start(30);
    m_hydrationTimer->start(60000);
    m_loadTimer->start(m_loadIntervalSec * 1000);
    update();
}

StatusWidget::~StatusWidget() {}

void StatusWidget::loadFromDatabase()
{
    if (!m_db) return;
    m_playerName = m_db->getUserState("player_name", "V4L3R1E");
    m_totalExp = m_db->getUserState("exp_total", "0").toInt();
    m_hydration = m_db->getUserState("hydration", "100").toInt();
    m_avatarPath = m_db->getUserState("avatar_path", "");
    // 加载头像（内部目录优先，兼容旧数据迁移）
    if (!m_avatarPath.isEmpty()) {
        // 检查是否已经是内部路径
        QFileInfo fileInfo(m_avatarPath);
        QDir avatarDir(AVATAR_DIR);
        if (!fileInfo.exists()) {
            // 源文件已丢失，清空路径
            m_avatarPath.clear();
            if (m_db) m_db->setUserState("avatar_path", "");
        } else if (!fileInfo.absolutePath().startsWith(avatarDir.absolutePath())) {
            // 旧数据：外部路径，尝试迁移到内部目录
            QDir().mkpath(AVATAR_DIR);
            QString newName = m_playerName.isEmpty() ? DEFAULT_AVATAR_KEY : m_playerName;
            newName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_"); // 去除非法字符
            QString newPath = avatarDir.filePath(newName + ".png");
            if (QFile::copy(m_avatarPath, newPath)) {
                m_avatarPath = newPath;
                if (m_db) m_db->setUserState("avatar_path", newPath);
            } else {
                // 复制失败，使用默认头像
                m_avatarPath.clear();
                if (m_db) m_db->setUserState("avatar_path", "");
            }
        }
        // 加载图片
        if (!m_avatarPath.isEmpty())
            m_avatarPixmap.load(m_avatarPath);
    }

    m_hydrationRate = m_db->getUserState("hydration_rate", "1").toInt();
    m_loadIntervalSec = m_db->getUserState("load_rate_seconds", "8").toInt();
    m_alertThreshold = m_db->getUserState("hydration_alert_threshold", "30").toInt();
    m_lightSpeed = m_db->getUserState("light_speed", "5").toFloat() / 1000.0f;
    m_lightDirection = m_db->getUserState("light_direction", "-1").toInt();

    refreshFromTotalExp();
}

void StatusWidget::reloadSettings()
{
    loadFromDatabase();
    m_hydrationTimer->start(60000);
    m_loadTimer->start(m_loadIntervalSec * 1000);
    update();
}

void StatusWidget::refreshFromTotalExp()
{
    int level = 1;
    qreal total = m_totalExp;
    while (level < 999) {
        qreal threshold = 100.0 * pow(level, 1.4);
        if (total >= threshold) level++;
        else break;
    }
    if (level > 999) level = 999;
    m_expLevel = level;
    qreal prevThreshold = (level > 1) ? (100.0 * pow(level - 1, 1.4)) : 0.0;
    m_expCurrent = qMax(0, (int)(total - prevThreshold));
    if (level < 999) {
        qreal nextThreshold = 100.0 * pow(level, 1.4);
        m_expMax = qMax(1, (int)(nextThreshold - prevThreshold));
    } else {
        m_expCurrent = 100;
        m_expMax = 100;
    }
}

void StatusWidget::addExp(int amount)
{
    int oldLevel = m_expLevel;
    m_expOldValue = m_expCurrent;
    m_totalExp += amount;
    if (m_db) m_db->setUserState("exp_total", QString::number(m_totalExp));
    refreshFromTotalExp();

    m_expAnim->stop();
    m_expAnim->setTargetObject(this);
    m_expAnim->setPropertyName("expAnimValue");
    m_expAnim->setStartValue(0.0f);
    m_expAnim->setEndValue(1.0f);
    m_expAnim->start();

    if (m_expLevel > oldLevel) {
        m_levelUpAnim = true;
        m_levelUpAnimTimer = 30;
        m_animTimer->start(16);
    }

    m_floatingExpText = QString("+%1 EXP").arg(amount);
    m_floatingExpTimer = 60;
    update();
}

void StatusWidget::playStartupAnimation()
{
    // 重置动画状态
    m_startupFinished = false;
    m_startupFrame = 0;
    m_lightFastPhase = 0.0f;
    m_innerAlpha = 0.0f;
    m_avatarScale = 0.8f;
    m_expProgress = 0.0f;
    m_hydProgress = 0.0f;
    m_loadProgress = 0.0f;
    m_typingIndex = 0;

    // 停止正常运行的定时器（动画期间不要干扰）
    m_lightTimer->stop();
    m_hydrationTimer->stop();
    m_loadTimer->stop();

    // 启动动画定时器
    if (!m_startupTimer) {
        m_startupTimer = new QTimer(this);
        connect(m_startupTimer, &QTimer::timeout, this, &StatusWidget::updateStartupAnimation);
    }
    m_startupTimer->start(16);
}

void StatusWidget::onHydrationTimer()
{
    m_hydOldValue = m_hydration;
    m_hydration = qMax(0, m_hydration - m_hydrationRate);
    m_hydAnim->stop();
    m_hydAnim->setTargetObject(this);
    m_hydAnim->setPropertyName("hydAnimValue");
    m_hydAnim->setStartValue(0.0f);
    m_hydAnim->setEndValue(1.0f);
    m_hydAnim->start();
    if (m_db) m_db->setUserState("hydration", QString::number(m_hydration));
    if (m_hydration <= m_alertThreshold && m_hydration % 10 == 0) {
        if (!m_hydrationAlertActive) {
            m_hydrationAlertActive = true;
            m_hydrationAlertTimer = 120;
            m_alertTimer->start(16);
        }
    }
    update();
}

void StatusWidget::onLoadTimer()
{
    m_loadOldValue = m_systemLoad;
    int inc = 1 + (std::rand() % 3);
    if (std::rand() % 10 == 0) inc += (std::rand() % 5);
    m_systemLoad = qMin(100, m_systemLoad + inc);
    m_loadAnim->stop();
    m_loadAnim->setTargetObject(this);
    m_loadAnim->setPropertyName("loadAnimValue");
    m_loadAnim->setStartValue(0.0f);
    m_loadAnim->setEndValue(1.0f);
    m_loadAnim->start();
    if (m_systemLoad >= 100) {
        m_systemLoad = 5 + (std::rand() % 11);
    }
    update();
}

void StatusWidget::onAlertTimer()
{
    m_hydrationAlertTimer--;
    if (m_hydrationAlertTimer <= 0) {
        m_hydrationAlertActive = false;
        m_alertTimer->stop();
    }
    update();
}

void StatusWidget::updateStartupAnimation()
{
    m_startupFrame++;

    // 阶段0：灯线快速旋转一圈 (0~30帧, ~0.5s)
    if (m_startupFrame <= 30) {
        m_lightFastPhase = m_startupFrame / 30.0f;
        update();
        return;
    }

    // 阶段1：内部渐显 + 头像缩放 (30~50帧)
    if (m_startupFrame <= 50) {
        m_lightFastPhase = -1.0f; // 快速灯线消失
        float t = (m_startupFrame - 30) / 20.0f;
        m_innerAlpha = t;
        m_avatarScale = 0.8f + t * 0.2f;
        update();
        return;
    }

    // 阶段2：三条状态条依次扫过 (50~110帧)
    float barT = (m_startupFrame - 50) / 60.0f;
    if (barT < 0.33f) {
        m_expProgress = barT / 0.33f;
    } else if (barT < 0.66f) {
        m_expProgress = 1.0f;
        m_hydProgress = (barT - 0.33f) / 0.33f;
    } else if (barT <= 1.0f) {
        m_expProgress = 1.0f;
        m_hydProgress = 1.0f;
        m_loadProgress = (barT - 0.66f) / 0.34f;
    }

    // 阶段3：打字机文字 (80~140帧)
    if (m_startupFrame >= 80 && m_startupFrame <= 140) {
        m_typingIndex = (m_startupFrame - 80) / 3;
    }

    // 动画结束条件
    if (m_startupFrame >= 140) {
        m_startupTimer->stop();
        m_startupFinished = true;
        m_expProgress = m_hydProgress = m_loadProgress = 1.0f;
        m_typingIndex = 100;
        // 恢复正常运转的定时器
        m_lightTimer->start(30);
        m_hydrationTimer->start(60000);
        m_loadTimer->start(m_loadIntervalSec * 1000);
        emit startupAnimationFinished();
    }
    update();
}

// ==================== 绘制函数 ====================
void StatusWidget::drawNormalContent(QPainter &painter)
{
    int w = width(), h = height();

    drawHexagon(painter);
    drawAvatar(painter);
    drawName(painter);
    drawProgressBars(painter);

    // 浮动经验文字
    if (m_floatingExpTimer > 0) {
        m_floatingExpTimer--;
        float t = m_floatingExpTimer / 60.0f;
        float alpha = t;
        float offsetY = (1.0f - t) * 30.0f;
        QFont expFont("Courier New", 9, QFont::Bold);
        painter.setFont(expFont);
        painter.setPen(QColor(0, 255, 200, (int)(alpha * 255)));
        painter.drawText(QRectF(190, 20 - offsetY, 250, 20), Qt::AlignCenter, m_floatingExpText);
    }

    // 口渴提示
    if (m_hydrationAlertActive) {
        int alpha = 255;
        if (m_hydrationAlertTimer < 30) {
            alpha = m_hydrationAlertTimer * 255 / 30;
        } else if (m_hydrationAlertTimer > 90) {
            alpha = (120 - m_hydrationAlertTimer) * 255 / 30;
        }
        QColor alertColor = (m_hydrationAlertTimer / 10) % 2 == 0
                                ? QColor(255, 50, 50, alpha)
                                : QColor(255, 200, 50, alpha);
        QFont alertFont("Courier New", 10, QFont::Bold);
        painter.setFont(alertFont);
        painter.setPen(alertColor);
        QRectF alertRect(400, 60, 80, 30);
        painter.drawText(alertRect, Qt::AlignCenter, "HYDR\nLOW");
    }
}

void StatusWidget::drawHexagon(QPainter &painter)
{
    int cx = 100, cy = 100, r = 60;
    QPolygonF hex;
    for (int i = 0; i < 6; ++i) {
        double angle = i * M_PI / 3.0 - M_PI / 6.0;
        hex << QPointF(cx + r * cos(angle), cy + r * sin(angle));
    }

    painter.setPen(QPen(QColor(0, 255, 200, 80), 2));
    painter.drawPolygon(hex);

    drawLightLines(painter, hex, -1);

    if (m_startupFinished || m_innerAlpha > 0) {
        float alpha = m_startupFinished ? 1.0f : m_innerAlpha;
        painter.setPen(QPen(QColor(0, 255, 200, (int)(80 * alpha)), 1));
        QPolygonF inner;
        for (int i = 0; i < 6; ++i) {
            double angle = i * M_PI / 3.0 - M_PI / 6.0;
            inner << QPointF(cx + (r - 10) * cos(angle), cy + (r - 10) * sin(angle));
        }
        painter.drawPolygon(inner);
    }
}

void StatusWidget::drawLightLines(QPainter &painter, const QPolygonF &hex, qreal highlightPhase)
{
    qreal sideLength = QLineF(hex[0], hex[1]).length();
    qreal perimeter = 6.0 * sideLength;

    auto pointOnHex = [&](qreal dist) -> QPointF {
        if (dist < 0) dist += perimeter;
        if (dist >= perimeter) dist -= perimeter;
        qreal accum = 0;
        for (int i = 0; i < 6; ++i) {
            QPointF p1 = hex[i], p2 = hex[(i+1)%6];
            qreal edgeLen = QLineF(p1, p2).length();
            if (accum + edgeLen >= dist) {
                qreal t = (dist - accum) / edgeLen;
                return p1 + t * (p2 - p1);
            }
            accum += edgeLen;
        }
        return hex[5];
    };

    auto drawArcSegment = [&](qreal start, qreal end, const QColor &color) {
        if (end <= start) return;
        painter.setPen(QPen(color, 2));
        qreal accum = 0;
        for (int i = 0; i < 6; ++i) {
            QPointF p1 = hex[i], p2 = hex[(i+1)%6];
            qreal edgeLen = QLineF(p1, p2).length();
            qreal edgeStart = accum;
            qreal edgeEnd = accum + edgeLen;
            qreal segStart = qMax(edgeStart, start);
            qreal segEnd = qMin(edgeEnd, end);
            if (segStart < segEnd) {
                qreal t1 = (segStart - edgeStart) / edgeLen;
                qreal t2 = (segEnd - edgeStart) / edgeLen;
                QPointF dp1 = p1 + t1 * (p2 - p1);
                QPointF dp2 = p1 + t2 * (p2 - p1);
                painter.drawLine(dp1, dp2);
            }
            accum += edgeLen;
        }
    };

    if (highlightPhase >= 0) {
        qreal start = highlightPhase * perimeter;
        qreal lightLen = perimeter * 0.15;
        qreal end = start + lightLen;
        start = fmod(start, perimeter);
        end = fmod(end, perimeter);
        if (end > start) {
            drawArcSegment(start, end, QColor(0, 255, 200, 255));
        } else {
            drawArcSegment(start, perimeter, QColor(0, 255, 200, 255));
            drawArcSegment(0, end, QColor(0, 255, 200, 255));
        }
        return;
    }

    if (!m_startupFinished) return;

    qreal lightLen = perimeter * 0.15;
    for (int i = 0; i < 3; ++i) {
        qreal start = fmod(m_lightPhase * perimeter + i * perimeter / 3.0, perimeter);
        qreal end = fmod(start + lightLen, perimeter);
        QColor lineColor = QColor(0, 255, 200, 220);
        if (end > start) {
            drawArcSegment(start, end, lineColor);
        } else {
            drawArcSegment(start, perimeter, lineColor);
            drawArcSegment(0, end, lineColor);
        }
    }
}

void StatusWidget::drawAvatar(QPainter &painter)
{
    int cx = 100, cy = 100, r = 38;
    float scale = m_startupFinished ? 1.0f : m_avatarScale;
    float alpha = m_startupFinished ? 1.0f : m_innerAlpha;
    if (alpha <= 0) return;

    painter.save();
    painter.setOpacity(alpha);
    painter.translate(cx, cy);
    painter.scale(scale, scale);

    if (!m_avatarPixmap.isNull()) {
        QPixmap scaled = m_avatarPixmap.scaled(r*2, r*2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap mask(r*2, r*2);
        mask.fill(Qt::transparent);
        QPainter mp(&mask);
        mp.setBrush(Qt::white);
        mp.drawEllipse(0, 0, r*2, r*2);
        mp.end();
        scaled.setMask(mask.createMaskFromColor(Qt::transparent, Qt::MaskInColor));
        painter.drawPixmap(-r, -r, scaled);
    } else {
        painter.setBrush(QColor(0, 255, 200, 30));
        painter.setPen(QPen(QColor(255, 0, 150), 1));
        painter.drawEllipse(QPointF(0, 0), r, r);
        QFont f("Courier New", 14, QFont::Bold);
        painter.setFont(f);
        painter.setPen(QColor(0, 255, 200));
        painter.drawText(QRect(-r, -r, r*2, r*2), Qt::AlignCenter, "SYS");
    }
    painter.restore();
}

void StatusWidget::drawName(QPainter &painter)
{
    float alpha = m_startupFinished ? 1.0f : m_innerAlpha;
    if (alpha <= 0) return;
    painter.setOpacity(alpha);
    QFont f("Courier New", 10, QFont::Bold);
    painter.setFont(f);
    painter.setPen(QColor(255, 0, 150));
    painter.drawText(nameRect(), Qt::AlignCenter, m_playerName);
    painter.setOpacity(1.0f);
}

void StatusWidget::drawProgressBars(QPainter &painter)
{
    int barX = 190;
    int barWidth = width() - barX - 25;
    int barH = 16;
    drawExpBar(painter, barX, barWidth, barH, 25);
    drawHydrationBar(painter, barX, barWidth, barH, 75);
    drawLoadBar(painter, barX, barWidth, barH, 125);
}

void StatusWidget::drawExpBar(QPainter &painter, int barX, int barWidth, int barH, int y)
{
    QFont f("Courier New", 8);
    painter.setFont(f);
    painter.setPen(QColor(0, 255, 200));

    // 等级文字（带弹跳动画）
    painter.save();
    if (m_levelUpAnim) {
        float t = m_levelUpAnimTimer / 30.0f;
        float scale = 1.0f + t * (1.0f - t) * 1.5f;
        QFont scaledFont = f;
        scaledFont.setPixelSize(qMax(8, (int)(8 * scale)));
        painter.setFont(scaledFont);
    }
    painter.drawText(barX, y - 5, QString("Lv.%1").arg(m_expLevel));
    painter.restore();

    painter.setFont(f);
    painter.drawText(barX + 100, y - 5, QString("%1/%2 EXP").arg(m_expCurrent).arg(m_expMax));

    painter.fillRect(barX, y + 8, barWidth, barH, QColor(10, 10, 20));
    float ratio = m_startupFinished ? (float)m_expCurrent / m_expMax : m_expProgress;
    int targetFillW = (int)(barWidth * (float)m_expCurrent / m_expMax);
    int oldFillW = (int)(barWidth * (float)m_expOldValue / m_expMax);
    int fillW;
    if (m_expAnimValue < 1.0f) {
        fillW = oldFillW + (targetFillW - oldFillW) * m_expAnimValue;
        // 淡色预填充条
        painter.fillRect(barX, y + 8, oldFillW, barH, QColor(0, 255, 200, 80));
    } else {
        fillW = targetFillW;
    }
    painter.fillRect(barX, y + 8, fillW, barH, QColor(0, 255, 200, 200));

    // 流动光效 (略)
}

void StatusWidget::drawHydrationBar(QPainter &painter, int barX, int barWidth, int barH, int y)
{
    QFont f("Courier New", 8);
    painter.setFont(f);
    painter.setPen(QColor(0, 255, 200));
    painter.drawText(barX, y - 5, QString("HYDR %1%").arg(m_hydration));

    int maxW = (int)(barWidth * 0.77);
    int targetFillW = (int)(maxW * (float)m_hydration / m_hydrationMax);
    int oldFillW = (int)(maxW * (float)m_hydOldValue / m_hydrationMax);
    int fillW;
    if (m_hydAnimValue < 1.0f) {
        fillW = oldFillW + (targetFillW - oldFillW) * m_hydAnimValue;
        painter.fillRect(barX, y + 8, oldFillW, barH, QColor(0, 150, 255, 80));
    } else {
        fillW = targetFillW;
    }
    painter.fillRect(barX, y + 8, fillW, barH, QColor(0, 150, 255, 200));

    /*QPolygonF tri;
    tri << QPointF(barX + fillW, y + 8) << QPointF(barX + fillW + barH, y + 8)
        << QPointF(barX + fillW, y + 8 + barH);
    painter.setBrush(QColor(0, 150, 255, 200));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(tri);*/

    painter.setPen(QColor(0, 150, 255));
    painter.drawLine(barX, y + 8, barX + maxW, y + 8);
    painter.drawLine(barX, y + 8 + barH, barX + maxW, y + 8 + barH);

    QRect btn(barX + maxW + 5, y + 8, 14, barH);
    painter.fillRect(btn, QColor(0, 150, 255, 150));
    painter.setPen(QColor(0, 255, 200));
    painter.drawRect(btn);
    painter.drawText(btn, Qt::AlignCenter, "+");
}

void StatusWidget::drawLoadBar(QPainter &painter, int barX, int barWidth, int barH, int y)
{
    QFont f("Courier New", 8);
    painter.setFont(f);
    painter.setPen(QColor(255, 0, 150));
    painter.drawText(barX, y - 5, QString("LOAD %1%").arg(m_systemLoad));

    int maxW = (int)(barWidth * 0.31);
    int targetFillW = (int)(maxW * (float)m_systemLoad / 100.0f);
    int oldFillW = (int)(maxW * (float)m_loadOldValue / 100.0f);
    int fillW;
    QColor loadColor = m_systemLoad > 80 ? QColor(255, 0, 0, 200) : QColor(255, 0, 150, 200);
    if (m_loadAnimValue < 1.0f) {
        fillW = oldFillW + (targetFillW - oldFillW) * m_loadAnimValue;
        QColor oldColor = m_loadOldValue > 80 ? QColor(255, 0, 0, 80) : QColor(255, 0, 150, 80);
        painter.fillRect(barX, y + 8, oldFillW, barH, oldColor);
    } else {
        fillW = targetFillW;
    }
    painter.fillRect(barX, y + 8, fillW, barH, loadColor);

    /*QPolygonF tri;
    tri << QPointF(barX + fillW, y + 8) << QPointF(barX + fillW - barH, y + 8)
        << QPointF(barX + fillW, y + 8 + barH);
    painter.setBrush(loadColor);
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(tri);*/

    painter.setPen(QColor(255, 0, 150));
    painter.drawLine(barX, y + 8, barX + maxW, y + 8);
    painter.drawLine(barX, y + 8 + barH, barX + maxW, y + 8 + barH);

    QRect btn(barX + maxW + 5, y + 8, 14, barH);
    painter.fillRect(btn, QColor(255, 0, 150, 150));
    painter.setPen(QColor(255, 0, 150));
    painter.drawRect(btn);
    painter.drawText(btn, Qt::AlignCenter, "-");
}

// ==================== 交互 ====================
QRect StatusWidget::nameRect() const { return QRect(15, 170, 170, 20); }
QRect StatusWidget::avatarRect() const { return QRect(62, 62, 76, 76); }
QRect StatusWidget::hydrationButtonRect() const {
    int barX = 190; int barWidth = width() - barX - 25; int maxW = (int)(barWidth * 0.77);
    return QRect(barX + maxW + 5, 83, 18, 16);
}
QRect StatusWidget::loadButtonRect() const {
    int barX = 190; int barWidth = width() - barX - 25; int maxW = (int)(barWidth * 0.31);
    return QRect(barX + maxW + 5, 133, 18, 16);
}

void StatusWidget::setHydration(int value, int max)
{
    m_hydration = value;
    m_hydrationMax = max;
    if (m_db) m_db->setUserState("hydration", QString::number(m_hydration));
    update();
}

void StatusWidget::setSystemLoad(int value)
{
    m_systemLoad = qBound(0, value, 100);
    update();
}

void StatusWidget::mousePressEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();

    if (hydrationButtonRect().contains(pos)) {
        m_hydOldValue = m_hydration;
        m_hydration = m_hydrationMax;
        m_hydAnim->stop();
        m_hydAnim->setTargetObject(this);
        m_hydAnim->setPropertyName("hydAnimValue");
        m_hydAnim->setStartValue(0.0f);
        m_hydAnim->setEndValue(1.0f);
        m_hydAnim->start();
        if (m_db) m_db->setUserState("hydration", QString::number(m_hydration));
        m_floatingExpText = "+Hydrated";
        m_floatingExpTimer = 40;
        update();
        return;
    }
    if (loadButtonRect().contains(pos)) {
        m_loadOldValue = m_systemLoad;
        int exp = (int)(m_systemLoad * 0.5);
        m_systemLoad = 20 + (std::rand() % 11);
        m_loadAnim->stop();
        m_loadAnim->setTargetObject(this);
        m_loadAnim->setPropertyName("loadAnimValue");
        m_loadAnim->setStartValue(0.0f);
        m_loadAnim->setEndValue(1.0f);
        m_loadAnim->start();
        addExp(exp);
        update();
        return;
    }
    BaseFloatingWidget::mousePressEvent(event);
}

void StatusWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    if (avatarRect().contains(pos)) {
        QString path = QFileDialog::getOpenFileName(this, "选择头像", "", "Images (*.png *.jpg)");
        if (!path.isEmpty()) {
            // 创建内部目录
            QDir().mkpath(AVATAR_DIR);
            QDir avatarDir(AVATAR_DIR);
            // 生成内部文件名（使用玩家名或默认键）
            QString saveName = m_playerName.isEmpty() ? DEFAULT_AVATAR_KEY : m_playerName;
            saveName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
            QString savePath = avatarDir.filePath(saveName + ".png");
            // 复制到内部目录
            if (QFile::copy(path, savePath)) {
                m_avatarPixmap.load(savePath);
                m_avatarPath = savePath;
                if (m_db) m_db->setUserState("avatar_path", savePath);
            } else {
                // 复制失败，仍然尝试加载原图（但风险自担）
                m_avatarPixmap.load(path);
                m_avatarPath = path;
                if (m_db) m_db->setUserState("avatar_path", path);
            }
            update();
        }
        return;
    }

    if (nameRect().contains(pos)) {
        bool ok;
        QString name = QInputDialog::getText(this, "编辑姓名", "新名字:", QLineEdit::Normal, m_playerName, &ok);
        if (ok && !name.isEmpty()) {
            m_playerName = name;
            if (m_db) m_db->setUserState("player_name", name);
            update();
        }
        return;
    }
    //SettingsWidget *settings = new SettingsWidget(m_db, this);
    //settings->show();
}