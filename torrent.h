#ifndef TORRENT_H
#define TORRENT_H

#include <QObject>
#include <QJsonObject>
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

private:
    int id;
    QString name;
    double percentDone;
    double rateDownload;
    double rateUpload;
    double uploadRatio;
    int status;
    int eta;

signals:

};

#endif // TORRENT_H
