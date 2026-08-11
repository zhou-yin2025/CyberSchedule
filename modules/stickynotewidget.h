#ifndef STICKYNOTEWIDGET_H
#define STICKYNOTEWIDGET_H

#include "BaseFloatingWidget.h"
#include <QTextEdit>
#include <QLabel>
#include <QTimer>
#include <QPushButton>

class DatabaseManager;

class StickyNoteWidget : public BaseFloatingWidget
{
    Q_OBJECT
public:
    explicit StickyNoteWidget(DatabaseManager *db, QWidget *parent = nullptr);
    ~StickyNoteWidget();

    // 设置用于保存的独立数据库键
    void setDbKey(const QString &key);

protected:
    void drawNormalContent(QPainter &painter) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private slots:
    void onTextChanged();
    void doSave();
    void togglePin();

private:
    void loadContent();
    void showStatus(const QString &msg, const QColor &color);
    void checkEdgeAndHide();      // 检测是否靠边，符合条件则自动隐藏
    void autoRestore();           // 恢复自动隐藏前的状态

    DatabaseManager *m_db;
    QTextEdit *m_edit;
    QLabel *m_statusLabel;
    QTimer *m_saveTimer;
    QPushButton *m_pinBtn;
    QString m_dbKey = "sticky_note";

    // 自动隐藏相关
    bool m_pinned = false;        // 钉子状态（true时不会自动隐藏）
    bool m_autoHidden = false;    // 是否正处于自动隐藏状态
    QRect m_restoreGeometry;      // 进入隐藏前的位置
    QPoint m_dragStartPos;        // 拖动起始点（继承自基类，这里不重复）
};

#endif