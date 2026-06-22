#pragma once

#include <QString>

class QWidget;

namespace SettingsImportExport
{
bool exportSettings(QWidget *parent, const QString &filePath);
bool importSettings(QWidget *parent, const QString &filePath);
}