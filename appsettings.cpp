#include "appsettings.h"
#include "ui_appsettings.h"
#include "settingskeys.h"

#include <QPushButton>
#include <QSettings>

namespace {
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
    updateTrayOptionAvailability();

    connect(ui->showTrayIcon, &QCheckBox::toggled,
            this, &AppSettings::updateTrayOptionAvailability);

    connect(ui->settingsOK, &QPushButton::clicked, this, [this]() {
        saveSettings();
        accept();
    });

    connect(ui->settingsCancel, &QPushButton::clicked, this, [this]() {
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
        settings.value(SettingsKeys::UpdateInterval,
                       DefaultUpdateIntervalSeconds).toInt();

    ui->updateInterval->setValue(
        qBound(MinimumUpdateIntervalSeconds,
               intervalSeconds,
               MaximumUpdateIntervalSeconds)
        );

    ui->deleteTorrentOnAdd->setChecked(
        settings.value(SettingsKeys::DeleteTorrentOnAdd, false).toBool()
        );

    ui->showTrayIcon->setChecked(
        settings.value(SettingsKeys::ShowTrayIcon, true).toBool()
        );

    ui->showNotifications->setChecked(
        settings.value(SettingsKeys::ShowTrayNotifications, true).toBool()
        );
}

void AppSettings::saveSettings()
{
    QSettings settings;

    const bool trayIconEnabled = ui->showTrayIcon->isChecked();

    settings.setValue(SettingsKeys::UpdateInterval,
                      ui->updateInterval->value());

    settings.setValue(SettingsKeys::DeleteTorrentOnAdd,
                      ui->deleteTorrentOnAdd->isChecked());

    settings.setValue(SettingsKeys::ShowTrayIcon,
                      trayIconEnabled);

    settings.setValue(SettingsKeys::ShowTrayNotifications,
                      trayIconEnabled && ui->showNotifications->isChecked());

    settings.setValue(SettingsKeys::HideApplicationIcon,
                      trayIconEnabled && false);

    settings.sync();
}

void AppSettings::updateTrayOptionAvailability()
{
    const bool trayIconEnabled = ui->showTrayIcon->isChecked();

    ui->showNotifications->setEnabled(trayIconEnabled);

    if (!trayIconEnabled) {
        ui->showNotifications->setChecked(false);
    }
}