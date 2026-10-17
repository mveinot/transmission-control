#ifndef SERVERSELECTIONCONTROLLER_H
#define SERVERSELECTIONCONTROLLER_H

#include "serverprofile.h"

#include <QObject>

class QComboBox;
class TorrentBackend;

// Owns the main-window server selector and its persistence contract. The
// window reacts to successful activations but does not decode profile storage.
class ServerSelectionController : public QObject
{
    Q_OBJECT

public:
    ServerSelectionController(QComboBox *selector,
                              TorrentBackend *backend,
                              QObject *parent = nullptr);

    void reloadProfiles();
    bool activateCurrent(bool notify = true);
    bool selectSettingsIndex(int settingsIndex);

    ServerProfile currentProfile() const;
    QList<FolderMapping> currentFolderMappings() const;
    QString currentDisplayText() const;
    bool hasProfiles() const;

signals:
    void serverActivated(const ServerProfile &profile);
    void activationFailed(const QString &message);

private:
    QComboBox *m_selector = nullptr;
    TorrentBackend *m_backend = nullptr;
    ServerProfileRepository m_repository;
    QVector<ServerProfile> m_profiles;

    int comboIndexForSettingsIndex(int settingsIndex) const;
    QString profileLabel(const ServerProfile &profile, int defaultIndex) const;
};

#endif // SERVERSELECTIONCONTROLLER_H
