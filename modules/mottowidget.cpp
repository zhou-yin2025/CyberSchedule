#include "MottoWidget.h"
#include "DatabaseManager.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QScreen>
#include <QGuiApplication>
#include <QTime>

const QString MottoWidget::DB_KEY = "mantra_text";

MottoWidget::MottoWidget(DatabaseManager *db, QWidget *parent)
    : BaseFloatingWidget(parent), m_db(db)
{
    // 编辑区域 (留出更多内边距以适配四行文字)
    m_edit = new QTextEdit(this);
    m_edit->setGeometry(10, 10, width() - 20, height() - 40);
    m_edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_edit->setLineWrapMode(QTextEdit::WidgetWidth); // 自动换行
    m_edit->setAcceptRichText(false);                // 纯文本
    m_edit->setStyleSheet(R"(
        QTextEdit {
            background-color: rgba(0,0,0,0);
            color: #00BFFF;
            font-family: "Courier New";
            font-size: 16px;
            font-weight: bold;
            border: none;
            padding: 8px;
        }
    )");

    // 状态标签
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusLabel->setGeometry(5, height() , width() - 10, 18);
    m_statusLabel->setStyleSheet(
        "color: #00FFC8; font-family: 'Courier New'; font-size: 8px; background: transparent;");

    // 加载已保存的内容
    loadFromDatabase();

    //showStatus("就绪", QColor(0, 255, 200, 120));

    // 保存定时器（停止输入1秒后保存）
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    connect(m_saveTimer, &QTimer::timeout, this, &MottoWidget::doSave);

    // 内容变化时重置定时器
    connect(m_edit, &QTextEdit::textChanged, this, &MottoWidget::onTextChanged);
}

MottoWidget::~MottoWidget()
{
    if (m_saveTimer->isActive()) {
        m_saveTimer->stop();
        doSave();
    }
}

void MottoWidget::setMotto(const QString &text)
{
    m_edit->setPlainText(text);
    if (m_saveTimer->isActive()) m_saveTimer->stop();
    doSave();
}

QString MottoWidget::motto() const
{
    return m_edit->toPlainText().trimmed();
}

void MottoWidget::loadFromDatabase()
{
    if (m_db) {
        // 默认显示四行座右铭
        QString content = m_db->getUserState(DB_KEY, "STAY\nFOCUSED\nON YOUR\nGOALS");
        m_edit->setPlainText(content);
    }
}

void MottoWidget::onTextChanged()
{
    //showStatus("● 修改中...", QColor(255, 200, 50));
    m_saveTimer->start(1000);
}

void MottoWidget::doSave()
{
    if (m_db) {
        bool ok = m_db->setUserState(DB_KEY, m_edit->toPlainText());
        if (ok) {
            //showStatus(QStringLiteral("✓ 已保存 %1").arg(QTime::currentTime().toString("hh:mm:ss")),
                       //QColor(0, 255, 200));
        } else {
            //showStatus("✗ 保存失败", QColor(255, 50, 50));
        }
    }
}

void MottoWidget::showStatus(const QString &msg, const QColor &color)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-family: 'Courier New'; font-size: 8px; background: transparent;")
            .arg(color.name()));
}

void MottoWidget::drawNormalContent(QPainter &painter)
{
    painter.fillRect(rect(), QColor(20, 10, 30, 240));
    painter.setPen(QPen(QColor(0, 255, 200), 2));
    painter.drawRect(1, 1, width() - 2, height() - 2);
}