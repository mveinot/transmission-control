#include "appsettings.h"
#include "ui_appsettings.h"
#include "settingskeys.h"

#include <QPushButton>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
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
    updateNotificationOptionAvailability();

    connect(ui->enableNotifications, &QCheckBox::toggled,
            this, &AppSettings::updateNotificationOptionAvailability);
    connect(ui->enableDesktopNotifications, &QCheckBox::toggled,
            this, &AppSettings::updateNotificationOptionAvailability);

    connect(ui->buttonTestNotification, &QPushButton::clicked,
            this, &AppSettings::testNotificationRequested);
    connect(ui->enableExternalCommand, &QCheckBox::toggled,
            this, &AppSettings::updateNotificationOptionAvailability);
    connect(ui->externalCommandExecutable, &QLineEdit::textChanged,
            this, &AppSettings::updateNotificationOptionAvailability);
    connect(ui->buttonBrowseExternalCommand, &QPushButton::clicked, this, [this]() {
        const QString current = ui->externalCommandExecutable->text().trimmed();
        const QString selected = QFileDialog::getOpenFileName(
            this, tr("Select Notification Command"),
            current.isEmpty() ? QString() : QFileInfo(current).absolutePath());
        if (!selected.isEmpty())
            ui->externalCommandExecutable->setText(selected);
    });
    connect(ui->buttonTestExternalCommand, &QPushButton::clicked, this, [this]() {
        emit testExternalCommandRequested(ui->externalCommandExecutable->text().trimmed(),
                                          ui->externalCommandArguments->text());
    });

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

    connect(ui->buttonResetWatchFolderHistory, &QPushButton::clicked,
            this, [this]() {
                QMessageBox confirmation(
                    QMessageBox::Warning,
                    tr("Reset Imported Torrent History"),
                    tr("This clears Planetary's record of .torrent files already imported from the watch folder.\n\n"
                       "Any .torrent files still present may be submitted to the configured torrent server again. Continue?"),
                    QMessageBox::Yes | QMessageBox::No,
                    this);
                confirmation.setDefaultButton(QMessageBox::No);
#ifdef Q_OS_MACOS
                confirmation.setWindowModality(Qt::WindowModal);
                confirmation.setWindowFlag(Qt::Sheet, true);
#endif
                const QMessageBox::StandardButton choice =
                    static_cast<QMessageBox::StandardButton>(confirmation.exec());

                if (choice != QMessageBox::Yes)
                    return;

                emit clearWatchFolderHistoryRequested();
                QMessageBox::information(
                    this,
                    tr("Imported Torrent History Reset"),
                    tr("The watch folder import history has been cleared.")
                    );
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

    ui->startTorrentPaused->setChecked(
        settings.value(SettingsKeys::StartTorrentPaused, false).toBool()
        );

    ui->showTrayIcon->setChecked(
        settings.value(SettingsKeys::ShowTrayIcon, true).toBool()
        );

    ui->enableNotifications->setChecked(
        settings.value(
            SettingsKeys::ShowNotifications,
            settings.value(SettingsKeys::ShowTrayNotifications, true)
            ).toBool()
        );

    ui->notifyTorrentAdded->setChecked(
        settings.value(SettingsKeys::NotifyTorrentAdded, true).toBool()
        );
    ui->notifyTorrentCompleted->setChecked(
        settings.value(SettingsKeys::NotifyTorrentCompleted, true).toBool()
        );
    ui->notifyTorrentError->setChecked(
        settings.value(SettingsKeys::NotifyTorrentError, true).toBool()
        );
    ui->notifyTorrentStalled->setChecked(
        settings.value(SettingsKeys::NotifyTorrentStalled, true).toBool()
        );

    ui->enableDesktopNotifications->setChecked(
        settings.value(SettingsKeys::DesktopNotificationsEnabled, true).toBool());
    ui->enableExternalCommand->setChecked(
        settings.value(SettingsKeys::ExternalCommandEnabled, false).toBool());
    ui->externalCommandExecutable->setText(
        settings.value(SettingsKeys::ExternalCommandExecutable).toString());
    ui->externalCommandArguments->setText(
        settings.value(SettingsKeys::ExternalCommandArguments,
                       QStringLiteral("--event {event} --name \"{name}\"")).toString());

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

    settings.setValue(SettingsKeys::UpdateInterval,
                      ui->updateInterval->value());

    settings.setValue(SettingsKeys::DeleteTorrentOnAdd,
                      ui->deleteTorrentOnAdd->isChecked());

    // This is the same persisted default consumed and optionally updated by
    // TorrentAddDialog, keeping both configuration surfaces synchronized.
    settings.setValue(SettingsKeys::StartTorrentPaused,
                      ui->startTorrentPaused->isChecked());

    settings.setValue(SettingsKeys::ShowTrayIcon,
                      ui->showTrayIcon->isChecked());

    settings.setValue(SettingsKeys::ShowNotifications,
                      ui->enableNotifications->isChecked());

    settings.setValue(SettingsKeys::NotifyTorrentAdded,
                      ui->notifyTorrentAdded->isChecked());
    settings.setValue(SettingsKeys::NotifyTorrentCompleted,
                      ui->notifyTorrentCompleted->isChecked());
    settings.setValue(SettingsKeys::NotifyTorrentError,
                      ui->notifyTorrentError->isChecked());
    settings.setValue(SettingsKeys::NotifyTorrentStalled,
                      ui->notifyTorrentStalled->isChecked());

    settings.setValue(SettingsKeys::DesktopNotificationsEnabled,
                      ui->enableDesktopNotifications->isChecked());
    settings.setValue(SettingsKeys::ExternalCommandEnabled,
                      ui->enableExternalCommand->isChecked());
    settings.setValue(SettingsKeys::ExternalCommandExecutable,
                      ui->externalCommandExecutable->text().trimmed());
    settings.setValue(SettingsKeys::ExternalCommandArguments,
                      ui->externalCommandArguments->text());

    // Keep writing the old key for downgrade/backward compatibility, but do
    // not make notifications depend on the tray icon anymore.
    settings.setValue(SettingsKeys::ShowTrayNotifications,
                      ui->enableNotifications->isChecked());

    settings.setValue(QString::fromLatin1(SettingsKeys::WatchFolderEnabled),
                      ui->checkWatchFolderEnabled->isChecked());

    settings.setValue(QString::fromLatin1(SettingsKeys::WatchFolderPath),
                      ui->editWatchFolderPath->text().trimmed());

    settings.setValue(QString::fromLatin1(SettingsKeys::WatchFolderStableChecks),
                      ui->spinWatchFolderStableChecks->value());

    settings.sync();
}

void AppSettings::updateNotificationOptionAvailability()
{
    const bool enabled = ui->enableNotifications->isChecked();
    ui->notificationEventsGroup->setEnabled(enabled);
    ui->notificationDeliveryGroup->setEnabled(enabled);
    ui->buttonTestNotification->setEnabled(
        enabled && ui->enableDesktopNotifications->isChecked());
    const bool externalEnabled = enabled && ui->enableExternalCommand->isChecked();
    ui->externalCommandOptions->setEnabled(externalEnabled);
    ui->buttonTestExternalCommand->setEnabled(
        externalEnabled && !ui->externalCommandExecutable->text().trimmed().isEmpty());
}
