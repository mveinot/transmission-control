#ifndef SERVERPROFILE_H
#define SERVERPROFILE_H

#include "foldermapping.h"

#include <QList>
#include <QString>
#include <QUuid>
#include <QVector>

// Backend-neutral connection definition. id is stable across array reordering;
// settingsIndex records the current position in the persisted profile array.
struct ServerProfile
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    int settingsIndex = -1;
    QString backendType = QStringLiteral("transmission");
    QString name;
    QString rpcUrl;
    QString username;
    QString password;
    QList<FolderMapping> folderMappings;

    bool isValid() const;
    QString displayName() const;
};

// Sole decoder for the legacy-compatible QSettings server array. Callers work
// with typed snapshots instead of independently interpreting persistent keys.
class ServerProfileRepository
{
public:
    QVector<ServerProfile> loadProfiles() const;
    bool saveProfiles(const QVector<ServerProfile> &profiles) const;
    ServerProfile profileAtSettingsIndex(int index) const;

    int defaultIndex() const;
    int currentIndex() const;
    int preferredIndex() const;
    bool setDefaultIndex(int index) const;
    bool setCurrentIndex(int index) const;
};

#endif // SERVERPROFILE_H
