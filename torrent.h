#ifndef TORRENT_H
#define TORRENT_H

#include <QObject>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>

class torrent
{

public:
    explicit torrent(QJsonValue val /*, QObject *parent = nullptr*/);
    QString getName() const;
    double getPercentDone() const;
    QString getStatus() const;
    QString getRateDownload() const;
    QString getRateUpload() const;
    QString getUploadRatio() const;
    QString getEta() const;
    int getId() const;

private:
    int id = 0;
    QString name;
    double percentDone = 0.0;
    double rateDownload = 0.0;
    double rateUpload = 0.0;
    double uploadRatio =0.0;
    QJsonValue files;
    QJsonValue peers;
    int status = 0;
    int eta = 0;

signals:

};

#endif // TORRENT_H
