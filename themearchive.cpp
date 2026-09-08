#include "themearchive.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#ifdef PLANETARY_HAVE_MINIZ
#include <miniz/miniz.h>
#endif

namespace AppThemes {
namespace {

constexpr quint64 MaxManifestBytes = 1024 * 1024;
constexpr quint64 MaxEntryBytes = 16 * 1024 * 1024;
constexpr quint64 MaxExtractedBytes = 128 * 1024 * 1024;
constexpr int MaxEntries = 2048;

bool safeRelativePath(const QString &path)
{
    if (path.isEmpty() || path.contains(QLatin1Char('\\'))
        || QDir::isAbsolutePath(path)) {
        return false;
    }
    const QString clean = QDir::cleanPath(path);
    return clean != QStringLiteral(".") && clean != QStringLiteral("..")
           && !clean.startsWith(QStringLiteral("../"))
           && !clean.contains(QStringLiteral("/../"));
}

#ifdef PLANETARY_HAVE_MINIZ
QString archiveError(mz_zip_archive *archive, const QString &context)
{
    return QStringLiteral("%1: %2")
        .arg(context,
             QString::fromLatin1(mz_zip_get_error_string(
                 mz_zip_get_last_error(archive))));
}

QString entryName(const mz_zip_archive_file_stat &stat)
{
    return QString::fromUtf8(stat.m_filename).replace(QLatin1Char('\\'),
                                                       QLatin1Char('/'));
}
#endif

} // namespace

ThemeArchiveManifest ThemeArchive::readManifest(const QString &archivePath)
{
#ifndef PLANETARY_HAVE_MINIZ
    Q_UNUSED(archivePath)
    return {{}, {}, QStringLiteral("This build does not include archive support")};
#else
    mz_zip_archive archive {};
    const QByteArray nativePath = QFile::encodeName(archivePath);
    if (!mz_zip_reader_init_file(&archive, nativePath.constData(), 0))
        return {{}, {}, archiveError(&archive, QStringLiteral("Could not open archive"))};

    ThemeArchiveManifest result;
    QStringList manifests;
    int manifestIndex = -1;
    const mz_uint count = mz_zip_reader_get_num_files(&archive);
    if (count > MaxEntries) {
        result.error = QStringLiteral("Archive contains too many entries");
    } else {
        for (mz_uint index = 0; index < count; ++index) {
            mz_zip_archive_file_stat stat {};
            if (!mz_zip_reader_file_stat(&archive, index, &stat)) {
                result.error = archiveError(&archive,
                                            QStringLiteral("Could not inspect archive"));
                break;
            }
            const QString name = entryName(stat);
            if (!safeRelativePath(name)) {
                result.error = QStringLiteral("Archive contains an unsafe path: %1")
                                   .arg(name);
                break;
            }
            if (!stat.m_is_directory
                && (name == QStringLiteral("theme.json")
                    || name.endsWith(QStringLiteral("/theme.json")))) {
                manifests.append(name);
                manifestIndex = static_cast<int>(index);
            }
        }
    }

    if (result.error.isEmpty() && manifests.size() != 1) {
        result.error = manifests.isEmpty()
                           ? QStringLiteral("Archive does not contain theme.json")
                           : QStringLiteral("Archive contains multiple theme manifests");
    }

    if (result.error.isEmpty()) {
        mz_zip_archive_file_stat stat {};
        if (!mz_zip_reader_file_stat(&archive,
                                     static_cast<mz_uint>(manifestIndex),
                                     &stat)) {
            result.error = archiveError(&archive,
                                        QStringLiteral("Could not inspect manifest"));
        } else if (!stat.m_is_supported || stat.m_is_encrypted
                   || stat.m_uncomp_size > MaxManifestBytes) {
            result.error = QStringLiteral("Theme manifest is unsupported or too large");
        } else {
            size_t size = 0;
            void *contents = mz_zip_reader_extract_to_heap(
                &archive, static_cast<mz_uint>(manifestIndex), &size, 0);
            if (!contents) {
                result.error = archiveError(&archive,
                                            QStringLiteral("Could not read manifest"));
            } else {
                result.data = QByteArray(static_cast<const char *>(contents),
                                         static_cast<qsizetype>(size));
                mz_free(contents);
                const QString manifestName = manifests.constFirst();
                const qsizetype slash = manifestName.lastIndexOf(QLatin1Char('/'));
                result.entryPrefix = slash < 0
                                         ? QString()
                                         : manifestName.left(slash + 1);
            }
        }
    }

    mz_zip_reader_end(&archive);
    return result;
#endif
}

bool ThemeArchive::extract(const QString &archivePath,
                           const QString &entryPrefix,
                           const QString &destination,
                           QString *error)
{
#ifndef PLANETARY_HAVE_MINIZ
    Q_UNUSED(archivePath)
    Q_UNUSED(entryPrefix)
    Q_UNUSED(destination)
    if (error)
        *error = QStringLiteral("This build does not include archive support");
    return false;
#else
    mz_zip_archive archive {};
    const QByteArray nativePath = QFile::encodeName(archivePath);
    if (!mz_zip_reader_init_file(&archive, nativePath.constData(), 0)) {
        if (error)
            *error = archiveError(&archive, QStringLiteral("Could not open archive"));
        return false;
    }

    bool succeeded = true;
    quint64 totalSize = 0;
    const mz_uint count = mz_zip_reader_get_num_files(&archive);
    if (count > MaxEntries) {
        succeeded = false;
        if (error)
            *error = QStringLiteral("Archive contains too many entries");
    }

    for (mz_uint index = 0; succeeded && index < count; ++index) {
        mz_zip_archive_file_stat stat {};
        if (!mz_zip_reader_file_stat(&archive, index, &stat)) {
            succeeded = false;
            if (error)
                *error = archiveError(&archive, QStringLiteral("Could not inspect archive"));
            break;
        }

        const QString archiveName = entryName(stat);
        if (!safeRelativePath(archiveName)) {
            succeeded = false;
            if (error)
                *error = QStringLiteral("Archive contains an unsafe path: %1")
                             .arg(archiveName);
            break;
        }
        if (!archiveName.startsWith(entryPrefix))
            continue;

        QString relative = archiveName.mid(entryPrefix.size());
        if (relative.endsWith(QLatin1Char('/')))
            relative.chop(1);
        if (relative.isEmpty())
            continue;
        if (!safeRelativePath(relative)) {
            succeeded = false;
            if (error)
                *error = QStringLiteral("Archive contains an unsafe theme path: %1")
                             .arg(relative);
            break;
        }

        const QString outputPath = QDir(destination).filePath(relative);
        if (stat.m_is_directory) {
            succeeded = QDir().mkpath(outputPath);
            continue;
        }
        if (!stat.m_is_supported || stat.m_is_encrypted
            || stat.m_uncomp_size > MaxEntryBytes
            || totalSize + stat.m_uncomp_size > MaxExtractedBytes) {
            succeeded = false;
            if (error)
                *error = QStringLiteral("Archive entry is unsupported or exceeds size limits: %1")
                             .arg(archiveName);
            break;
        }
        totalSize += stat.m_uncomp_size;
        if (!QDir().mkpath(QFileInfo(outputPath).absolutePath())) {
            succeeded = false;
            if (error)
                *error = QStringLiteral("Could not create theme cache directory");
            break;
        }

        size_t size = 0;
        void *contents = mz_zip_reader_extract_to_heap(&archive, index, &size, 0);
        QFile output(outputPath);
        if (!contents || !output.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || output.write(static_cast<const char *>(contents),
                            static_cast<qint64>(size)) != static_cast<qint64>(size)) {
            succeeded = false;
            if (error)
                *error = contents
                             ? QStringLiteral("Could not write cached theme asset: %1")
                                   .arg(relative)
                             : archiveError(&archive,
                                            QStringLiteral("Could not extract theme asset"));
        }
        if (contents)
            mz_free(contents);
    }

    mz_zip_reader_end(&archive);
    return succeeded;
#endif
}

} // namespace AppThemes
