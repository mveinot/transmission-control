#include "serverselectioncontroller.h"

#include "torrentbackend.h"

#include <QComboBox>
#include <QSignalBlocker>

#include <utility>

ServerSelectionController::ServerSelectionController(QComboBox *selector,
                                                     TorrentBackend *backend,
                                                     QObject *parent)
    : QObject(parent)
    , m_selector(selector)
    , m_backend(backend)
{
    if (!m_selector)
        return;

    connect(m_selector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                activateCurrent();
            });
}

void ServerSelectionController::reloadProfiles()
{
    if (!m_selector)
        return;

    const int previousSettingsIndex = currentProfile().settingsIndex;
    const bool hadPreviousSelection = previousSettingsIndex >= 0;
    m_profiles = m_repository.loadProfiles();
    const int defaultIndex = m_repository.defaultIndex();

    QSignalBlocker blocker(m_selector);
    m_selector->clear();

    for (const ServerProfile &profile : std::as_const(m_profiles)) {
        m_selector->addItem(profileLabel(profile, defaultIndex),
                            profile.settingsIndex);
        m_selector->setItemData(m_selector->count() - 1,
                                profile.backendType,
                                Qt::UserRole + 1);
    }

    if (m_profiles.isEmpty()) {
        m_selector->addItem(tr("No servers configured"), -1);
        m_selector->setEnabled(false);
        return;
    }

    m_selector->setEnabled(true);
    int comboIndex = hadPreviousSelection
                         ? comboIndexForSettingsIndex(previousSettingsIndex)
                         : -1;
    if (comboIndex < 0)
        comboIndex = comboIndexForSettingsIndex(m_repository.preferredIndex());
    if (comboIndex < 0)
        comboIndex = 0;
    m_selector->setCurrentIndex(comboIndex);
}

bool ServerSelectionController::activateCurrent(bool notify)
{
    if (!m_backend)
        return false;

    const ServerProfile profile = currentProfile();
    if (!profile.isValid() || !m_backend->setServerProfile(profile)) {
        if (notify)
            emit activationFailed(tr("Could not switch server."));
        return false;
    }

    m_repository.setCurrentIndex(profile.settingsIndex);
    if (notify)
        emit serverActivated(profile);
    return true;
}

bool ServerSelectionController::selectSettingsIndex(int settingsIndex)
{
    if (!m_selector)
        return false;

    const int comboIndex = comboIndexForSettingsIndex(settingsIndex);
    if (comboIndex < 0)
        return false;

    if (m_selector->currentIndex() == comboIndex)
        return activateCurrent();

    m_selector->setCurrentIndex(comboIndex);
    return true;
}

ServerProfile ServerSelectionController::currentProfile() const
{
    if (!m_selector)
        return {};

    const int settingsIndex = m_selector->currentData().toInt();
    for (const ServerProfile &profile : m_profiles) {
        if (profile.settingsIndex == settingsIndex)
            return profile;
    }
    return {};
}

QList<FolderMapping> ServerSelectionController::currentFolderMappings() const
{
    return currentProfile().folderMappings;
}

QString ServerSelectionController::currentDisplayText() const
{
    return m_selector ? m_selector->currentText() : QString();
}

bool ServerSelectionController::hasProfiles() const
{
    return !m_profiles.isEmpty();
}

int ServerSelectionController::comboIndexForSettingsIndex(int settingsIndex) const
{
    return m_selector ? m_selector->findData(settingsIndex) : -1;
}

QString ServerSelectionController::profileLabel(const ServerProfile &profile,
                                                int defaultIndex) const
{
    QString name = profile.displayName();
    if (name.isEmpty())
        name = tr("(unnamed server)");

    QString backendName = tr("Transmission");
    if (profile.backendType == QStringLiteral("qbittorrent"))
        backendName = tr("qBittorrent");
    else if (profile.backendType == QStringLiteral("deluge"))
        backendName = tr("Deluge");

    if (profile.settingsIndex == defaultIndex)
        name += tr(" (default)");
    return tr("%1 — %2").arg(name, backendName);
}
