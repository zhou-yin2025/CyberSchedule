#ifndef MOTTOWIDGET_H
#define MOTTOWIDGET_H

#include "BaseFloatingWidget.h"
#include <QTextEdit>
#include <QLabel>
#include <QTimer>

class DatabaseManager;

class MottoWidget : public BaseFloatingWidget
{
    Q_OBJECT

public:
    explicit MottoWidget(DatabaseManager *db, QWidget *parent = nullptr);
    ~MottoWidget();

    void setMotto(const QString &text);
    QString motto() const;

protected:
    void drawNormalContent(QPainter &painter) override;

private slots:
    void onTextChanged();
    void doSave();

private:
    void showStatus(const QString &msg, const QColor &color);
    void loadFromDatabase();

    DatabaseManager *m_db;
    QTextEdit *m_edit;
    QLabel *m_statusLabel;
    QTimer *m_saveTimer;

    static const QString DB_KEY;  // 数据库键
};

#endif // MOTTOWIDGET_H