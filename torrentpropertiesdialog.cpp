#include "torrentpropertiesdialog.h"

#include "torrentbackend.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {
constexpr int GlobalMode = 0;
constexpr int SingleMode = 1;
constexpr int UnlimitedMode = 2;

QStringList labelsFromJsonArray(const QJsonArray &array)
{
    QStringList labels;

    for (const QJsonValue &value : array) {
        const QString label = value.toString().trimmed();

        if (!label.isEmpty())
            labels.append(label);
    }

    return labels;
}

QJsonArray labelsToJsonArray(const QString &text)
{
    QJsonArray labels;
    const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);

    for (const QString &part : parts) {
        const QString label = part.trimmed();

        if (!label.isEmpty())
            labels.append(label);
    }

    return labels;
}

int comboIndexForData(QComboBox *combo, int value)
{
    const int index = combo->findData(value);
    return index >= 0 ? index : 0;
}
}

TorrentPropertiesDialog::TorrentPropertiesDialog(TorrentBackend *client,
                                                 TorrentKey torrentKey,
                                                 QWidget *parent)
    : QDialog(parent)
    , m_client(client)
    , m_torrentKey(torrentKey)
{
    buildUi();
    setControlsEnabled(false);

    connect(m_client, &TorrentBackend::torrentPropertiesReceived,
            this, &TorrentPropertiesDialog::handlePropertiesReceived);

    connect(m_client, &TorrentBackend::commandSucceeded,
            this, &TorrentPropertiesDialog::handleCommandSucceeded);

    connect(m_client, &TorrentBackend::commandFailed,
            this, &TorrentPropertiesDialog::handleCommandFailed);

    QTimer::singleShot(0, this, [this]() {
        if (!m_client || !isValidTorrentKey(m_torrentKey))
            return;

        m_statusLabel->setText(tr("Loading torrent properties…"));
        m_client->getTorrentProperties(m_torrentKey);
    });
}

void TorrentPropertiesDialog::buildUi()
{
    setWindowTitle(tr("Torrent Properties"));
    resize(760, 560);

    auto *mainLayout = new QVBoxLayout(this);

    m_headerLabel = new QLabel(tr("Loading torrent properties…"), this);
    m_headerLabel->setWordWrap(true);
    mainLayout->addWidget(m_headerLabel);

    auto *tabs = new QTabWidget(this);
    mainLayout->addWidget(tabs, 1);

    auto *limitsTab = new QWidget(tabs);
    auto *limitsLayout = new QVBoxLayout(limitsTab);

    auto *bandwidthGroup = new QGroupBox(tr("Bandwidth and Queue"), limitsTab);
    auto *bandwidthForm = new QFormLayout(bandwidthGroup);

    m_bandwidthPriorityCombo = new QComboBox(bandwidthGroup);
    m_bandwidthPriorityCombo->addItem(tr("Low"), -1);
    m_bandwidthPriorityCombo->addItem(tr("Normal"), 0);
    m_bandwidthPriorityCombo->addItem(tr("High"), 1);
    bandwidthForm->addRow(tr("Bandwidth priority:"), m_bandwidthPriorityCombo);

    m_honorsSessionLimitsCheckBox = new QCheckBox(
        tr("Honor global session speed limits"),
        bandwidthGroup
        );
    bandwidthForm->addRow(QString(), m_honorsSessionLimitsCheckBox);

    m_queuePositionSpinBox = new QSpinBox(bandwidthGroup);
    m_queuePositionSpinBox->setRange(0, 999999);
    bandwidthForm->addRow(tr("Queue position:"), m_queuePositionSpinBox);

    m_peerLimitSpinBox = new QSpinBox(bandwidthGroup);
    m_peerLimitSpinBox->setRange(-1, 1000000);
    m_peerLimitSpinBox->setSpecialValueText(tr("Use session default"));
    bandwidthForm->addRow(tr("Peer limit:"), m_peerLimitSpinBox);

    limitsLayout->addWidget(bandwidthGroup);

    auto *speedGroup = new QGroupBox(tr("Speed Limits"), limitsTab);
    auto *speedForm = new QFormLayout(speedGroup);

    m_downloadLimitedCheckBox = new QCheckBox(tr("Limit download speed"), speedGroup);
    m_downloadLimitSpinBox = new QSpinBox(speedGroup);
    m_downloadLimitSpinBox->setRange(0, 10000000);
    m_downloadLimitSpinBox->setSuffix(tr(" KB/s"));
    speedForm->addRow(m_downloadLimitedCheckBox, m_downloadLimitSpinBox);

    m_uploadLimitedCheckBox = new QCheckBox(tr("Limit upload speed"), speedGroup);
    m_uploadLimitSpinBox = new QSpinBox(speedGroup);
    m_uploadLimitSpinBox->setRange(0, 10000000);
    m_uploadLimitSpinBox->setSuffix(tr(" KB/s"));
    speedForm->addRow(m_uploadLimitedCheckBox, m_uploadLimitSpinBox);

    connect(m_downloadLimitedCheckBox, &QCheckBox::toggled,
            m_downloadLimitSpinBox, &QSpinBox::setEnabled);

    connect(m_uploadLimitedCheckBox, &QCheckBox::toggled,
            m_uploadLimitSpinBox, &QSpinBox::setEnabled);

    limitsLayout->addWidget(speedGroup);
    limitsLayout->addStretch(1);

    tabs->addTab(limitsTab, tr("Limits"));

    auto *seedingTab = new QWidget(tabs);
    auto *seedingLayout = new QVBoxLayout(seedingTab);

    auto *ratioGroup = new QGroupBox(tr("Ratio"), seedingTab);
    auto *ratioForm = new QFormLayout(ratioGroup);

    m_seedRatioModeCombo = new QComboBox(ratioGroup);
    m_seedRatioModeCombo->addItem(tr("Use global setting"), GlobalMode);
    m_seedRatioModeCombo->addItem(tr("Stop at ratio"), SingleMode);
    m_seedRatioModeCombo->addItem(tr("Seed regardless of ratio"), UnlimitedMode);
    ratioForm->addRow(tr("Ratio mode:"), m_seedRatioModeCombo);

    m_seedRatioLimitSpinBox = new QDoubleSpinBox(ratioGroup);
    m_seedRatioLimitSpinBox->setRange(0.0, 1000000.0);
    m_seedRatioLimitSpinBox->setDecimals(2);
    m_seedRatioLimitSpinBox->setSingleStep(0.1);
    ratioForm->addRow(tr("Ratio limit:"), m_seedRatioLimitSpinBox);

    seedingLayout->addWidget(ratioGroup);

    auto *idleGroup = new QGroupBox(tr("Idle Seeding"), seedingTab);
    auto *idleForm = new QFormLayout(idleGroup);

    m_seedIdleModeCombo = new QComboBox(idleGroup);
    m_seedIdleModeCombo->addItem(tr("Use global setting"), GlobalMode);
    m_seedIdleModeCombo->addItem(tr("Stop when idle"), SingleMode);
    m_seedIdleModeCombo->addItem(tr("Seed regardless of idle time"), UnlimitedMode);
    idleForm->addRow(tr("Idle mode:"), m_seedIdleModeCombo);

    m_seedIdleLimitSpinBox = new QSpinBox(idleGroup);
    m_seedIdleLimitSpinBox->setRange(0, 1000000);
    m_seedIdleLimitSpinBox->setSuffix(tr(" min"));
    idleForm->addRow(tr("Idle limit:"), m_seedIdleLimitSpinBox);

    seedingLayout->addWidget(idleGroup);
    seedingLayout->addStretch(1);

    tabs->addTab(seedingTab, tr("Seeding"));

    auto *labelsTab = new QWidget(tabs);
    auto *labelsLayout = new QVBoxLayout(labelsTab);

    auto *labelsGroup = new QGroupBox(tr("Organization"), labelsTab);
    auto *labelsForm = new QFormLayout(labelsGroup);

    m_labelsEdit = new QLineEdit(labelsGroup);
    m_labelsEdit->setPlaceholderText(tr("Comma-separated labels"));
    labelsForm->addRow(tr("Labels:"), m_labelsEdit);

    m_groupEdit = new QLineEdit(labelsGroup);
    labelsForm->addRow(tr("Group:"), m_groupEdit);

    labelsLayout->addWidget(labelsGroup);

    auto *labelsNote = new QLabel(
        tr("Labels and group support depend on the Transmission server version."),
        labelsTab
        );
    labelsNote->setWordWrap(true);
    labelsLayout->addWidget(labelsNote);
    labelsLayout->addStretch(1);

    tabs->addTab(labelsTab, tr("Labels"));

    auto *rawTab = new QWidget(tabs);
    auto *rawLayout = new QVBoxLayout(rawTab);

    m_rawTreeWidget = new QTreeWidget(rawTab);
    m_rawTreeWidget->setColumnCount(3);
    m_rawTreeWidget->setHeaderLabels({ tr("Property"), tr("Value"), tr("Type") });
    m_rawTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_rawTreeWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_rawTreeWidget->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    rawLayout->addWidget(m_rawTreeWidget);

    tabs->addTab(rawTab, tr("Raw RPC"));

    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel,
        this
        );
    mainLayout->addWidget(m_buttonBox);

    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &TorrentPropertiesDialog::applyChanges);

    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &TorrentPropertiesDialog::accept);

    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &TorrentPropertiesDialog::reject);
}

void TorrentPropertiesDialog::setControlsEnabled(bool enabled)
{
    const QList<QWidget *> widgets = {
        m_bandwidthPriorityCombo,
        m_honorsSessionLimitsCheckBox,
        m_queuePositionSpinBox,
        m_peerLimitSpinBox,
        m_downloadLimitedCheckBox,
        m_uploadLimitedCheckBox,
        m_seedRatioModeCombo,
        m_seedRatioLimitSpinBox,
        m_seedIdleModeCombo,
        m_seedIdleLimitSpinBox,
        m_labelsEdit,
        m_groupEdit
    };

    for (QWidget *widget : widgets) {
        if (widget)
            widget->setEnabled(enabled);
    }

    if (m_downloadLimitSpinBox)
        m_downloadLimitSpinBox->setEnabled(enabled && m_downloadLimitedCheckBox->isChecked());

    if (m_uploadLimitSpinBox)
        m_uploadLimitSpinBox->setEnabled(enabled && m_uploadLimitedCheckBox->isChecked());

    if (m_buttonBox) {
        if (auto *okButton = m_buttonBox->button(QDialogButtonBox::Ok))
            okButton->setEnabled(enabled);

        if (auto *applyButton = m_buttonBox->button(QDialogButtonBox::Apply))
            applyButton->setEnabled(enabled);
    }
}

void TorrentPropertiesDialog::handlePropertiesReceived(const TorrentProperties &properties)
{
    // Property requests share a signal; accept only this dialog's torrent.
    if (properties.key != m_torrentKey)
        return;

    m_properties = properties;
    m_loaded = true;

    populateControls(properties);
    populateRawTree(QJsonObject::fromVariantMap(properties.fields));
    setControlsEnabled(true);

    m_statusLabel->setText(tr("Properties loaded."));
}

void TorrentPropertiesDialog::populateControls(const TorrentProperties &properties)
{
    m_headerLabel->setText(
        tr("%1\nHash: %2")
            .arg(properties.name.isEmpty() ? tr("Unknown torrent") : properties.name,
                 properties.hashString.isEmpty() ? tr("Unknown") : properties.hashString)
        );

    m_bandwidthPriorityCombo->setCurrentIndex(
        comboIndexForData(m_bandwidthPriorityCombo, properties.bandwidthPriority)
        );

    m_honorsSessionLimitsCheckBox->setChecked(properties.honorsSessionLimits);
    m_queuePositionSpinBox->setValue(properties.queuePosition);
    m_peerLimitSpinBox->setValue(properties.peerLimit);
    m_downloadLimitedCheckBox->setChecked(properties.downloadLimited);
    m_downloadLimitSpinBox->setValue(properties.downloadLimit);
    m_uploadLimitedCheckBox->setChecked(properties.uploadLimited);
    m_uploadLimitSpinBox->setValue(properties.uploadLimit);
    m_seedRatioModeCombo->setCurrentIndex(
        comboIndexForData(m_seedRatioModeCombo, properties.seedRatioMode)
        );
    m_seedRatioLimitSpinBox->setValue(properties.seedRatioLimit);
    m_seedIdleModeCombo->setCurrentIndex(
        comboIndexForData(m_seedIdleModeCombo, properties.seedIdleMode)
        );
    m_seedIdleLimitSpinBox->setValue(properties.seedIdleLimit);
    m_labelsEdit->setText(properties.labels.join(QStringLiteral(", ")));
    m_groupEdit->setText(properties.group);
}

void TorrentPropertiesDialog::populateRawTree(const QJsonObject &properties)
{
    m_rawTreeWidget->clear();

    const QStringList keys = properties.keys();

    for (const QString &key : keys)
        addJsonTreeItem(nullptr, key, properties.value(key));

    m_rawTreeWidget->sortItems(0, Qt::AscendingOrder);
}

void TorrentPropertiesDialog::addJsonTreeItem(QTreeWidgetItem *parent,
                                              const QString &key,
                                              const QJsonValue &value)
{
    auto *item = parent
        ? new QTreeWidgetItem(parent)
        : new QTreeWidgetItem(m_rawTreeWidget);

    item->setText(0, key);
    item->setText(1, jsonValueDisplayText(value));
    item->setText(2, jsonValueTypeName(value));

    if (value.isObject()) {
        const QJsonObject object = value.toObject();

        for (const QString &childKey : object.keys())
            addJsonTreeItem(item, childKey, object.value(childKey));
    } else if (value.isArray()) {
        const QJsonArray array = value.toArray();

        for (int i = 0; i < array.size(); ++i)
            addJsonTreeItem(item, QString::number(i), array.at(i));
    }
}

QString TorrentPropertiesDialog::jsonValueTypeName(const QJsonValue &value)
{
    if (value.isBool())
        return tr("Boolean");

    if (value.isDouble())
        return tr("Number");

    if (value.isString())
        return tr("String");

    if (value.isArray())
        return tr("Array");

    if (value.isObject())
        return tr("Object");

    if (value.isNull())
        return tr("Null");

    return tr("Undefined");
}

QString TorrentPropertiesDialog::jsonValueDisplayText(const QJsonValue &value)
{
    if (value.isBool())
        return value.toBool() ? tr("true") : tr("false");

    if (value.isDouble())
        return QLocale().toString(value.toDouble());

    if (value.isString())
        return value.toString();

    if (value.isArray())
        return tr("%n item(s)", nullptr, value.toArray().size());

    if (value.isObject())
        return tr("%n field(s)", nullptr, value.toObject().size());

    if (value.isNull())
        return tr("null");

    return QString();
}

TorrentPropertyChanges TorrentPropertiesDialog::editedProperties() const
{
    TorrentPropertyChanges properties;
    properties.bandwidthPriority = m_bandwidthPriorityCombo->currentData().toInt();
    properties.honorsSessionLimits = m_honorsSessionLimitsCheckBox->isChecked();
    properties.queuePosition = m_queuePositionSpinBox->value();
    properties.peerLimit = m_peerLimitSpinBox->value();
    properties.downloadLimited = m_downloadLimitedCheckBox->isChecked();
    properties.downloadLimit = m_downloadLimitSpinBox->value();
    properties.uploadLimited = m_uploadLimitedCheckBox->isChecked();
    properties.uploadLimit = m_uploadLimitSpinBox->value();
    properties.seedRatioMode = m_seedRatioModeCombo->currentData().toInt();
    properties.seedRatioLimit = m_seedRatioLimitSpinBox->value();
    properties.seedIdleMode = m_seedIdleModeCombo->currentData().toInt();
    properties.seedIdleLimit = m_seedIdleLimitSpinBox->value();
    properties.labels = labelsFromJsonArray(labelsToJsonArray(m_labelsEdit->text()));

    const QString group = m_groupEdit->text().trimmed();

    properties.setGroup = !group.isEmpty() || m_properties.hasGroup;
    properties.group = group;

    return properties;
}

void TorrentPropertiesDialog::applyChanges()
{
    if (!m_client || !isValidTorrentKey(m_torrentKey) || !m_loaded)
        return;

    m_statusLabel->setText(tr("Applying torrent properties…"));
    m_client->setTorrentProperties(m_torrentKey, editedProperties());
}

void TorrentPropertiesDialog::accept()
{
    applyChanges();
    QDialog::accept();
}

void TorrentPropertiesDialog::handleCommandSucceeded(const QString &method)
{
    if (method != QStringLiteral("torrent-set"))
        return;

    m_statusLabel->setText(tr("Torrent properties updated."));

    if (m_client && isValidTorrentKey(m_torrentKey))
        m_client->getTorrentProperties(m_torrentKey);
}

void TorrentPropertiesDialog::handleCommandFailed(const QString &method,
                                                  const QString &message)
{
    if (method != QStringLiteral("torrent-set"))
        return;

    m_statusLabel->setText(
        tr("Could not update torrent properties: %1").arg(message)
        );
}
