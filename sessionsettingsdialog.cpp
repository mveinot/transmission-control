#include "sessionsettingsdialog.h"
#include "ui_sessionsettingsdialog.h"

#include <QComboBox>
#include <QJsonValue>
#include <QVariant>

namespace {

bool jsonBool(const QJsonObject &object,
              const QString &key,
              bool defaultValue = false)
{
    const QJsonValue value = object.value(key);

    if (!value.isBool())
        return defaultValue;

    return value.toBool();
}

int jsonInt(const QJsonObject &object,
            const QString &key,
            int defaultValue = 0)
{
    const QJsonValue value = object.value(key);

    if (!value.isDouble())
        return defaultValue;

    return value.toInt();
}

double jsonDouble(const QJsonObject &object,
                  const QString &key,
                  double defaultValue = 0.0)
{
    const QJsonValue value = object.value(key);

    if (!value.isDouble())
        return defaultValue;

    return value.toDouble();
}

QString jsonString(const QJsonObject &object,
                   const QString &key,
                   const QString &defaultValue = QString())
{
    const QJsonValue value = object.value(key);

    if (!value.isString())
        return defaultValue;

    return value.toString();
}

QString encryptionLabelForValue(const QString &value)
{
    if (value == QStringLiteral("required"))
        return QStringLiteral("Required");

    if (value == QStringLiteral("preferred"))
        return QStringLiteral("Preferred");

    if (value == QStringLiteral("tolerated") ||
        value == QStringLiteral("allowed")) {
        return QStringLiteral("Allowed");
    }

    if (!value.isEmpty())
        return value;

    return QStringLiteral("Allowed");
}

void addIfChanged(QJsonObject &changes,
                  const QJsonObject &original,
                  const QString &key,
                  const QJsonValue &value)
{
    /*
     * Only send settings the daemon actually returned.
     * This avoids writing defaults for unsupported/newer/older fields.
     */
    if (!original.contains(key))
        return;

    if (original.value(key) != value)
        changes.insert(key, value);
}

} // namespace

SessionSettingsDialog::SessionSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SessionSettingsDialog)
{
    ui->setupUi(this);

    setWindowTitle(QStringLiteral("Transmission Session Settings"));

    populateEncryptionCombo(QStringLiteral("tolerated"));

    connect(ui->checkDownloadLimit, &QCheckBox::toggled,
            this, &SessionSettingsDialog::updateEnabledStates);

    connect(ui->checkUploadLimit, &QCheckBox::toggled,
            this, &SessionSettingsDialog::updateEnabledStates);

    connect(ui->checkAltSpeed, &QCheckBox::toggled,
            this, &SessionSettingsDialog::updateEnabledStates);

    connect(ui->checkDownloadQueue, &QCheckBox::toggled,
            this, &SessionSettingsDialog::updateEnabledStates);

    connect(ui->checkSeedQueue, &QCheckBox::toggled,
            this, &SessionSettingsDialog::updateEnabledStates);

    connect(ui->checkQueueStalled, &QCheckBox::toggled,
            this, &SessionSettingsDialog::updateEnabledStates);

    connect(ui->checkIncompleteDir, &QCheckBox::toggled,
            this, &SessionSettingsDialog::updateEnabledStates);

    connect(ui->checkSeedRatioLimit, &QCheckBox::toggled,
            this, &SessionSettingsDialog::updateEnabledStates);

    connect(ui->checkIdleSeedingLimit, &QCheckBox::toggled,
            this, &SessionSettingsDialog::updateEnabledStates);

    updateEnabledStates();
}

SessionSettingsDialog::~SessionSettingsDialog()
{
    delete ui;
}

void SessionSettingsDialog::populateEncryptionCombo(const QString &currentValue)
{
    ui->comboEncryption->clear();

    /*
     * Transmission has used both "tolerated" and "allowed" depending on RPC
     * generation/version. Preserve whichever value the daemon returned so
     * merely opening and saving the dialog does not rewrite it for no reason.
     * Because naturally one setting needed two spellings.
     */
    QString allowedValue = QStringLiteral("tolerated");

    if (currentValue == QStringLiteral("allowed"))
        allowedValue = QStringLiteral("allowed");

    ui->comboEncryption->addItem(QStringLiteral("Allowed"), allowedValue);
    ui->comboEncryption->addItem(QStringLiteral("Preferred"), QStringLiteral("preferred"));
    ui->comboEncryption->addItem(QStringLiteral("Required"), QStringLiteral("required"));

    setComboCurrentData(currentValue);
}

void SessionSettingsDialog::setComboCurrentData(const QString &value)
{
    const int index = ui->comboEncryption->findData(value);

    if (index >= 0) {
        ui->comboEncryption->setCurrentIndex(index);
        return;
    }

    if (!value.isEmpty()) {
        ui->comboEncryption->addItem(encryptionLabelForValue(value), value);
        ui->comboEncryption->setCurrentIndex(ui->comboEncryption->count() - 1);
    }
}

void SessionSettingsDialog::setSessionSettings(const QJsonObject &settings)
{
    originalSettings = settings;

    const QString encryption =
        jsonString(settings, QStringLiteral("encryption"), QStringLiteral("tolerated"));

    populateEncryptionCombo(encryption);

    ui->spinPeerPort->setValue(
        jsonInt(settings, QStringLiteral("peer-port"), 51413)
        );

    ui->checkRandomPortOnStart->setChecked(
        jsonBool(settings, QStringLiteral("peer-port-random-on-start"), false)
        );

    ui->checkPortForwarding->setChecked(
        jsonBool(settings, QStringLiteral("port-forwarding-enabled"), false)
        );

    ui->checkDht->setChecked(
        jsonBool(settings, QStringLiteral("dht-enabled"), true)
        );

    ui->checkPex->setChecked(
        jsonBool(settings, QStringLiteral("pex-enabled"), true)
        );

    ui->checkLpd->setChecked(
        jsonBool(settings, QStringLiteral("lpd-enabled"), false)
        );

    ui->spinPeerLimitGlobal->setValue(
        jsonInt(settings, QStringLiteral("peer-limit-global"), 200)
        );

    ui->spinPeerLimitPerTorrent->setValue(
        jsonInt(settings, QStringLiteral("peer-limit-per-torrent"), 50)
        );

    ui->checkDownloadLimit->setChecked(
        jsonBool(settings, QStringLiteral("speed-limit-down-enabled"), false)
        );

    ui->spinDownloadLimit->setValue(
        jsonInt(settings, QStringLiteral("speed-limit-down"), 100)
        );

    ui->checkUploadLimit->setChecked(
        jsonBool(settings, QStringLiteral("speed-limit-up-enabled"), false)
        );

    ui->spinUploadLimit->setValue(
        jsonInt(settings, QStringLiteral("speed-limit-up"), 100)
        );

    ui->checkAltSpeed->setChecked(
        jsonBool(settings, QStringLiteral("alt-speed-enabled"), false)
        );

    ui->spinAltDownloadLimit->setValue(
        jsonInt(settings, QStringLiteral("alt-speed-down"), 50)
        );

    ui->spinAltUploadLimit->setValue(
        jsonInt(settings, QStringLiteral("alt-speed-up"), 50)
        );

    ui->checkDownloadQueue->setChecked(
        jsonBool(settings, QStringLiteral("download-queue-enabled"), true)
        );

    ui->spinDownloadQueueSize->setValue(
        jsonInt(settings, QStringLiteral("download-queue-size"), 5)
        );

    ui->checkSeedQueue->setChecked(
        jsonBool(settings, QStringLiteral("seed-queue-enabled"), false)
        );

    ui->spinSeedQueueSize->setValue(
        jsonInt(settings, QStringLiteral("seed-queue-size"), 5)
        );

    ui->checkQueueStalled->setChecked(
        jsonBool(settings, QStringLiteral("queue-stalled-enabled"), true)
        );

    ui->spinQueueStalledMinutes->setValue(
        jsonInt(settings, QStringLiteral("queue-stalled-minutes"), 30)
        );

    ui->editDownloadDir->setText(
        jsonString(settings, QStringLiteral("download-dir"))
        );

    ui->checkIncompleteDir->setChecked(
        jsonBool(settings, QStringLiteral("incomplete-dir-enabled"), false)
        );

    ui->editIncompleteDir->setText(
        jsonString(settings, QStringLiteral("incomplete-dir"))
        );

    ui->checkRenamePartialFiles->setChecked(
        jsonBool(settings, QStringLiteral("rename-partial-files"), true)
        );

    ui->checkStartAddedTorrents->setChecked(
        jsonBool(settings, QStringLiteral("start-added-torrents"), true)
        );

    ui->checkSeedRatioLimit->setChecked(
        jsonBool(settings, QStringLiteral("seedRatioLimited"), false)
        );

    ui->spinSeedRatioLimit->setValue(
        jsonDouble(settings, QStringLiteral("seedRatioLimit"), 2.0)
        );

    ui->checkIdleSeedingLimit->setChecked(
        jsonBool(settings, QStringLiteral("idle-seeding-limit-enabled"), false)
        );

    ui->spinIdleSeedingLimit->setValue(
        jsonInt(settings, QStringLiteral("idle-seeding-limit"), 30)
        );

    updateEnabledStates();
}

QJsonObject SessionSettingsDialog::changedSettings() const
{
    QJsonObject changes;

    addIfChanged(changes, originalSettings,
                 QStringLiteral("peer-port"),
                 ui->spinPeerPort->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("peer-port-random-on-start"),
                 ui->checkRandomPortOnStart->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("port-forwarding-enabled"),
                 ui->checkPortForwarding->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("encryption"),
                 ui->comboEncryption->currentData().toString());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("dht-enabled"),
                 ui->checkDht->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("pex-enabled"),
                 ui->checkPex->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("lpd-enabled"),
                 ui->checkLpd->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("peer-limit-global"),
                 ui->spinPeerLimitGlobal->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("peer-limit-per-torrent"),
                 ui->spinPeerLimitPerTorrent->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("speed-limit-down-enabled"),
                 ui->checkDownloadLimit->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("speed-limit-down"),
                 ui->spinDownloadLimit->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("speed-limit-up-enabled"),
                 ui->checkUploadLimit->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("speed-limit-up"),
                 ui->spinUploadLimit->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("alt-speed-enabled"),
                 ui->checkAltSpeed->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("alt-speed-down"),
                 ui->spinAltDownloadLimit->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("alt-speed-up"),
                 ui->spinAltUploadLimit->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("download-queue-enabled"),
                 ui->checkDownloadQueue->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("download-queue-size"),
                 ui->spinDownloadQueueSize->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("seed-queue-enabled"),
                 ui->checkSeedQueue->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("seed-queue-size"),
                 ui->spinSeedQueueSize->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("queue-stalled-enabled"),
                 ui->checkQueueStalled->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("queue-stalled-minutes"),
                 ui->spinQueueStalledMinutes->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("download-dir"),
                 ui->editDownloadDir->text().trimmed());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("incomplete-dir-enabled"),
                 ui->checkIncompleteDir->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("incomplete-dir"),
                 ui->editIncompleteDir->text().trimmed());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("rename-partial-files"),
                 ui->checkRenamePartialFiles->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("start-added-torrents"),
                 ui->checkStartAddedTorrents->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("seedRatioLimited"),
                 ui->checkSeedRatioLimit->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("seedRatioLimit"),
                 ui->spinSeedRatioLimit->value());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("idle-seeding-limit-enabled"),
                 ui->checkIdleSeedingLimit->isChecked());

    addIfChanged(changes, originalSettings,
                 QStringLiteral("idle-seeding-limit"),
                 ui->spinIdleSeedingLimit->value());

    return changes;
}

void SessionSettingsDialog::updateEnabledStates()
{
    ui->spinDownloadLimit->setEnabled(
        ui->checkDownloadLimit->isChecked()
        );

    ui->spinUploadLimit->setEnabled(
        ui->checkUploadLimit->isChecked()
        );

    ui->spinAltDownloadLimit->setEnabled(
        ui->checkAltSpeed->isChecked()
        );

    ui->spinAltUploadLimit->setEnabled(
        ui->checkAltSpeed->isChecked()
        );

    ui->spinDownloadQueueSize->setEnabled(
        ui->checkDownloadQueue->isChecked()
        );

    ui->spinSeedQueueSize->setEnabled(
        ui->checkSeedQueue->isChecked()
        );

    ui->spinQueueStalledMinutes->setEnabled(
        ui->checkQueueStalled->isChecked()
        );

    ui->editIncompleteDir->setEnabled(
        ui->checkIncompleteDir->isChecked()
        );

    ui->spinSeedRatioLimit->setEnabled(
        ui->checkSeedRatioLimit->isChecked()
        );

    ui->spinIdleSeedingLimit->setEnabled(
        ui->checkIdleSeedingLimit->isChecked()
        );
}