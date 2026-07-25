#ifndef TORRENTTRACKERSCONTROLLER_H
#define TORRENTTRACKERSCONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QPoint>
#include <QString>
#include <QVariant>
#include <memory>

class QTableWidget;
class QTableWidgetItem;
class QWidget;
class TorrentBackend;
class TableColumnController;
class TablePlaceholderController;

// Displays tracker statistics for the selected torrent and maps context-menu
// edits to tracker IDs required by Transmission's RPC protocol.
class TorrentTrackersController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentTrackersController(QTableWidget *trackerTableWidget,
                                       TorrentBackend *client,
                                       QWidget *dialogParent,
                                       QObject *parent = nullptr);
    ~TorrentTrackersController() override;

    void setup();
    void clear();
    void populate(const QJsonObject &details);
    void setTorrentId(int torrentId);
    void saveViewState() const;
    void restoreViewState();
    void setLoading();

signals:
    void statusMessageRequested(const QString &message, int timeoutMs);

private:
    enum TrackerColumn {
        TierColumn = 0,
        HostColumn,
        SiteColumn,
        AnnounceColumn,
        ScrapeColumn,
        AnnounceStateColumn,
        ScrapeStateColumn,
        SeedsColumn,
        LeechersColumn,
        DownloadsColumn,
        LastAnnounceColumn,
        NextAnnounceColumn,
        LastScrapeColumn,
        NextScrapeColumn,
        LastAnnounceResultColumn,
        LastScrapeResultColumn,
        TrackerColumnCount
    };

    enum TrackerRole {
        TrackerAnnounceRole = Qt::UserRole,
        TrackerIdRole = Qt::UserRole + 1
    };

    int trackerIdForRow(int row) const;
    QString trackerAnnounceUrlForRow(int row) const;
    void addTrackerFromContextMenu();
    void editTrackerFromContextMenu(int row);
    void removeTrackerFromContextMenu(int row);
    void showContextMenu(const QPoint &pos);
    void copyTrackerUrlToClipboard(const QString &trackerUrl);
    QString formatTrackerTime(qint64 seconds, const QString &emptyText) const;
    QString formatTrackerCount(int count) const;
    QString formatTrackerState(int state) const;
    QString displayTrackerResult(const QString &result, bool succeeded, bool timedOut) const;
    QTableWidgetItem *makeTextItem(const QString &text, const QVariant &sortValue = QVariant()) const;

    QTableWidget *trackerTableWidget = nullptr;
    TorrentBackend *client = nullptr;
    QWidget *dialogParent = nullptr;
    int torrentId = -1;
    std::unique_ptr<TableColumnController> columnController;
    std::unique_ptr<TablePlaceholderController> placeholderController;
};

#endif // TORRENTTRACKERSCONTROLLER_H
