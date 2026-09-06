#include "torrentfilemodel.h"
#include "appicons.h"

#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QtTest>

static TorrentFile file(int index, const QString &path, qint64 done = 0);

class TestTorrentFileModel : public QObject
{
    Q_OBJECT

private slots:
    void buildsHierarchyAndMapsFoldersToFiles();
    void progressUpdatePreservesPersistentIndexes();
    void structuralChangeResetsModel();
    void flatProjectionExposesFullPaths();
    void themeChangeRefreshesDecorations();
};

void TestTorrentFileModel::themeChangeRefreshesDecorations()
{
    auto &icons = AppIcons::IconManager::instance();
    const QString originalTheme = icons.themeId();
    icons.setThemeId(QString::fromLatin1(AppIcons::GlassTheme));

    TorrentFileModel model;
    model.reconcile({file(0, QStringLiteral("folder/movie.mkv"), 50)});
    const QModelIndex folder = model.index(0, TorrentFileModel::NameColumn);
    const QImage glass = folder.data(Qt::DecorationRole).value<QIcon>()
                             .pixmap(128, 128).toImage();
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);

    icons.setThemeId(QString::fromLatin1(AppIcons::ClassicTheme));

    QVERIFY(changedSpy.count() >= 2);
    QVERIFY(folder.data(Qt::DecorationRole).value<QIcon>()
                .pixmap(128, 128).toImage() != glass);

    icons.setThemeId(originalTheme);
}

static TorrentFile file(int index, const QString &path, qint64 done)
{
    TorrentFile result;
    result.index = index;
    result.path = path;
    result.length = 100;
    result.bytesCompleted = done;
    return result;
}

void TestTorrentFileModel::buildsHierarchyAndMapsFoldersToFiles()
{
    TorrentFileModel model;
    model.reconcile({file(0, QStringLiteral("Season 1/one.mkv")),
                     file(1, QStringLiteral("Season 1/two.mkv")),
                     file(2, QStringLiteral("cover.jpg"))});

    QCOMPARE(model.rowCount(), 2);
    const QModelIndex folder = model.index(0, TorrentFileModel::NameColumn);
    QCOMPARE(folder.data().toString(), QStringLiteral("Season 1"));
    QCOMPARE(model.rowCount(folder), 2);
    QCOMPARE(model.fileIndices(folder), QList<int>({0, 1}));
    QCOMPARE(model.torrentPath(model.index(1, 0, folder)),
             QStringLiteral("Season 1/two.mkv"));
}

void TestTorrentFileModel::progressUpdatePreservesPersistentIndexes()
{
    TorrentFileModel model;
    QVector<TorrentFile> files {file(0, QStringLiteral("folder/movie.mkv"), 10)};
    model.reconcile(files);
    const QModelIndex folder = model.index(0, 0);
    QPersistentModelIndex persistentFile(model.index(0, 0, folder));
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);

    files[0].bytesCompleted = 75;
    model.reconcile(files);

    QCOMPARE(resetSpy.count(), 0);
    QVERIFY(changedSpy.count() >= 1);
    QVERIFY(persistentFile.isValid());
    QCOMPARE(QModelIndex(persistentFile).siblingAtColumn(TorrentFileModel::DoneColumn)
                 .data(TorrentFileModel::SortRole).toLongLong(),
             75);
}

void TestTorrentFileModel::structuralChangeResetsModel()
{
    TorrentFileModel model;
    model.reconcile({file(0, QStringLiteral("one.mkv"))});
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    model.reconcile({file(0, QStringLiteral("renamed.mkv"))});

    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model.index(0, 0).data().toString(), QStringLiteral("renamed.mkv"));
}

void TestTorrentFileModel::flatProjectionExposesFullPaths()
{
    TorrentFileModel model;
    model.reconcile({file(0, QStringLiteral("folder/movie.mkv"))});
    model.setFlat(true);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rowCount(model.index(0, 0)), 0);
    QCOMPARE(model.index(0, 0).data().toString(), QStringLiteral("folder/movie.mkv"));
}

QTEST_MAIN(TestTorrentFileModel)
#include "test_torrentfilemodel.moc"
