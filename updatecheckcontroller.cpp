#include "updatecheckcontroller.h"

#include "settingskeys.h"
#include "updatechecker.h"
#include "version.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QApplication>
#include <QProgressBar>
#include <QStyle>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

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
                   const QUrl &downloadUrl,
                   const QString &sha256,
                   const QUrl &releaseUrl,
                   const QString &releaseNotesMarkdown,
                   bool userInitiated) {
                Q_UNUSED(userInitiated)

                QDialog dialog(m_parentWidget);
                dialog.setWindowTitle(m_betaCheckInFlight
                                          ? tr("Beta Update Available")
                                          : tr("Update Available"));
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
                    (m_betaCheckInFlight
                         ? tr("A newer beta version of Planetary is available.")
                         : tr("A newer version of Planetary is available.")).toHtmlEscaped();
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
                auto *progress = new QProgressBar(&dialog);
                progress->setRange(0, 100);
                progress->setValue(0);
                progress->setTextVisible(true);
                progress->setVisible(false);
                layout->addWidget(progress);
                QPushButton *downloadButton =
                    buttons->addButton(tr("Download Release"),
                                       QDialogButtonBox::ActionRole);
                QPushButton *openButton =
                    buttons->addButton(tr("Open Release Page"),
                                       QDialogButtonBox::AcceptRole);
                openButton->setDefault(true);
                buttons->addButton(QDialogButtonBox::Close);
                auto *network = new QNetworkAccessManager(&dialog);
                connect(downloadButton, &QPushButton::clicked, &dialog,
                        [&, network, downloadUrl, sha256, downloadButton, progress]() {
                    downloadButton->setEnabled(false);
                    downloadButton->setText(tr("Downloading…"));
                    progress->setVisible(true);
                    progress->setRange(0, 0);
                    QString name = QFileInfo(downloadUrl.path()).fileName();
                    if (name.isEmpty())
                        name = QStringLiteral("Planetary-update.dmg");
                    QString downloads = QStandardPaths::writableLocation(
                        QStandardPaths::DownloadLocation);
                    if (downloads.isEmpty())
                        downloads = QDir::tempPath();
                    const QString path = QDir(downloads).filePath(name);
                    auto file = std::make_shared<QSaveFile>(path);
                    auto hash = std::make_shared<QCryptographicHash>(
                        QCryptographicHash::Sha256);
                    auto writeFailed = std::make_shared<bool>(false);
                    if (!file->open(QIODevice::WriteOnly)) {
                        progress->setVisible(false);
                        downloadButton->setEnabled(true);
                        downloadButton->setText(tr("Download Release"));
                        QMessageBox::warning(&dialog, tr("Download Failed"),
                                             tr("Planetary could not prepare the download file."));
                        return;
                    }
                    QNetworkRequest request(downloadUrl);
                    request.setRawHeader("User-Agent", "Planetary");
                    QNetworkReply *reply = network->get(request);
                    connect(reply, &QNetworkReply::downloadProgress, &dialog,
                            [progress](qint64 received, qint64 total) {
                        if (total > 0) {
                            progress->setRange(0, 100);
                            progress->setValue(static_cast<int>(
                                (received * 100) / total));
                        } else {
                            progress->setRange(0, 0);
                        }
                    });
                    connect(reply, &QNetworkReply::readyRead, &dialog,
                            [reply, file, hash, writeFailed]() {
                        const QByteArray chunk = reply->readAll();
                        if (!chunk.isEmpty()) {
                            hash->addData(chunk);
                            if (file->write(chunk) != chunk.size())
                                *writeFailed = true;
                        }
                    });
                    connect(reply, &QNetworkReply::finished, &dialog,
                            [&, reply, downloadUrl, sha256, downloadButton,
                             progress, file, hash, writeFailed, path]() {
                        reply->deleteLater();
                        downloadButton->setEnabled(true);
                        downloadButton->setText(tr("Download Release"));
                        progress->setVisible(false);
                        if (reply->error() != QNetworkReply::NoError) {
                            QMessageBox::warning(&dialog, tr("Download Failed"),
                                                 reply->errorString());
                            return;
                        }
                        const QByteArray finalChunk = reply->readAll();
                        if (!finalChunk.isEmpty()) {
                            hash->addData(finalChunk);
                            if (file->write(finalChunk) != finalChunk.size())
                                *writeFailed = true;
                        }
                        if (*writeFailed) {
                            QMessageBox::warning(&dialog, tr("Download Failed"),
                                                 tr("Planetary could not save the downloaded release."));
                            return;
                        }
                        const QString digest = QString::fromLatin1(hash->result().toHex());
                        if (digest.compare(sha256, Qt::CaseInsensitive) != 0) {
                            QMessageBox::critical(
                                &dialog, tr("Download Verification Failed"),
                                tr("The downloaded release did not match the published SHA-256 checksum."));
                            return;
                        }
                        if (!file->commit()) {
                            QMessageBox::warning(&dialog, tr("Download Failed"),
                                                 tr("Planetary could not save the downloaded release."));
                            return;
                        }
                        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                        const auto choice = QMessageBox::question(
                            &dialog, tr("Release Ready"),
                            tr("The release disk image is open. Quit Planetary now so you can replace the application?"),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                        if (choice == QMessageBox::Yes)
                            qApp->quit();
                    });
                });
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

void UpdateCheckController::checkNow(bool optionClick)
{
    setup();

    QSettings settings;
    bool beta = settings.value(SettingsKeys::UpdateBetaChannel, false).toBool();
    if (optionClick) {
        beta = !beta;
        settings.setValue(SettingsKeys::UpdateBetaChannel, beta);
    }
    m_betaCheckInFlight = beta;
    if (m_updateChecker)
        m_updateChecker->checkForUpdates(true, beta);
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

    m_betaCheckInFlight = QSettings().value(
        SettingsKeys::UpdateBetaChannel, false).toBool();
    if (m_updateChecker)
        m_updateChecker->checkForUpdates(false, m_betaCheckInFlight);
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
