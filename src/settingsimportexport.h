#pragma once

#include <QString>

class QWidget;

namespace SettingsImportExport
{
// Serializes the application's QSettings namespace to a portable JSON object.
// Import validates the complete document before mutating persistent settings.
bool exportSettings(QWidget *parent, const QString &filePath);
bool importSettings(QWidget *parent, const QString &filePath);
}
