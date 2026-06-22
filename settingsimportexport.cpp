#include "settingsimportexport.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMessageBox>
#include <QSettings>
#include <QVariant>

namespace {

QJsonValue variantToJson(const QVariant &value)
{
    switch (value.metaType().id()) {
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

QVariant jsonToVariant(const QJsonValue &value)
{
    if (value.isBool())
        return value.toBool();

    if (value.isDouble())
        return value.toDouble();

    if (value.isString())
        return value.toString();

    if (value.isArray()) {
        QStringList list;

        const QJsonArray array = value.toArray();

        for (const QJsonValue &item : array)
            list.append(item.toString());

        return list;
    }

    return {};
}

} // namespace

namespace SettingsImportExport {

bool exportSettings(QWidget *parent, const QString &filePath)
{
    QSettings settings;

    QJsonObject root;
    root.insert(QStringLiteral("application"), QStringLiteral("Planetary"));
    root.insert(QStringLiteral("formatVersion"), 1);

    QJsonObject values;

    const QStringList keys = settings.allKeys();

    for (const QString &key : keys)
        values.insert(key, variantToJson(settings.value(key)));

    root.insert(QStringLiteral("settings"), values);

    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(
            parent,
            QObject::tr("Export Settings Failed"),
            QObject::tr("Could not write settings file:\n%1").arg(filePath)
            );
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

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

    const QByteArray data = file.readAll();

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

    const QMessageBox::StandardButton choice =
        QMessageBox::question(
            parent,
            QObject::tr("Import Settings"),
            QObject::tr("Importing settings will replace your current Planetary settings.\n\nContinue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
            );

    if (choice != QMessageBox::Yes)
        return false;

    QSettings settings;

    settings.clear();

    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
        settings.setValue(it.key(), jsonToVariant(it.value()));

    settings.sync();

    return true;
}

} // namespace SettingsImportExport