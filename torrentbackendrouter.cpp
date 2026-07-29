#include "torrentbackendrouter.h"

#include "delugebackend.h"
#include "qbittorrentbackend.h"
#include "settingskeys.h"
#include "transmissionbackend.h"

#include <QSettings>

TorrentBackendRouter::TorrentBackendRouter(QObject *parent)
    : TorrentBackend(parent) {}

QString TorrentBackendRouter::backendTypeForServer(int index) const {
  QSettings settings;
  const int count = settings.beginReadArray(SettingsKeys::ServersArray);
  if (index < 0 || index >= count) {
    settings.endArray();
    return {};
  }
  settings.setArrayIndex(index);
  const QString type = settings
                           .value(SettingsKeys::ServerBackendType,
                                  QStringLiteral("transmission"))
                           .toString()
                           .trimmed()
                           .toLower();
  settings.endArray();
  return type;
}

TorrentBackend *TorrentBackendRouter::createBackend(const QString &type) {
  if (type == QStringLiteral("qbittorrent"))
    return new QBittorrentBackend(this);
  if (type == QStringLiteral("deluge"))
    return new DelugeBackend(this);
  if (type == QStringLiteral("transmission"))
    return new TransmissionBackend(this);
  return nullptr;
}

void TorrentBackendRouter::replaceBackend(TorrentBackend *backend) {
  if (m_backend == backend)
    return;

  if (m_backend) {
    m_backend->disconnect(this);
    m_backend->deleteLater();
  }

  m_backend = backend;
  if (m_backend)
    forwardSignals(m_backend);
}

void TorrentBackendRouter::forwardSignals(TorrentBackend *backend) {
  connect(backend, &TorrentBackend::updateStarted, this,
          &TorrentBackend::updateStarted);
  connect(backend, &TorrentBackend::updateFinished, this,
          &TorrentBackend::updateFinished);
  connect(backend, &TorrentBackend::updateFailed, this,
          &TorrentBackend::updateFailed);
  connect(backend, &TorrentBackend::torrentsReceived, this,
          &TorrentBackend::torrentsReceived);
  connect(backend, &TorrentBackend::torrentTrackerMetadataUpdated, this,
          &TorrentBackend::torrentTrackerMetadataUpdated);
  connect(backend, &TorrentBackend::torrentDetailsReceived, this,
          &TorrentBackend::torrentDetailsReceived);
  connect(backend, &TorrentBackend::torrentFilesReceived, this,
          &TorrentBackend::torrentFilesReceived);
  connect(backend, &TorrentBackend::torrentPeersReceived, this,
          &TorrentBackend::torrentPeersReceived);
  connect(backend, &TorrentBackend::torrentTrackersReceived, this,
          &TorrentBackend::torrentTrackersReceived);
  connect(backend, &TorrentBackend::torrentPiecesReceived, this,
          &TorrentBackend::torrentPiecesReceived);
  connect(backend, &TorrentBackend::torrentPropertiesReceived, this,
          &TorrentBackend::torrentPropertiesReceived);
  connect(backend, &TorrentBackend::commandSucceeded, this,
          &TorrentBackend::commandSucceeded);
  connect(backend, &TorrentBackend::commandFailed, this,
          &TorrentBackend::commandFailed);
  connect(backend, &TorrentBackend::torrentFileAddSucceeded, this,
          &TorrentBackend::torrentFileAddSucceeded);
  connect(backend, &TorrentBackend::torrentAdded, this,
          &TorrentBackend::torrentAdded);
  connect(backend, &TorrentBackend::torrentFileAddFailed, this,
          &TorrentBackend::torrentFileAddFailed);
  connect(backend, &TorrentBackend::serverChanged, this,
          &TorrentBackend::serverChanged);
  connect(backend, &TorrentBackend::capabilitiesChanged, this,
          &TorrentBackend::capabilitiesChanged);
  connect(backend, &TorrentBackend::sessionSettingsReceived, this,
          &TorrentBackend::sessionSettingsReceived);
  connect(backend, &TorrentBackend::sessionStatisticsReceived, this,
          &TorrentBackend::sessionStatisticsReceived);
  connect(backend, &TorrentBackend::sessionStatisticsFailed, this,
          &TorrentBackend::sessionStatisticsFailed);
  connect(backend, &TorrentBackend::freeSpaceReceived, this,
          &TorrentBackend::freeSpaceReceived);
  connect(backend, &TorrentBackend::portTestFinished, this,
          &TorrentBackend::portTestFinished);
  connect(backend, &TorrentBackend::portTestFailed, this,
          &TorrentBackend::portTestFailed);
  connect(backend, &TorrentBackend::blocklistUpdateFinished, this,
          &TorrentBackend::blocklistUpdateFinished);
  connect(backend, &TorrentBackend::blocklistUpdateFailed, this,
          &TorrentBackend::blocklistUpdateFailed);
}

bool TorrentBackendRouter::setServerFromSettingsIndex(int index) {
  const QString type = backendTypeForServer(index);
  if (type.isEmpty())
    return false;

  if (!m_backend ||
      (type == QStringLiteral("qbittorrent") &&
       m_backend->backendName() != QStringLiteral("qBittorrent")) ||
      (type == QStringLiteral("deluge") &&
       m_backend->backendName() != QStringLiteral("Deluge")) ||
      (type == QStringLiteral("transmission") &&
       m_backend->backendName() != QStringLiteral("Transmission"))) {
    replaceBackend(createBackend(type));
  }

  return m_backend && m_backend->setServerFromSettingsIndex(index);
}

bool TorrentBackendRouter::loadCurrentServerFromSettings() {
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

QString TorrentBackendRouter::backendName() const {
  return m_backend ? m_backend->backendName() : QString();
}
QString TorrentBackendRouter::serverDisplayName() const {
  return m_backend ? m_backend->serverDisplayName()
                   : tr("No server configured");
}
QString TorrentBackendRouter::endpointUrl() const {
  return m_backend ? m_backend->endpointUrl() : QString();
}
TorrentBackendCapabilities TorrentBackendRouter::capabilities() const {
  return m_backend ? m_backend->capabilities() : TorrentBackendCapabilities();
}
void TorrentBackendRouter::init() {
  if (!loadCurrentServerFromSettings()) {
    emit updateFailed(tr("No valid torrent server configured."));
    return;
  }

  if (m_backend)
    m_backend->init();
}
void TorrentBackendRouter::getTorrentList() {
  if (m_backend)
    m_backend->getTorrentList();
}
void TorrentBackendRouter::getTorrentTrackerMetadata() {
  if (m_backend)
    m_backend->getTorrentTrackerMetadata();
}
void TorrentBackendRouter::getTorrentDetails(const TorrentKey &v) {
  if (m_backend)
    m_backend->getTorrentDetails(v);
}
void TorrentBackendRouter::getTorrentFiles(const TorrentKey &v) {
  if (m_backend)
    m_backend->getTorrentFiles(v);
}
void TorrentBackendRouter::getTorrentPeers(const TorrentKey &v) {
  if (m_backend)
    m_backend->getTorrentPeers(v);
}
void TorrentBackendRouter::getTorrentTrackers(const TorrentKey &v) {
  if (m_backend)
    m_backend->getTorrentTrackers(v);
}
void TorrentBackendRouter::getTorrentPieces(const TorrentKey &v) {
  if (m_backend)
    m_backend->getTorrentPieces(v);
}
void TorrentBackendRouter::getTorrentProperties(const TorrentKey &v) {
  if (m_backend)
    m_backend->getTorrentProperties(v);
}
void TorrentBackendRouter::cancelTorrentDetailRequests() {
  if (m_backend)
    m_backend->cancelTorrentDetailRequests();
}
void TorrentBackendRouter::addTorrentFromFile(const QString &a, bool b) {
  if (m_backend)
    m_backend->addTorrentFromFile(a, b);
}
void TorrentBackendRouter::addTorrentFromMagnet(const QString &a) {
  if (m_backend)
    m_backend->addTorrentFromMagnet(a);
}
void TorrentBackendRouter::addTorrentFile(const QString &a, const QString &b,
                                          bool c, const QList<int> &d,
                                          const QList<int> &e,
                                          const QList<int> &f, bool g) {
  if (m_backend)
    m_backend->addTorrentFile(a, b, c, d, e, f, g);
}
void TorrentBackendRouter::addMagnetLink(const QString &a, const QString &b,
                                         bool c) {
  if (m_backend)
    m_backend->addMagnetLink(a, b, c);
}
void TorrentBackendRouter::startTorrents(const QList<TorrentKey> &v) {
  if (m_backend)
    m_backend->startTorrents(v);
}
void TorrentBackendRouter::startAllTorrents() {
  if (m_backend)
    m_backend->startAllTorrents();
}
void TorrentBackendRouter::startTorrentsNow(const QList<TorrentKey> &v) {
  if (m_backend)
    m_backend->startTorrentsNow(v);
}
void TorrentBackendRouter::stopTorrents(const QList<TorrentKey> &v) {
  if (m_backend)
    m_backend->stopTorrents(v);
}
void TorrentBackendRouter::stopAllTorrents() {
  if (m_backend)
    m_backend->stopAllTorrents();
}
void TorrentBackendRouter::removeTorrents(const QList<TorrentKey> &v, bool b) {
  if (m_backend)
    m_backend->removeTorrents(v, b);
}
void TorrentBackendRouter::verifyTorrents(const QList<TorrentKey> &v) {
  if (m_backend)
    m_backend->verifyTorrents(v);
}
void TorrentBackendRouter::reannounceTorrents(const QList<TorrentKey> &v) {
  if (m_backend)
    m_backend->reannounceTorrents(v);
}
void TorrentBackendRouter::setTorrentLocation(const QList<TorrentKey> &v,
                                              const QString &s, bool b) {
  if (m_backend)
    m_backend->setTorrentLocation(v, s, b);
}
void TorrentBackendRouter::setTorrentFilesWanted(const TorrentKey &k,
                                                 const QList<int> &v, bool b) {
  if (m_backend)
    m_backend->setTorrentFilesWanted(k, v, b);
}
void TorrentBackendRouter::setTorrentFilesPriority(const TorrentKey &k,
                                                   const QList<int> &v, int p) {
  if (m_backend)
    m_backend->setTorrentFilesPriority(k, v, p);
}
void TorrentBackendRouter::setTorrentFilesWantedAndPriority(const TorrentKey &k,
                                                            const QList<int> &v,
                                                            bool b, int p) {
  if (m_backend)
    m_backend->setTorrentFilesWantedAndPriority(k, v, b, p);
}
void TorrentBackendRouter::addTorrentTracker(const TorrentKey &k,
                                             const QString &s) {
  if (m_backend)
    m_backend->addTorrentTracker(k, s);
}
void TorrentBackendRouter::editTorrentTracker(const TorrentKey &k, int i,
                                              const QString &s) {
  if (m_backend)
    m_backend->editTorrentTracker(k, i, s);
}
void TorrentBackendRouter::removeTorrentTracker(const TorrentKey &k, int i) {
  if (m_backend)
    m_backend->removeTorrentTracker(k, i);
}
void TorrentBackendRouter::renameTorrentPath(const TorrentKey &k,
                                             const QString &a,
                                             const QString &b) {
  if (m_backend)
    m_backend->renameTorrentPath(k, a, b);
}
void TorrentBackendRouter::setTorrentProperties(
    const TorrentKey &k, const TorrentPropertyChanges &v) {
  if (m_backend)
    m_backend->setTorrentProperties(k, v);
}
void TorrentBackendRouter::setTorrentsSequentialDownload(
    const QList<TorrentKey> &v, bool b) {
  if (m_backend)
    m_backend->setTorrentsSequentialDownload(v, b);
}
void TorrentBackendRouter::setTorrentsBandwidthPriority(
    const QList<TorrentKey> &v, int p) {
  if (m_backend)
    m_backend->setTorrentsBandwidthPriority(v, p);
}
void TorrentBackendRouter::queueMoveTop(const QList<TorrentKey> &v) {
  if (m_backend)
    m_backend->queueMoveTop(v);
}
void TorrentBackendRouter::queueMoveUp(const QList<TorrentKey> &v) {
  if (m_backend)
    m_backend->queueMoveUp(v);
}
void TorrentBackendRouter::queueMoveDown(const QList<TorrentKey> &v) {
  if (m_backend)
    m_backend->queueMoveDown(v);
}
void TorrentBackendRouter::queueMoveBottom(const QList<TorrentKey> &v) {
  if (m_backend)
    m_backend->queueMoveBottom(v);
}
void TorrentBackendRouter::getSessionSettings() {
  if (m_backend)
    m_backend->getSessionSettings();
}
void TorrentBackendRouter::getSessionStatistics() {
  if (m_backend)
    m_backend->getSessionStatistics();
}
void TorrentBackendRouter::setSessionSettings(const QJsonObject &v) {
  if (m_backend)
    m_backend->setSessionSettings(v);
}
void TorrentBackendRouter::getFreeSpace(const QString &v) {
  if (m_backend)
    m_backend->getFreeSpace(v);
}
void TorrentBackendRouter::testPortForwarding() {
  if (m_backend)
    m_backend->testPortForwarding();
}
void TorrentBackendRouter::updateBlocklist(const QJsonObject &v) {
  if (m_backend)
    m_backend->updateBlocklist(v);
}
