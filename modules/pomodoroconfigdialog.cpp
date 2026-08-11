#include "pomodoroconfigdialog.h"
#include "ui_pomodoroconfigdialog.h"
#include <QScreen>
#include <QGuiApplication>

PomodoroConfigDialog::PomodoroConfigDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PomodoroConfigDialog)
{
    ui->setupUi(this);

    // 设置无边框
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, false);  // 这里我们希望它有背景，所以 false

    // 设置初始位置：屏幕中上方靠下一些
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen->geometry();
    move((screenRect.width() - width()) / 2,screenRect.height() / 5);

    // 设置滑块范围
    ui->sliderWork->setRange(1, 60);
    ui->sliderRest->setRange(1, 30);
    ui->sliderCycles->setRange(1, 10);

    // 预设列表
    ui->comboPreset->addItem(QStringLiteral("自定义"));
    ui->comboPreset->addItem(QStringLiteral("25 分钟工作 + 5 分钟休息"));
    ui->comboPreset->addItem(QStringLiteral("45 分钟工作 + 15 分钟休息"));

    // 默认值（设置后会自动触发 valueChanged 信号，更新 LCD）
    ui->sliderWork->setValue(25);
    ui->sliderRest->setValue(5);
    ui->sliderCycles->setValue(1);

    // 连接信号
    connect(ui->comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PomodoroConfigDialog::onPresetChanged);

    // 滑块变化 → LCD 数字更新
    connect(ui->sliderWork, &QSlider::valueChanged, this,
            [this](int v) { ui->lcdWork->display(v); });
    connect(ui->sliderRest, &QSlider::valueChanged, this,
            [this](int v) { ui->lcdRest->display(v); });
    connect(ui->sliderCycles, &QSlider::valueChanged, this,
            [this](int v) { ui->lcdCycles->display(v); });

    // 按钮
    connect(ui->btnStart, &QPushButton::clicked, this, &PomodoroConfigDialog::onStartClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    // 赛博朋克样式表
    setStyleSheet(R"(
        QDialog {
            background-color: #0F0F1A;
            border: 2px solid #00FFC8;
        }
        QLabel {
            color: #00FFC8;
            font-family: "Courier New";
            font-size: 12px;
        }
        QLCDNumber {
            color: #00FFC8;
            background-color: #0A0A0F;
            border: 1px solid #FF007F;
            font-family: "Courier New";
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #1A1A2E;
            border: 1px solid #FF007F;
        }
        QSlider::handle:horizontal {
            background: #00FFC8;
            border: 1px solid #00FFC8;
            width: 14px;
            margin: -4px 0;
        }
        QComboBox {
            background-color: #0F0F1A;
            color: #00FFC8;
            border: 1px solid #FF007F;
            padding: 2px;
        }
        QPushButton {
            background-color: #1A1A2E;
            border: 1px solid #00FFC8;
            color: #00FFC8;
            font-family: "Courier New";
            padding: 4px 12px;
        }
        QPushButton:hover {
            background-color: #00FFC8;
            color: #0A0A0F;
        }
    )");
}

PomodoroConfigDialog::~PomodoroConfigDialog()
{
    delete ui;
}

int PomodoroConfigDialog::workMinutes() const { return ui->sliderWork->value(); }
int PomodoroConfigDialog::restMinutes() const { return ui->sliderRest->value(); }
int PomodoroConfigDialog::totalCycles() const { return ui->sliderCycles->value(); }

void PomodoroConfigDialog::onPresetChanged(int index)
{
    if (index == 1)
    {
        ui->sliderWork->setValue(25);
        ui->sliderRest->setValue(5);
    }
    else if (index == 2)
    {
        ui->sliderWork->setValue(45);
        ui->sliderRest->setValue(15);
    }
}

void PomodoroConfigDialog::onStartClicked()
{
    emit startRequested(workMinutes(), restMinutes(), totalCycles());
    accept();
}
