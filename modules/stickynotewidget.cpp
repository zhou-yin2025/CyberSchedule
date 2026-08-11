#include "StickyNoteWidget.h"
#include "DatabaseManager.h"
#include <QPainter>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QTime>
#include <QHBoxLayout>

StickyNoteWidget::StickyNoteWidget(DatabaseManager *db, QWidget *parent)
    : BaseFloatingWidget(parent), m_db(db)
{
    resize(250, 200);
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sr = screen->geometry();
        move(sr.width() - width() - 10, 100);
    }

    // 编辑区域
    m_edit = new QTextEdit(this);
    m_edit->setGeometry(5, 5, width() - 10, height() - 35);
    m_edit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_edit->setStyleSheet(R"(
        QTextEdit {
            background-color: rgba(10,10,15,220);
            color: #00FFC8;
            border: 1px solid #FF007F;
            font-family: "Courier New";
            font-size: 11px;
            padding: 4px;
            selection-background-color: #FF007F;
        }
        QScrollBar:vertical {
            background: #0A0A0F;
            width: 10px;
        }
        QScrollBar::handle:vertical {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #00FFC8, stop:0.25 #0A0A0F,
                stop:0.5 #00FFC8, stop:0.75 #0A0A0F,
                stop:1 #00FFC8);
            min-height: 20px;
        }
    )");

    // 状态标签 + 钉子按钮
    QWidget *bottomWidget = new QWidget(this);
    bottomWidget->setGeometry(5, height() - 25, width() - 10, 20);
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(4);

    // 钉子按钮
    m_pinBtn = new QPushButton(this);
    m_pinBtn->setFixedSize(16, 16);
    m_pinBtn->setCheckable(true);
    m_pinBtn->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            border: 1px solid #FF007F;
            color: #FF007F;
            font-family: "Courier New";
            font-size: 10px;
        }
        QPushButton:checked {
            background-color: #FF007F;
            color: #0A0A0F;
        }
    )");
    m_pinBtn->setText("📍");
    connect(m_pinBtn, &QPushButton::clicked, this, &StickyNoteWidget::togglePin);
    bottomLayout->addWidget(m_pinBtn);

    // 状态标签
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusLabel->setStyleSheet("color: #00FFC8; font-family: 'Courier New'; font-size: 9px; background: transparent;");
    bottomLayout->addWidget(m_statusLabel, 1);

    // 加载内容
    loadContent();
    showStatus("就绪", QColor(0, 255, 200, 120));

    // 自动保存定时器
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    connect(m_saveTimer, &QTimer::timeout, this, &StickyNoteWidget::doSave);

    connect(m_edit, &QTextEdit::textChanged, this, &StickyNoteWidget::onTextChanged);
}

StickyNoteWidget::~StickyNoteWidget()
{
    if (m_saveTimer->isActive()) {
        m_saveTimer->stop();
        doSave();
    }
}

void StickyNoteWidget::setDbKey(const QString &key)
{
    m_dbKey = key;
    loadContent();
}

void StickyNoteWidget::loadContent()
{
    if (!m_db) return;
    QString content;
    if (m_dbKey == QStringLiteral("sticky_note"))
        content = m_db->loadStickyNote();
    else
        content = m_db->getUserState(m_dbKey, "");
    m_edit->setPlainText(content);
}

void StickyNoteWidget::onTextChanged()
{
    showStatus("● 修改中...", QColor(255, 200, 50));
    m_saveTimer->start(1000);
}

void StickyNoteWidget::doSave()
{
    if (!m_db) return;
    bool ok = false;
    if (m_dbKey == QStringLiteral("sticky_note"))
        ok = m_db->saveStickyNote(m_edit->toPlainText());
    else
        ok = m_db->setUserState(m_dbKey, m_edit->toPlainText());

    if (ok)
        showStatus(QStringLiteral("✓ 已保存 %1").arg(QTime::currentTime().toString("hh:mm:ss")), QColor(0, 255, 200));
    else
        showStatus("✗ 保存失败", QColor(255, 50, 50));
}

void StickyNoteWidget::showStatus(const QString &msg, const QColor &color)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-family: 'Courier New'; font-size: 9px; background: transparent;").arg(color.name()));
}

void StickyNoteWidget::togglePin()
{
    m_pinned = m_pinBtn->isChecked();
    // 如果正在自动隐藏状态且钉子被激活，立即恢复
    if (m_pinned && m_autoHidden) {
        autoRestore();
    }
}

void StickyNoteWidget::drawNormalContent(QPainter &painter)
{
    painter.fillRect(rect(), QColor(20, 10, 30, 240));
    painter.setPen(QPen(QColor(0, 255, 200), 2));
    painter.drawRect(1, 1, width() - 2, height() - 2);
}

// ---------- 自动隐藏核心逻辑 ----------
void StickyNoteWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // 先调用基类确保拖动正常
    BaseFloatingWidget::mouseReleaseEvent(event);
    // 检测是否应该自动隐藏
    checkEdgeAndHide();
}

void StickyNoteWidget::moveEvent(QMoveEvent *event)
{
    BaseFloatingWidget::moveEvent(event);
    // 非拖动状态下（隐藏边缘露出时）鼠标进入会自动恢复，这里不需要额外操作
}

void StickyNoteWidget::checkEdgeAndHide()
{
    if (m_pinned || m_autoHidden) return;

    QRect screenGeo = QGuiApplication::primaryScreen()->availableGeometry();
    QRect myGeo = frameGeometry();
    const int threshold = 30; // 距离边缘30px以内触发

    bool nearLeft   = myGeo.left() <= screenGeo.left() + threshold;
    bool nearRight  = myGeo.right() >= screenGeo.right() - threshold;
    bool nearTop    = myGeo.top() <= screenGeo.top() + threshold;
    bool nearBottom = myGeo.bottom() >= screenGeo.bottom() - threshold;

    if (!nearLeft && !nearRight && !nearTop && !nearBottom)
        return;

    // 保存当前几何
    m_restoreGeometry = myGeo;
    m_autoHidden = true;

    // 计算隐藏目标位置（留出10px边缘）
    QRect hiddenGeo = myGeo;
    if (nearLeft && nearTop) {
        // 角落：优先向左上隐藏
        hiddenGeo.moveLeft(screenGeo.left() - width() + 10);
        hiddenGeo.moveTop(screenGeo.top() - height() + 10);
    } else if (nearLeft && nearBottom) {
        hiddenGeo.moveLeft(screenGeo.left() - width() + 10);
        hiddenGeo.moveBottom(screenGeo.bottom() + height() - 10);
    } else if (nearRight && nearTop) {
        hiddenGeo.moveRight(screenGeo.right() + width() - 10);
        hiddenGeo.moveTop(screenGeo.top() - height() + 10);
    } else if (nearRight && nearBottom) {
        hiddenGeo.moveRight(screenGeo.right() + width() - 10);
        hiddenGeo.moveBottom(screenGeo.bottom() + height() - 10);
    } else if (nearLeft) {
        hiddenGeo.moveLeft(screenGeo.left() - width() + 10);
    } else if (nearRight) {
        hiddenGeo.moveRight(screenGeo.right() + width() - 10);
    } else if (nearTop) {
        hiddenGeo.moveTop(screenGeo.top() - height() + 10);
    } else if (nearBottom) {
        hiddenGeo.moveBottom(screenGeo.bottom() + height() - 10);
    }

    setGeometry(hiddenGeo);
}

void StickyNoteWidget::enterEvent(QEnterEvent *event)
{
    BaseFloatingWidget::enterEvent(event);
    if (m_autoHidden && !m_pinned) {
        autoRestore();
    }
}

void StickyNoteWidget::leaveEvent(QEvent *event)
{
    BaseFloatingWidget::leaveEvent(event);
    // 鼠标离开窗口后，等待一段时间再次检测是否需要隐藏
    QTimer::singleShot(500, this, [this]() {
        if (!m_pinned && m_autoHidden && !underMouse()) {
            // 已经隐藏又离开，不做操作
        }
    });
}

void StickyNoteWidget::autoRestore()
{
    if (!m_autoHidden) return;
    setGeometry(m_restoreGeometry);
    m_autoHidden = false;
}