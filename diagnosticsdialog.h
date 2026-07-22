#ifndef DIAGNOSTICSDIALOG_H
#define DIAGNOSTICSDIALOG_H

#include <QDialog>
#include <QJsonObject>

class GeoIpService;
class QTextEdit;

// Read-only snapshot of client, server, build, and GeoIP state intended for
// troubleshooting, clipboard export, or attachment to a composed support email.
class DiagnosticsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DiagnosticsDialog(const QJsonObject &sessionSettings,
                               const QString &serverName,
                               const QString &rpcUrl,
                               GeoIpService *geoIpService,
                               int refreshIntervalMs,
                               QWidget *parent = nullptr);

private:
    QString buildReport() const;
    QJsonObject sessionSettings;
    QString serverName;
    QString rpcUrl;
    GeoIpService *geoIpService = nullptr;
    int refreshIntervalMs = 0;
    QTextEdit *reportEdit = nullptr;
};

#endif
