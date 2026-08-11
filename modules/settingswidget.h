#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include "BaseFloatingWidget.h"
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QComboBox>
#include <QCheckBox>

class DatabaseManager;
class StatusWidget;

class SettingsWidget : public BaseFloatingWidget
{
    Q_OBJECT
public:
    explicit SettingsWidget(DatabaseManager *db, StatusWidget *status = nullptr, QWidget *parent = nullptr);
    ~SettingsWidget();

private slots:
    void saveHydrationSettings();
    void saveLoadSettings();
    void savePomodoroPresets();
    void saveAppearanceSettings();
    void applyAutoStart();          // 应用开机自启动设置

private:
    void setupUI();
    void loadSettings();

    DatabaseManager *m_db;
    StatusWidget *m_status;

    // 选项卡按钮
    QPushButton *m_tabStatus;
    QPushButton *m_tabPomodoro;
    QPushButton *m_tabAppearance;
    QVector<QPushButton*> m_tabs;
    int m_currentTab = 0;

    // 状态栏设置控件
    QSlider *m_sliderHydrationRate;
    QSpinBox *m_spinHydrationRate;
    QSlider *m_sliderLoadInterval;
    QSpinBox *m_spinLoadInterval;
    QSlider *m_sliderAlertThreshold;
    QSpinBox *m_spinAlertThreshold;

    // 番茄钟预设
    QComboBox *m_comboPreset;
    QSpinBox *m_spinWork;
    QSpinBox *m_spinRest;
    QSpinBox *m_spinCycles;
    QPushButton *m_btnSavePreset;

    // 外观设置
    QSlider *m_sliderLightSpeed;
    QSpinBox *m_spinLightSpeed;
    QComboBox *m_comboLightDirection;

    // 开机自启动
    QCheckBox *m_chkAutoStart;

    QPushButton *m_btnApply;
};

#endif