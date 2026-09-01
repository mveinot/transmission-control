#include "updatecheckcontroller.h"

#include "settingskeys.h"
#include "updatechecker.h"
#include "version.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>
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

    connect(m_updateChecker, &UpdateChecker::updateAvailable,
            this,
            [this](const QString &currentVersion,
                   const QString &latestVersion,
                   const QUrl &releaseUrl,
                   const QString &releaseNotesMarkdown,
                   bool userInitiated) {
                Q_UNUSED(userInitiated)

                QDialog dialog(m_parentWidget);
                dialog.setWindowTitle(tr("Update Available"));
                dialog.resize(620, 480);

                auto *layout = new QVBoxLayout(&dialog);
                auto *summaryLayout = new QHBoxLayout;

                auto *iconLabel = new QLabel(&dialog);
                iconLabel->setPixmap(
                    dialog.style()
                        ->standardIcon(QStyle::SP_MessageBoxInformation)
                        .pixmap(48, 48));
                iconLabel->setAlignment(Qt::AlignTop);
                summaryLayout->addWidget(iconLabel);

                const QString summary =
                    tr("A newer version of Planetary is available.").toHtmlEscaped();
                QString versions =
                    tr("Installed version: %1\nLatest version: %2")
                        .arg(displayVersion(currentVersion),
                             displayVersion(latestVersion))
                        .toHtmlEscaped();
                versions.replace(QLatin1Char('\n'), QStringLiteral("<br>"));

                auto *summaryLabel = new QLabel(&dialog);
                summaryLabel->setWordWrap(true);
                summaryLabel->setText(
                    QStringLiteral("<b>%1</b><br><br>%2").arg(summary, versions));
                summaryLayout->addWidget(summaryLabel, 1);
                layout->addLayout(summaryLayout);

                auto *releaseNotesLabel = new QLabel(tr("Release Notes"), &dialog);
                QFont releaseNotesFont = releaseNotesLabel->font();
                releaseNotesFont.setBold(true);
                releaseNotesLabel->setFont(releaseNotesFont);
                layout->addWidget(releaseNotesLabel);

                auto *releaseNotesBrowser = new QTextBrowser(&dialog);
                releaseNotesBrowser->setReadOnly(true);
                releaseNotesBrowser->setOpenExternalLinks(true);
                releaseNotesLabel->setBuddy(releaseNotesBrowser);

                QString markdown = releaseNotesMarkdown;
                if (markdown.trimmed().isEmpty()) {
                    markdown = QStringLiteral("[%1](%2)")
                                   .arg(tr("Open Release Page"),
                                        releaseUrl.toString(QUrl::FullyEncoded));
                }
                releaseNotesBrowser->document()->setMarkdown(
                    markdown,
                    QTextDocument::MarkdownFeatures(
                        QTextDocument::MarkdownDialectGitHub)
                        | QTextDocument::MarkdownNoHTML);
                layout->addWidget(releaseNotesBrowser, 1);

                auto *buttons = new QDialogButtonBox(&dialog);
                QPushButton *openButton =
                    buttons->addButton(tr("Open Release Page"),
                                       QDialogButtonBox::AcceptRole);
                openButton->setDefault(true);
                buttons->addButton(QDialogButtonBox::Cancel);
                connect(buttons, &QDialogButtonBox::accepted,
                        &dialog, &QDialog::accept);
                connect(buttons, &QDialogButtonBox::rejected,
                        &dialog, &QDialog::reject);
                layout->addWidget(buttons);

                if (dialog.exec() == QDialog::Accepted)
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
