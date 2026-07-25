#include "qbittorrentbackend.h"

#include "settingskeys.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

#include <utility>

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

QByteArray hashesForm(const QList<TorrentKey> &keys) {
  QStringList values;
  values.reserve(keys.size());
  for (const TorrentKey &key : keys)
    values.append(key);

  return QByteArrayLiteral("hashes=") +
         formField(values.join(QLatin1Char('|')));
}

QByteArray filePriorityForm(const TorrentKey &key,
                            const QList<int> &indices,
                            int nativePriority) {
  QStringList ids;
  ids.reserve(indices.size());
  for (const int index : indices)
    ids.append(QString::number(index));

  return QByteArrayLiteral("hash=") + formField(key) +
         QByteArrayLiteral("&id=") +
         formField(ids.join(QLatin1Char('|'))) +
         QByteArrayLiteral("&priority=") +
         QByteArray::number(nativePriority);
}

void appendAddStateFields(QByteArray &form, bool startPaused) {
  const QByteArray value =
      startPaused ? QByteArrayLiteral("true") : QByteArrayLiteral("false");
  // qBittorrent 5.x renamed the add option from "paused" to "stopped".
  // Supplying both retains compatibility with older WebAPI implementations;
  // unknown form fields are ignored by the endpoint.
  form += QByteArrayLiteral("&paused=") + value;
  form += QByteArrayLiteral("&stopped=") + value;
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
  result.torrentProperties = true;
  result.torrentSpeedLimits = true;
  result.torrentShareLimits = true;
  result.filePriorities = true;
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
  m_sessionSettingsPendingAfterLogin = false;
  m_commandsPendingAfterLogin.clear();
  m_addsPendingAfterLogin.clear();
  m_infoByKey.clear();
  m_editorPropertiesByKey.clear();
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
  if (kind == RequestKind::TorrentInfo ||
      kind == RequestKind::TorrentProperties ||
      kind == RequestKind::TorrentPieces ||
      kind == RequestKind::TorrentFiles ||
      kind == RequestKind::TorrentPeers ||
      kind == RequestKind::TorrentTrackers ||
      kind == RequestKind::TorrentPropertyEditor) {
    // Live detail polling may tick again before a remote daemon responds.
    // Retain one in-flight projection of each type for the selected torrent.
    for (auto it = m_requests.cbegin(); it != m_requests.cend(); ++it) {
      if (it.value().kind == kind && it.value().key == key)
        return;
    }
  }

  QNetworkReply *reply = m_network.get(makeRequest(path, query));
  m_requests.insert(reply, {kind, key});
}

void QBittorrentBackend::postCommand(const QString &path,
                                     const QByteArray &form,
                                     const QString &method,
                                     const QString &fallbackPath) {
  RequestContext context;
  context.kind = RequestKind::Command;
  context.commandMethod = method;
  context.path = path;
  context.fallbackPath = fallbackPath;
  context.form = form;

  if (!m_authenticated) {
    m_commandsPendingAfterLogin.append(context);
    authenticate();
    return;
  }

  sendCommand(context);
}

void QBittorrentBackend::sendCommand(const RequestContext &context) {
  QNetworkRequest request = makeRequest(context.path);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/x-www-form-urlencoded"));
  QNetworkReply *reply = m_network.post(request, context.form);
  m_requests.insert(reply, context);
}

void QBittorrentBackend::queueOrSendAdd(const RequestContext &context) {
  if (!m_authenticated) {
    m_addsPendingAfterLogin.append(context);
    authenticate();
    return;
  }

  sendAdd(context);
}

void QBittorrentBackend::sendAdd(const RequestContext &context) {
  QNetworkRequest request =
      makeRequest(QStringLiteral("/api/v2/torrents/add"));

  if (!context.magnetLink.isEmpty()) {
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QByteArray form =
        QByteArrayLiteral("urls=") + formField(context.magnetLink);
    if (!context.downloadDir.isEmpty())
      form += QByteArrayLiteral("&savepath=") + formField(context.downloadDir);
    appendAddStateFields(form, context.startPaused);

    QNetworkReply *reply = m_network.post(request, form);
    m_requests.insert(reply, context);
    return;
  }

  auto *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
  const QFileInfo fileInfo(context.torrentFilePath);
  QString uploadFileName = fileInfo.fileName();
  uploadFileName.replace(QLatin1Char('\\'), QLatin1Char('_'));
  uploadFileName.replace(QLatin1Char('"'), QLatin1Char('_'));
  auto *torrentFile = new QFile(context.torrentFilePath, multipart);
  if (!torrentFile->open(QIODevice::ReadOnly)) {
    const QString reason =
        tr("Could not open torrent file: %1").arg(torrentFile->errorString());
    delete multipart;
    failAdd(context, reason);
    return;
  }

  QHttpPart torrentPart;
  torrentPart.setHeader(
      QNetworkRequest::ContentDispositionHeader,
      QStringLiteral("form-data; name=\"torrents\"; filename=\"%1\"")
          .arg(uploadFileName));
  torrentPart.setHeader(QNetworkRequest::ContentTypeHeader,
                        QStringLiteral("application/x-bittorrent"));
  torrentPart.setBodyDevice(torrentFile);
  multipart->append(torrentPart);

  const auto appendTextPart = [multipart](const QByteArray &name,
                                          const QByteArray &value) {
    QHttpPart part;
    part.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QStringLiteral("form-data; name=\"%1\"").arg(QString::fromLatin1(name)));
    part.setBody(value);
    multipart->append(part);
  };

  if (!context.downloadDir.isEmpty())
    appendTextPart(QByteArrayLiteral("savepath"),
                   context.downloadDir.toUtf8());
  const QByteArray stateValue =
      context.startPaused ? QByteArrayLiteral("true")
                          : QByteArrayLiteral("false");
  appendTextPart(QByteArrayLiteral("paused"), stateValue);
  appendTextPart(QByteArrayLiteral("stopped"), stateValue);

  QNetworkReply *reply = m_network.post(request, multipart);
  // QNetworkAccessManager does not assume ownership of multipart bodies. Keep
  // the upload and its QFile alive until the corresponding reply is destroyed.
  multipart->setParent(reply);
  m_requests.insert(reply, context);
}

void QBittorrentBackend::failAdd(const RequestContext &context,
                                 const QString &reason) {
  if (!context.torrentFilePath.isEmpty())
    emit torrentFileAddFailed(context.torrentFilePath, reason);
  emit commandFailed(QStringLiteral("torrent-add"), reason);
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

  // qBittorrent distinguishes currently connected peers from the swarm totals
  // reported by trackers. Torrent's shared seed/peer summaries derive their
  // denominator from normalized tracker statistics.
  QJsonObject trackerStats{
      {QStringLiteral("announce"), trackerUrl},
      {QStringLiteral("seederCount"),
       source.value(QStringLiteral("num_complete")).toInt(-1)},
      {QStringLiteral("leecherCount"),
       source.value(QStringLiteral("num_incomplete")).toInt(-1)}};
  result.insert(QStringLiteral("trackerStats"), QJsonArray{trackerStats});

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
  const auto copyProperty = [&normalized, &properties](
                                const char *target, const char *source) {
    const QString sourceKey = QString::fromLatin1(source);
    if (properties.contains(sourceKey))
      normalized.insert(QString::fromLatin1(target),
                        QJsonValue::fromVariant(properties.value(sourceKey)));
  };

  // Normalize qBittorrent's properties resource to the established detail
  // vocabulary so the General and technical Details tabs remain backend-free.
  copyProperty("name", "name");
  copyProperty("comment", "comment");
  copyProperty("creator", "created_by");
  copyProperty("dateCreated", "creation_date");
  copyProperty("addedDate", "addition_date");
  copyProperty("doneDate", "completion_date");
  copyProperty("activityDate", "last_seen");
  copyProperty("downloadDir", "save_path");
  copyProperty("totalSize", "total_size");
  copyProperty("sizeWhenDone", "total_size");
  copyProperty("pieceSize", "piece_size");
  copyProperty("pieceCount", "pieces_num");
  copyProperty("downloadedEver", "total_downloaded");
  copyProperty("uploadedEver", "total_uploaded");
  copyProperty("corruptEver", "total_wasted");
  copyProperty("rateDownload", "dl_speed");
  copyProperty("rateUpload", "up_speed");
  copyProperty("uploadRatio", "share_ratio");
  copyProperty("eta", "eta");
  copyProperty("secondsDownloading", "time_elapsed");
  copyProperty("secondsSeeding", "seeding_time");
  copyProperty("peersConnected", "nb_connections");
  copyProperty("maxConnectedPeers", "nb_connections_limit");
  copyProperty("peersSendingToUs", "seeds");
  copyProperty("peersGettingFromUs", "peers");
  copyProperty("isPrivate", "private");
  copyProperty("percentDone", "progress");

  const qint64 totalSize = properties.value(QStringLiteral("total_size"))
                               .toLongLong();
  const qint64 downloaded =
      properties.value(QStringLiteral("total_downloaded")).toLongLong();
  normalized.insert(QStringLiteral("haveValid"),
                    qMin(totalSize, downloaded));
  normalized.insert(QStringLiteral("leftUntilDone"),
                    qMax<qint64>(0, totalSize - downloaded));
  normalized.insert(QStringLiteral("isFinished"),
                    properties.value(QStringLiteral("progress")).toDouble() >=
                        1.0);

  const qint64 downloadLimit =
      properties.value(QStringLiteral("dl_limit")).toLongLong();
  const qint64 uploadLimit =
      properties.value(QStringLiteral("up_limit")).toLongLong();
  // The shared detail view stores limits in kB/s to match Transmission's RPC.
  normalized.insert(QStringLiteral("downloadLimited"), downloadLimit > 0);
  normalized.insert(QStringLiteral("downloadLimit"),
                    downloadLimit > 0 ? downloadLimit / 1000 : 0);
  normalized.insert(QStringLiteral("uploadLimited"), uploadLimit > 0);
  normalized.insert(QStringLiteral("uploadLimit"),
                    uploadLimit > 0 ? uploadLimit / 1000 : 0);
  normalized.insert(QStringLiteral("magnetLink"),
                    info.value(QStringLiteral("magnet_uri")).toString());

  TorrentDetails details;
  details.key = normalized.value(QStringLiteral("hashString")).toString();
  details.name = normalized.value(QStringLiteral("name")).toString();
  details.comment = normalized.value(QStringLiteral("comment")).toString();
  details.creator = normalized.value(QStringLiteral("creator")).toString();
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
        (status == 200 && responseText.compare(QByteArrayLiteral("Ok."),
                                               Qt::CaseInsensitive) == 0);
    m_authenticated = error == QNetworkReply::NoError && successfulResponse;

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
      for (const RequestContext &pending : m_commandsPendingAfterLogin) {
        emit commandFailed(pending.commandMethod, reason);
      }
      m_commandsPendingAfterLogin.clear();
      for (const RequestContext &pending : m_addsPendingAfterLogin)
        failAdd(pending, reason);
      m_addsPendingAfterLogin.clear();
      m_sessionSettingsPendingAfterLogin = false;
      return;
    }

    if (m_listPendingAfterLogin) {
      m_listPendingAfterLogin = false;
      getTorrentList();
    }

    if (m_sessionSettingsPendingAfterLogin) {
      m_sessionSettingsPendingAfterLogin = false;
      getSessionSettings();
    }

    const QList<RequestContext> pendingCommands =
        std::exchange(m_commandsPendingAfterLogin, {});
    for (const RequestContext &pending : pendingCommands)
      sendCommand(pending);

    const QList<RequestContext> pendingAdds =
        std::exchange(m_addsPendingAfterLogin, {});
    for (const RequestContext &pending : pendingAdds)
      sendAdd(pending);
    return;
  }

  if (context.kind == RequestKind::AddTorrent) {
    const QByteArray responseText = body.trimmed();
    const bool explicitlyRejected =
        responseText.compare(QByteArrayLiteral("Fails."),
                             Qt::CaseInsensitive) == 0;
    if (error == QNetworkReply::NoError && (status == 200 || status == 204) &&
        !explicitlyRejected) {
      if (!context.torrentFilePath.isEmpty()) {
        if (context.deleteTorrentFileOnSuccess)
          QFile::remove(context.torrentFilePath);
        emit torrentFileAddSucceeded(context.torrentFilePath);
      }
      emit commandSucceeded(QStringLiteral("torrent-add"));
      return;
    }

    if (status == 403 && !context.retriedAuthentication) {
      RequestContext retry = context;
      retry.retriedAuthentication = true;
      m_authenticated = false;
      m_addsPendingAfterLogin.append(retry);
      authenticate();
      return;
    }

    QString reason;
    if (explicitlyRejected) {
      reason = tr("qBittorrent rejected the torrent.");
    } else if (status == 415) {
      reason = tr("qBittorrent reported that the torrent file is invalid.");
    } else if (status > 0) {
      reason = tr("qBittorrent returned HTTP %1").arg(status);
    } else {
      reason = networkError;
    }
    failAdd(context, reason);
    return;
  }

  if (context.kind == RequestKind::Command) {
    if (error == QNetworkReply::NoError && (status == 200 || status == 204)) {
      emit commandSucceeded(context.commandMethod);
      return;
    }

    if (status == 404 && !context.fallbackPath.isEmpty() &&
        !context.usedFallback) {
      RequestContext fallback = context;
      fallback.path = context.fallbackPath;
      fallback.usedFallback = true;
      sendCommand(fallback);
      return;
    }

    if (status == 403 && !context.retriedAuthentication) {
      RequestContext retry = context;
      retry.retriedAuthentication = true;
      m_authenticated = false;
      m_commandsPendingAfterLogin.append(retry);
      authenticate();
      return;
    }

    const QString reason = status > 0
                               ? tr("qBittorrent returned HTTP %1").arg(status)
                               : networkError;
    emit commandFailed(context.commandMethod, reason);
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
  } else if (context.kind == RequestKind::TorrentPieces) {
    const QJsonArray states = document.array();
    TorrentPieces pieces;
    pieces.key = context.key;
    pieces.pieceCount = states.size();
    pieces.completedPieces =
        QByteArray((pieces.pieceCount + 7) / 8, '\0');

    int completedCount = 0;
    for (int index = 0; index < states.size(); ++index) {
      // qBittorrent states are 0=missing, 1=downloading, 2=complete. Planetary
      // uses Transmission's compact, most-significant-bit-first bitfield.
      if (states.at(index).toInt() != 2)
        continue;
      ++completedCount;
      const int byteIndex = index / 8;
      const int bitIndex = 7 - (index % 8);
      pieces.completedPieces[byteIndex] =
          static_cast<char>(
              static_cast<uchar>(pieces.completedPieces.at(byteIndex)) |
              static_cast<uchar>(1u << bitIndex));
    }

    const QVariantMap info = m_infoByKey.value(context.key);
    pieces.percentDone =
        info.value(QStringLiteral("progress")).toDouble();
    if (!info.contains(QStringLiteral("progress")) && pieces.pieceCount > 0)
      pieces.percentDone =
          static_cast<double>(completedCount) / pieces.pieceCount;
    emit torrentPiecesReceived(pieces);
  } else if (context.kind == RequestKind::TorrentFiles) {
    const QJsonArray sourceFiles = document.array();
    TorrentFiles files;
    files.key = context.key;
    files.downloadDirectory =
        m_infoByKey.value(context.key)
            .value(QStringLiteral("save_path")).toString();
    files.files.reserve(sourceFiles.size());

    for (const QJsonValue &value : sourceFiles) {
      const QJsonObject source = value.toObject();
      TorrentFile file;
      file.index = source.value(QStringLiteral("index")).toInt(
          files.files.size());
      file.path = source.value(QStringLiteral("name")).toString();
      file.length =
          source.value(QStringLiteral("size")).toVariant().toLongLong();
      file.bytesCompleted = qRound64(
          file.length * source.value(QStringLiteral("progress")).toDouble());

      const int nativePriority =
          source.value(QStringLiteral("priority")).toInt();
      file.wanted = nativePriority != 0;
      // qBittorrent priorities are 0=skip, 1=normal, 6=high, 7=maximal.
      file.priority = nativePriority >= 6 ? 1 : 0;
      files.files.append(file);
    }
    emit torrentFilesReceived(files);
  } else if (context.kind == RequestKind::TorrentPeers) {
    TorrentPeers peers;
    peers.key = context.key;
    const QJsonObject sourcePeers =
        document.object().value(QStringLiteral("peers")).toObject();
    peers.peers.reserve(sourcePeers.size());

    for (auto it = sourcePeers.constBegin(); it != sourcePeers.constEnd(); ++it) {
      const QJsonObject source = it.value().toObject();
      TorrentPeer peer;
      peer.address = source.value(QStringLiteral("ip")).toString();
      if (peer.address.isEmpty())
        peer.address = source.value(QStringLiteral("i2p_dest")).toString();
      peer.port = source.value(QStringLiteral("port")).toInt();
      peer.clientName = source.value(QStringLiteral("client")).toString();
      peer.progress = source.value(QStringLiteral("progress")).toDouble();
      peer.downloadRate =
          source.value(QStringLiteral("dl_speed")).toVariant().toLongLong();
      peer.uploadRate =
          source.value(QStringLiteral("up_speed")).toVariant().toLongLong();

      const QString flags = source.value(QStringLiteral("flags")).toString();
      peer.incoming = flags.contains(QLatin1Char('I'));
      peer.encrypted = flags.contains(QLatin1Char('E')) ||
                       flags.contains(QLatin1Char('H'));
      peers.peers.append(peer);
    }
    emit torrentPeersReceived(peers);
  } else if (context.kind == RequestKind::TorrentTrackers) {
    const QJsonArray sourceTrackers = document.array();
    TorrentTrackers trackers;
    trackers.key = context.key;
    trackers.trackers.reserve(sourceTrackers.size());

    for (const QJsonValue &value : sourceTrackers) {
      const QJsonObject source = value.toObject();
      TorrentTracker tracker;
      tracker.tier = source.value(QStringLiteral("tier")).toInt(-1);
      tracker.announceUrl = source.value(QStringLiteral("url")).toString();
      const QUrl trackerUrl(tracker.announceUrl);
      tracker.host = trackerUrl.host();
      if (tracker.host.isEmpty() &&
          tracker.announceUrl.startsWith(QStringLiteral("**"))) {
        tracker.host = tracker.announceUrl;
      }

      const int nativeStatus =
          source.value(QStringLiteral("status")).toInt();
      // qBittorrent reports a persistent "working" state (2) and a transient
      // "updating" state (3); map those to the shared wait/active vocabulary.
      tracker.announceState =
          source.value(QStringLiteral("updating")).toBool() ||
                  nativeStatus == 3
              ? 3
              : (nativeStatus == 0 ? 0 : 1);
      tracker.scrapeState = tracker.announceState;
      tracker.seederCount =
          source.value(QStringLiteral("num_seeds")).toInt(-1);
      tracker.leecherCount =
          source.value(QStringLiteral("num_leeches")).toInt(-1);
      tracker.downloadCount =
          source.value(QStringLiteral("num_downloaded")).toInt(-1);
      tracker.lastAnnounceTime =
          source.value(QStringLiteral("last_announce")).toVariant().toLongLong();
      tracker.nextAnnounceTime =
          source.value(QStringLiteral("next_announce")).toVariant().toLongLong();
      tracker.lastAnnounceResult =
          source.value(QStringLiteral("msg")).toString();
      tracker.lastAnnounceSucceeded = nativeStatus == 2;
      trackers.trackers.append(tracker);
    }
    emit torrentTrackersReceived(trackers);
  } else if (context.kind == RequestKind::TorrentPropertyEditor) {
    const QVariantMap nativeProperties = document.object().toVariantMap();
    const QVariantMap info = m_infoByKey.value(context.key);
    TorrentProperties properties;
    properties.key = context.key;
    properties.name = nativeProperties.value(QStringLiteral("name"),
                                             info.value(QStringLiteral("name")))
                          .toString();
    properties.hashString = context.key;
    properties.queuePosition =
        info.value(QStringLiteral("priority")).toInt();
    properties.peerLimit =
        nativeProperties.value(QStringLiteral("nb_connections_limit"), -1)
            .toInt();

    const qint64 downloadLimit =
        nativeProperties.value(QStringLiteral("dl_limit"), -1).toLongLong();
    const qint64 uploadLimit =
        nativeProperties.value(QStringLiteral("up_limit"), -1).toLongLong();
    properties.downloadLimited = downloadLimit > 0;
    properties.downloadLimit =
        downloadLimit > 0 ? static_cast<int>(downloadLimit / 1000) : 0;
    properties.uploadLimited = uploadLimit > 0;
    properties.uploadLimit =
        uploadLimit > 0 ? static_cast<int>(uploadLimit / 1000) : 0;

    const double ratioLimit =
        info.value(QStringLiteral("ratio_limit"), -2.0).toDouble();
    const int inactiveLimit =
        info.value(QStringLiteral("inactive_seeding_time_limit"), -2).toInt();
    properties.seedRatioMode =
        ratioLimit <= -2.0 ? 0 : (ratioLimit < 0.0 ? 2 : 1);
    properties.seedRatioLimit = ratioLimit >= 0.0 ? ratioLimit : 0.0;
    properties.seedIdleMode =
        inactiveLimit <= -2 ? 0 : (inactiveLimit < 0 ? 2 : 1);
    properties.seedIdleLimit = inactiveLimit >= 0 ? inactiveLimit : 0;
    properties.labels =
        splitTags(info.value(QStringLiteral("tags")).toString());
    properties.group =
        info.value(QStringLiteral("category")).toString();
    properties.hasGroup = true;

    QVariantMap fields = info;
    for (auto it = nativeProperties.cbegin(); it != nativeProperties.cend();
         ++it) {
      fields.insert(it.key(), it.value());
    }
    properties.fields = fields;
    m_editorPropertiesByKey.insert(context.key, properties);
    emit torrentPropertiesReceived(properties);
  } else if (context.kind == RequestKind::SessionSettings) {
    QJsonObject settings = document.object();
    // MainWindow consumes a small backend-neutral subset using Transmission's
    // established field names. Preserve the native object for future mappings.
    settings.insert(QStringLiteral("download-dir"),
                    settings.value(QStringLiteral("save_path")).toString());
    emit sessionSettingsReceived(settings);
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
        kind == RequestKind::TorrentProperties ||
        kind == RequestKind::TorrentPieces ||
        kind == RequestKind::TorrentFiles ||
        kind == RequestKind::TorrentPeers ||
        kind == RequestKind::TorrentTrackers ||
        kind == RequestKind::TorrentPropertyEditor) {
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
void QBittorrentBackend::getTorrentFiles(const TorrentKey &key) {
  if (!isValidTorrentKey(key) || !m_authenticated)
    return;

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("hash"), key);
  sendGet(RequestKind::TorrentFiles,
          QStringLiteral("/api/v2/torrents/files"), query, key);
}
void QBittorrentBackend::getTorrentPeers(const TorrentKey &key) {
  if (!isValidTorrentKey(key) || !m_authenticated)
    return;

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("hash"), key);
  // rid=0 requests a self-contained peer snapshot. Delta responses would need
  // per-torrent reconciliation state and provide little benefit for one tab.
  query.addQueryItem(QStringLiteral("rid"), QStringLiteral("0"));
  sendGet(RequestKind::TorrentPeers,
          QStringLiteral("/api/v2/sync/torrentPeers"), query, key);
}
void QBittorrentBackend::getTorrentTrackers(const TorrentKey &key) {
  if (!isValidTorrentKey(key) || !m_authenticated)
    return;

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("hash"), key);
  sendGet(RequestKind::TorrentTrackers,
          QStringLiteral("/api/v2/torrents/trackers"), query, key);
}
void QBittorrentBackend::getTorrentPieces(const TorrentKey &key) {
  if (!isValidTorrentKey(key) || !m_authenticated)
    return;

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("hash"), key);
  sendGet(RequestKind::TorrentPieces,
          QStringLiteral("/api/v2/torrents/pieceStates"), query, key);
}
void QBittorrentBackend::getTorrentProperties(const TorrentKey &key) {
  if (!isValidTorrentKey(key) || !m_authenticated)
    return;

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("hash"), key);
  sendGet(RequestKind::TorrentPropertyEditor,
          QStringLiteral("/api/v2/torrents/properties"), query, key);
}
void QBittorrentBackend::addTorrentFromFile(const QString &filePath,
                                            bool deleteFileOnSuccess) {
  addTorrentFile(filePath, QString(), false, {}, {}, {},
                 deleteFileOnSuccess);
}
void QBittorrentBackend::addTorrentFromMagnet(const QString &magnetLink) {
  addMagnetLink(magnetLink, QString(), false);
}
void QBittorrentBackend::addTorrentFile(const QString &filePath,
                                        const QString &downloadDir,
                                        bool paused,
                                        const QList<int> &, const QList<int> &,
                                        const QList<int> &,
                                        bool deleteFileOnSuccess) {
  const QFileInfo fileInfo(filePath);
  RequestContext context;
  context.kind = RequestKind::AddTorrent;
  context.torrentFilePath = fileInfo.absoluteFilePath();
  context.downloadDir = downloadDir;
  context.startPaused = paused;
  context.deleteTorrentFileOnSuccess = deleteFileOnSuccess;

  if (!fileInfo.exists() || !fileInfo.isFile()) {
    failAdd(context, tr("Torrent file does not exist: %1").arg(filePath));
    return;
  }

  // qBittorrent's add endpoint accepts the file and destination atomically.
  // Per-file wanted/priority changes require the resulting hash and therefore
  // remain a separate follow-up capability.
  queueOrSendAdd(context);
}
void QBittorrentBackend::addMagnetLink(const QString &magnetLink,
                                       const QString &downloadDir,
                                       bool paused) {
  RequestContext context;
  context.kind = RequestKind::AddTorrent;
  context.magnetLink = magnetLink.trimmed();
  context.downloadDir = downloadDir;
  context.startPaused = paused;

  if (context.magnetLink.isEmpty()) {
    failAdd(context, tr("No magnet link was specified."));
    return;
  }

  queueOrSendAdd(context);
}
void QBittorrentBackend::startTorrents(const QList<TorrentKey> &keys) {
  if (keys.isEmpty())
    return;
  postCommand(QStringLiteral("/api/v2/torrents/start"), hashesForm(keys),
              QStringLiteral("torrent-start"),
              QStringLiteral("/api/v2/torrents/resume"));
}
void QBittorrentBackend::startAllTorrents() {
  postCommand(QStringLiteral("/api/v2/torrents/start"),
              QByteArrayLiteral("hashes=all"), QStringLiteral("torrent-start"),
              QStringLiteral("/api/v2/torrents/resume"));
}
void QBittorrentBackend::startTorrentsNow(const QList<TorrentKey> &) {
  emitUnsupported(tr("Force start"));
}
void QBittorrentBackend::stopTorrents(const QList<TorrentKey> &keys) {
  if (keys.isEmpty())
    return;
  postCommand(QStringLiteral("/api/v2/torrents/stop"), hashesForm(keys),
              QStringLiteral("torrent-stop"),
              QStringLiteral("/api/v2/torrents/pause"));
}
void QBittorrentBackend::stopAllTorrents() {
  postCommand(QStringLiteral("/api/v2/torrents/stop"),
              QByteArrayLiteral("hashes=all"), QStringLiteral("torrent-stop"),
              QStringLiteral("/api/v2/torrents/pause"));
}
void QBittorrentBackend::removeTorrents(const QList<TorrentKey> &keys,
                                        bool deleteLocalData) {
  if (keys.isEmpty())
    return;
  QByteArray form = hashesForm(keys);
  form += QByteArrayLiteral("&deleteFiles=");
  form +=
      deleteLocalData ? QByteArrayLiteral("true") : QByteArrayLiteral("false");
  postCommand(QStringLiteral("/api/v2/torrents/delete"), form,
              QStringLiteral("torrent-remove"));
}
void QBittorrentBackend::verifyTorrents(const QList<TorrentKey> &keys) {
  if (keys.isEmpty())
    return;
  postCommand(QStringLiteral("/api/v2/torrents/recheck"), hashesForm(keys),
              QStringLiteral("torrent-verify"));
}
void QBittorrentBackend::reannounceTorrents(const QList<TorrentKey> &keys) {
  if (keys.isEmpty())
    return;
  postCommand(QStringLiteral("/api/v2/torrents/reannounce"), hashesForm(keys),
              QStringLiteral("torrent-reannounce"));
}
void QBittorrentBackend::setTorrentLocation(const QList<TorrentKey> &,
                                            const QString &, bool) {
  emitUnsupported(tr("Set location"));
}
void QBittorrentBackend::setTorrentFilesWanted(const TorrentKey &key,
                                               const QList<int> &indices,
                                               bool wanted) {
  if (!isValidTorrentKey(key) || indices.isEmpty())
    return;
  postCommand(QStringLiteral("/api/v2/torrents/filePrio"),
              filePriorityForm(key, indices, wanted ? 1 : 0),
              QStringLiteral("torrent-set"));
}
void QBittorrentBackend::setTorrentFilesPriority(const TorrentKey &key,
                                                 const QList<int> &indices,
                                                 int priority) {
  if (!isValidTorrentKey(key) || indices.isEmpty())
    return;
  const int nativePriority = priority > 0 ? 6 : 1;
  postCommand(QStringLiteral("/api/v2/torrents/filePrio"),
              filePriorityForm(key, indices, nativePriority),
              QStringLiteral("torrent-set"));
}
void QBittorrentBackend::setTorrentFilesWantedAndPriority(
    const TorrentKey &key, const QList<int> &indices, bool wanted,
    int priority) {
  if (!isValidTorrentKey(key) || indices.isEmpty())
    return;
  const int nativePriority = !wanted ? 0 : (priority > 0 ? 6 : 1);
  postCommand(QStringLiteral("/api/v2/torrents/filePrio"),
              filePriorityForm(key, indices, nativePriority),
              QStringLiteral("torrent-set"));
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
void QBittorrentBackend::setTorrentProperties(
    const TorrentKey &key, const TorrentPropertyChanges &changes) {
  if (!isValidTorrentKey(key))
    return;

  const TorrentProperties current = m_editorPropertiesByKey.value(key);
  const QByteArray hashes =
      QByteArrayLiteral("hashes=") + formField(key);
  bool commandPosted = false;

  if (changes.downloadLimited != current.downloadLimited ||
      (changes.downloadLimited &&
       changes.downloadLimit != current.downloadLimit)) {
    const qint64 limit =
        changes.downloadLimited
            ? static_cast<qint64>(changes.downloadLimit) * 1000
            : 0;
    postCommand(QStringLiteral("/api/v2/torrents/setDownloadLimit"),
                hashes + QByteArrayLiteral("&limit=") +
                    QByteArray::number(limit),
                QStringLiteral("torrent-set"));
    commandPosted = true;
  }

  if (changes.uploadLimited != current.uploadLimited ||
      (changes.uploadLimited && changes.uploadLimit != current.uploadLimit)) {
    const qint64 limit =
        changes.uploadLimited
            ? static_cast<qint64>(changes.uploadLimit) * 1000
            : 0;
    postCommand(QStringLiteral("/api/v2/torrents/setUploadLimit"),
                hashes + QByteArrayLiteral("&limit=") +
                    QByteArray::number(limit),
                QStringLiteral("torrent-set"));
    commandPosted = true;
  }

  if (changes.seedRatioMode != current.seedRatioMode ||
      changes.seedRatioLimit != current.seedRatioLimit ||
      changes.seedIdleMode != current.seedIdleMode ||
      changes.seedIdleLimit != current.seedIdleLimit) {
    const QVariantMap info = m_infoByKey.value(key);
    const double ratioLimit =
        changes.seedRatioMode == 0
            ? -2.0
            : (changes.seedRatioMode == 2 ? -1.0
                                          : changes.seedRatioLimit);
    const int inactiveLimit =
        changes.seedIdleMode == 0
            ? -2
            : (changes.seedIdleMode == 2 ? -1 : changes.seedIdleLimit);
    const int seedingTimeLimit =
        info.value(QStringLiteral("seeding_time_limit"), -2).toInt();
    QString limitsMode =
        info.value(QStringLiteral("share_limits_mode"),
                   QStringLiteral("Default")).toString();
    if (changes.seedRatioMode == 0 && changes.seedIdleMode == 0)
      limitsMode = QStringLiteral("Default");
    else if (limitsMode.isEmpty() ||
             limitsMode == QStringLiteral("Default"))
      limitsMode = QStringLiteral("MatchAny");
    const QString limitAction =
        info.value(QStringLiteral("share_limit_action"),
                   QStringLiteral("Default")).toString();

    QByteArray form = hashes;
    form += QByteArrayLiteral("&ratioLimit=") +
            QByteArray::number(ratioLimit, 'g', 16);
    form += QByteArrayLiteral("&seedingTimeLimit=") +
            QByteArray::number(seedingTimeLimit);
    form += QByteArrayLiteral("&inactiveSeedingTimeLimit=") +
            QByteArray::number(inactiveLimit);
    form += QByteArrayLiteral("&shareLimitAction=") +
            formField(limitAction);
    form += QByteArrayLiteral("&shareLimitsMode=") +
            formField(limitsMode);
    postCommand(QStringLiteral("/api/v2/torrents/setShareLimits"), form,
                QStringLiteral("torrent-set"));
    commandPosted = true;
  }

  QStringList normalizedLabels = changes.labels;
  normalizedLabels.sort(Qt::CaseInsensitive);
  QStringList currentLabels = current.labels;
  currentLabels.sort(Qt::CaseInsensitive);
  if (normalizedLabels != currentLabels) {
    postCommand(QStringLiteral("/api/v2/torrents/setTags"),
                hashes + QByteArrayLiteral("&tags=") +
                    formField(changes.labels.join(QLatin1Char(','))),
                QStringLiteral("torrent-set"));
    commandPosted = true;
  }

  if (changes.setGroup && changes.group != current.group) {
    postCommand(QStringLiteral("/api/v2/torrents/setCategory"),
                hashes + QByteArrayLiteral("&category=") +
                    formField(changes.group),
                QStringLiteral("torrent-set"));
    commandPosted = true;
  }

  // The properties resource does not repeat tags, category, or share limits.
  // Keep the cached list projection coherent so Apply's immediate confirmation
  // request does not momentarily restore the pre-command values.
  QVariantMap info = m_infoByKey.value(key);
  info.insert(QStringLiteral("tags"),
              changes.labels.join(QStringLiteral(", ")));
  if (changes.setGroup)
    info.insert(QStringLiteral("category"), changes.group);
  info.insert(QStringLiteral("ratio_limit"),
              changes.seedRatioMode == 0
                  ? -2.0
                  : (changes.seedRatioMode == 2 ? -1.0
                                                : changes.seedRatioLimit));
  info.insert(QStringLiteral("inactive_seeding_time_limit"),
              changes.seedIdleMode == 0
                  ? -2
                  : (changes.seedIdleMode == 2 ? -1
                                              : changes.seedIdleLimit));
  m_infoByKey.insert(key, info);

  TorrentProperties confirmed = current;
  confirmed.downloadLimited = changes.downloadLimited;
  confirmed.downloadLimit = changes.downloadLimit;
  confirmed.uploadLimited = changes.uploadLimited;
  confirmed.uploadLimit = changes.uploadLimit;
  confirmed.seedRatioMode = changes.seedRatioMode;
  confirmed.seedRatioLimit = changes.seedRatioLimit;
  confirmed.seedIdleMode = changes.seedIdleMode;
  confirmed.seedIdleLimit = changes.seedIdleLimit;
  confirmed.labels = changes.labels;
  if (changes.setGroup)
    confirmed.group = changes.group;
  m_editorPropertiesByKey.insert(key, confirmed);

  if (!commandPosted)
    emit commandSucceeded(QStringLiteral("torrent-set"));
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
void QBittorrentBackend::getSessionSettings() {
  if (!m_authenticated) {
    m_sessionSettingsPendingAfterLogin = true;
    authenticate();
    return;
  }

  sendGet(RequestKind::SessionSettings,
          QStringLiteral("/api/v2/app/preferences"));
}
void QBittorrentBackend::getSessionStatistics() {}
void QBittorrentBackend::setSessionSettings(const QJsonObject &) {
  emitUnsupported(tr("Session settings"));
}
void QBittorrentBackend::getFreeSpace(const QString &) {}
void QBittorrentBackend::testPortForwarding() {}
void QBittorrentBackend::updateBlocklist(const QJsonObject &) {
  emitUnsupported(tr("Update blocklist"));
}
