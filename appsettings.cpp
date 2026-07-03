#include "appsettings.h"
#include "ui_appsettings.h"
#include "settingskeys.h"

#include <QPushButton>
#include <QFileDialog>
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

    setWindowTitle(tr("Application Settings"));

    ui->updateInterval->setMinimum(MinimumUpdateIntervalSeconds);
    ui->updateInterval->setMaximum(MaximumUpdateIntervalSeconds);
    ui->updateInterval->setSuffix(tr(" seconds"));

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

    connect(ui->buttonBrowseWatchFolder, &QPushButton::clicked,
            this, [this]() {
                const QString folder = QFileDialog::getExistingDirectory(
                    this,
                    tr("Select Watch Folder"),
                    ui->editWatchFolderPath->text()
                    );

                if (!folder.isEmpty())
                    ui->editWatchFolderPath->setText(folder);
            });

    connect(ui->checkWatchFolderEnabled, &QCheckBox::toggled,
            this, [this](bool enabled) {
                ui->editWatchFolderPath->setEnabled(enabled);
                ui->buttonBrowseWatchFolder->setEnabled(enabled);
                ui->spinWatchFolderStableChecks->setEnabled(enabled);
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
        settings.value(
            SettingsKeys::ShowNotifications,
            settings.value(SettingsKeys::ShowTrayNotifications, true)
            ).toBool()
        );

    ui->checkWatchFolderEnabled->setChecked(
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderEnabled),
                       false).toBool()
        );

    ui->editWatchFolderPath->setText(
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderPath))
            .toString()
        );

    ui->spinWatchFolderStableChecks->setValue(
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderStableChecks),
                       2).toInt()
        );

    const bool watchFolderEnabled = ui->checkWatchFolderEnabled->isChecked();
    ui->editWatchFolderPath->setEnabled(watchFolderEnabled);
    ui->buttonBrowseWatchFolder->setEnabled(watchFolderEnabled);
    ui->spinWatchFolderStableChecks->setEnabled(watchFolderEnabled);
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

    settings.setValue(SettingsKeys::ShowNotifications,
                      ui->showNotifications->isChecked());

    // Keep writing the old key for downgrade/backward compatibility, but do
    // not make notifications depend on the tray icon anymore.
    settings.setValue(SettingsKeys::ShowTrayNotifications,
                      ui->showNotifications->isChecked());

    settings.setValue(SettingsKeys::HideApplicationIcon,
                      trayIconEnabled && false);

    settings.setValue(QString::fromLatin1(SettingsKeys::WatchFolderEnabled),
                      ui->checkWatchFolderEnabled->isChecked());

    settings.setValue(QString::fromLatin1(SettingsKeys::WatchFolderPath),
                      ui->editWatchFolderPath->text().trimmed());

    settings.setValue(QString::fromLatin1(SettingsKeys::WatchFolderStableChecks),
                      ui->spinWatchFolderStableChecks->value());

    settings.sync();
}

void AppSettings::updateTrayOptionAvailability()
{
    const bool trayIconEnabled = ui->showTrayIcon->isChecked();

    Q_UNUSED(trayIconEnabled);

    // Notifications are no longer coupled to the tray/menu-bar icon. On macOS
    // Planetary can use basic native notifications even when the menu-bar icon
    // is disabled, and other platforms fall back gracefully.
    ui->showNotifications->setEnabled(true);
}