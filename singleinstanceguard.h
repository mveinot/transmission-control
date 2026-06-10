#ifndef SINGLEINSTANCEGUARD_H
#define SINGLEINSTANCEGUARD_H

#include <QObject>
#include <QString>

class QLocalServer;

class SingleInstanceGuard : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstanceGuard(const QString &serverName, QObject *parent = nullptr);
    ~SingleInstanceGuard() override;

    bool tryStartPrimaryInstance();
    bool notifyPrimaryInstance(const QString &message = QStringLiteral("activate"));

signals:
    void activationRequested();

private:
    QString serverName;
    QLocalServer *server = nullptr;

    void handleNewConnection();
    bool canContactPrimaryInstance() const;
};

#endif // SINGLEINSTANCEGUARD_H