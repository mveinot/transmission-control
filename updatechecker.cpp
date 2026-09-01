#include "updatechecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace {
constexpr int SupportedManifestSchemaVersion = 1;

bool isHttpsUrl(const QUrl &url)
{
    return url.isValid()
        && url.scheme() == QStringLiteral("https")
        && !url.host().isEmpty();
}
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent),
    m_network(new QNetworkAccessManager(this))
{
    connect(m_network, &QNetworkAccessManager::finished,
            this, &UpdateChecker::handleReplyFinished);
}

void UpdateChecker::setCurrentVersion(const QString &version)
{
    m_currentVersion = version.trimmed();
}

void UpdateChecker::checkForUpdates(bool userInitiated)
{
    // The initiating mode travels with the reply so completion can suppress
    // routine automatic-check UI without changing the transport API.
    QNetworkRequest request(manifestUrl());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader("User-Agent", "Planetary");

    QNetworkReply *reply = m_network->get(request);
    reply->setProperty("userInitiated", userInitiated);
}

QUrl UpdateChecker::manifestUrl()
{
    return QUrl(QStringLiteral("https://planetary.mvgrafx.net/updates/v1/stable.json"));
}

void UpdateChecker::handleReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    const bool userInitiated =
        reply->property("userInitiated").toBool();

    if (reply->error() != QNetworkReply::NoError) {
        emit updateCheckFailed(reply->errorString(), userInitiated);
        return;
    }

    Manifest manifest;
    QString errorMessage;
    if (!parseManifest(reply->readAll(), &manifest, &errorMessage)) {
        emit updateCheckFailed(errorMessage, userInitiated);
        return;
    }

    if (isVersionNewer(manifest.displayVersion, m_currentVersion)) {
        emit updateAvailable(m_currentVersion,
                             manifest.displayVersion,
                             manifest.releaseNotesUrl,
                             manifest.releaseNotesMarkdown,
                             userInitiated);
        return;
    }

    emit noUpdateAvailable(m_currentVersion,
                           manifest.displayVersion,
                           manifest.releaseNotesUrl,
                           userInitiated);
}

bool UpdateChecker::parseManifest(const QByteArray &data,
                                  Manifest *manifest,
                                  QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    if (!manifest)
        return fail(tr("Invalid update response."));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(tr("Invalid update response."));

    const QJsonObject object = document.object();
    const int schemaVersion =
        object.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (schemaVersion != SupportedManifestSchemaVersion)
        return fail(tr("Invalid update response."));

    if (object.value(QStringLiteral("channel")).toString()
        != QStringLiteral("stable")) {
        return fail(tr("Invalid update response."));
    }

    const QString version =
        object.value(QStringLiteral("version")).toString().trimmed();
    static const QRegularExpression semanticVersionPattern(
        QStringLiteral("^\\d+\\.\\d+\\.\\d+$"));
    if (!semanticVersionPattern.match(version).hasMatch())
        return fail(tr("Update response did not include release information."));

    const int build = object.value(QStringLiteral("build")).toInt(-1);
    if (build < 0)
        return fail(tr("Update response did not include release information."));

    const QString displayVersion =
        object.value(QStringLiteral("displayVersion")).toString().trimmed();
    if (displayVersion != QStringLiteral("%1.%2").arg(version).arg(build))
        return fail(tr("Invalid update response."));

    const QString minimumMacOSVersion =
        object.value(QStringLiteral("minimumMacOSVersion")).toString().trimmed();
    static const QRegularExpression macOSVersionPattern(
        QStringLiteral("^\\d+\\.\\d+$"));
    if (!macOSVersionPattern.match(minimumMacOSVersion).hasMatch())
        return fail(tr("Invalid update response."));

    const QUrl downloadUrl(
        object.value(QStringLiteral("downloadUrl")).toString().trimmed());
    const QUrl releaseNotesUrl(
        object.value(QStringLiteral("releaseNotesUrl")).toString().trimmed());
    if (!isHttpsUrl(downloadUrl) || !isHttpsUrl(releaseNotesUrl))
        return fail(tr("Update response did not include release information."));

    const QJsonValue releaseNotesValue =
        object.value(QStringLiteral("releaseNotesMarkdown"));
    if (!releaseNotesValue.isUndefined() && !releaseNotesValue.isString())
        return fail(tr("Invalid update response."));

    const QString releaseNotesMarkdown = releaseNotesValue.toString();
    constexpr qsizetype MaximumReleaseNotesLength = 256 * 1024;
    if (releaseNotesMarkdown.size() > MaximumReleaseNotesLength)
        return fail(tr("Invalid update response."));

    const QString sha256 =
        object.value(QStringLiteral("sha256")).toString().trimmed().toLower();
    static const QRegularExpression sha256Pattern(
        QStringLiteral("^[0-9a-f]{64}$"));
    if (!sha256Pattern.match(sha256).hasMatch())
        return fail(tr("Invalid update response."));

    manifest->version = version;
    manifest->build = build;
    manifest->displayVersion = displayVersion;
    manifest->minimumMacOSVersion = minimumMacOSVersion;
    manifest->downloadUrl = downloadUrl;
    manifest->releaseNotesUrl = releaseNotesUrl;
    manifest->releaseNotesMarkdown = releaseNotesMarkdown;
    manifest->sha256 = sha256;

    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool UpdateChecker::isVersionNewer(const QString &latestVersion,
                                   const QString &currentVersion)
{
    const QList<int> latest = parseVersionParts(latestVersion);
    const QList<int> current = parseVersionParts(currentVersion);

    const int maxParts = qMax(latest.size(), current.size());

    for (int i = 0; i < maxParts; ++i) {
        const int latestPart = i < latest.size() ? latest.at(i) : 0;
        const int currentPart = i < current.size() ? current.at(i) : 0;

        if (latestPart > currentPart)
            return true;

        if (latestPart < currentPart)
            return false;
    }

    return false;
}

QList<int> UpdateChecker::parseVersionParts(const QString &version)
{
    QString cleaned = version.trimmed();

    if (cleaned.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        cleaned.remove(0, 1);

    QList<int> result;

    const QStringList parts = cleaned.split(QLatin1Char('.'),
                                            Qt::SkipEmptyParts);

    for (const QString &part : parts) {
        bool ok = false;
        const int value = part.toInt(&ok);

        if (!ok)
            break;

        result.append(value);
    }

    return result;
}
