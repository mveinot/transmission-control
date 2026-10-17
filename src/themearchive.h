#ifndef THEMEARCHIVE_H
#define THEMEARCHIVE_H

#include <QByteArray>
#include <QString>

namespace AppThemes {

struct ThemeArchiveManifest
{
    QByteArray data;
    QString entryPrefix;
    QString error;

    bool succeeded() const { return error.isEmpty(); }
};

class ThemeArchive
{
public:
    static ThemeArchiveManifest readManifest(const QString &archivePath);
    static bool extract(const QString &archivePath,
                        const QString &entryPrefix,
                        const QString &destination,
                        QString *error);
};

} // namespace AppThemes

#endif // THEMEARCHIVE_H
