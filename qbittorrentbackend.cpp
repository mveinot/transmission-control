#include "qbittorrentbackend.h"

#include "settingskeys.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

namespace {

int normalizedStatus(const QString &state) {
  const QString value = state.toLower();

  if (value.contains(QStringLiteral("checking")))
    return 2;
  if (value == QStringLiteral("queueddl"))
    return 3;
  if (value == QStringLiteral("queuedup"))
    return 5;
  if (value.contains(QStringLiteral("upload")) ||
      value == QStringLiteral("stalledup") ||
      value == QStringLiteral("forcedup")) {
    return 6;
  }
  if (value.contains(QStringLiteral("download")) ||
      value == QStringLiteral("metadl") ||
      value == QStringLiteral("stalleddl") ||
      value == QStringLiteral("forceddl") ||
      value == QStringLiteral("allocating")) {
    return 4;
  }

  return 0;
}

QStringList splitTags(const QString &tags) {
  QStringList result;
  for (const QString &tag : tags.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
    const QString trimmed = tag.trimmed();
    if (!trimmed.isEmpty())
      result.append(trimmed);
  }
  return result;
}

QByteArray formField(const QString &value) {
  // qBittorrent expects application/x-www-form-urlencoded data. QUrlQuery
  // follows URL-query rules and may leave a literal '+' ambiguous.
  return QUrl::toPercentEncoding(value);
}

} // namespace

QBittorrentBackend::QBittorrentBackend(QObject *parent)
    : TorrentBackend(parent) {
  connect(&m_network, &QNetworkAccessManager::finished, this,
          &QBittorrentBackend::handleReply);
}

QString QBittorrentBackend::backendName() const {
  return QStringLiteral("qBittorrent");
}
QString QBittorrentBackend::serverDisplayName() const {
  return !m_serverName.isEmpty() ? m_serverName : m_baseUrl;
}
QString QBittorrentBackend::endpointUrl() const { return m_baseUrl; }

TorrentBackendCapabilities QBittorrentBackend::capabilities() const {
  TorrentBackendCapabilities result;
  result.labels = true; // qBittorrent tags map to Planetary labels.
  result.groups = true; // qBittorrent categories map to Planetary groups.
  return result;
}

bool QBittorrentBackend::loadCurrentServerFromSettings() {
  QSettings settings;
  const int defaultIndex =
      settings.value(SettingsKeys::ServersDefaultIndex, -1).toInt();
  if (setServerFromSettingsIndex(defaultIndex))
    return true;

  const int currentIndex =
      settings.value(SettingsKeys::ServersCurrentIndex, -1).toInt();
  if (currentIndex != defaultIndex && setServerFromSettingsIndex(currentIndex))
    return true;

  return setServerFromSettingsIndex(0);
}

bool QBittorrentBackend::setServerFromSettingsIndex(int index) {
  QSettings settings;
  const int count = settings.beginReadArray(SettingsKeys::ServersArray);
  if (index < 0 || index >= count) {
    settings.endArray();
    return false;
  }

  settings.setArrayIndex(index);
  const QString type = settings
                           .value(SettingsKeys::ServerBackendType,
                                  QStringLiteral("transmission"))
                           .toString()
                           .trimmed()
                           .toLower();
  const QString name =
      settings.value(SettingsKeys::ServerName).toString().trimmed();
  const QString url =
      settings.value(SettingsKeys::ServerRpcUrl).toString().trimmed();
  const QString username =
      settings.value(SettingsKeys::ServerUsername).toString();
  const QString password =
      settings.value(SettingsKeys::ServerPassword).toString();
  settings.endArray();

  if (type != QStringLiteral("qbittorrent") || !QUrl(url).isValid())
    return false;

  setServer(name, url, username, password);
  return true;
}

void QBittorrentBackend::setServer(const QString &name, const QString &url,
                                   const QString &username,
                                   const QString &password) {
  abortRequests();
  m_serverName = name;
  m_baseUrl = url.trimmed();
  while (m_baseUrl.endsWith(QLatin1Char('/')))
    m_baseUrl.chop(1);
  m_username = username;
  m_password = password;
  m_authenticated = false;
  m_authenticationPending = false;
  m_listPendingAfterLogin = false;
  m_infoByKey.clear();
  emit serverChanged();
  emit capabilitiesChanged(capabilities());
}

void QBittorrentBackend::init() { authenticate(); }

QNetworkRequest QBittorrentBackend::makeRequest(const QString &path,
                                                const QUrlQuery &query) const {
  QUrl url(m_baseUrl + path);
  url.setQuery(query);
  QNetworkRequest request(url);

  // qBittorrent's CSRF validation expects Origin/Referer to match the
  // externally-visible scheme, host, and port.
  QUrl origin(m_baseUrl);
  origin.setPath(QString());
  origin.setQuery(QString());
  origin.setFragment(QString());
  const QByteArray originValue = origin.toString(QUrl::RemovePath).toUtf8();
  request.setRawHeader("Origin", originValue);
  request.setRawHeader("Referer", originValue + '/');
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QStringLiteral("Planetary"));
  return request;
}

void QBittorrentBackend::authenticate() {
  if (m_baseUrl.isEmpty() || m_authenticationPending)
    return;

  m_authenticationPending = true;
  QNetworkRequest request = makeRequest(QStringLiteral("/api/v2/auth/login"));
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/x-www-form-urlencoded"));
  const QByteArray form =
      QByteArrayLiteral("username=") + formField(m_username) +
      QByteArrayLiteral("&password=") + formField(m_password);
  QNetworkReply *reply = m_network.post(request, form);
  m_requests.insert(reply, {RequestKind::Login, {}});
}

void QBittorrentBackend::sendGet(RequestKind kind, const QString &path,
                                 const QUrlQuery &query,
                                 const TorrentKey &key) {
  QNetworkReply *reply = m_network.get(makeRequest(path, query));
  m_requests.insert(reply, {kind, key});
}

void QBittorrentBackend::getTorrentList() {
  if (!m_authenticated) {
    m_listPendingAfterLogin = true;
    authenticate();
    return;
  }

  emit updateStarted();
  sendGet(RequestKind::TorrentList, QStringLiteral("/api/v2/torrents/info"));
}

void QBittorrentBackend::getTorrentDetails(const TorrentKey &key) {
  if (!isValidTorrentKey(key) || !m_authenticated)
    return;

  QUrlQuery infoQuery;
  infoQuery.addQueryItem(QStringLiteral("hashes"), key);
  sendGet(RequestKind::TorrentInfo, QStringLiteral("/api/v2/torrents/info"),
          infoQuery, key);

  QUrlQuery propertiesQuery;
  propertiesQuery.addQueryItem(QStringLiteral("hash"), key);
  sendGet(RequestKind::TorrentProperties,
          QStringLiteral("/api/v2/torrents/properties"), propertiesQuery, key);
}

QJsonObject QBittorrentBackend::normalizeTorrent(const QJsonObject &source) {
  QJsonObject result;
  const QString hash = source.value(QStringLiteral("hash")).toString();
  const QString state = source.value(QStringLiteral("state")).toString();
  const qint64 totalSize =
      source.value(QStringLiteral("total_size")).toVariant().toLongLong();

  result.insert(QStringLiteral("hashString"), hash);
  result.insert(QStringLiteral("name"), source.value(QStringLiteral("name")));
  result.insert(QStringLiteral("status"), normalizedStatus(state));
  result.insert(QStringLiteral("percentDone"),
                source.value(QStringLiteral("progress")));
  result.insert(QStringLiteral("eta"), source.value(QStringLiteral("eta")));
  result.insert(QStringLiteral("rateDownload"),
                source.value(QStringLiteral("dlspeed")));
  result.insert(QStringLiteral("rateUpload"),
                source.value(QStringLiteral("upspeed")));
  result.insert(QStringLiteral("uploadRatio"),
                source.value(QStringLiteral("ratio")));
  result.insert(QStringLiteral("sizeWhenDone"), totalSize);
  result.insert(QStringLiteral("totalSize"), totalSize);
  result.insert(QStringLiteral("addedDate"),
                source.value(QStringLiteral("added_on")));
  result.insert(QStringLiteral("downloadedEver"),
                source.value(QStringLiteral("downloaded")));
  result.insert(QStringLiteral("uploadedEver"),
                source.value(QStringLiteral("uploaded")));
  result.insert(QStringLiteral("downloadDir"),
                source.value(QStringLiteral("save_path")));
  result.insert(
      QStringLiteral("isStalled"),
      state.startsWith(QStringLiteral("stalled"), Qt::CaseInsensitive));
  result.insert(QStringLiteral("peersConnected"),
                source.value(QStringLiteral("num_leechs")).toInt() +
                    source.value(QStringLiteral("num_seeds")).toInt());
  result.insert(QStringLiteral("peersSendingToUs"),
                source.value(QStringLiteral("num_seeds")));
  result.insert(QStringLiteral("peersGettingFromUs"),
                source.value(QStringLiteral("num_leechs")));
  result.insert(QStringLiteral("queuePosition"),
                source.value(QStringLiteral("priority")));
  result.insert(
      QStringLiteral("leftUntilDone"),
      qMax<qint64>(0, totalSize - source.value(QStringLiteral("completed"))
                                      .toVariant()
                                      .toLongLong()));
  result.insert(QStringLiteral("desiredAvailable"),
                result.value(QStringLiteral("leftUntilDone")));
  result.insert(QStringLiteral("group"),
                source.value(QStringLiteral("category")));

  QJsonArray labels;
  for (const QString &tag :
       splitTags(source.value(QStringLiteral("tags")).toString())) {
    labels.append(tag);
  }
  result.insert(QStringLiteral("labels"), labels);

  const QString trackerUrl = source.value(QStringLiteral("tracker")).toString();
  if (!trackerUrl.isEmpty()) {
    result.insert(
        QStringLiteral("trackers"),
        QJsonArray{QJsonObject{{QStringLiteral("announce"), trackerUrl}}});
  }

  if (state == QStringLiteral("error") ||
      state == QStringLiteral("missingFiles")) {
    result.insert(QStringLiteral("error"), 1);
    result.insert(QStringLiteral("errorString"), state);
  }

  return result;
}

TorrentDetails
QBittorrentBackend::normalizeDetails(const QVariantMap &info,
                                     const QVariantMap &properties) {
  QJsonObject normalized = normalizeTorrent(QJsonObject::fromVariantMap(info));
  normalized.insert(QStringLiteral("comment"),
                    properties.value(QStringLiteral("comment")).toString());
  normalized.insert(
      QStringLiteral("dateCreated"),
      properties.value(QStringLiteral("creation_date")).toLongLong());
  normalized.insert(
      QStringLiteral("pieceSize"),
      properties.value(QStringLiteral("piece_size")).toLongLong());
  normalized.insert(QStringLiteral("pieceCount"),
                    properties.value(QStringLiteral("pieces_num")).toInt());
  normalized.insert(QStringLiteral("magnetLink"),
                    info.value(QStringLiteral("magnet_uri")).toString());

  TorrentDetails details;
  details.key = normalized.value(QStringLiteral("hashString")).toString();
  details.name = normalized.value(QStringLiteral("name")).toString();
  details.comment = normalized.value(QStringLiteral("comment")).toString();
  details.downloadDirectory =
      normalized.value(QStringLiteral("downloadDir")).toString();
  details.hashString = details.key;
  details.magnetLink =
      normalized.value(QStringLiteral("magnetLink")).toString();
  details.totalSize =
      normalized.value(QStringLiteral("totalSize")).toVariant().toLongLong();
  details.creationTime =
      normalized.value(QStringLiteral("dateCreated")).toVariant().toLongLong();
  details.fields = normalized.toVariantMap();
  return details;
}

void QBittorrentBackend::handleReply(QNetworkReply *reply) {
  const RequestContext context = m_requests.take(reply);
  const QByteArray body = reply->readAll();
  const auto error = reply->error();
  const QString networkError = reply->errorString();
  const int status =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  reply->deleteLater();

  if (context.kind == RequestKind::Login) {
    m_authenticationPending = false;
    const QByteArray responseText = body.trimmed();
    // Current qBittorrent builds may represent a successful no-content WebAPI
    // response as HTTP 204; older releases return HTTP 200 with "Ok.".
    const bool successfulResponse =
        status == 204 ||
        (status == 200 &&
         responseText.compare(QByteArrayLiteral("Ok."),
                              Qt::CaseInsensitive) == 0);
    m_authenticated =
        error == QNetworkReply::NoError && successfulResponse;

    if (!m_authenticated) {
      QString reason;

      if (error != QNetworkReply::NoError) {
        reason = networkError;
      } else if (status == 403) {
        reason = tr("access was forbidden; qBittorrent may have banned this "
                    "client IP after repeated login failures");
      } else if (status != 200) {
        reason = tr("the server returned HTTP %1").arg(status);
      } else if (responseText.compare(QByteArrayLiteral("Fails."),
                                      Qt::CaseInsensitive) == 0) {
        reason = tr("the server rejected the username or password");
      } else {
        reason = tr("unexpected login response: %1")
                     .arg(QString::fromUtf8(responseText.left(160)));
      }

      emit updateFailed(
          tr("qBittorrent authentication failed: %1").arg(reason));
      return;
    }

    if (m_listPendingAfterLogin) {
      m_listPendingAfterLogin = false;
      getTorrentList();
    }
    return;
  }

  if (error != QNetworkReply::NoError) {
    if (context.kind == RequestKind::TorrentList) {
      emit updateFailed(tr("qBittorrent request failed: %1").arg(networkError));
      emit updateFinished();
    }
    return;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
  if (parseError.error != QJsonParseError::NoError)
    return;

  if (context.kind == RequestKind::TorrentList) {
    QVector<torrent> torrents;
    const QJsonArray array = document.array();
    torrents.reserve(array.size());
    m_infoByKey.clear();
    for (const QJsonValue &value : array) {
      const QJsonObject source = value.toObject();
      const QString key = source.value(QStringLiteral("hash")).toString();
      if (key.isEmpty())
        continue;
      m_infoByKey.insert(key, source.toVariantMap());
      torrents.append(torrent(normalizeTorrent(source)));
    }
    emit torrentsReceived(torrents);
    emit updateFinished();
    return;
  }

  if (context.kind == RequestKind::TorrentInfo) {
    const QJsonArray array = document.array();
    if (!array.isEmpty())
      m_infoByKey.insert(context.key, array.first().toObject().toVariantMap());
  } else if (context.kind == RequestKind::TorrentProperties) {
    const QVariantMap properties = document.object().toVariantMap();
    const QVariantMap info = m_infoByKey.value(context.key);
    if (!info.isEmpty())
      emit torrentDetailsReceived(normalizeDetails(info, properties));
  }
}

void QBittorrentBackend::abortRequests() {
  const QList<QNetworkReply *> replies = m_requests.keys();
  m_requests.clear();
  for (QNetworkReply *reply : replies) {
    reply->abort();
    reply->deleteLater();
  }
}

void QBittorrentBackend::cancelTorrentDetailRequests() {
  const QList<QNetworkReply *> replies = m_requests.keys();
  for (QNetworkReply *reply : replies) {
    const RequestKind kind = m_requests.value(reply).kind;
    if (kind == RequestKind::TorrentInfo ||
        kind == RequestKind::TorrentProperties) {
      m_requests.remove(reply);
      reply->abort();
      reply->deleteLater();
    }
  }
}

void QBittorrentBackend::emitUnsupported(const QString &operation) {
  emit commandFailed(
      operation,
      tr("%1 is not implemented for qBittorrent yet.").arg(operation));
}

void QBittorrentBackend::getTorrentTrackerMetadata() {}
void QBittorrentBackend::getTorrentFiles(const TorrentKey &) {}
void QBittorrentBackend::getTorrentPeers(const TorrentKey &) {}
void QBittorrentBackend::getTorrentTrackers(const TorrentKey &) {}
void QBittorrentBackend::getTorrentPieces(const TorrentKey &) {}
void QBittorrentBackend::getTorrentProperties(const TorrentKey &) {}
void QBittorrentBackend::addTorrentFromFile(const QString &, bool) {
  emitUnsupported(tr("Add torrent"));
}
void QBittorrentBackend::addTorrentFromMagnet(const QString &) {
  emitUnsupported(tr("Add torrent"));
}
void QBittorrentBackend::addTorrentFile(const QString &, const QString &, bool,
                                        const QList<int> &, const QList<int> &,
                                        const QList<int> &, bool) {
  emitUnsupported(tr("Add torrent"));
}
void QBittorrentBackend::addMagnetLink(const QString &, const QString &, bool) {
  emitUnsupported(tr("Add torrent"));
}
void QBittorrentBackend::startTorrents(const QList<TorrentKey> &) {
  emitUnsupported(tr("Start torrents"));
}
void QBittorrentBackend::startAllTorrents() {
  emitUnsupported(tr("Start torrents"));
}
void QBittorrentBackend::startTorrentsNow(const QList<TorrentKey> &) {
  emitUnsupported(tr("Force start"));
}
void QBittorrentBackend::stopTorrents(const QList<TorrentKey> &) {
  emitUnsupported(tr("Stop torrents"));
}
void QBittorrentBackend::stopAllTorrents() {
  emitUnsupported(tr("Stop torrents"));
}
void QBittorrentBackend::removeTorrents(const QList<TorrentKey> &, bool) {
  emitUnsupported(tr("Remove torrents"));
}
void QBittorrentBackend::verifyTorrents(const QList<TorrentKey> &) {
  emitUnsupported(tr("Verify torrents"));
}
void QBittorrentBackend::reannounceTorrents(const QList<TorrentKey> &) {
  emitUnsupported(tr("Reannounce torrents"));
}
void QBittorrentBackend::setTorrentLocation(const QList<TorrentKey> &,
                                            const QString &, bool) {
  emitUnsupported(tr("Set location"));
}
void QBittorrentBackend::setTorrentFilesWanted(const TorrentKey &,
                                               const QList<int> &, bool) {
  emitUnsupported(tr("Set files"));
}
void QBittorrentBackend::setTorrentFilesPriority(const TorrentKey &,
                                                 const QList<int> &, int) {
  emitUnsupported(tr("Set file priority"));
}
void QBittorrentBackend::setTorrentFilesWantedAndPriority(const TorrentKey &,
                                                          const QList<int> &,
                                                          bool, int) {
  emitUnsupported(tr("Set files"));
}
void QBittorrentBackend::addTorrentTracker(const TorrentKey &,
                                           const QString &) {
  emitUnsupported(tr("Add tracker"));
}
void QBittorrentBackend::editTorrentTracker(const TorrentKey &, int,
                                            const QString &) {
  emitUnsupported(tr("Edit tracker"));
}
void QBittorrentBackend::removeTorrentTracker(const TorrentKey &, int) {
  emitUnsupported(tr("Remove tracker"));
}
void QBittorrentBackend::renameTorrentPath(const TorrentKey &, const QString &,
                                           const QString &) {
  emitUnsupported(tr("Rename path"));
}
void QBittorrentBackend::setTorrentProperties(const TorrentKey &,
                                              const TorrentPropertyChanges &) {
  emitUnsupported(tr("Set properties"));
}
void QBittorrentBackend::setTorrentsSequentialDownload(
    const QList<TorrentKey> &, bool) {
  emitUnsupported(tr("Sequential download"));
}
void QBittorrentBackend::setTorrentsBandwidthPriority(const QList<TorrentKey> &,
                                                      int) {
  emitUnsupported(tr("Set priority"));
}
void QBittorrentBackend::queueMoveTop(const QList<TorrentKey> &) {
  emitUnsupported(tr("Move queue"));
}
void QBittorrentBackend::queueMoveUp(const QList<TorrentKey> &) {
  emitUnsupported(tr("Move queue"));
}
void QBittorrentBackend::queueMoveDown(const QList<TorrentKey> &) {
  emitUnsupported(tr("Move queue"));
}
void QBittorrentBackend::queueMoveBottom(const QList<TorrentKey> &) {
  emitUnsupported(tr("Move queue"));
}
void QBittorrentBackend::getSessionSettings() {}
void QBittorrentBackend::getSessionStatistics() {}
void QBittorrentBackend::setSessionSettings(const QJsonObject &) {
  emitUnsupported(tr("Session settings"));
}
void QBittorrentBackend::getFreeSpace(const QString &) {}
void QBittorrentBackend::testPortForwarding() {}
void QBittorrentBackend::updateBlocklist(const QJsonObject &) {
  emitUnsupported(tr("Update blocklist"));
}
