#ifndef APPLICATIONLOCALE_H
#define APPLICATIONLOCALE_H

#include <QByteArray>
#include <QList>
#include <QLocale>
#include <QString>

struct ApplicationLocaleOption
{
    QString code;
    QString displayName;
};

// Discovers embedded application translations and resolves the locale used at
// startup. Preference values remain stable locale codes, never display text.
namespace ApplicationLocale {

inline constexpr const char *SystemDefault = "system";
inline constexpr const char *English = "en";

QList<ApplicationLocaleOption> availableOptions();
QLocale resolve(const QString &preference,
                const QByteArray &environmentOverride = {});

} // namespace ApplicationLocale

#endif // APPLICATIONLOCALE_H
