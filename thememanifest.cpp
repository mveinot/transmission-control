#include "thememanifest.h"

#include "appcolors.h"
#include "appicons.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace AppThemes {
namespace {

ThemeManifestResult failure(const QString &message)
{
    return {Theme(), message};
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

std::optional<AppColors::Mode> colorMode(const QString &name)
{
    const QString candidate = name.trimmed().toLower();
    if (candidate.isEmpty() || candidate == QLatin1String(AppColors::SystemTheme))
        return AppColors::Mode::System;
    if (candidate == QLatin1String(AppColors::LightTheme))
        return AppColors::Mode::Light;
    if (candidate == QLatin1String(AppColors::DarkTheme))
        return AppColors::Mode::Dark;
    return std::nullopt;
}

std::optional<AppIcons::IconTheme> parseIcons(const QJsonValue &value,
                                               const QString &id,
                                               const QString &name,
                                               const QString &basePath,
                                               const QString &fallback,
                                               QString *error)
{
    if (value.isUndefined())
        return std::nullopt;
    if (!value.isObject()) {
        *error = QStringLiteral("icons must be an object");
        return std::nullopt;
    }

    AppIcons::IconTheme::IconFiles icons;
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const std::optional<AppIcons::Id> iconId =
            AppIcons::idFromSemanticName(it.key());
        if (!iconId) {
            *error = QStringLiteral("Unknown semantic icon id: %1").arg(it.key());
            return std::nullopt;
        }
        if (!it.value().isString()) {
            *error = QStringLiteral("Icon path for %1 must be a string")
                         .arg(it.key());
            return std::nullopt;
        }

        const QString path = it.value().toString().trimmed();
        if (!isSafeRelativePath(path)) {
            *error = QStringLiteral("Icon path for %1 must stay inside the theme directory")
                         .arg(it.key());
            return std::nullopt;
        }
        icons.insert(*iconId, QDir::cleanPath(path));
    }

    if (icons.isEmpty())
        return std::nullopt;
    return AppIcons::IconTheme(id, name, basePath, icons, fallback, false);
}

std::optional<AppColors::ColorTheme> parseColors(const QJsonValue &value,
                                                  const QString &id,
                                                  const QString &name,
                                                  QString *error)
{
    if (value.isUndefined())
        return std::nullopt;
    if (!value.isObject()) {
        *error = QStringLiteral("colors must be an object");
        return std::nullopt;
    }

    const QJsonObject object = value.toObject();
    const QJsonValue modeValue = object.value(QStringLiteral("mode"));
    if (!modeValue.isUndefined() && !modeValue.isString()) {
        *error = QStringLiteral("colors.mode must be a string");
        return std::nullopt;
    }
    const std::optional<AppColors::Mode> mode =
        colorMode(modeValue.toString());
    if (!mode) {
        *error = QStringLiteral("colors.mode must be system, light, or dark");
        return std::nullopt;
    }

    AppColors::ColorTheme::PaletteColors paletteColors;
    const QJsonValue paletteValue = object.value(QStringLiteral("palette"));
    if (!paletteValue.isUndefined() && !paletteValue.isObject()) {
        *error = QStringLiteral("colors.palette must be an object");
        return std::nullopt;
    }
    const QJsonObject palette = paletteValue.toObject();
    for (auto it = palette.constBegin(); it != palette.constEnd(); ++it) {
        const std::optional<QPalette::ColorRole> role =
            AppColors::paletteRoleFromName(it.key());
        if (!role) {
            *error = QStringLiteral("Unknown palette color: %1").arg(it.key());
            return std::nullopt;
        }
        const QColor color(it.value().toString());
        if (!it.value().isString() || !color.isValid()) {
            *error = QStringLiteral("Invalid color for palette role %1").arg(it.key());
            return std::nullopt;
        }
        paletteColors.insert(*role, color);
    }

    AppColors::ColorTheme::SemanticColors semanticColors;
    const QJsonValue semanticValue = object.value(QStringLiteral("semantic"));
    if (!semanticValue.isUndefined() && !semanticValue.isObject()) {
        *error = QStringLiteral("colors.semantic must be an object");
        return std::nullopt;
    }
    const QJsonObject semantic = semanticValue.toObject();
    for (auto it = semantic.constBegin(); it != semantic.constEnd(); ++it) {
        const std::optional<AppColors::Role> role =
            AppColors::roleFromSemanticName(it.key());
        if (!role) {
            *error = QStringLiteral("Unknown semantic color: %1").arg(it.key());
            return std::nullopt;
        }
        const QColor color(it.value().toString());
        if (!it.value().isString() || !color.isValid()) {
            *error = QStringLiteral("Invalid semantic color for %1").arg(it.key());
            return std::nullopt;
        }
        semanticColors.insert(*role, color);
    }

    return AppColors::ColorTheme(
        id, name, *mode, paletteColors, semanticColors, false);
}

} // namespace

ThemeManifestResult ThemeManifestParser::parseFile(const QString &manifestPath)
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

    const QStringList reservedIds {
        QString::fromLatin1(AppIcons::ClassicTheme),
        QString::fromLatin1(AppIcons::GlassTheme),
        QString::fromLatin1(AppColors::SystemTheme),
        QString::fromLatin1(AppColors::LightTheme),
        QString::fromLatin1(AppColors::DarkTheme)
    };
    if (reservedIds.contains(id))
        return failure(QStringLiteral("Theme id is reserved for a built-in theme"));

    const QString name = root.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
        return failure(QStringLiteral("Missing theme name"));

    QString fallback = root.value(QStringLiteral("fallback")).toString()
                           .trimmed().toLower();
    if (fallback.isEmpty() || fallback == id)
        fallback = QString::fromLatin1(AppIcons::GlassTheme);

    QString componentError;
    const QString basePath = QFileInfo(manifestPath).absolutePath();
    const std::optional<AppIcons::IconTheme> icons =
        parseIcons(root.value(QStringLiteral("icons")),
                   id, name, basePath, fallback, &componentError);
    if (!componentError.isEmpty())
        return failure(componentError);

    const std::optional<AppColors::ColorTheme> colors =
        parseColors(root.value(QStringLiteral("colors")),
                    id, name, &componentError);
    if (!componentError.isEmpty())
        return failure(componentError);

    const Theme theme(id, name, icons, colors, false);
    if (!theme.isValid())
        return failure(QStringLiteral("Theme must define icons, colors, or both"));
    return {theme, QString()};
}

} // namespace AppThemes
