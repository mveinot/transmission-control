#ifndef TORRENTDETAILSTABCONTROLLER_H
#define TORRENTDETAILSTABCONTROLLER_H

#include <QObject>
#include <QVariantMap>

class QTableWidget;
class QTabWidget;
class QWidget;

// Formats a torrent detail snapshot into a stable diagnostic table. It owns no
// RPC state and may be refreshed from merged general/piece responses.
class TorrentDetailsTabController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentDetailsTabController(QTabWidget *tabWidget,
                                         QWidget *generalTab,
                                         QObject *parent = nullptr);

    QWidget *widget() const;

    void clear();
    void update(const QVariantMap &details);

private:
    QWidget *m_tab = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // TORRENTDETAILSTABCONTROLLER_H
