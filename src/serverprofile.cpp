#include "serverprofile.h"

#include "settingskeys.h"

#include <QSettings>

#include <algorithm>

namespace {

QString normalizedBackendType(QString type)
{
    type = type.trimmed().toLower();
    if (type == QStringLiteral("transmission")
        || type == QStringLiteral("qbittorrent")
        || type == QStringLiteral("deluge")) {
        return type;
    }
    return QStringLiteral("transmission");
}

} // namespace

bool ServerProfile::isValid() const
{
    return !rpcUrl.trimmed().isEmpty();
}

QString ServerProfile::displayName() const
{
    const QString trimmedName = name.trimmed();
    return trimmedName.isEmpty() ? rpcUrl.trimmed() : trimmedName;
}

QVector<ServerProfile> ServerProfileRepository::loadProfiles() const
{
    QVector<ServerProfile> profiles;
    QSettings settings;
    const int count = settings.beginReadArray(SettingsKeys::ServersArray);
    profiles.reserve(count);

    for (int index = 0; index < count; ++index) {
        settings.setArrayIndex(index);

        ServerProfile profile;
        profile.settingsIndex = index;
        profile.backendType = normalizedBackendType(
            settings.value(SettingsKeys::ServerBackendType,
                           QStringLiteral("transmission")).toString());
        profile.name = settings.value(SettingsKeys::ServerName).toString().trimmed();
        profile.rpcUrl = settings.value(SettingsKeys::ServerRpcUrl).toString().trimmed();
        profile.username = settings.value(SettingsKeys::ServerUsername).toString();
        profile.password = settings.value(SettingsKeys::ServerPassword).toString();

        const int mappingCount =
            settings.beginReadArray(SettingsKeys::ServerFolderMappingsArray);
        for (int mappingIndex = 0; mappingIndex < mappingCount; ++mappingIndex) {
            settings.setArrayIndex(mappingIndex);
            FolderMapping mapping;
            mapping.remotePath =
                settings.value(SettingsKeys::FolderMappingRemotePath).toString().trimmed();
            mapping.localPath =
                settings.value(SettingsKeys::FolderMappingLocalPath).toString().trimmed();
            if (!mapping.remotePath.isEmpty() || !mapping.localPath.isEmpty())
                profile.folderMappings.append(mapping);
        }
        settings.endArray();
        profiles.append(profile);
    }

    settings.endArray();
    return profiles;
}

void ServerProfileRepository::saveProfiles(
    const QVector<ServerProfile> &profiles) const
{
    QSettings settings;
    settings.beginWriteArray(SettingsKeys::ServersArray, profiles.size());

    for (int index = 0; index < profiles.size(); ++index) {
        const ServerProfile &profile = profiles.at(index);
        settings.setArrayIndex(index);
        settings.setValue(SettingsKeys::ServerName, profile.name);
        settings.setValue(SettingsKeys::ServerBackendType,
                          normalizedBackendType(profile.backendType));
        settings.setValue(SettingsKeys::ServerRpcUrl, profile.rpcUrl);
        settings.setValue(SettingsKeys::ServerUsername, profile.username);
        settings.setValue(SettingsKeys::ServerPassword, profile.password);

        settings.beginWriteArray(SettingsKeys::ServerFolderMappingsArray,
                                 profile.folderMappings.size());
        for (int mappingIndex = 0;
             mappingIndex < profile.folderMappings.size();
             ++mappingIndex) {
            settings.setArrayIndex(mappingIndex);
            const FolderMapping &mapping =
                profile.folderMappings.at(mappingIndex);
            settings.setValue(SettingsKeys::FolderMappingRemotePath,
                              mapping.remotePath);
            settings.setValue(SettingsKeys::FolderMappingLocalPath,
                              mapping.localPath);
        }
        settings.endArray();
    }

    settings.endArray();
    settings.sync();
}

ServerProfile ServerProfileRepository::profileAtSettingsIndex(int index) const
{
    const QVector<ServerProfile> profiles = loadProfiles();
    for (const ServerProfile &profile : profiles) {
        if (profile.settingsIndex == index)
            return profile;
    }
    return {};
}

int ServerProfileRepository::defaultIndex() const
{
    return QSettings().value(SettingsKeys::ServersDefaultIndex, -1).toInt();
}

int ServerProfileRepository::currentIndex() const
{
    return QSettings().value(SettingsKeys::ServersCurrentIndex, -1).toInt();
}

int ServerProfileRepository::preferredIndex() const
{
    const QVector<ServerProfile> profiles = loadProfiles();
    const auto containsIndex = [&profiles](int index) {
        return std::any_of(profiles.cbegin(), profiles.cend(),
                           [index](const ServerProfile &profile) {
                               return profile.settingsIndex == index
                                      && profile.isValid();
                           });
    };

    const int configuredDefault = defaultIndex();
    if (containsIndex(configuredDefault))
        return configuredDefault;

    const int configuredCurrent = currentIndex();
    if (containsIndex(configuredCurrent))
        return configuredCurrent;

    for (const ServerProfile &profile : profiles) {
        if (profile.isValid())
            return profile.settingsIndex;
    }
    return -1;
}

void ServerProfileRepository::setCurrentIndex(int index) const
{
    QSettings settings;
    settings.setValue(SettingsKeys::ServersCurrentIndex, index);
    settings.sync();
}

void ServerProfileRepository::setDefaultIndex(int index) const
{
    QSettings settings;
    settings.setValue(SettingsKeys::ServersDefaultIndex, index);
    settings.sync();
}
