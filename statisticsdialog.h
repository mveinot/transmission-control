#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include <QDialog>
#include <QJsonObject>

class QLabel;
class QPushButton;
class rpc_client;

class StatisticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatisticsDialog(rpc_client *client, QWidget *parent = nullptr);

private slots:
    void refresh();
    void updateStatistics(const QJsonObject &statistics);
    void showError(const QString &message);

public:
    struct StatisticLabels {
        QLabel *downloaded = nullptr;
        QLabel *uploaded = nullptr;
        QLabel *ratio = nullptr;
        QLabel *activeTime = nullptr;
        QLabel *sessionCount = nullptr;
        QLabel *filesAdded = nullptr;
    };

private:
    rpc_client *m_client = nullptr;
    StatisticLabels m_currentLabels;
    StatisticLabels m_cumulativeLabels;
    QPushButton *m_refreshButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    static QString formatBytes(qint64 bytes);
    static QString formatDuration(qint64 seconds);
    static QString formatRatio(qint64 uploadedBytes, qint64 downloadedBytes);
    static qint64 jsonInteger(const QJsonObject &object, const QString &key);
    static void populateLabels(const QJsonObject &statistics,
                               const StatisticLabels &labels);
};

#endif // STATISTICSDIALOG_H
