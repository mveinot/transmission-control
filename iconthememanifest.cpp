#include "iconthememanifest.h"

#include "appicons.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace AppIcons {
namespace {

IconThemeManifestResult failure(const QString &message)
{
    return {IconTheme(), message};
}

bool isSafeRelativePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QStringLiteral(":/"))
        || QDir::isAbsolutePath(path)) {
        return false;
    }

    const QString clean = QDir::cleanPath(path);
    return clean != QStringLiteral(".")
           && clean != QStringLiteral("..")
           && !clean.startsWith(QStringLiteral("../"));
}

} // namespace

IconThemeManifestResult IconThemeManifestParser::parseFile(
    const QString &manifestPath)
{
    QFile manifest(manifestPath);
    if (!manifest.open(QIODevice::ReadOnly)) {
        return failure(QStringLiteral("Could not open manifest: %1")
                           .arg(manifest.errorString()));
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(manifest.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return failure(QStringLiteral("Invalid JSON: %1")
                           .arg(parseError.errorString()));
    }
    if (!document.isObject())
        return failure(QStringLiteral("Manifest root must be an object"));

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("formatVersion")).toInt(-1)
        != CurrentFormatVersion) {
        return failure(QStringLiteral("Unsupported or missing formatVersion"));
    }

    const QString id = root.value(QStringLiteral("id")).toString().trimmed().toLower();
    static const QRegularExpression validId(
        QStringLiteral("^[a-z0-9][a-z0-9._-]*$"));
    if (!validId.match(id).hasMatch())
        return failure(QStringLiteral("Invalid theme id"));
    if (id == QString::fromLatin1(ClassicTheme)
        || id == QString::fromLatin1(GlassTheme)) {
        return failure(QStringLiteral("Theme id is reserved for a built-in theme"));
    }

    const QString name = root.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
        return failure(QStringLiteral("Missing theme name"));

    const QJsonValue iconsValue = root.value(QStringLiteral("icons"));
    if (!iconsValue.isObject())
        return failure(QStringLiteral("Missing icons object"));

    IconTheme::IconFiles icons;
    const QJsonObject iconObject = iconsValue.toObject();
    for (auto it = iconObject.constBegin(); it != iconObject.constEnd(); ++it) {
        const std::optional<Id> iconId = idFromSemanticName(it.key());
        if (!iconId)
            return failure(QStringLiteral("Unknown semantic icon id: %1").arg(it.key()));
        if (!it.value().isString()) {
            return failure(QStringLiteral("Icon path for %1 must be a string")
                               .arg(it.key()));
        }

        const QString path = it.value().toString().trimmed();
        if (!isSafeRelativePath(path)) {
            return failure(QStringLiteral("Icon path for %1 must stay inside the theme directory")
                               .arg(it.key()));
        }
        icons.insert(*iconId, QDir::cleanPath(path));
    }

    if (icons.isEmpty())
        return failure(QStringLiteral("Theme must define at least one icon"));

    QString fallback = root.value(QStringLiteral("fallback")).toString()
                           .trimmed().toLower();
    if (fallback.isEmpty() || fallback == id)
        fallback = QString::fromLatin1(GlassTheme);

    const QString themeDirectory = QFileInfo(manifestPath).absolutePath();
    return {IconTheme(id, name, themeDirectory, icons, fallback, false), QString()};
}

} // namespace AppIcons
