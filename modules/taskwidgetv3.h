#ifndef TASKWIDGETV3_H
#define TASKWIDGETV3_H

#include "BaseFloatingWidget.h"
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QPropertyAnimation>
#include <QLineEdit>
#include <QDate>
#include <QDateEdit>
#include <QTimer>

struct CategoryData;

class DatabaseManager;  // 前置声明

struct TaskItemV3 {
    int id = -1;
    QString title;
    int priority = 0;
    QDateTime deadline;
    bool completed = false;
    int categoryId = 0;       // 新增分类ID
};


class TaskWidgetV3 : public BaseFloatingWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal editHeight READ editHeight WRITE setEditHeight)

public:
    explicit TaskWidgetV3(DatabaseManager *db = nullptr, QWidget *parent = nullptr);
    ~TaskWidgetV3();

public slots:
    void showTasksForDate(const QDate &date);

signals:
    void taskCompleted(int expGained);
    void settingsRequested();
    void requestPomodoro(const QString &taskTitle);
    void tasksChanged();  // 新增：任务数据变更通知

protected:
    void drawNormalContent(QPainter &painter) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void confirmEdit();
    void cancelEdit();

private:
    void loadTasks();                    // 从数据库加载任务
    void startEdit(int taskIndex = -1);
    void drawCollapsedView(QPainter &painter);
    void drawExpandedView(QPainter &painter);
    void drawEditPanel(QPainter &painter);
    QColor priorityColor(int p) const;

    QRect taskCardRect(int index) const;
    QRect closeButtonRect() const;
    QRect addButtonRect() const;
    QRect gearButtonRect() const;
    QRect editConfirmRect() const;
    QRect editCancelRect() const;
    QRect priorityButtonRect(int prio) const;
    QRect deadlineRect() const;

    qreal editHeight() const;
    void setEditHeight(qreal h);

    enum ViewMode { Collapsed, Expanded };
    ViewMode m_mode = Collapsed;
    bool m_hidden = false;
    QVector<TaskItemV3> m_tasks;
    QDate m_filterDate;

    DatabaseManager *m_db;

    // 编辑相关
    bool m_editing = false;
    int m_editingTaskId = -1;
    QString m_editTitle;
    int m_editPriority = 0;
    QDateTime m_editDeadline;
    qreal m_editHeight = 0;
    int m_editCategoryId = 0;                // 当前编辑中的分类 ID
    QVector<QRect> m_editCategoryRects;      // 分类按钮的点击区域（动态计算）
    QRect categoryButtonRect(int index) const; // 获取第 index 个分类按钮的矩形（需在绘制时计算）

    QLineEdit *m_editLine;
    Qt::WindowFlags m_oldFlags;
    QPropertyAnimation *m_editAnimation;

    QDateEdit *m_editDate;

    QTimer *m_lightTimer;       // 灯线旋转定时器
    float m_lightPhase = 0.0f;  // 灯线相位
    bool m_hovering = false;    // 鼠标是否悬停在圆点上
    float m_hoverSpeed = 1.0f;  // 当前速度倍率
    QTimer *m_speedRecoverTimer;       // 减速恢复定时器
    QPropertyAnimation *m_speedAnim; // 速度动画

    enum SortMode { SortByDeadline, SortByPriority, SortById };
    SortMode m_sortMode = SortByDeadline;
    void sortTasks();

    QRect sortButtonRect(int index) const; // 0=时间, 1=优先级, 2=编号

    // 分类筛选
    QVector<CategoryData> m_categories;
    int m_selectedCategoryId = -1;   // -1 表示全部
    QVector<QRect> m_categoryRects;  // 标签点击区域
    void loadCategories();

    void drawCategoryButtons(QPainter &painter, const QRect &panel, int &currentY);

    // 窗口切换动画
    QPropertyAnimation *m_resizeAnim;
    QRect m_targetGeometry;          // 动画目标矩形
    void animateToSize(const QSize &size);

    // 任务完成消失动画
    struct RemovingTask {
        int taskId;
        QRect cardRect;           // 任务卡片的矩形区域
        float progress = 0.0f;    // 0~1，冲刷进度
    };
    QVector<RemovingTask> m_removingTasks;
    QTimer *m_removeTimer;        // 动画定时器
    void startTaskRemoval(int taskId, const QRect &cardRect);
    void updateRemovingTasks();
};

#endif