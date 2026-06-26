#ifndef TORRENTTRACKERSCONTROLLER_H
#define TORRENTTRACKERSCONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QPoint>
#include <QString>

class QTableWidget;
class QWidget;
class rpc_client;

class TorrentTrackersController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentTrackersController(QTableWidget *trackerTableWidget,
                                       rpc_client *client,
                                       QWidget *dialogParent,
                                       QObject *parent = nullptr);

    void setup();
    void clear();
    void populate(const QJsonObject &details);
    void setTorrentId(int torrentId);

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
};

#endif // TORRENTTRACKERSCONTROLLER_H
