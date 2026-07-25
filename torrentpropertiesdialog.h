#ifndef TORRENTPROPERTIESDIALOG_H
#define TORRENTPROPERTIESDIALOG_H

#include <QDialog>
#include <QJsonObject>

#include "torrentkey.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;
class TorrentBackend;

// Transactional editor for per-torrent limits and organization fields, with a
// raw RPC view for diagnostics. Apply refreshes the server-confirmed baseline.
class TorrentPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TorrentPropertiesDialog(TorrentBackend *client,
                                     TorrentKey torrentKey,
                                     QWidget *parent = nullptr);

private slots:
    void handlePropertiesReceived(TorrentKey torrentKey, const QJsonObject &properties);
    void handleCommandSucceeded(const QString &method);
    void handleCommandFailed(const QString &method, const QString &message);
    void applyChanges();
    void accept() override;

private:
    void buildUi();
    void setControlsEnabled(bool enabled);
    void populateControls(const QJsonObject &properties);
    void populateRawTree(const QJsonObject &properties);
    void addJsonTreeItem(QTreeWidgetItem *parent,
                         const QString &key,
                         const QJsonValue &value);
    QJsonObject editedProperties() const;

    static QString jsonValueTypeName(const QJsonValue &value);
    static QString jsonValueDisplayText(const QJsonValue &value);

    TorrentBackend *m_client = nullptr;
    TorrentKey m_torrentKey;
    QJsonObject m_properties;
    bool m_loaded = false;

    QLabel *m_headerLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;

    QComboBox *m_bandwidthPriorityCombo = nullptr;
    QCheckBox *m_honorsSessionLimitsCheckBox = nullptr;
    QSpinBox *m_queuePositionSpinBox = nullptr;
    QSpinBox *m_peerLimitSpinBox = nullptr;

    QCheckBox *m_downloadLimitedCheckBox = nullptr;
    QSpinBox *m_downloadLimitSpinBox = nullptr;
    QCheckBox *m_uploadLimitedCheckBox = nullptr;
    QSpinBox *m_uploadLimitSpinBox = nullptr;

    QComboBox *m_seedRatioModeCombo = nullptr;
    QDoubleSpinBox *m_seedRatioLimitSpinBox = nullptr;
    QComboBox *m_seedIdleModeCombo = nullptr;
    QSpinBox *m_seedIdleLimitSpinBox = nullptr;

    QLineEdit *m_labelsEdit = nullptr;
    QLineEdit *m_groupEdit = nullptr;

    QTreeWidget *m_rawTreeWidget = nullptr;
};

#endif // TORRENTPROPERTIESDIALOG_H
