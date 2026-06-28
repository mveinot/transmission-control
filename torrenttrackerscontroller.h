#ifndef TORRENTTRACKERSCONTROLLER_H
#define TORRENTTRACKERSCONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QPoint>
#include <QString>
#include <memory>

class QTableWidget;
class QWidget;
class rpc_client;
class TableColumnController;

class TorrentTrackersController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentTrackersController(QTableWidget *trackerTableWidget,
                                       rpc_client *client,
                                       QWidget *dialogParent,
                                       QObject *parent = nullptr);
    ~TorrentTrackersController() override;

    void setup();
    void clear();
    void populate(const QJsonObject &details);
    void setTorrentId(int torrentId);
    void saveViewState() const;
    void restoreViewState();

signals:
    void statusMessageRequested(const QString &message, int timeoutMs);

private:
    enum TrackerColumn {
        HostColumn = 0,
        AnnounceColumn,
        SeedsColumn,
        LeechersColumn,
        LastAnnounceColumn,
        ResultColumn,
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

    QTableWidget *trackerTableWidget = nullptr;
    rpc_client *client = nullptr;
    QWidget *dialogParent = nullptr;
    int torrentId = -1;
    std::unique_ptr<TableColumnController> columnController;
};

#endif // TORRENTTRACKERSCONTROLLER_H
