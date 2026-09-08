#include "appsettings.h"
#include "applicationlocale.h"
#include "colorthememanager.h"
#include "iconthememanager.h"
#include "themeregistry.h"
#include "ui_appsettings.h"
#include "settingskeys.h"

#include <QComboBox>
#include <QPushButton>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>

#include <algorithm>

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

    populateThemeOptions();

    populateLanguageOptions();
    loadSettings();
    updateNotificationOptionAvailability();
    // Launch Services queries can block while macOS resolves the registered
    // handlers. Defer them until the dialog's event loop is running so the
    // Preferences window can appear immediately.
    QTimer::singleShot(250, this, [this]() {
        if (isVisible())
            refreshDefaultHandlerStatus();
    });

    connect(ui->enableNotifications, &QCheckBox::toggled,
            this, &AppSettings::updateNotificationOptionAvailability);
    connect(ui->enableDesktopNotifications, &QCheckBox::toggled,
            this, &AppSettings::updateNotificationOptionAvailability);
    connect(ui->colorThemeCombo, &QComboBox::currentIndexChanged,
            this, [this]() {
                // Preview the choice without persisting it until the dialog is accepted.
                AppColors::ColorThemeManager::instance().setThemeId(
                    selectedColorTheme());
            });
    connect(ui->iconThemeCombo, &QComboBox::currentIndexChanged,
            this, [this]() {
                // Preview every icon consumer; accepting the dialog persists it.
                AppIcons::IconThemeManager::instance().setThemeId(
                ui->iconThemeCombo->currentData().toString());
            });
    connect(ui->refreshThemePacks, &QPushButton::clicked,
            this, [this]() {
                AppThemes::ThemeRegistry::instance().rescanExternalThemes();
                populateThemeOptions();
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
        AppColors::ColorThemeManager::instance().setThemeId(
            m_initialColorTheme);
        AppIcons::IconThemeManager::instance().setThemeId(m_initialIconTheme);
        AppColors::ColorThemeManager::instance().setStylesheetEnabled(false);
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

void AppSettings::populateThemeOptions()
{
    const QString selectedColor = ui->colorThemeCombo->currentData().toString();
    const QString selectedIcon = ui->iconThemeCombo->currentData().toString();
    const QSignalBlocker colorBlocker(ui->colorThemeCombo);
    const QSignalBlocker iconBlocker(ui->iconThemeCombo);
    auto &themeRegistry = AppThemes::ThemeRegistry::instance();

    const auto displayColorName = [this](const AppColors::ColorTheme &theme) {
        QString displayName = theme.displayName();
        if (theme.id() == QString::fromLatin1(AppColors::SystemTheme))
            displayName = tr("Follow System");
        else if (theme.id() == QString::fromLatin1(AppColors::LightTheme))
            displayName = tr("Light");
        else if (theme.id() == QString::fromLatin1(AppColors::DarkTheme))
            displayName = tr("Dark");
        return displayName;
    };
    const auto displayIconName = [this](const AppIcons::IconTheme &theme) {
        QString displayName = theme.displayName();
        if (theme.id() == QString::fromLatin1(AppIcons::GlassTheme))
            displayName = tr("Glass");
        else if (theme.id() == QString::fromLatin1(AppIcons::ClassicTheme))
            displayName = tr("Classic");
        return displayName;
    };
    const auto alphabetical = [](const auto &left, const auto &right) {
        return QString::localeAwareCompare(left.displayName(), right.displayName()) < 0;
    };

    QList<AppColors::ColorTheme> nativeColors;
    QList<AppColors::ColorTheme> externalColors;
    for (const AppColors::ColorTheme &theme : themeRegistry.colorThemes()) {
        if (theme.isBuiltIn())
            nativeColors.append(theme);
        else
            externalColors.append(theme);
    }
    std::sort(externalColors.begin(), externalColors.end(), alphabetical);

    ui->colorThemeCombo->clear();
    for (const AppColors::ColorTheme &theme : nativeColors)
        ui->colorThemeCombo->addItem(displayColorName(theme), theme.id());
    for (const AppColors::ColorTheme &theme : externalColors)
        ui->colorThemeCombo->addItem(displayColorName(theme), theme.id());

    QList<AppIcons::IconTheme> nativeIcons;
    QList<AppIcons::IconTheme> externalIcons;
    for (const AppIcons::IconTheme &theme : themeRegistry.iconThemes()) {
        if (theme.isBuiltIn())
            nativeIcons.append(theme);
        else
            externalIcons.append(theme);
    }
    std::sort(externalIcons.begin(), externalIcons.end(), alphabetical);
    ui->iconThemeCombo->clear();
    for (const AppIcons::IconTheme &theme : nativeIcons)
        ui->iconThemeCombo->addItem(displayIconName(theme), theme.id());
    for (const AppIcons::IconTheme &theme : externalIcons)
        ui->iconThemeCombo->addItem(displayIconName(theme), theme.id());

    int index = ui->colorThemeCombo->findData(selectedColor);
    if (index >= 0)
        ui->colorThemeCombo->setCurrentIndex(index);
    index = ui->iconThemeCombo->findData(selectedIcon);
    if (index >= 0)
        ui->iconThemeCombo->setCurrentIndex(index);
}

void AppSettings::loadSettings()
{
    QSettings settings;

    auto &colors = AppColors::ColorThemeManager::instance();
    m_initialColorTheme = colors.themeId();
    const int colorThemeIndex = ui->colorThemeCombo->findData(m_initialColorTheme);
    ui->colorThemeCombo->setCurrentIndex(
        colorThemeIndex >= 0 ? colorThemeIndex : 0);

    auto &icons = AppIcons::IconThemeManager::instance();
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

    auto &colorManager = AppColors::ColorThemeManager::instance();
    colorManager.setThemeId(selectedColorTheme());
    settings.setValue(SettingsKeys::ColorTheme, colorManager.themeId());
    // Theme stylesheets are intentionally disabled while the QSS treatment is
    // being revised; palette-based colour themes remain active.
    settings.setValue(SettingsKeys::ApplyThemeStylesheet, false);
    const QString iconTheme = ui->iconThemeCombo->currentData().toString();
    auto &iconManager = AppIcons::IconThemeManager::instance();
    iconManager.setThemeId(iconTheme);
    settings.setValue(SettingsKeys::IconTheme, iconManager.themeId());
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

QString AppSettings::selectedColorTheme() const
{
    return ui->colorThemeCombo->currentData().toString();
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
