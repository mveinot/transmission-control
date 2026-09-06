#include "appsettings.h"
#include "applicationappearance.h"
#include "applicationlocale.h"
#include "appicons.h"
#include "ui_appsettings.h"
#include "settingskeys.h"

#include <QComboBox>
#include <QPushButton>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QPointer>

#ifdef Q_OS_MACOS
#include "macdefaulthandlerbackend.h"
#endif

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

    ui->appearanceCombo->addItem(tr("Follow System"),
                                 QString::fromLatin1(ApplicationAppearance::FollowSystem));
    ui->appearanceCombo->addItem(tr("Light"),
                                 QString::fromLatin1(ApplicationAppearance::Light));
    ui->appearanceCombo->addItem(tr("Dark"),
                                 QString::fromLatin1(ApplicationAppearance::Dark));

    ui->iconThemeCombo->addItem(tr("Glass"),
                                QString::fromLatin1(AppIcons::GlassTheme));
    ui->iconThemeCombo->addItem(tr("Classic"),
                                QString::fromLatin1(AppIcons::ClassicTheme));

    populateLanguageOptions();
    loadSettings();
    updateNotificationOptionAvailability();
    refreshDefaultHandlerStatus();

    connect(ui->enableNotifications, &QCheckBox::toggled,
            this, &AppSettings::updateNotificationOptionAvailability);
    connect(ui->enableDesktopNotifications, &QCheckBox::toggled,
            this, &AppSettings::updateNotificationOptionAvailability);
    connect(ui->appearanceCombo, &QComboBox::currentIndexChanged,
            this, [this]() {
                // Preview the choice without persisting it until the dialog is accepted.
                ApplicationAppearance::apply(selectedAppearance());
            });
    connect(ui->iconThemeCombo, &QComboBox::currentIndexChanged,
            this, [this]() {
                // Preview every icon consumer; accepting the dialog persists it.
                AppIcons::IconManager::instance().setThemeId(
                    ui->iconThemeCombo->currentData().toString());
            });

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
    connect(ui->buttonDefaultMagnet, &QPushButton::clicked,
            this, [this]() { requestDefaultHandler(true); });
    connect(ui->buttonDefaultTorrent, &QPushButton::clicked,
            this, [this]() { requestDefaultHandler(false); });

    connect(ui->settingsOK, &QPushButton::clicked, this, [this]() {
        saveSettings();
        accept();
    });

    connect(ui->settingsCancel, &QPushButton::clicked, this, [this]() {
        reject();
    });
    connect(this, &QDialog::rejected, this, [this]() {
        // Escape and the window close button have the same rollback semantics
        // as the explicit Cancel button.
        ApplicationAppearance::apply(m_initialAppearance);
        AppIcons::IconManager::instance().setThemeId(m_initialIconTheme);
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

void AppSettings::populateLanguageOptions()
{
    ui->languageCombo->clear();
    for (const ApplicationLocaleOption &option :
         ApplicationLocale::availableOptions()) {
        ui->languageCombo->addItem(option.displayName, option.code);
    }
}

void AppSettings::loadSettings()
{
    QSettings settings;

    m_initialAppearance =
        settings.value(SettingsKeys::Appearance,
                       QString::fromLatin1(ApplicationAppearance::FollowSystem)).toString();
    const int appearanceIndex = ui->appearanceCombo->findData(m_initialAppearance);
    ui->appearanceCombo->setCurrentIndex(appearanceIndex >= 0 ? appearanceIndex : 0);

    auto &icons = AppIcons::IconManager::instance();
    m_initialIconTheme = icons.themeId();
    const int iconThemeIndex = ui->iconThemeCombo->findData(m_initialIconTheme);
    ui->iconThemeCombo->setCurrentIndex(iconThemeIndex >= 0 ? iconThemeIndex : 0);

    const QString localePreference =
        settings.value(
            SettingsKeys::ApplicationLocale,
            QString::fromLatin1(ApplicationLocale::SystemDefault)).toString();
    const int localeIndex = ui->languageCombo->findData(localePreference);
    ui->languageCombo->setCurrentIndex(localeIndex >= 0 ? localeIndex : 0);

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

    ui->showTorrentFileOptionsDialog->setChecked(
        settings.value(SettingsKeys::ShowTorrentFileOptionsDialog, true).toBool()
        );

    ui->showMagnetLinkOptionsDialog->setChecked(
        settings.value(SettingsKeys::ShowMagnetLinkOptionsDialog, true).toBool()
        );

    ui->showSessionOverview->setChecked(
        settings.value(SettingsKeys::ShowSessionOverview, false).toBool()
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

    settings.setValue(SettingsKeys::Appearance, selectedAppearance());
    const QString iconTheme = ui->iconThemeCombo->currentData().toString();
    AppIcons::IconManager::instance().setThemeId(iconTheme);
    settings.setValue(SettingsKeys::IconTheme, iconTheme);
    settings.setValue(SettingsKeys::ApplicationLocale,
                      ui->languageCombo->currentData().toString());

    settings.setValue(SettingsKeys::UpdateInterval,
                      ui->updateInterval->value());

    settings.setValue(SettingsKeys::DeleteTorrentOnAdd,
                      ui->deleteTorrentOnAdd->isChecked());

    // This is the same persisted default consumed and optionally updated by
    // TorrentAddDialog, keeping both configuration surfaces synchronized.
    settings.setValue(SettingsKeys::StartTorrentPaused,
                      ui->startTorrentPaused->isChecked());

    settings.setValue(SettingsKeys::ShowTorrentFileOptionsDialog,
                      ui->showTorrentFileOptionsDialog->isChecked());

    settings.setValue(SettingsKeys::ShowMagnetLinkOptionsDialog,
                      ui->showMagnetLinkOptionsDialog->isChecked());

    settings.setValue(SettingsKeys::ShowSessionOverview,
                      ui->showSessionOverview->isChecked());

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

QString AppSettings::selectedAppearance() const
{
    return ui->appearanceCombo->currentData().toString();
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

void AppSettings::refreshDefaultHandlerStatus()
{
#ifdef Q_OS_MACOS
    const MacDefaultHandlerStatus status = macDefaultHandlerStatus();
    ui->defaultHandlersGroup->setVisible(true);

    if (!status.supported) {
        ui->labelDefaultHandlerDescription->setText(
            tr("Default-handler requests require macOS 12 or later."));
        ui->labelMagnetStatus->setText(tr("Unavailable"));
        ui->labelTorrentStatus->setText(tr("Unavailable"));
        ui->buttonDefaultMagnet->setEnabled(false);
        ui->buttonDefaultTorrent->setEnabled(false);
        return;
    }

    ui->labelDefaultHandlerDescription->setText(
        tr("Planetary can ask macOS to become the default application for "
           "torrent links and files."));
    ui->labelMagnetStatus->setText(
        status.magnetLinks ? tr("Planetary is the default")
                           : tr("Another application is the default"));
    ui->labelTorrentStatus->setText(
        status.torrentFiles ? tr("Planetary is the default")
                            : tr("Another application is the default"));
    ui->buttonDefaultMagnet->setEnabled(!status.magnetLinks);
    ui->buttonDefaultTorrent->setEnabled(!status.torrentFiles);
#else
    ui->defaultHandlersGroup->hide();
#endif
}

void AppSettings::requestDefaultHandler(bool magnetLinks)
{
#ifdef Q_OS_MACOS
    QPushButton *button =
        magnetLinks ? ui->buttonDefaultMagnet : ui->buttonDefaultTorrent;
    button->setEnabled(false);

    const QPointer<AppSettings> guard(this);
    requestMacDefaultHandler(
        magnetLinks ? MacDefaultHandlerKind::MagnetLinks
                    : MacDefaultHandlerKind::TorrentFiles,
        [guard](const QString &error) {
            if (!guard)
                return;

            if (!error.isEmpty()) {
                QMessageBox::warning(
                    guard,
                    AppSettings::tr("Default Application"),
                    AppSettings::tr(
                        "Planetary could not become the default application:\n\n%1")
                        .arg(error));
            }
            guard->refreshDefaultHandlerStatus();
        });
#else
    Q_UNUSED(magnetLinks)
#endif
}
