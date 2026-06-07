#include "appsettings.h"
#include "ui_appsettings.h"

#include <QPushButton>
#include <QSettings>

namespace {
constexpr const char *DeleteTorrentOnAddKey = "app/deleteTorrentFileOnSuccessfulAdd";
constexpr const char *UpdateIntervalKey = "app/updateIntervalSeconds";
constexpr int DefaultUpdateIntervalSeconds = 10;
constexpr int MinimumUpdateIntervalSeconds = 1;
constexpr int MaximumUpdateIntervalSeconds = 3600;
}

AppSettings::AppSettings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AppSettings)
{
    ui->setupUi(this);

    setWindowTitle("Application Settings");

    ui->updateInterval->setMinimum(MinimumUpdateIntervalSeconds);
    ui->updateInterval->setMaximum(MaximumUpdateIntervalSeconds);
    ui->updateInterval->setSuffix(" seconds");

    loadSettings();

    connect(ui->settingsOK, &QPushButton::clicked,
            this,
            [this]() {
                saveSettings();
                accept();
            });

    connect(ui->settingsCancel, &QPushButton::clicked,
            this,
            [this]() {
                reject();
            });
}

AppSettings::~AppSettings()
{
    delete ui;
}

void AppSettings::loadSettings()
{
    QSettings settings;

    const int intervalSeconds =
        settings.value(UpdateIntervalKey, DefaultUpdateIntervalSeconds).toInt();

    ui->updateInterval->setValue(
        qBound(MinimumUpdateIntervalSeconds,
               intervalSeconds,
               MaximumUpdateIntervalSeconds)
        );

    ui->deleteTorrentOnAdd->setChecked(
        settings.value(DeleteTorrentOnAddKey, false).toBool()
        );
}

void AppSettings::saveSettings()
{
    QSettings settings;

    settings.setValue(UpdateIntervalKey, ui->updateInterval->value());

    settings.setValue(
        DeleteTorrentOnAddKey,
        ui->deleteTorrentOnAdd->isChecked()
        );

    settings.sync();
}