#ifndef TORRENTSORTPROXYMODEL_H
#define TORRENTSORTPROXYMODEL_H

#include <QSortFilterProxyModel>

// Combines orthogonal state, text, tracker, and download-directory predicates
// while providing type-aware ordering for TorrentModel columns.
class TorrentSortProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum class StateFilter {
        All,
        Downloading,
        Waiting,
        Completed,
        Active,
        Inactive,
        Stopped,
        Error
    };

    explicit TorrentSortProxyModel(QObject *parent = nullptr);

    void setStateFilter(StateFilter filter);
    StateFilter stateFilter() const;

    // Shared by proxy row filtering and sidebar counts so both surfaces apply
    // exactly the same interpretation of Transmission's status values.
    static bool matchesState(StateFilter filter,
                             int statusValue,
                             double percentDone,
                             bool hasError,
                             double downloadRate,
                             double uploadRate);

    void setSearchText(const QString &searchText);
    QString searchText() const;

    void setTrackerFilter(const QString &trackerHost);
    QString trackerFilter() const;

    void setDownloadDirFilter(const QString &downloadDir);
    QString downloadDirFilter() const;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    StateFilter m_stateFilter = StateFilter::All;
    QString m_searchText;
    QString m_trackerFilter;
    QString m_downloadDirFilter;

    bool matchesTrackerFilter(int sourceRow,
                              const QModelIndex &sourceParent) const;

    bool matchesDownloadDirFilter(int sourceRow,
                                  const QModelIndex &sourceParent) const;

    bool matchesSearchFilter(int sourceRow,
                             const QModelIndex &sourceParent) const;

    bool matchesStateFilter(int sourceRow,
                            const QModelIndex &sourceParent) const;

    void refreshFilter();
};

#endif // TORRENTSORTPROXYMODEL_H
