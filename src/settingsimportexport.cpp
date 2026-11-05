#include "settingsimportexport.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QMessageBox>
#include <QSettings>
#include <QSaveFile>
#include <QVariant>

namespace {

constexpr qint64 MaximumSettingsImportBytes = 10 * 1024 * 1024;

QJsonValue variantToJson(const QVariant &value)
{
    switch (value.metaType().id()) {
    case QMetaType::QByteArray: {
        QJsonObject encoded;
        encoded.insert(QStringLiteral("$planetaryType"),
                       QStringLiteral("QByteArray"));
        encoded.insert(QStringLiteral("base64"),
                       QString::fromLatin1(value.toByteArray().toBase64()));
        return encoded;
    }

    case QMetaType::Bool:
        return value.toBool();

    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
        return QJsonValue::fromVariant(value);

    case QMetaType::QString:
        return value.toString();

    case QMetaType::QStringList: {
        QJsonArray array;

        for (const QString &item : value.toStringList())
            array.append(item);

        return array;
    }

    default:
        // Safe fallback. QSettings is mostly strings/bools/ints anyway.
        return value.toString();
    }
}

bool jsonToVariant(const QJsonValue &value, QVariant *result)
{
    if (!result)
        return false;

    if (value.isBool()) {
        *result = value.toBool();
        return true;
    }

    if (value.isDouble()) {
        *result = value.toDouble();
        return true;
    }

    if (value.isString()) {
        *result = value.toString();
        return true;
    }

    if (value.isArray()) {
        QStringList list;

        const QJsonArray array = value.toArray();

        for (const QJsonValue &item : array) {
            if (!item.isString())
                return false;

            list.append(item.toString());
        }

        *result = list;
        return true;
    }

    if (value.isObject()) {
        const QJsonObject encoded = value.toObject();
        if (encoded.value(QStringLiteral("$planetaryType")).toString()
                != QStringLiteral("QByteArray")
            || !encoded.value(QStringLiteral("base64")).isString()) {
            return false;
        }

        const QByteArray base64 =
            encoded.value(QStringLiteral("base64")).toString().toLatin1();
        const QByteArray bytes = QByteArray::fromBase64(
            base64, QByteArray::AbortOnBase64DecodingErrors);
        if (bytes.toBase64() != base64)
            return false;

        *result = bytes;
        return true;
    }

    if (value.isNull()) {
        *result = QVariant();
        return true;
    }

    return false;
}

} // namespace

namespace SettingsImportExport {

bool exportSettings(QWidget *parent, const QString &filePath)
{
    QSettings settings;

    QJsonObject root;
    root.insert(QStringLiteral("application"), QStringLiteral("Planetary"));
    root.insert(QStringLiteral("formatVersion"), 2);

    QJsonObject values;

    const QStringList keys = settings.allKeys();

    for (const QString &key : keys)
        values.insert(key, variantToJson(settings.value(key)));

    root.insert(QStringLiteral("settings"), values);

    QSaveFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(
            parent,
            QObject::tr("Export Settings Failed"),
            QObject::tr("Could not write settings file:\n%1\n\n%2")
                .arg(filePath, file.errorString())
            );
        return false;
    }

    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        const QString error = file.errorString();
        file.cancelWriting();
        QMessageBox::warning(
            parent,
            QObject::tr("Export Settings Failed"),
            QObject::tr("Could not write settings file:\n%1\n\n%2")
                .arg(filePath, error)
            );
        return false;
    }

    // commit() flushes and closes the temporary file, then atomically replaces
    // the destination. It reports errors that a plain close() cannot expose.
    if (!file.commit()) {
        QMessageBox::warning(
            parent,
            QObject::tr("Export Settings Failed"),
            QObject::tr("Could not complete settings file:\n%1\n\n%2")
                .arg(filePath, file.errorString())
            );
        return false;
    }

    return true;
}

bool importSettings(QWidget *parent, const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(
            parent,
            QObject::tr("Import Settings Failed"),
            QObject::tr("Could not read settings file:\n%1").arg(filePath)
            );
        return false;
    }

    if (file.size() > MaximumSettingsImportBytes) {
        QMessageBox::warning(
            parent,
            QObject::tr("Import Settings Failed"),
            QObject::tr("The settings file exceeds the maximum supported size of 10 MiB.")
            );
        return false;
    }

    const QByteArray data = file.read(MaximumSettingsImportBytes + 1);
    if (data.size() > MaximumSettingsImportBytes || file.error() != QFileDevice::NoError) {
        QMessageBox::warning(
            parent,
            QObject::tr("Import Settings Failed"),
            QObject::tr("Could not read a valid settings file within the 10 MiB size limit.")
            );
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::warning(
            parent,
            QObject::tr("Import Settings Failed"),
            QObject::tr("The selected file is not valid JSON:\n%1")
                .arg(parseError.errorString())
            );
        return false;
    }

    const QJsonObject root = document.object();

    const QJsonValue formatVersionValue =
        root.value(QStringLiteral("formatVersion"));
    const int formatVersion = formatVersionValue.isUndefined()
        ? 1
        : formatVersionValue.toInt(-1);
    if (formatVersion < 1 || formatVersion > 2) {
        QMessageBox::warning(
            parent,
            QObject::tr("Import Settings Failed"),
            QObject::tr("The settings file uses an unsupported format version.")
            );
        return false;
    }

    if (root.value(QStringLiteral("application")).toString()
        != QStringLiteral("Planetary")) {
        QMessageBox::warning(
            parent,
            QObject::tr("Import Settings Failed"),
            QObject::tr("The selected file does not appear to be a Planetary settings export.")
            );
        return false;
    }

    const QJsonObject values =
        root.value(QStringLiteral("settings")).toObject();

    if (values.isEmpty()) {
        QMessageBox::warning(
            parent,
            QObject::tr("Import Settings Failed"),
            QObject::tr("The selected file does not contain any settings.")
            );
        return false;
    }

    QMap<QString, QVariant> parsedValues;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        QVariant converted;
        if (!jsonToVariant(it.value(), &converted)) {
            QMessageBox::warning(
                parent,
                QObject::tr("Import Settings Failed"),
                QObject::tr("The setting '%1' has an unsupported value.")
                    .arg(it.key())
                );
            return false;
        }
        parsedValues.insert(it.key(), converted);
    }

    QMessageBox confirmation(
        QMessageBox::Question,
        QObject::tr("Import Settings"),
        QObject::tr("Importing settings will replace your current Planetary settings.\n\nContinue?"),
        QMessageBox::Yes | QMessageBox::No,
        parent);
    confirmation.setDefaultButton(QMessageBox::No);
#ifdef Q_OS_MACOS
    confirmation.setWindowModality(Qt::WindowModal);
    confirmation.setWindowFlag(Qt::Sheet, true);
#endif

    const QMessageBox::StandardButton choice =
        static_cast<QMessageBox::StandardButton>(confirmation.exec());

    if (choice != QMessageBox::Yes)
        return false;

    // Parsing and structural validation complete before clear(), preventing a
    // malformed document from partially replacing the current configuration.
    QSettings settings;

    settings.clear();

    for (auto it = parsedValues.constBegin(); it != parsedValues.constEnd(); ++it)
        settings.setValue(it.key(), it.value());

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        QMessageBox::warning(
            parent,
            QObject::tr("Import Settings Failed"),
            QObject::tr("Planetary could not save the imported settings. They may not be available after restarting the app.")
            );
        return false;
    }

    return true;
}

} // namespace SettingsImportExport
