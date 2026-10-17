#include "applicationlocale.h"

#include <QCoreApplication>
#include <QDir>
#include <QSet>

#include <algorithm>

namespace {

QString nativeLocaleName(const QString &code)
{
    const QLocale locale(code);
    QString language = locale.nativeLanguageName();
    if (language.isEmpty())
        language = QLocale::languageToString(locale.language());
    if (!language.isEmpty())
        language[0] = language.at(0).toUpper();

    // Only explicitly regional catalogs need a territory qualifier. A plain
    // language code such as "es" should remain the compact "Español".
    if (code.contains(QLatin1Char('_')) || code.contains(QLatin1Char('-'))) {
        QString territory = locale.nativeTerritoryName();
        if (territory.isEmpty())
            territory = QLocale::territoryToString(locale.territory());
        if (!territory.isEmpty())
            language = QStringLiteral("%1 (%2)").arg(language, territory);
    }

    return language;
}

bool isUsableLocale(const QString &code)
{
    return !code.trimmed().isEmpty()
           && QLocale(code).language() != QLocale::C;
}

} // namespace

QList<ApplicationLocaleOption> ApplicationLocale::availableOptions()
{
    QList<ApplicationLocaleOption> translatedOptions;
    QSet<QString> seenCodes;

    const auto appendOption = [&translatedOptions, &seenCodes](
                                  const QString &code) {
        QString stableCode = code.trimmed();
        stableCode.replace(QLatin1Char('-'), QLatin1Char('_'));
        if (!isUsableLocale(stableCode)
            || seenCodes.contains(stableCode)) {
            return;
        }

        seenCodes.insert(stableCode);
        translatedOptions.append(
            {stableCode, nativeLocaleName(stableCode)});
    };

    // English is the source language and therefore has no .qm catalog.
    appendOption(QString::fromLatin1(English));

    const QDir translationResources(QStringLiteral(":/translations"));
    const QStringList files = translationResources.entryList(
        {QStringLiteral("planetary_*.qm")}, QDir::Files, QDir::Name);
    for (const QString &fileName : files) {
        QString code = fileName;
        code.remove(0, QStringLiteral("planetary_").size());
        code.chop(QStringLiteral(".qm").size());
        appendOption(code);
    }

    std::sort(translatedOptions.begin(), translatedOptions.end(),
              [](const ApplicationLocaleOption &left,
                 const ApplicationLocaleOption &right) {
                  return QString::localeAwareCompare(left.displayName,
                                                     right.displayName) < 0;
              });

    QList<ApplicationLocaleOption> options;
    options.reserve(translatedOptions.size() + 1);
    options.append(
        {QString::fromLatin1(SystemDefault),
         QCoreApplication::translate("ApplicationLocale", "System Default")});
    options.append(translatedOptions);
    return options;
}

QLocale ApplicationLocale::resolve(const QString &preference,
                                   const QByteArray &environmentOverride)
{
    const QString forced = QString::fromUtf8(environmentOverride).trimmed();
    if (isUsableLocale(forced))
        return QLocale(forced);

    const QString selected = preference.trimmed();
    if (!selected.isEmpty()
        && selected != QString::fromLatin1(SystemDefault)
        && isUsableLocale(selected)) {
        return QLocale(selected);
    }

    return QLocale::system();
}
