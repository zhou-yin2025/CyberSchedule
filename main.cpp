#include <QApplication>
#include <QPropertyAnimation>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QDebug>

#include "core/DatabaseManager.h"
#include "animations/SplashScreen.h"
#include "animations/GridBackground.h"
#include "modules/StatusWidget.h"
#include "modules/HUDWidget.h"
#include "modules/CalendarWidget.h"
#include "modules/StickyNoteWidget.h"
#include "modules/TaskWidgetV3.h"
#include "modules/MottoWidget.h"
#include "core/PomodoroTimer.h"
#include "modules/SettingsWidget.h"
#include <QSettings>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. 初始化数据库
    DatabaseManager db("data/cyberschedule.db");
    if (!db.initialize()) {
        qWarning() << "数据库初始化失败！";
        return -1;
    }

    // 同步开机自启动注册表
    {
        bool autoStart = (db.getUserState("auto_start", "0") == "1");
        QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      QSettings::NativeFormat);
        if (autoStart) {
            QString appPath = QApplication::applicationFilePath();
            reg.setValue("CyberScheduleV2", QDir::toNativeSeparators(appPath));
        } else {
            reg.remove("CyberScheduleV2");
        }
    }

    // 2. 创建全局背景（先于所有窗口，显示在最底层）
    GridBackground *gridBg = new GridBackground();
    gridBg->show();

    // 3. 创建所有悬浮窗（先隐藏）
    StatusWidget *status = new StatusWidget(&db);
    status->hide();

    HUDWidget *hud = new HUDWidget();
    hud->hide();

    CalendarWidget *cal = new CalendarWidget(&db);
    cal->hide();

    StickyNoteWidget *note = new StickyNoteWidget(&db);
    note->hide();

    TaskWidgetV3 *task = new TaskWidgetV3(&db);
    task->hide();

    MottoWidget *motto = new MottoWidget(&db);
    // 设置尺寸和位置
    motto->resize(400, 120);
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sr = screen->geometry();
        int x = sr.width() * 0.65;
        int y = sr.height() * 0.0001;
        motto->move(x, y);
    }
    motto->hide();

    // 4. 信号连接
    QObject::connect(hud->pomodoroTimer(), &PomodoroTimer::cycleCompleted,
                     status, [status](int) { status->addExp(10); });
    QObject::connect(hud->pomodoroTimer(), &PomodoroTimer::allCompleted,
                     status, [status]() { status->addExp(30); });

    QObject::connect(cal, &CalendarWidget::dateClicked, task, &TaskWidgetV3::showTasksForDate);

    QObject::connect(task, &TaskWidgetV3::taskCompleted, status,
                     [status](int exp) { status->addExp(exp); });

    QObject::connect(task, &TaskWidgetV3::tasksChanged, cal, &CalendarWidget::reloadAndUpdate);

    QObject::connect(task, &TaskWidgetV3::settingsRequested, [&db, status]() {
        SettingsWidget *settings = new SettingsWidget(&db, status);
        settings->show();
    });

    QObject::connect(task, &TaskWidgetV3::requestPomodoro, hud, &HUDWidget::startPomodoroForTask);

    // 5. 开屏动画
    SplashScreen *splash = new SplashScreen();
    splash->show();

    // 将背景提升到开屏动画之上（确保两个圆可见）
    gridBg->raise();

    // 6. 开屏结束后：状态栏启动动画 → 其他模块依次潮汐登场
    QObject::connect(splash, &SplashScreen::finished, [status, hud, cal, note, task, motto, splash]() {
        splash->deleteLater();
        status->show();
        status->playStartupAnimation();

        QObject::connect(status, &StatusWidget::startupAnimationFinished,
                         [hud, cal, note, task, motto]() {
                             auto launchWithWash = [](BaseFloatingWidget *w, int delayMs) {
                                 QTimer::singleShot(delayMs, w, [w]() {
                                     w->show();
                                     w->startLeftToRightWash();
                                 });
                             };

                             launchWithWash(hud, 0);
                             launchWithWash(cal, 150);
                             launchWithWash(note, 300);
                             launchWithWash(task, 450);
                             launchWithWash(motto, 600);
                         });
    });

    return a.exec();
}