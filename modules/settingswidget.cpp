#include "SettingsWidget.h"
#include "DatabaseManager.h"
#include "StatusWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QGroupBox>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QCheckBox>
#include <QSettings>
#include <QDir>

SettingsWidget::SettingsWidget(DatabaseManager *db, StatusWidget *status, QWidget *parent)
    : BaseFloatingWidget(parent), m_db(db), m_status(status)
{
    setWindowFlags(windowFlags() | Qt::Tool | Qt::WindowStaysOnTopHint);
    resize(400, 350);
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect r = screen->geometry();
        move(r.center() - QPoint(width() / 2, height() / 2));
    }

    setupUI();
    loadSettings();
}

SettingsWidget::~SettingsWidget() {}

void SettingsWidget::setupUI()
{
    setStyleSheet(R"(
        QWidget {
            background-color: #15101E;
            color: #00FFC8;
            font-family: "Courier New";
            font-size: 12px;
        }
        QPushButton {
            background-color: #2A1A3A;
            border: 1px solid #00FFC8;
            padding: 5px 12px;
            color: #00FFC8;
        }
        QPushButton:hover { background-color: #00FFC8; color: #0A0A0F; }
        QPushButton:checked { background-color: #FF007F; color: white; }
        QSlider::groove:horizontal { height: 4px; background: #1A1A2E; border: 1px solid #FF007F; }
        QSlider::handle:horizontal { background: #00FFC8; width: 12px; margin: -4px 0; }
        QSpinBox {
            background-color: #0A0A0F;
            border: 1px solid #FF007F;
            color: #00FFC8;
            padding: 2px;
        }
        QComboBox {
            background-color: #0A0A0F;
            border: 1px solid #FF007F;
            color: #00FFC8;
            padding: 2px;
        }
        QLabel { color: #00FFC8; }
        QCheckBox {
            color: #00FFC8;
            font-family: "Courier New";
        }
    )");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 选项卡
    QHBoxLayout *tabLayout = new QHBoxLayout();
    m_tabStatus = new QPushButton("状态栏");
    m_tabPomodoro = new QPushButton("番茄钟");
    m_tabAppearance = new QPushButton("外观");
    m_tabs.append(m_tabStatus);
    m_tabs.append(m_tabPomodoro);
    m_tabs.append(m_tabAppearance);
    for (int i = 0; i < m_tabs.size(); ++i) {
        m_tabs[i]->setCheckable(true);
        tabLayout->addWidget(m_tabs[i]);
    }
    m_tabStatus->setChecked(true);
    mainLayout->addLayout(tabLayout);

    // 堆叠页面
    QStackedWidget *stack = new QStackedWidget();

    // 状态栏设置页
    QWidget *statusPage = new QWidget();
    QVBoxLayout *statusLayout = new QVBoxLayout(statusPage);
    // 口渴速率
    QHBoxLayout *hydLayout = new QHBoxLayout();
    hydLayout->addWidget(new QLabel("口渴速率(每分%)"));
    m_sliderHydrationRate = new QSlider(Qt::Horizontal);
    m_sliderHydrationRate->setRange(1, 10);
    hydLayout->addWidget(m_sliderHydrationRate);
    m_spinHydrationRate = new QSpinBox();
    m_spinHydrationRate->setRange(1, 10);
    hydLayout->addWidget(m_spinHydrationRate);
    statusLayout->addLayout(hydLayout);

    // 负载增速
    QHBoxLayout *loadLayout = new QHBoxLayout();
    loadLayout->addWidget(new QLabel("负载升高间隔(秒)"));
    m_sliderLoadInterval = new QSlider(Qt::Horizontal);
    m_sliderLoadInterval->setRange(3, 60);
    loadLayout->addWidget(m_sliderLoadInterval);
    m_spinLoadInterval = new QSpinBox();
    m_spinLoadInterval->setRange(3, 60);
    loadLayout->addWidget(m_spinLoadInterval);
    statusLayout->addLayout(loadLayout);

    // 提醒阈值
    QHBoxLayout *alertLayout = new QHBoxLayout();
    alertLayout->addWidget(new QLabel("缺水提醒阈值(%)"));
    m_sliderAlertThreshold = new QSlider(Qt::Horizontal);
    m_sliderAlertThreshold->setRange(5, 50);
    alertLayout->addWidget(m_sliderAlertThreshold);
    m_spinAlertThreshold = new QSpinBox();
    m_spinAlertThreshold->setRange(5, 50);
    alertLayout->addWidget(m_spinAlertThreshold);
    statusLayout->addLayout(alertLayout);

    // 开机自启动
    m_chkAutoStart = new QCheckBox("开机自启动");
    m_chkAutoStart->setStyleSheet("color: #00FFC8; font-family: 'Courier New';");
    statusLayout->addWidget(m_chkAutoStart);
    connect(m_chkAutoStart, &QCheckBox::toggled, this, &SettingsWidget::applyAutoStart);

    statusPage->setLayout(statusLayout);

    // 番茄钟预设页
    QWidget *pomPage = new QWidget();
    QVBoxLayout *pomLayout = new QVBoxLayout(pomPage);
    m_comboPreset = new QComboBox();
    pomLayout->addWidget(new QLabel("选择预设"));
    pomLayout->addWidget(m_comboPreset);
    QHBoxLayout *workLayout = new QHBoxLayout();
    workLayout->addWidget(new QLabel("工作分钟"));
    m_spinWork = new QSpinBox(); m_spinWork->setRange(1, 120);
    workLayout->addWidget(m_spinWork);
    pomLayout->addLayout(workLayout);
    QHBoxLayout *restLayout = new QHBoxLayout();
    restLayout->addWidget(new QLabel("休息分钟"));
    m_spinRest = new QSpinBox(); m_spinRest->setRange(1, 30);
    restLayout->addWidget(m_spinRest);
    pomLayout->addLayout(restLayout);
    QHBoxLayout *cyclesLayout = new QHBoxLayout();
    cyclesLayout->addWidget(new QLabel("循环次数"));
    m_spinCycles = new QSpinBox(); m_spinCycles->setRange(1, 20);
    cyclesLayout->addWidget(m_spinCycles);
    pomLayout->addLayout(cyclesLayout);
    m_btnSavePreset = new QPushButton("保存/添加预设");
    pomLayout->addWidget(m_btnSavePreset);
    pomPage->setLayout(pomLayout);

    // 外观设置页
    QWidget *appPage = new QWidget();
    QVBoxLayout *appLayout = new QVBoxLayout(appPage);
    QHBoxLayout *speedLayout = new QHBoxLayout();
    speedLayout->addWidget(new QLabel("灯线速度增量"));
    m_sliderLightSpeed = new QSlider(Qt::Horizontal);
    m_sliderLightSpeed->setRange(1, 20);
    speedLayout->addWidget(m_sliderLightSpeed);
    m_spinLightSpeed = new QSpinBox();
    m_spinLightSpeed->setRange(1, 20);
    speedLayout->addWidget(m_spinLightSpeed);
    appLayout->addLayout(speedLayout);
    QHBoxLayout *dirLayout = new QHBoxLayout();
    dirLayout->addWidget(new QLabel("旋转方向"));
    m_comboLightDirection = new QComboBox();
    m_comboLightDirection->addItem("逆时针 (-1)");
    m_comboLightDirection->addItem("顺时针 (1)");
    dirLayout->addWidget(m_comboLightDirection);
    appLayout->addLayout(dirLayout);
    appPage->setLayout(appLayout);

    stack->addWidget(statusPage);
    stack->addWidget(pomPage);
    stack->addWidget(appPage);
    mainLayout->addWidget(stack);

    // 选项卡切换
    connect(m_tabStatus, &QPushButton::clicked, [this, stack]() { m_currentTab = 0; stack->setCurrentIndex(0); });
    connect(m_tabPomodoro, &QPushButton::clicked, [this, stack]() { m_currentTab = 1; stack->setCurrentIndex(1); });
    connect(m_tabAppearance, &QPushButton::clicked, [this, stack]() { m_currentTab = 2; stack->setCurrentIndex(2); });

    // 滑块与输入框同步
    connect(m_sliderHydrationRate, &QSlider::valueChanged, m_spinHydrationRate, &QSpinBox::setValue);
    connect(m_spinHydrationRate, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderHydrationRate, &QSlider::setValue);
    connect(m_sliderLoadInterval, &QSlider::valueChanged, m_spinLoadInterval, &QSpinBox::setValue);
    connect(m_spinLoadInterval, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderLoadInterval, &QSlider::setValue);
    connect(m_sliderAlertThreshold, &QSlider::valueChanged, m_spinAlertThreshold, &QSpinBox::setValue);
    connect(m_spinAlertThreshold, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderAlertThreshold, &QSlider::setValue);
    connect(m_sliderLightSpeed, &QSlider::valueChanged, m_spinLightSpeed, &QSpinBox::setValue);
    connect(m_spinLightSpeed, QOverload<int>::of(&QSpinBox::valueChanged), m_sliderLightSpeed, &QSlider::setValue);

    // 应用按钮
    m_btnApply = new QPushButton("应用设置");
    mainLayout->addWidget(m_btnApply);
    connect(m_btnApply, &QPushButton::clicked, [this]() {
        saveHydrationSettings();
        saveLoadSettings();
        saveAppearanceSettings();
        savePomodoroPresets();
        if (m_status) m_status->reloadSettings();
    });
}

void SettingsWidget::loadSettings()
{
    if (!m_db) return;
    m_spinHydrationRate->setValue(m_db->getUserState("hydration_rate", "1").toInt());
    m_spinLoadInterval->setValue(m_db->getUserState("load_rate_seconds", "8").toInt());
    m_spinAlertThreshold->setValue(m_db->getUserState("hydration_alert_threshold", "30").toInt());
    m_spinWork->setValue(25);
    m_spinRest->setValue(5);
    m_spinCycles->setValue(1);
    m_spinLightSpeed->setValue(m_db->getUserState("light_speed", "5").toInt());
    int dir = m_db->getUserState("light_direction", "-1").toInt();
    m_comboLightDirection->setCurrentIndex(dir == 1 ? 1 : 0);

    // 加载开机自启动状态
    bool autoStart = (m_db->getUserState("auto_start", "0") == "1");
    m_chkAutoStart->setChecked(autoStart);
}

void SettingsWidget::saveHydrationSettings()
{
    m_db->setUserState("hydration_rate", QString::number(m_spinHydrationRate->value()));
    m_db->setUserState("hydration_alert_threshold", QString::number(m_spinAlertThreshold->value()));
}

void SettingsWidget::saveLoadSettings()
{
    m_db->setUserState("load_rate_seconds", QString::number(m_spinLoadInterval->value()));
}

void SettingsWidget::savePomodoroPresets()
{
    m_db->setUserState("pomodoro_work", QString::number(m_spinWork->value()));
    m_db->setUserState("pomodoro_rest", QString::number(m_spinRest->value()));
    m_db->setUserState("pomodoro_cycles", QString::number(m_spinCycles->value()));
}

void SettingsWidget::saveAppearanceSettings()
{
    m_db->setUserState("light_speed", QString::number(m_spinLightSpeed->value()));
    int dir = m_comboLightDirection->currentIndex() == 1 ? 1 : -1;
    m_db->setUserState("light_direction", QString::number(dir));
}

void SettingsWidget::applyAutoStart()
{
    bool enable = m_chkAutoStart->isChecked();
    m_db->setUserState("auto_start", enable ? "1" : "0");

    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    if (enable) {
        QString appPath = QApplication::applicationFilePath();
        reg.setValue("CyberScheduleV2", QDir::toNativeSeparators(appPath));
    } else {
        reg.remove("CyberScheduleV2");
    }
}