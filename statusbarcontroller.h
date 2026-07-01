#ifndef STATUSBARCONTROLLER_H
#define STATUSBARCONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QVector>

#include "torrent.h"

class QLabel;
class QStatusBar;
class rpc_client;

class StatusBarController : public QObject
{
    Q_OBJECT

public:
    explicit StatusBarController(QStatusBar *statusBar,
                                 rpc_client *client,
                                 QObject *parent = nullptr);

    void setup();
    void showMessage(const QString &message, int timeoutMs = 0);
    void updateTorrents(const QVector<torrent> &torrents);
    void setFreeSpace(qint64 sizeBytes);
    void clearFreeSpace();
    void setSessionSettings(const QJsonObject &settings);
    void setUpdateIntervalSeconds(int seconds);
    void setServerName(const QString &serverName);
    void setTorrentResultCount(int visibleCount, int totalCount);
    void setFilterSummary(const QString &summary);

private:
    QStatusBar *m_statusBar = nullptr;
    rpc_client *m_client = nullptr;

    QLabel *m_activityLabel = nullptr;
    QLabel *m_serverLabel = nullptr;
    QLabel *m_torrentCountLabel = nullptr;
    QLabel *m_filterLabel = nullptr;
    QLabel *m_rateLabel = nullptr;
    QLabel *m_freeSpaceLabel = nullptr;
    QLabel *m_speedModeLabel = nullptr;
    QLabel *m_intervalLabel = nullptr;

    int m_torrentCount = 0;
    int m_visibleTorrentCount = -1;
    QString m_filterSummary;
    double m_downloadRateBytesPerSecond = 0.0;
    double m_uploadRateBytesPerSecond = 0.0;
    qint64 m_freeSpaceBytes = -1;
    bool m_altSpeedEnabled = false;
    int m_updateIntervalSeconds = 0;
    QString m_serverName;

    QLabel *makeSectionLabel(const QString &text = QString()) const;
    QString normalizedErrorMessage(const QString &message) const;
    QString formattedRate(double bytesPerSecond) const;
    QString formattedBytes(qint64 bytes) const;

    void refreshServerLabel();
    void refreshTorrentCountLabel();
    void refreshFilterLabel();
    void refreshRateLabel();
    void refreshFreeSpaceLabel();
    void refreshSpeedModeLabel();
    void refreshIntervalLabel();
    void setActivityText(const QString &text, bool error = false);
};

#endif // STATUSBARCONTROLLER_H
