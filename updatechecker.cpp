#include "updatechecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

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

void UpdateChecker::setRepository(const QString &owner, const QString &repo)
{
    m_owner = owner.trimmed();
    m_repo = repo.trimmed();
}

void UpdateChecker::checkForUpdates(bool userInitiated)
{
    // The initiating mode travels with the reply so completion can suppress
    // routine automatic-check UI without changing the transport API.
    QNetworkRequest request(latestReleaseUrl());
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Planetary");

    QNetworkReply *reply = m_network->get(request);
    reply->setProperty("userInitiated", userInitiated);
}

QUrl UpdateChecker::latestReleaseUrl() const
{
    return QUrl(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
                    .arg(m_owner, m_repo));
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

    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll());

    if (!document.isObject()) {
        emit updateCheckFailed(tr("Invalid update response."), userInitiated);
        return;
    }

    const QJsonObject object = document.object();

    const QString tagName =
        object.value(QStringLiteral("tag_name")).toString().trimmed();

    const QString releaseUrlText =
        object.value(QStringLiteral("html_url")).toString().trimmed();

    if (tagName.isEmpty() || releaseUrlText.isEmpty()) {
        emit updateCheckFailed(tr("Update response did not include release information."),
                               userInitiated);
        return;
    }

    if (isVersionNewer(tagName, m_currentVersion)) {
        emit updateAvailable(m_currentVersion,
                             tagName,
                             QUrl(releaseUrlText),
                             userInitiated);
        return;
    }

    emit noUpdateAvailable(m_currentVersion,
                           tagName,
                           QUrl(releaseUrlText),
                           userInitiated);
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
