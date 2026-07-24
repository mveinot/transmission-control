#ifndef TORRENTGENERALCONTROLLER_H
#define TORRENTGENERALCONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class QLabel;
class QLineEdit;
class QTabWidget;
class QVBoxLayout;
class QWidget;
class PieceProgressController;
class TorrentDetailsTabController;

// Owns the selected torrent's merged detail cache and distributes it to the
// general, linear piece-progress, and raw-details presentations.
class TorrentGeneralController : public QObject
{
    Q_OBJECT

public:
    struct Widgets
    {
        QWidget *generalTab = nullptr;
        QVBoxLayout *generalLayout = nullptr;
        QTabWidget *tabWidget = nullptr;
        QLabel *nameLabel = nullptr;
        QLabel *totalSizeLabel = nullptr;
        QLabel *creatorLabel = nullptr;
        QLabel *createdLabel = nullptr;
        QLabel *downloadDirLabel = nullptr;
        QLabel *hashLabel = nullptr;
        QLabel *commentLabel = nullptr;
        QLineEdit *magnetLineEdit = nullptr;
    };

    explicit TorrentGeneralController(const Widgets &widgets,
                                      QObject *parent = nullptr);

    void setup();
    void clear();
    void update(const QJsonObject &details);
    void updatePieces(int torrentId, const QJsonObject &details);

    int currentTorrentId() const;
    QString currentHashString() const;
    QString currentMagnetLink() const;
    QString currentDownloadDir() const;
    bool wantsLiveTorrentDetails(QWidget *currentTab) const;

signals:
    void currentTorrentDetailsChanged(int torrentId,
                                      const QString &hashString,
                                      const QString &magnetLink);
    void currentTorrentDetailsCleared();

private:
    static bool looksLikeUrl(const QString &text);
    void configureMagnetLineEdit();
    void updateGeneralFields(const QJsonObject &details);

    Widgets m_widgets;
    PieceProgressController *m_pieceProgressController = nullptr;
    TorrentDetailsTabController *m_detailsTabController = nullptr;
    QJsonObject m_currentDetailsCache;
    int m_currentTorrentId = -1;
    QString m_currentHashString;
    QString m_currentMagnetLink;
};

#endif // TORRENTGENERALCONTROLLER_H
