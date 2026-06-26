#ifndef TORRENTDETAILSTABCONTROLLER_H
#define TORRENTDETAILSTABCONTROLLER_H

#include <QObject>

class QJsonObject;
class QTableWidget;
class QTabWidget;
class QWidget;

class TorrentDetailsTabController : public QObject
{
    Q_OBJECT

public:
    explicit TorrentDetailsTabController(QTabWidget *tabWidget,
                                         QWidget *generalTab,
                                         QObject *parent = nullptr);

    QWidget *widget() const;

    void clear();
    void update(const QJsonObject &details);

private:
    QWidget *m_tab = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // TORRENTDETAILSTABCONTROLLER_H
