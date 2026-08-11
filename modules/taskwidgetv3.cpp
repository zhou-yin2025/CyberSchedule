#include "TaskWidgetV3.h"
#include "DatabaseManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QDateTime>
#include <QMenu>
#include <QAction>
#include <algorithm>
#include <QtMath>
#include <QTimer>

TaskWidgetV3::TaskWidgetV3(DatabaseManager *db, QWidget *parent)
    : BaseFloatingWidget(parent), m_db(db)
{
    resize(320, 200);
    move(0, 300);

    loadTasks();
    loadCategories();

    m_editAnimation = new QPropertyAnimation(this, "editHeight");
    m_editAnimation->setDuration(250);
    m_editAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    m_editLine = new QLineEdit(this);
    m_editLine->setStyleSheet(R"(
        QLineEdit {
            background-color: rgba(10,10,15,220);
            color: #00FFC8;
            border: 1px solid #FF007F;
            font-family: "Courier New";
            font-size: 11px;
            padding: 2px;
        }
    )");
    m_editLine->hide();
    connect(m_editLine, &QLineEdit::returnPressed, this, &TaskWidgetV3::confirmEdit);

    m_editDate = new QDateEdit(this);
    m_editDate->setCalendarPopup(true);
    m_editDate->setDisplayFormat("yyyy-MM-dd");
    m_editDate->setStyleSheet(R"(
        QDateEdit {
            background-color: rgba(10,10,15,220);
            color: #00FFC8;
            border: 1px solid #FF007F;
            font-family: "Courier New";
            font-size: 11px;
            padding: 2px;
        }
        QCalendarWidget {
            background-color: #0F0F1A;
            color: #00FFC8;
        }
    )");
    m_editDate->hide();

    // 灯线旋转定时器
    m_lightTimer = new QTimer(this);
    connect(m_lightTimer, &QTimer::timeout, this, [this]() {
        m_lightPhase += 0.005f * m_hoverSpeed;
        if (m_lightPhase >= 1.0f) m_lightPhase -= 1.0f;
        update();
    });
    m_lightTimer->start(30);

    // 悬停减速恢复定时器
    m_speedRecoverTimer = new QTimer(this);
    m_speedRecoverTimer->setSingleShot(true);
    connect(m_speedRecoverTimer, &QTimer::timeout, this, [this]() {
        m_hoverSpeed = 1.0f;
    });

    // 窗口切换动画
    m_resizeAnim = new QPropertyAnimation(this, "geometry");
    m_resizeAnim->setDuration(300);  // 300ms 过渡
    m_resizeAnim->setEasingCurve(QEasingCurve::InOutQuad);

    m_removeTimer = new QTimer(this);
    connect(m_removeTimer, &QTimer::timeout, this, &TaskWidgetV3::updateRemovingTasks);
}

TaskWidgetV3::~TaskWidgetV3() {}

void TaskWidgetV3::loadTasks()
{
    if (!m_db) return;
    m_tasks.clear();
    QVector<TaskData> dbTasks = m_db->getAllTasks("deadline");
    for (const auto &td : dbTasks) {
        TaskItemV3 item;
        item.id = td.id;
        item.title = td.title;
        item.priority = td.priority;
        item.deadline = td.deadline;
        item.completed = td.completed;
        item.categoryId = td.categoryId;
        if (!item.completed)
            m_tasks.append(item);
    }
    // 默认按截止时间排序
    std::sort(m_tasks.begin(), m_tasks.end(), [](const TaskItemV3 &a, const TaskItemV3 &b) {
        return a.deadline < b.deadline;
    });
}

void TaskWidgetV3::loadCategories()
{
    if (!m_db) return;
    m_categories = m_db->getAllCategories();
}

qreal TaskWidgetV3::editHeight() const { return m_editHeight; }
void TaskWidgetV3::setEditHeight(qreal h) { m_editHeight = h; update(); }

void TaskWidgetV3::showTasksForDate(const QDate &date)
{
    m_filterDate = date;
    m_mode = Expanded;
    resize(480, 600);
    update();
}

void TaskWidgetV3::startEdit(int taskIndex)
{
    m_oldFlags = windowFlags();
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    show();

    m_editing = true;
    m_editingTaskId = taskIndex;
    if (taskIndex >= 0 && taskIndex < m_tasks.size()) {
        m_editTitle = m_tasks[taskIndex].title;
        m_editPriority = m_tasks[taskIndex].priority;
        m_editDeadline = m_tasks[taskIndex].deadline;
        m_editCategoryId = m_tasks[taskIndex].categoryId;
    } else {
        m_editTitle.clear();
        m_editPriority = 0;
        m_editDeadline = QDateTime::currentDateTime();
        // 新建任务，默认使用当前选中的分类，或者 0
        m_editCategoryId = (m_selectedCategoryId >= 0) ? m_selectedCategoryId : 0;
    }

    m_editAnimation->setStartValue(0);
    m_editAnimation->setEndValue(120);
    m_editAnimation->start();

    int panelY = 60 + m_tasks.size() * 32 + 10;
    QRect titleRect(25, panelY + 10, width() - 50, 24);
    m_editLine->setGeometry(titleRect);
    m_editLine->setText(m_editTitle);
    m_editLine->show();
    m_editLine->setFocus();

    QRect dateRect(30, panelY + 85, 130, 22);
    m_editDate->setGeometry(dateRect);
    m_editDate->setDate(m_editDeadline.date());
    m_editDate->show();

    update();
}

void TaskWidgetV3::animateToSize(const QSize &targetSize)
{
    QRect startRect = geometry();
    QRect endRect(startRect.topLeft(), targetSize);

    m_resizeAnim->stop();
    m_resizeAnim->setStartValue(startRect);
    m_resizeAnim->setEndValue(endRect);
    m_resizeAnim->start();
}

// ==================== 绘制函数 ====================
void TaskWidgetV3::drawNormalContent(QPainter &painter)
{
    // 绘制正在消失的任务动画
    for (const auto &rt : m_removingTasks) {
        // 保存当前画笔状态
        painter.save();

        // 潮水方向：从左向右，已冲刷区域在左侧逐渐增加
        int threshold = (int)(rt.cardRect.width() * rt.progress);

        // 设置裁剪区域为整个卡片矩形，防止粒子溢出
        painter.setClipRect(rt.cardRect);

        // 用深色背景覆盖卡片（模拟冲刷后的空白）
        painter.fillRect(rt.cardRect, QColor(15, 10, 25, 220));

        // 绘制冲刷前沿的像素粒子
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < 20; ++i) {
            // 粒子集中在阈值线附近
            int px = rt.cardRect.left() + threshold + (rand() % 8) - 4;
            // 粒子纵向随机分布在卡片高度内
            int py = rt.cardRect.top() + (rand() % rt.cardRect.height());
            int size = 3 + (rand() % 3);
            // 蓝色调，带透明度
            QColor particleColor(0, 160, 255, 180 + (rand() % 75));
            painter.setBrush(particleColor);
            painter.drawRect(px, py, size, size);
        }

        // 恢复画笔状态
        painter.restore();
    }
    // 然后正常绘制其余任务...

    if (m_mode == Collapsed) {
        if (!m_hidden) {
            painter.fillRect(rect(), QColor(15, 10, 25, 200));
            painter.setPen(QPen(QColor(0, 255, 200, 120), 1));
            painter.drawRect(0, 0, width() - 1, height() - 1);
        }
        drawCollapsedView(painter);
    } else {
        painter.fillRect(rect(), QColor(15, 10, 25, 250));
        painter.setPen(QPen(QColor(0, 255, 200), 2));
        painter.drawRect(1, 1, width() - 2, height() - 2);
        drawExpandedView(painter);
        if (m_editing) drawEditPanel(painter);
    }
}

void TaskWidgetV3::drawCollapsedView(QPainter &painter)
{
    // 隐藏状态：只绘制实心小圆点 + 一条弧形灯线
    if (m_hidden) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0xCC, 0xFF, 0x00));
        painter.drawEllipse(QPointF(18, 22), 6, 6);

        float outerRadius = 10;
        float lightSpan = 0.35;
        int segments = 8;
        float phase = m_lightPhase;
        float startAngle = phase * 2 * M_PI;
        float endAngle = startAngle + lightSpan * 2 * M_PI;
        float step = (endAngle - startAngle) / segments;
        int alpha = 220;
        QColor lightColor(0xCC, 0xFF, 0x00, alpha);
        painter.setPen(QPen(lightColor, 1.5));
        QPointF center(18, 22);

        for (int s = 0; s < segments; ++s) {
            float a1 = startAngle + s * step;
            float a2 = a1 + step;
            QPointF p1(center.x() + outerRadius * cos(a1),
                       center.y() + outerRadius * sin(a1));
            QPointF p2(center.x() + outerRadius * cos(a2),
                       center.y() + outerRadius * sin(a2));
            painter.drawLine(p1, p2);
        }
        return;
    }

    // 正常状态：大圆点 + 三条灯线 + 任务卡片
    painter.setPen(QPen(QColor(0xCC, 0xFF, 0x00), 3));
    painter.setBrush(Qt::NoBrush);
    QPointF center(18, 22);
    float radius = 10;
    painter.drawEllipse(center, radius, radius);

    // 外侧三条弧形灯线
    float outerRadius = 14;
    float lightSpan = 0.25;
    int segments = 10;
    for (int i = 0; i < 3; ++i) {
        float phase = m_lightPhase + i / 3.0f;
        float startAngle = phase * 2 * M_PI;
        float endAngle = startAngle + lightSpan * 2 * M_PI;
        float step = (endAngle - startAngle) / segments;

        int alpha = 200 + (int)(55 * (m_hoverSpeed - 1.0f) / 2.0f);
        if (alpha > 255) alpha = 255;
        QColor lightColor(0xCC, 0xFF, 0x00, alpha);
        painter.setPen(QPen(lightColor, 2));

        for (int s = 0; s < segments; ++s) {
            float a1 = startAngle + s * step;
            float a2 = a1 + step;
            QPointF p1(center.x() + outerRadius * cos(a1),
                       center.y() + outerRadius * sin(a1));
            QPointF p2(center.x() + outerRadius * cos(a2),
                       center.y() + outerRadius * sin(a2));
            painter.drawLine(p1, p2);
        }
    }

    // 任务卡片
    int count = qMin(m_tasks.size(), 3);
    for (int i = 0; i < count; ++i) {
        painter.save();
        qreal scale = 1.0 - i * 0.2;
        int alpha = 255 - i * 60;
        painter.setOpacity(alpha / 255.0);
        QRect baseRect = taskCardRect(i);
        painter.translate(baseRect.left(), baseRect.top());
        painter.scale(scale, scale);
        int w = baseRect.width(), h = baseRect.height();
        painter.setBrush(priorityColor(m_tasks[i].priority));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(4, (h - 12) / 2, 12, 12);
        QFont f("Courier New", 11);
        painter.setFont(f);
        painter.setPen(QColor(0, 255, 200));
        painter.drawText(20, 0, w - 60, h, Qt::AlignVCenter | Qt::AlignLeft, m_tasks[i].title);
        if (i == 0) {
            int tw = painter.fontMetrics().horizontalAdvance(m_tasks[i].title);
            painter.drawLine(20, h / 2 + 7, 20 + tw, h / 2 + 7);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(0, 255, 200), 1));
        painter.drawRect(w - 30, (h - 14) / 2, 14, 14);
        painter.setBrush(QColor(255, 0, 150));
        painter.drawEllipse(QPointF(w - 8, h / 2), 3, 3);
        painter.restore();
    }
}

void TaskWidgetV3::drawExpandedView(QPainter &painter)
{
    // 收缩按钮
    QRect btn = closeButtonRect();
    painter.fillRect(btn, QColor(255, 0, 150, 150));
    painter.setPen(QPen(QColor(255, 0, 150), 2));
    painter.drawRect(btn);
    painter.setFont(QFont("Courier New", 12, QFont::Bold));
    painter.drawText(btn, Qt::AlignCenter, "-");

    // 排序按钮
    QStringList sortLabels = {"时", "级", "号"};
    for (int i = 0; i < 3; ++i) {
        QRect r = sortButtonRect(i);
        painter.setBrush(i == m_sortMode ? QColor(0, 255, 200, 80) : QColor(0, 0, 0, 0));
        painter.setPen(QPen(i == m_sortMode ? QColor(0, 255, 200) : QColor(0, 255, 200, 100), 1));
        painter.drawRoundedRect(r, 2, 2);
        painter.setFont(QFont("Courier New", 8, QFont::Bold));
        painter.setPen(i == m_sortMode ? QColor(0, 255, 200) : QColor(0, 255, 200, 180));
        painter.drawText(r, Qt::AlignCenter, sortLabels[i]);
    }

    // 齿轮按钮
    QRect gearR = gearButtonRect();
    painter.setPen(QPen(QColor(0, 255, 200, 180), 1));
    painter.setBrush(QColor(0, 255, 200, 40));
    painter.drawRoundedRect(gearR, 3, 3);
    int cx = gearR.center().x(), cy = gearR.center().y();
    painter.setPen(QPen(QColor(0, 255, 200), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPoint(cx, cy), 4, 4);
    painter.setBrush(QColor(0, 255, 200));
    painter.setPen(Qt::NoPen);
    painter.drawRect(cx - 6, cy - 2, 3, 4);
    painter.drawRect(cx + 3, cy - 2, 3, 4);
    painter.drawRect(cx - 2, cy - 6, 4, 3);
    painter.drawRect(cx - 2, cy + 3, 4, 3);
    painter.drawRect(cx - 5, cy - 5, 3, 3);
    painter.drawRect(cx + 2, cy - 5, 3, 3);
    painter.drawRect(cx - 5, cy + 2, 3, 3);
    painter.drawRect(cx + 2, cy + 2, 3, 3);

    // 添加按钮
    QRect addR = addButtonRect();
    painter.fillRect(addR, QColor(0, 255, 200, 60));
    painter.setPen(QPen(QColor(0, 255, 200), 1));
    painter.drawRect(addR);
    painter.setFont(QFont("Courier New", 9));
    painter.drawText(addR, Qt::AlignCenter, "+ 新增");

    // 分类标签
    m_categoryRects.clear();
    int catX = 10, catY = 35;
    // ALL 标签
    QRect allRect(catX, catY, 50, 18);
    m_categoryRects.append(allRect);
    bool allSelected = (m_selectedCategoryId == -1);
    painter.setPen(QPen(QColor(allSelected ? 255 : 0, allSelected ? 0 : 255, allSelected ? 150 : 200), 1));
    painter.setBrush(QColor(allSelected ? 60 : 0, 0, 0, allSelected ? 60 : 0));
    painter.drawRoundedRect(allRect, 3, 3);
    painter.setPen(QPen(QColor(allSelected ? 255 : 0, allSelected ? 0 : 255, allSelected ? 150 : 200), 1));
    painter.setFont(QFont("Courier New", 9, QFont::Bold));
    painter.drawText(allRect, Qt::AlignCenter, "ALL");
    catX += 55;

    for (int i = 0; i < m_categories.size(); ++i) {
        const auto &cat = m_categories[i];
        QRect catRect(catX, catY, 70, 18);
        m_categoryRects.append(catRect);
        bool selected = (cat.id == m_selectedCategoryId);
        painter.setPen(QPen(QColor(selected ? 255 : 0, selected ? 0 : 255, selected ? 150 : 200), 1));
        painter.setBrush(QColor(selected ? 60 : 0, 0, 0, selected ? 60 : 0));
        painter.drawRoundedRect(catRect, 3, 3);
        painter.setPen(QPen(QColor(selected ? 255 : 0, selected ? 0 : 255, selected ? 150 : 200), 1));
        painter.drawText(catRect, Qt::AlignCenter, cat.name);
        catX += 75;
    }

    // 任务列表（按日期分组显示）
    QVector<int> filtered;
    for (int i = 0; i < m_tasks.size(); ++i) {
        const auto &task = m_tasks[i];
        bool match = true;
        if (m_filterDate.isValid() && task.deadline.date() != m_filterDate) match = false;
        if (m_selectedCategoryId >= 0 && task.categoryId != m_selectedCategoryId) match = false;
        if (match) filtered.append(i);
    }

    if (filtered.isEmpty() && (m_filterDate.isValid() || m_selectedCategoryId >= 0)) {
        painter.setPen(QColor(0, 255, 200, 150));
        painter.setFont(QFont("Courier New", 11));
        painter.drawText(QRect(15, 60, width() - 30, 30), Qt::AlignCenter, "无匹配任务");
    } else {
        int yOff = 60;
        QDate lastDate;  // 记录上一个任务的日期，用于分组

        for (int idx : filtered) {
            const TaskItemV3 &task = m_tasks[idx];
            QDate taskDate = task.deadline.date();

            // 如果日期变了，先绘制日期标题行
            if (taskDate != lastDate) {
                lastDate = taskDate;

                // 日期标题背景
                QRect dateHeader(15, yOff, width() - 30, 22);
                painter.fillRect(dateHeader, QColor(0, 255, 200, 30));
                painter.setPen(QPen(QColor(0, 255, 200), 1));
                painter.drawLine(15, yOff + 21, width() - 15, yOff + 21);

                // 日期文字
                painter.setPen(QColor(0, 255, 200));
                painter.setFont(QFont("Courier New", 9, QFont::Bold));
                painter.drawText(dateHeader.adjusted(8, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
                                 taskDate.toString("yyyy-MM-dd"));

                yOff += 28;  // 日期标题行高度
            }

            // 任务卡片（略微缩进）
            QRect card(30, yOff, width() - 45, 28);

            // 优先级点
            painter.setPen(Qt::NoPen);
            painter.setBrush(priorityColor(task.priority));
            painter.drawEllipse(card.left() + 4, card.top() + (card.height() - 10) / 2, 10, 10);

            // 任务名
            painter.setPen(QColor(0, 255, 200));
            painter.setFont(QFont("Courier New", 11));
            painter.drawText(card.left() + 20, card.top(), card.width() - 55, card.height(),
                             Qt::AlignVCenter | Qt::AlignLeft, task.title);

            // 时间（右侧）
            painter.setFont(QFont("Courier New", 8));
            painter.setPen(QColor(0, 255, 200, 180));
            QString timeStr = task.deadline.toString("HH:mm");
            QRect timeRect(card.right() - 50, card.top(), 35, card.height());
            painter.drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, timeStr);

            // 完成方框
            int boxSize = 12;
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(0, 255, 200), 1));
            painter.drawRect(card.right() - 22, card.top() + (card.height() - boxSize) / 2, boxSize, boxSize);

            // 扩展小圆点
            painter.setBrush(QColor(255, 0, 150));
            painter.drawEllipse(QPointF(card.right() - 6, card.top() + card.height() / 2), 2.5, 2.5);

            yOff += 32;  // 任务卡片高度
        }
    }
}

void TaskWidgetV3::drawEditPanel(QPainter &painter)
{
    int panelY = 60 + m_tasks.size() * 32 + 10;
    int panelH = qMax(120, (int)m_editHeight);
    QRect panel(15, panelY, width() - 30, panelH);
    painter.fillRect(panel, QColor(10, 5, 20, 245));
    painter.setPen(QPen(QColor(0, 255, 200), 2));
    painter.drawRect(panel);

    QRect titleRect(panel.left() + 10, panel.top() + 10, panel.width() - 20, 24);
    painter.fillRect(titleRect, QColor(0, 0, 0, 180));
    painter.setPen(QPen(QColor(255, 0, 150), 1));
    painter.drawRect(titleRect);

    for (int i = 0; i < 4; ++i) {
        QRect btn = priorityButtonRect(i);
        btn.moveTop(panel.top() + 45);
        painter.setBrush(i == m_editPriority ? priorityColor(i) : QColor(30, 30, 30));
        painter.setPen(QPen(QColor(0, 255, 200), 1));
        painter.drawEllipse(btn);
        painter.setPen(QColor(0, 255, 200));
        painter.setFont(QFont("Courier New", 9));
        painter.drawText(btn, Qt::AlignCenter, QString::number(i));
    }

    QRect dRect = deadlineRect();
    dRect.moveTop(panel.top() + 85);
    painter.fillRect(dRect, QColor(0, 0, 0, 180));
    painter.setPen(QPen(QColor(0, 255, 200), 1));
    painter.drawRect(dRect);

    // 分类选择按钮
    drawCategoryButtons(painter, panel, panelY); // panelY 是当前面板的顶部 Y 坐标

    QRect confirmR = editConfirmRect();
    QRect cancelR = editCancelRect();
    confirmR.moveTop(panel.top() + 85);
    cancelR.moveTop(panel.top() + 85);
    painter.fillRect(confirmR, QColor(0, 200, 0, 100));
    painter.setPen(QPen(QColor(0, 255, 200), 1));
    painter.drawRect(confirmR);
    painter.drawText(confirmR, Qt::AlignCenter, "确认");
    painter.fillRect(cancelR, QColor(200, 0, 0, 100));
    painter.setPen(QPen(QColor(255, 0, 150), 1));
    painter.drawRect(cancelR);
    painter.drawText(cancelR, Qt::AlignCenter, "取消");
}

// ==================== 坐标辅助函数 ====================
QRect TaskWidgetV3::taskCardRect(int index) const
{
    if (m_mode == Collapsed) {
        int y = 15 + index * 40;
        return QRect(15, y, width() - 30, 30);
    }
    return QRect(15, 50, width() - 30, 30);
}

QRect TaskWidgetV3::closeButtonRect() const { return QRect(width() - 25, 5, 20, 20); }
QRect TaskWidgetV3::addButtonRect() const { return QRect(width() - 70, height() - 30, 60, 22); }
QRect TaskWidgetV3::editConfirmRect() const { return QRect(width() - 100, 0, 40, 22); }
QRect TaskWidgetV3::editCancelRect() const { return QRect(width() - 55, 0, 40, 22); }
QRect TaskWidgetV3::priorityButtonRect(int prio) const { return QRect(30 + prio * 35, 0, 28, 28); }
QRect TaskWidgetV3::deadlineRect() const { return QRect(30, 0, 130, 22); }
QRect TaskWidgetV3::gearButtonRect() const { return QRect(width() - 120, height() - 30, 24, 22); }

QRect TaskWidgetV3::sortButtonRect(int index) const
{
    int baseX = width() - 25 - 3 - 3 * 22;
    return QRect(baseX + index * 22, 5, 20, 20);
}

QColor TaskWidgetV3::priorityColor(int p) const
{
    switch (p) {
    case 3: return QColor(255, 50, 50);
    case 2: return QColor(255, 200, 50);
    case 1: return QColor(50, 200, 255);
    default: return QColor(200, 200, 200);
    }
}

void TaskWidgetV3::sortTasks()
{
    switch (m_sortMode) {
    case SortByDeadline:
        std::sort(m_tasks.begin(), m_tasks.end(), [](const TaskItemV3 &a, const TaskItemV3 &b) {
            return a.deadline < b.deadline;
        });
        break;
    case SortByPriority:
        std::sort(m_tasks.begin(), m_tasks.end(), [](const TaskItemV3 &a, const TaskItemV3 &b) {
            return a.priority > b.priority;
        });
        break;
    case SortById:
        std::sort(m_tasks.begin(), m_tasks.end(), [](const TaskItemV3 &a, const TaskItemV3 &b) {
            return a.id < b.id;
        });
        break;
    }
    update();
}

void TaskWidgetV3::drawCategoryButtons(QPainter &painter, const QRect &panel, int &currentY)
{
    if (m_categories.isEmpty()) return;

    int startY = panel.top() + 115; // 放在截止日期行下方
    int btnX = panel.left() + 15;
    int btnH = 18;
    int spacing = 8;

    m_editCategoryRects.clear();

    painter.setFont(QFont("Courier New", 7));

    for (int i = 0; i < m_categories.size(); ++i) {
        const auto &cat = m_categories[i];
        QString text = cat.name.left(3); // 截短名称
        QFontMetrics fm(painter.font());
        int textWidth = fm.horizontalAdvance(text) + 12;
        QRect btnRect(btnX, startY, textWidth, btnH);
        m_editCategoryRects.append(btnRect);

        bool selected = (cat.id == m_editCategoryId);
        QColor bg = selected ? QColor(cat.color) : QColor(30, 30, 30);
        painter.setBrush(bg);
        painter.setPen(QPen(selected ? QColor(255, 255, 255) : QColor(0, 255, 200), 1));
        painter.drawRoundedRect(btnRect, 4, 4);
        painter.setPen(selected ? QColor(0, 0, 0) : QColor(0, 255, 200));
        painter.drawText(btnRect, Qt::AlignCenter, text);

        btnX += textWidth + spacing;
    }

    // 更新当前可绘制区域的底部 Y 坐标（如果需要）
    // 这部分不改变 panel 高度，按钮绘制在已有空间内
}

// ==================== 事件处理 ====================
void TaskWidgetV3::mousePressEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();

    // ==================== 编辑模式 ====================
    if (m_editing) {
        int panelY = 60 + m_tasks.size() * 32 + 10;
        QRect confirmR = editConfirmRect(); confirmR.moveTop(panelY + 85);
        QRect cancelR = editCancelRect(); cancelR.moveTop(panelY + 85);
        if (confirmR.contains(pos)) { confirmEdit(); return; }
        if (cancelR.contains(pos)) { cancelEdit(); return; }
        for (int i = 0; i < 4; ++i) {
            QRect btn = priorityButtonRect(i); btn.moveTop(panelY + 45);
            if (btn.contains(pos)) { m_editPriority = i; update(); return; }
        }
        // 分类按钮点击
        int catY = panelY + 115; // 与 drawCategoryButtons 中的绘制位置一致
        for (int i = 0; i < m_editCategoryRects.size(); ++i) {
            QRect btn = m_editCategoryRects[i];
            btn.moveTop(catY);
            if (btn.contains(pos)) {
                m_editCategoryId = m_categories[i].id;
                update();
                return;
            }
        }
        return;
    }

    // ==================== 收缩模式 ====================
    if (m_mode == Collapsed) {
        QRect circleArea = m_hidden ? QRect(10, 14, 16, 16) : QRect(8, 12, 20, 20);
        if (circleArea.contains(pos)) {
            if (m_hidden) {
                m_hidden = false;
                update();
            } else {
                // 使用缩放动画展开
                m_mode = Expanded;
                QRect startRect = geometry();
                QRect endRect(startRect.topLeft(), QSize(480, 600));
                m_resizeAnim->stop();
                m_resizeAnim->setStartValue(startRect);
                m_resizeAnim->setEndValue(endRect);
                m_resizeAnim->start();
            }
            return;
        }

        if (!m_tasks.isEmpty()) {
            QRect baseRect = taskCardRect(0);
            int boxSize = 14;
            QRect boxRect(baseRect.right() - 30, baseRect.top() + (baseRect.height() - boxSize) / 2, boxSize, boxSize);
            if (boxRect.contains(pos)) {
                if (!m_tasks.isEmpty()) {
                    int taskId = m_tasks.first().id;
                    QRect cardRect = taskCardRect(0);
                    startTaskRemoval(taskId, cardRect);
                    // 延迟真正删除，避免当前事件中访问已删除元素
                    QTimer::singleShot(0, this, [this, taskId]() {
                        if (m_db) m_db->markTaskCompleted(taskId);
                        for (int j = 0; j < m_tasks.size(); ++j) {
                            if (m_tasks[j].id == taskId) {
                                m_tasks.removeAt(j);
                                break;
                            }
                        }
                        emit taskCompleted(50 * (m_tasks.isEmpty() ? 0 : m_tasks.first().priority + 1));
                        emit tasksChanged();
                        update();
                    });
                }
                return;
            }

            int dotCenterX = baseRect.right() - 8;
            int dotCenterY = baseRect.top() + baseRect.height() / 2;
            QRect dotRect(dotCenterX - 6, dotCenterY - 6, 12, 12);
            if (dotRect.contains(pos)) {
                QMenu menu;
                menu.setStyleSheet("QMenu { background-color: #0F0F1A; border:1px solid #00FFC8; color:#00FFC8; font-family:Courier New; font-size:10px; } QMenu::item:selected { background-color:#FF007F; }");
                QAction *timerAct = menu.addAction("定时 (番茄钟)");
                QAction *delayOneAct = menu.addAction("延后一项");
                QAction *delayDayAct = menu.addAction("延后一天");
                QAction *hideAct = menu.addAction("隐藏任务栏");
                QAction *chosen = menu.exec(event->globalPosition().toPoint());
                if (chosen == timerAct) {
                    if (!m_tasks.isEmpty())
                        emit requestPomodoro(m_tasks.first().title);
                } else if (chosen == delayOneAct && m_tasks.size() > 1) {
                    m_tasks.move(0, 1);
                    update();
                } else if (chosen == delayDayAct) {
                    if (!m_tasks.isEmpty()) {
                        m_tasks.first().deadline = m_tasks.first().deadline.addDays(1);
                        if (m_db) {
                            TaskData td; td.id = m_tasks.first().id; td.title = m_tasks.first().title;
                            td.priority = m_tasks.first().priority; td.deadline = m_tasks.first().deadline;
                            m_db->updateTask(td);
                        }
                        update();
                    }
                } else if (chosen == hideAct) {
                    m_hidden = true;
                    update();
                }
                return;
            }
        }
        BaseFloatingWidget::mousePressEvent(event);
        return;
    }

    // ==================== 展开模式 ====================
    // 收缩按钮
    if (closeButtonRect().contains(pos)) {
        m_mode = Collapsed;
        QRect startRect = geometry();
        QRect endRect(startRect.topLeft(), QSize(320, 200));
        m_resizeAnim->stop();
        m_resizeAnim->setStartValue(startRect);
        m_resizeAnim->setEndValue(endRect);
        m_resizeAnim->start();
        return;
    }

    // 排序按钮
    for (int i = 0; i < 3; ++i) {
        if (sortButtonRect(i).contains(pos)) {
            m_sortMode = (SortMode)i;
            sortTasks();
            update();
            return;
        }
    }

    // 分类标签
    for (int i = 0; i < m_categoryRects.size(); ++i) {
        if (m_categoryRects[i].contains(pos)) {
            if (i == 0) {
                m_selectedCategoryId = -1;
                m_filterDate = QDate();
            } else if (i - 1 < m_categories.size()) {
                m_selectedCategoryId = m_categories[i - 1].id;
            }
            update();
            return;
        }
    }

    // 齿轮设置按钮
    if (gearButtonRect().contains(pos)) { emit settingsRequested(); return; }

    // 添加按钮
    if (addButtonRect().contains(pos)) { startEdit(-1); return; }

    // 任务卡片区域（按日期分组绘制逻辑同步）
    QVector<int> filtered;
    for (int i = 0; i < m_tasks.size(); ++i) {
        const auto &task = m_tasks[i];
        bool match = true;
        if (m_filterDate.isValid() && task.deadline.date() != m_filterDate) match = false;
        if (m_selectedCategoryId >= 0 && task.categoryId != m_selectedCategoryId) match = false;
        if (match) filtered.append(i);
    }

    int yOff = 60;
    QDate lastDate;
    for (int idx : filtered) {
        if (idx < 0 || idx >= m_tasks.size()) continue; // 安全保护
        const TaskItemV3 &task = m_tasks[idx];
        QDate taskDate = task.deadline.date();

        if (taskDate != lastDate) {
            lastDate = taskDate;
            yOff += 28;
        }

        QRect card(30, yOff, width() - 45, 28);

        if (card.contains(pos)) {
            // 检测方框 (完成标记)
            int boxSize = 12;
            QRect boxRect(card.right() - 22, card.top() + (card.height() - boxSize) / 2, boxSize, boxSize);
            if (boxRect.contains(pos)) {
                int taskId = task.id;
                QRect cardRect(30, yOff, width() - 45, 28);
                startTaskRemoval(taskId, cardRect);
                QTimer::singleShot(0, this, [this, taskId]() {
                    if (m_db) m_db->markTaskCompleted(taskId);
                    for (int j = 0; j < m_tasks.size(); ++j) {
                        if (m_tasks[j].id == taskId) {
                            m_tasks.removeAt(j);
                            break;
                        }
                    }
                    emit taskCompleted(50 * (m_tasks.isEmpty() ? 0 : m_tasks.first().priority + 1));
                    emit tasksChanged();
                    update();
                });
                return;
            }

            // 检测小圆点 (扩展菜单)
            QPoint dotCenter(card.right() - 6, card.top() + card.height() / 2);
            QRect dotRect(dotCenter.x() - 4, dotCenter.y() - 4, 8, 8);
            if (dotRect.contains(pos)) {
                QMenu menu;
                menu.setStyleSheet("QMenu { background-color: #0F0F1A; border:1px solid #00FFC8; color:#00FFC8; font-family:Courier New; font-size:10px; } QMenu::item:selected { background-color:#FF007F; }");
                QAction *delayOneAct = menu.addAction("延后一项");
                QAction *delayDayAct = menu.addAction("延后一天");
                QAction *deleteAct = menu.addAction("删除任务");
                QAction *chosen = menu.exec(event->globalPosition().toPoint());

                if (chosen == delayOneAct && m_tasks.size() > 1) {
                    // 交换当前任务与后一个任务的位置
                    int curIdx = -1;
                    for (int j = 0; j < m_tasks.size(); ++j) {
                        if (m_tasks[j].id == task.id) { curIdx = j; break; }
                    }
                    if (curIdx >= 0 && curIdx < m_tasks.size() - 1) {
                        m_tasks.move(curIdx, curIdx + 1);
                        update();
                    }
                } else if (chosen == delayDayAct) {
                    int realIdx = -1;
                    for (int j = 0; j < m_tasks.size(); ++j) {
                        if (m_tasks[j].id == task.id) { realIdx = j; break; }
                    }
                    if (realIdx >= 0) {
                        m_tasks[realIdx].deadline = m_tasks[realIdx].deadline.addDays(1);
                        if (m_db) {
                            TaskData td;
                            td.id = m_tasks[realIdx].id;
                            td.title = m_tasks[realIdx].title;
                            td.priority = m_tasks[realIdx].priority;
                            td.deadline = m_tasks[realIdx].deadline;
                            m_db->updateTask(td);
                        }
                        emit tasksChanged();
                        update();
                    }
                } else if (chosen == deleteAct) {
                    int realIdx = -1;
                    for (int j = 0; j < m_tasks.size(); ++j) {
                        if (m_tasks[j].id == task.id) { realIdx = j; break; }
                    }
                    if (realIdx >= 0 && m_db) {
                        m_db->deleteTask(m_tasks[realIdx].id);
                        m_tasks.removeAt(realIdx);
                        emit tasksChanged();
                        update();
                    }
                }
                return;
            }

            // 点击卡片其他区域 → 编辑
            startEdit(idx);
            return;
        }

        yOff += 32;
    }

    // 没有命中任何交互元素，交给基类拖动处理
    BaseFloatingWidget::mousePressEvent(event);
}

void TaskWidgetV3::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_mode == Expanded && !m_editing) startEdit(-1);
}

void TaskWidgetV3::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mode == Collapsed) {
        QPoint pos = event->pos();
        QRectF circleArea(5, 9, 26, 26);
        if (circleArea.contains(pos)) {
            if (!m_hovering) {
                m_hovering = true;
                m_hoverSpeed = 3.0f;
                m_speedRecoverTimer->stop();
            }
        } else {
            if (m_hovering) {
                m_hovering = false;
                m_speedRecoverTimer->start(600);
            }
        }
    }
    BaseFloatingWidget::mouseMoveEvent(event);
}

void TaskWidgetV3::keyPressEvent(QKeyEvent *event)
{
    if (m_editing) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) { confirmEdit(); return; }
        if (event->key() == Qt::Key_Escape) { cancelEdit(); return; }
        QWidget::keyPressEvent(event);
        return;
    }
    BaseFloatingWidget::keyPressEvent(event);
}

void TaskWidgetV3::enterEvent(QEnterEvent *event) { BaseFloatingWidget::enterEvent(event); }
void TaskWidgetV3::leaveEvent(QEvent *event) { BaseFloatingWidget::leaveEvent(event); }

void TaskWidgetV3::confirmEdit()
{
    QString title = m_editLine->text().trimmed();
    if (title.isEmpty()) return;

    m_editDeadline = QDateTime(m_editDate->date(), QTime(23, 59, 59));

    TaskData td;
    td.title = title;
    td.priority = m_editPriority;
    td.deadline = m_editDeadline;
    td.categoryId = m_editCategoryId;

    if (m_editingTaskId >= 0 && m_editingTaskId < m_tasks.size()) {
        td.id = m_tasks[m_editingTaskId].id;
        if (m_db) m_db->updateTask(td);
    } else {
        if (m_db) m_db->addTask(td);
    }

    emit tasksChanged();
    cancelEdit();
    loadTasks();
    update();
}

void TaskWidgetV3::cancelEdit()
{
    m_editing = false;
    m_editLine->hide();
    m_editDate->hide();
    setWindowFlags(m_oldFlags);
    show();
    m_editAnimation->setStartValue(120);
    m_editAnimation->setEndValue(0);
    m_editAnimation->start();
    update();
}

void TaskWidgetV3::startTaskRemoval(int taskId, const QRect &cardRect)
{
    RemovingTask rt;
    rt.taskId = taskId;
    rt.cardRect = cardRect;
    rt.progress = 0.0f;
    m_removingTasks.append(rt);

    if (!m_removeTimer->isActive())
        m_removeTimer->start(16); // 60fps
}

void TaskWidgetV3::updateRemovingTasks()
{
    bool allDone = true;
    for (int i = 0; i < m_removingTasks.size(); ++i) {
        auto &rt = m_removingTasks[i];
        rt.progress += 0.04f; // 约0.6秒完成
        if (rt.progress >= 1.0f) {
            // 动画结束，真正删除任务
            rt.progress = 1.0f;
            for (int j = 0; j < m_tasks.size(); ++j) {
                if (m_tasks[j].id == rt.taskId) {
                    if (m_db) m_db->markTaskCompleted(rt.taskId);
                    m_tasks.removeAt(j);
                    emit taskCompleted(50 * (m_tasks[j].priority + 1));
                    emit tasksChanged();
                    break;
                }
            }
            m_removingTasks.removeAt(i);
            i--; // 因为删除了当前元素，调整索引
        } else {
            allDone = false;
        }
    }
    if (allDone)
        m_removeTimer->stop();
    update(); // 触发重绘
}