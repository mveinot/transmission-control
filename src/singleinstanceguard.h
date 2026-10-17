#ifndef SINGLEINSTANCEGUARD_H
#define SINGLEINSTANCEGUARD_H

#include <QObject>
#include <QString>
#include <QStringList>

class QLocalServer;

// Owns the local IPC endpoint used to elect a primary process and forward
// activation/open requests from later launches.
class SingleInstanceGuard : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstanceGuard(const QString &serverName, QObject *parent = nullptr);
    ~SingleInstanceGuard() override;

    bool tryStartPrimaryInstance();
    bool notifyPrimaryInstance(const QString &message = QStringLiteral("activate"));
    bool notifyPrimaryInstance(const QStringList &arguments);

signals:
    void activationRequested();
    void openRequested(const QStringList &arguments);

private:
    QString serverName;
    QLocalServer *server = nullptr;

    void handleNewConnection();
    bool canContactPrimaryInstance() const;
};

#endif // SINGLEINSTANCEGUARD_H
