#include "databasemanager.h"
#include <QSqlError>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

DatabaseManager::DatabaseManager(const QString &dbPath, QObject *parent)
    : QObject(parent)
{
    // 确保目录存在
    QFileInfo fileInfo(dbPath);
    QDir().mkpath(fileInfo.absolutePath());

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen())
        m_db.close();
}

bool DatabaseManager::initialize()
{
    if (!m_db.open()) {
        qWarning() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);

    // 创建任务表
    bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS tasks ("
        "id INTEGER PRIMARY KEY,"
        "title TEXT NOT NULL,"
        "description TEXT DEFAULT '',"
        "priority INTEGER DEFAULT 1,"
        "deadline TEXT,"
        "category_id INTEGER DEFAULT 0,"
        "repeat_rule TEXT DEFAULT '',"
        "custom_order INTEGER DEFAULT 0,"
        "completed INTEGER DEFAULT 0"
        ")"
        );
    if (!ok) qWarning() << "创建 tasks 表失败:" << query.lastError().text();

    // 创建分类表
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS categories ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "color TEXT DEFAULT '#00FFC8'"
        ")"
        );
    if (!ok) qWarning() << "创建 categories 表失败:" << query.lastError().text();

    // 插入默认分类（如果不存在）
    query.exec("INSERT OR IGNORE INTO categories (id, name, color) VALUES (1, '工作', '#FF007F')");
    query.exec("INSERT OR IGNORE INTO categories (id, name, color) VALUES (2, '学习', '#00FFC8')");
    query.exec("INSERT OR IGNORE INTO categories (id, name, color) VALUES (3, '健康', '#0066FF')");
    query.exec("INSERT OR IGNORE INTO categories (id, name, color) VALUES (4, '其他', '#888888')");

    // 创建便签表
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS sticky_note ("
        "id INTEGER PRIMARY KEY DEFAULT 1,"
        "content TEXT DEFAULT '',"
        "updated_at TEXT"
        ")"
        );
    if (!ok) qWarning() << "创建 sticky_note 表失败:" << query.lastError().text();

    // 确保便签表有一条默认记录
    query.exec("INSERT OR IGNORE INTO sticky_note (id, content) VALUES (1, '')");

    // 创建用户状态表（key-value 存储）
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS user_state ("
        "key TEXT PRIMARY KEY,"
        "value TEXT"
        ")"
        );
    if (!ok) qWarning() << "创建 user_state 表失败:" << query.lastError().text();

    // 插入默认状态
    query.exec("INSERT OR IGNORE INTO user_state (key, value) VALUES ('player_name', 'V4L3R1E')");
    query.exec("INSERT OR IGNORE INTO user_state (key, value) VALUES ('exp_total', '0')");
    query.exec("INSERT OR IGNORE INTO user_state (key, value) VALUES ('hydration', '100')");
    query.exec("INSERT OR IGNORE INTO user_state (key, value) VALUES ('hydration_max', '100')");
    query.exec("INSERT OR IGNORE INTO user_state (key, value) VALUES ('system_load', '25')");
    query.exec("INSERT OR IGNORE INTO user_state (key, value) VALUES ('avatar_path', '')");

    return true;
}

// ---------- 任务 CRUD ----------

int DatabaseManager::getNextTaskId()
{
    QSqlQuery query(m_db);
    query.exec("SELECT MAX(id) FROM tasks");
    if (query.next()) {
        int maxId = query.value(0).toInt();
        if (maxId >= 999999) {
            // 触发编号重排
            query.exec("SELECT id FROM tasks ORDER BY custom_order, deadline");
            int newId = 0;
            QVector<int> oldIds;
            while (query.next()) {
                oldIds.append(query.value(0).toInt());
            }
            for (int oldId : oldIds) {
                QSqlQuery update(m_db);
                update.prepare("UPDATE tasks SET id = ? WHERE id = ?");
                update.addBindValue(newId);
                update.addBindValue(oldId);
                update.exec();
                newId++;
                if (newId > 999999) newId = 0; // 理论上不会溢出
            }
            return newId;
        }
        return maxId + 1;
    }
    return 0; // 第一项任务编号为 0
}

bool DatabaseManager::addTask(TaskData &task)
{
    if (task.id < 0)
        task.id = getNextTaskId();

    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO tasks (id, title, description, priority, deadline, category_id, "
        "repeat_rule, custom_order, completed) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );
    query.addBindValue(task.id);
    query.addBindValue(task.title);
    query.addBindValue(task.description);
    query.addBindValue(task.priority);
    query.addBindValue(task.deadline.toString(Qt::ISODate));
    query.addBindValue(task.categoryId);
    query.addBindValue(task.repeatRule);
    query.addBindValue(task.customOrder);
    query.addBindValue(task.completed ? 1 : 0);

    if (!query.exec()) {
        qWarning() << "添加任务失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::updateTask(const TaskData &task)
{
    QSqlQuery query(m_db);
    query.prepare(
        "UPDATE tasks SET title=?, description=?, priority=?, deadline=?, "
        "category_id=?, repeat_rule=?, custom_order=?, completed=? WHERE id=?"
        );
    query.addBindValue(task.title);
    query.addBindValue(task.description);
    query.addBindValue(task.priority);
    query.addBindValue(task.deadline.toString(Qt::ISODate));
    query.addBindValue(task.categoryId);
    query.addBindValue(task.repeatRule);
    query.addBindValue(task.customOrder);
    query.addBindValue(task.completed ? 1 : 0);
    query.addBindValue(task.id);

    if (!query.exec()) {
        qWarning() << "更新任务失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::deleteTask(int id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tasks WHERE id = ?");
    query.addBindValue(id);
    return query.exec();
}

bool DatabaseManager::markTaskCompleted(int id)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE tasks SET completed = 1 WHERE id = ?");
    query.addBindValue(id);
    return query.exec();
}

QVector<TaskData> DatabaseManager::getAllTasks(const QString &sortBy)
{
    QVector<TaskData> tasks;
    QSqlQuery query(m_db);
    QString sql = "SELECT id, title, description, priority, deadline, category_id, "
                  "repeat_rule, custom_order, completed FROM tasks WHERE completed = 0";

    if (sortBy == "deadline")
        sql += " ORDER BY deadline ASC, priority DESC";
    else if (sortBy == "id")
        sql += " ORDER BY id ASC";
    else if (sortBy == "custom_order")
        sql += " ORDER BY custom_order ASC";
    else
        sql += " ORDER BY deadline ASC, priority DESC";

    query.exec(sql);
    while (query.next()) {
        TaskData t;
        t.id = query.value(0).toInt();
        t.title = query.value(1).toString();
        t.description = query.value(2).toString();
        t.priority = query.value(3).toInt();
        t.deadline = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
        t.categoryId = query.value(5).toInt();
        t.repeatRule = query.value(6).toString();
        t.customOrder = query.value(7).toInt();
        t.completed = query.value(8).toBool();
        tasks.append(t);
    }
    return tasks;
}

QVector<TaskData> DatabaseManager::getTasksByDate(const QDate &date)
{
    QVector<TaskData> tasks;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, title, description, priority, deadline, category_id, "
        "repeat_rule, custom_order, completed FROM tasks "
        "WHERE completed = 0 AND date(deadline) = ? "
        "ORDER BY deadline ASC, priority DESC"
        );
    query.addBindValue(date.toString(Qt::ISODate));
    query.exec();
    while (query.next()) {
        TaskData t;
        t.id = query.value(0).toInt();
        t.title = query.value(1).toString();
        t.description = query.value(2).toString();
        t.priority = query.value(3).toInt();
        t.deadline = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
        t.categoryId = query.value(5).toInt();
        t.repeatRule = query.value(6).toString();
        t.customOrder = query.value(7).toInt();
        t.completed = query.value(8).toBool();
        tasks.append(t);
    }
    return tasks;
}

TaskData DatabaseManager::getTaskById(int id)
{
    TaskData t;
    QSqlQuery query(m_db);
    query.prepare("SELECT id, title, description, priority, deadline, category_id, "
                  "repeat_rule, custom_order, completed FROM tasks WHERE id = ?");
    query.addBindValue(id);
    query.exec();
    if (query.next()) {
        t.id = query.value(0).toInt();
        t.title = query.value(1).toString();
        t.description = query.value(2).toString();
        t.priority = query.value(3).toInt();
        t.deadline = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
        t.categoryId = query.value(5).toInt();
        t.repeatRule = query.value(6).toString();
        t.customOrder = query.value(7).toInt();
        t.completed = query.value(8).toBool();
    }
    return t;
}

// ---------- 分类 CRUD ----------

bool DatabaseManager::addCategory(const QString &name, const QString &color)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO categories (name, color) VALUES (?, ?)");
    query.addBindValue(name);
    query.addBindValue(color);
    return query.exec();
}

bool DatabaseManager::deleteCategory(int id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM categories WHERE id = ?");
    query.addBindValue(id);
    return query.exec();
}

QVector<CategoryData> DatabaseManager::getAllCategories()
{
    QVector<CategoryData> cats;
    QSqlQuery query(m_db);
    query.exec("SELECT id, name, color FROM categories");
    while (query.next()) {
        CategoryData c;
        c.id = query.value(0).toInt();
        c.name = query.value(1).toString();
        c.color = query.value(2).toString();
        cats.append(c);
    }
    return cats;
}

// ---------- 便签 ----------

bool DatabaseManager::saveStickyNote(const QString &content)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE sticky_note SET content = ?, updated_at = datetime('now') WHERE id = 1");
    query.addBindValue(content);
    return query.exec();
}

QString DatabaseManager::loadStickyNote()
{
    QSqlQuery query(m_db);
    query.exec("SELECT content FROM sticky_note WHERE id = 1");
    if (query.next())
        return query.value(0).toString();
    return "";
}

// ---------- 用户状态 ----------

bool DatabaseManager::setUserState(const QString &key, const QString &value)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO user_state (key, value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value);
    return query.exec();
}

QString DatabaseManager::getUserState(const QString &key, const QString &defaultValue)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM user_state WHERE key = ?");
    query.addBindValue(key);
    query.exec();
    if (query.next())
        return query.value(0).toString();
    return defaultValue;
}