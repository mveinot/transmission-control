#include <QtTest/QtTest>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>

#include "sessionsettingsdialog.h"

namespace {

QJsonObject fullSessionSettings(const QString &encryption = QStringLiteral("tolerated"))
{
    return {
        {"peer-port", 51413},
        {"peer-port-random-on-start", false},
        {"port-forwarding-enabled", true},
        {"encryption", encryption},
        {"dht-enabled", true},
        {"pex-enabled", true},
        {"lpd-enabled", false},
        {"peer-limit-global", 200},
        {"peer-limit-per-torrent", 50},
        {"speed-limit-down-enabled", false},
        {"speed-limit-down", 100},
        {"speed-limit-up-enabled", false},
        {"speed-limit-up", 100},
        {"alt-speed-enabled", false},
        {"alt-speed-down", 50},
        {"alt-speed-up", 50},
        {"download-queue-enabled", true},
        {"download-queue-size", 5},
        {"seed-queue-enabled", true},
        {"seed-queue-size", 3},
        {"queue-stalled-enabled", true},
        {"queue-stalled-minutes", 30},
        {"download-dir", "/srv/downloads"},
        {"incomplete-dir-enabled", false},
        {"incomplete-dir", "/srv/downloads/incomplete"},
        {"rename-partial-files", true},
        {"start-added-torrents", true},
        {"seedRatioLimited", false},
        {"seedRatioLimit", 2.0},
        {"idle-seeding-limit-enabled", false},
        {"idle-seeding-limit", 30},
    };
}

template <typename T>
T *requiredChild(QObject &parent, const char *name)
{
    T *child = parent.findChild<T *>(QString::fromLatin1(name));
    Q_ASSERT(child);
    return child;
}

} // namespace

class TestSessionSettingsDialog : public QObject
{
    Q_OBJECT

private slots:
    void unchangedSettingsProduceNoChanges();
    void changedSettingsContainOnlyChangedReturnedFields();
    void preservesAllowedEncryptionSpelling();
    void togglesEnableDependentControls();
};

void TestSessionSettingsDialog::unchangedSettingsProduceNoChanges()
{
    SessionSettingsDialog dialog;
    dialog.setSessionSettings(fullSessionSettings());

    QVERIFY(dialog.changedSettings().isEmpty());
}

void TestSessionSettingsDialog::changedSettingsContainOnlyChangedReturnedFields()
{
    SessionSettingsDialog dialog;
    dialog.setSessionSettings(fullSessionSettings());

    requiredChild<QSpinBox>(dialog, "spinPeerPort")->setValue(50000);
    requiredChild<QCheckBox>(dialog, "checkDht")->setChecked(false);
    requiredChild<QLineEdit>(dialog, "editDownloadDir")->setText(QStringLiteral("/data/torrents"));
    requiredChild<QDoubleSpinBox>(dialog, "spinSeedRatioLimit")->setValue(1.75);

    const QJsonObject changes = dialog.changedSettings();

    QCOMPARE(changes.size(), 4);
    QCOMPARE(changes.value("peer-port").toInt(), 50000);
    QCOMPARE(changes.value("dht-enabled").toBool(), false);
    QCOMPARE(changes.value("download-dir").toString(), QStringLiteral("/data/torrents"));
    QCOMPARE(changes.value("seedRatioLimit").toDouble(), 1.75);
    QVERIFY(!changes.contains("speed-limit-down"));
}

void TestSessionSettingsDialog::preservesAllowedEncryptionSpelling()
{
    SessionSettingsDialog dialog;
    dialog.setSessionSettings(fullSessionSettings(QStringLiteral("allowed")));

    auto *comboEncryption = requiredChild<QComboBox>(dialog, "comboEncryption");
    QCOMPARE(comboEncryption->currentData().toString(), QStringLiteral("allowed"));
    QVERIFY(dialog.changedSettings().isEmpty());

    comboEncryption->setCurrentIndex(comboEncryption->findData(QStringLiteral("required")));

    const QJsonObject changes = dialog.changedSettings();
    QCOMPARE(changes.value("encryption").toString(), QStringLiteral("required"));
}

void TestSessionSettingsDialog::togglesEnableDependentControls()
{
    SessionSettingsDialog dialog;
    dialog.setSessionSettings(fullSessionSettings());

    auto *checkAltSpeed = requiredChild<QCheckBox>(dialog, "checkAltSpeed");
    auto *spinAltDownloadLimit = requiredChild<QSpinBox>(dialog, "spinAltDownloadLimit");
    auto *checkIncompleteDir = requiredChild<QCheckBox>(dialog, "checkIncompleteDir");
    auto *editIncompleteDir = requiredChild<QLineEdit>(dialog, "editIncompleteDir");
    auto *checkSeedRatioLimit = requiredChild<QCheckBox>(dialog, "checkSeedRatioLimit");
    auto *spinSeedRatioLimit = requiredChild<QDoubleSpinBox>(dialog, "spinSeedRatioLimit");

    QVERIFY(!checkAltSpeed->isChecked());
    QVERIFY(!spinAltDownloadLimit->isEnabled());
    checkAltSpeed->setChecked(true);
    QVERIFY(spinAltDownloadLimit->isEnabled());

    QVERIFY(!checkIncompleteDir->isChecked());
    QVERIFY(!editIncompleteDir->isEnabled());
    checkIncompleteDir->setChecked(true);
    QVERIFY(editIncompleteDir->isEnabled());

    QVERIFY(!checkSeedRatioLimit->isChecked());
    QVERIFY(!spinSeedRatioLimit->isEnabled());
    checkSeedRatioLimit->setChecked(true);
    QVERIFY(spinSeedRatioLimit->isEnabled());
}

QTEST_MAIN(TestSessionSettingsDialog)
#include "test_sessionsettingsdialog.moc"
