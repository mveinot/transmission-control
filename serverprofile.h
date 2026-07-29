#ifndef SERVERPROFILE_H
#define SERVERPROFILE_H

#include "foldermapping.h"

#include <QList>
#include <QString>
#include <QVector>

// Backend-neutral connection definition. The persistent array index remains
// metadata rather than identity so profiles can be validated before use.
struct ServerProfile
{
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
    void saveProfiles(const QVector<ServerProfile> &profiles) const;
    ServerProfile profileAtSettingsIndex(int index) const;

    int defaultIndex() const;
    int currentIndex() const;
    int preferredIndex() const;
    void setDefaultIndex(int index) const;
    void setCurrentIndex(int index) const;
};

#endif // SERVERPROFILE_H
