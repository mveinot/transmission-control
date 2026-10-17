#ifndef SERVERCONNECTIONPROBE_H
#define SERVERCONNECTIONPROBE_H

#include <QObject>
#include <QUrl>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

// Tests a server profile at its entered endpoint, then tries a bounded set of
// backend defaults when the failure indicates a missing port or RPC path.
class ServerConnectionProbe : public QObject
{
    Q_OBJECT

public:
    enum class FailureKind {
        Connection,
        Authentication,
        Tls,
        DaemonUnavailable
    };
    Q_ENUM(FailureKind)

    explicit ServerConnectionProbe(QObject *parent = nullptr);

    void start(const QString &backendType,
               const QUrl &enteredUrl,
               const QString &username,
               const QString &password);
    void cancel();
    bool isRunning() const;

    // Exposed for deterministic validation of URL correction rules.
    static QList<QUrl> candidateUrls(const QString &backendType,
                                     const QUrl &enteredUrl);

signals:
    void connectionSucceeded(const QUrl &workingUrl, bool adjusted);
    void connectionFailed(const QString &message,
                          ServerConnectionProbe::FailureKind kind);

private:
    enum class Stage {
        None,
        Transmission,
        QBittorrentLogin,
        DelugeLogin,
        DelugeConnection
    };

    enum class RetryScope {
        None,
        PortChanges,
        AllCandidates
    };

    struct Candidate {
        QUrl url;
        bool changesPort = false;
        bool changesPath = false;
    };

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QVector<Candidate> m_candidates;
    QString m_backendType;
    QString m_username;
    QString m_password;
    int m_candidateIndex = -1;
    Stage m_stage = Stage::None;
    RetryScope m_retryScope = RetryScope::None;
    bool m_transmissionSessionRetried = false;

    static QVector<Candidate> buildCandidates(const QString &backendType,
                                              const QUrl &enteredUrl);
    static QUrl appendPath(const QUrl &baseUrl, const QString &suffix);
    static bool isTlsError(QNetworkReply *reply);

    void beginCandidate(int index);
    void sendTransmissionRequest(const QByteArray &sessionToken = {});
    void sendQBittorrentLogin();
    void sendDelugeRequest(Stage stage);
    void handleReply(QNetworkReply *reply);
    void failAttempt(const QString &message,
                     FailureKind kind,
                     RetryScope retryScope);
    int nextCandidateIndex() const;
    void finishSuccess();
    void finishFailure(const QString &message, FailureKind kind);
};

#endif // SERVERCONNECTIONPROBE_H
