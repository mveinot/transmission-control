#ifndef TORRENT_H
#define TORRENT_H

#include <QObject>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>

class torrent
{

public:
    explicit torrent(QJsonValue val, QObject *parent = nullptr);
    QString getName();
    double getPercentDone();
    QString getStatus();
    QString getRateDownload();
    QString getRateUpload();
    QString getUploadRatio();
    QString getEta();
    int getId();

private:
    int id;
    QString name;
    double percentDone;
    double rateDownload;
    double rateUpload;
    double uploadRatio;
    QJsonValue files;
    QJsonValue peers;
    int status;
    int eta;

signals:

};

#endif // TORRENT_H
