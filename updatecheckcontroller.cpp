#include "updatecheckcontroller.h"

#include "settingskeys.h"
#include "updatechecker.h"
#include "version.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QWidget>

namespace {
constexpr int AutomaticUpdateCheckIntervalSeconds = 24 * 60 * 60;
}

UpdateCheckController::UpdateCheckController(QWidget *parentWidget, QObject *parent)
    : QObject(parent),
      m_parentWidget(parentWidget)
{
}

void UpdateCheckController::setup()
{
    if (m_updateChecker)
        return;

    m_updateChecker = new UpdateChecker(this);
    m_updateChecker->setCurrentVersion(QStringLiteral(PLANETARY_VERSION_STRING));
    m_updateChecker->setRepository(QStringLiteral("mveinot"),
                                   QStringLiteral("transmission-control"));

    connect(m_updateChecker, &UpdateChecker::updateAvailable,
            this,
            [this](const QString &currentVersion,
                   const QString &latestVersion,
                   const QUrl &releaseUrl,
                   bool userInitiated) {
                Q_UNUSED(userInitiated)

                QMessageBox messageBox(m_parentWidget);
                messageBox.setIcon(QMessageBox::Information);
                messageBox.setWindowTitle(tr("Update Available"));
                messageBox.setText(tr("A newer version of Planetary is available."));
                messageBox.setInformativeText(
                    tr("Installed version: %1\nLatest version: %2")
                        .arg(displayVersion(currentVersion),
                             displayVersion(latestVersion))
                    );

                QPushButton *openButton =
                    messageBox.addButton(tr("Open Release Page"),
                                         QMessageBox::AcceptRole);

                messageBox.addButton(QMessageBox::Cancel);

                messageBox.exec();

                if (messageBox.clickedButton() == openButton)
                    QDesktopServices::openUrl(releaseUrl);
            });

    connect(m_updateChecker, &UpdateChecker::noUpdateAvailable,
            this,
            [this](const QString &currentVersion,
                   const QString &latestVersion,
                   const QUrl &releaseUrl,
                   bool userInitiated) {
                if (!userInitiated)
                    return;

                QMessageBox messageBox(m_parentWidget);
                messageBox.setIcon(QMessageBox::Information);
                messageBox.setWindowTitle(tr("Planetary Is Up to Date"));
                messageBox.setText(tr("You are running the latest available version of Planetary."));
                messageBox.setInformativeText(
                    tr("Installed version: %1\nLatest version: %2")
                        .arg(displayVersion(currentVersion),
                             displayVersion(latestVersion))
                    );

                QPushButton *openButton =
                    messageBox.addButton(tr("Open Release Page"),
                                         QMessageBox::ActionRole);

                messageBox.addButton(QMessageBox::Ok);

                messageBox.exec();

                if (messageBox.clickedButton() == openButton)
                    QDesktopServices::openUrl(releaseUrl);
            });

    connect(m_updateChecker, &UpdateChecker::updateCheckFailed,
            this,
            [this](const QString &message, bool userInitiated) {
                if (userInitiated) {
                    QMessageBox::warning(
                        m_parentWidget,
                        tr("Update Check Failed"),
                        tr("Planetary could not check for updates.\n\n%1").arg(message)
                        );
                    return;
                }

                emit statusMessageRequested(
                    tr("Update check failed: %1").arg(message),
                    5000
                    );
            });
}

void UpdateCheckController::checkNow()
{
    setup();

    if (m_updateChecker)
        m_updateChecker->checkForUpdates(true);
}

void UpdateCheckController::maybeCheckAutomatically()
{
    setup();

    QSettings settings;

    const bool enabled =
        settings.value(SettingsKeys::UpdateCheckAutomatically, true).toBool();

    if (!enabled)
        return;

    const QDateTime lastCheck =
        settings.value(SettingsKeys::UpdateLastCheck).toDateTime();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (lastCheck.isValid()
        && lastCheck.secsTo(now) < AutomaticUpdateCheckIntervalSeconds) {
        return;
    }

    /*
     * Only record the automatic check after the request is about to be sent.
     * This avoids updating the timestamp when auto-checking is disabled or
     * when setup has not produced a checker instance.
     */
    settings.setValue(SettingsKeys::UpdateLastCheck, now);

    if (m_updateChecker)
        m_updateChecker->checkForUpdates(false);
}

QString UpdateCheckController::displayVersion(QString version)
{
    version = version.trimmed();

    if (version.isEmpty())
        return QStringLiteral("Unknown");

    if (!version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        version.prepend(QLatin1Char('v'));

    return version;
}
