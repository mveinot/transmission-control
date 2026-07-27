#include "applicationappearance.h"

#include <QGuiApplication>
#include <QStyleHints>
#include <QtGlobal>

void ApplicationAppearance::apply(const QString &appearance)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QStyleHints *styleHints = QGuiApplication::styleHints();

    if (appearance == QLatin1String(Light)) {
        styleHints->setColorScheme(Qt::ColorScheme::Light);
    } else if (appearance == QLatin1String(Dark)) {
        styleHints->setColorScheme(Qt::ColorScheme::Dark);
    } else {
        // Removing the override restores live platform appearance changes.
        styleHints->unsetColorScheme();
    }
#else
    Q_UNUSED(appearance)
#endif
}
