#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVector>
#include <QString>
#include <QDateTime>

// 任务数据结构（后续可迁移到独立 Model 文件）
struct TaskData {
    int id = -1;
    QString title;
    QString description;
    int priority = 1;       // 0低 1中 2高 3紧急
    QDateTime deadline;
    int categoryId = 0;
    QString repeatRule;     // 重复规则（RFC 5545 简化版）
    int customOrder = 0;
    bool completed = false;
};

// 分类数据结构
struct CategoryData {
    int id = -1;
    QString name;
    QString color;          // 如 "#FF007F"
};

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(const QString &dbPath, QObject *parent = nullptr);
    ~DatabaseManager();

    // 初始化数据库表
    bool initialize();

    // ---------- 任务 CRUD ----------
    bool addTask(TaskData &task);           // 添加后 task.id 会被赋值为新编号
    bool updateTask(const TaskData &task);
    bool deleteTask(int id);
    bool markTaskCompleted(int id);
    QVector<TaskData> getAllTasks(const QString &sortBy = "deadline");
    QVector<TaskData> getTasksByDate(const QDate &date);
    TaskData getTaskById(int id);

    // ---------- 分类 CRUD ----------
    bool addCategory(const QString &name, const QString &color);
    bool deleteCategory(int id);
    QVector<CategoryData> getAllCategories();

    // ---------- 便签 ----------
    bool saveStickyNote(const QString &content);
    QString loadStickyNote();

    // ---------- 用户状态 ----------
    bool setUserState(const QString &key, const QString &value);
    QString getUserState(const QString &key, const QString &defaultValue = "");

private:
    int getNextTaskId();
    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H