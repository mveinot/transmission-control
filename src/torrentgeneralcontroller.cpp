#include "torrentgeneralcontroller.h"

#include "pieceprogresscontroller.h"
#include "torrentdetailstabcontroller.h"

#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QSizePolicy>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

TorrentGeneralController::TorrentGeneralController(const Widgets &widgets,
                                                   QObject *parent)
    : QObject(parent)
    , m_widgets(widgets)
{
}

void TorrentGeneralController::setup()
{
    configureMagnetLabel();

    if (m_widgets.generalTab && m_widgets.generalLayout) {
        m_pieceProgressController = new PieceProgressController(
            m_widgets.generalTab,
            m_widgets.generalLayout,
            this
            );
    }

    if (m_widgets.tabWidget && m_widgets.generalTab) {
        m_detailsTabController = new TorrentDetailsTabController(
            m_widgets.tabWidget,
            m_widgets.generalTab,
            this
            );
    }

    clear();
}

void TorrentGeneralController::clear()
{
    if (m_widgets.nameLabel)
        m_widgets.nameLabel->clear();

    if (m_widgets.totalSizeLabel)
        m_widgets.totalSizeLabel->clear();

    if (m_widgets.creatorLabel)
        m_widgets.creatorLabel->clear();

    if (m_widgets.createdLabel)
        m_widgets.createdLabel->clear();

    if (m_widgets.downloadDirLabel)
        m_widgets.downloadDirLabel->clear();

    if (m_widgets.hashLabel)
        m_widgets.hashLabel->clear();

    if (m_widgets.commentLabel)
        m_widgets.commentLabel->clear();

    if (m_widgets.magnetLabel) {
        m_widgets.magnetLabel->clear();
        m_widgets.magnetLabel->setToolTip(QString());
    }

    if (m_pieceProgressController)
        m_pieceProgressController->clear();

    if (m_detailsTabController)
        m_detailsTabController->clear();

    m_currentDetailsCache.clear();
    m_currentPieces = {};
    m_currentTorrentKey.clear();
    m_currentHashString.clear();
    m_currentMagnetLink.clear();

    emit currentTorrentDetailsCleared();
}

void TorrentGeneralController::update(const TorrentDetails &details)
{
    m_currentDetailsCache = details.fields;
    m_currentTorrentKey = details.key;
    m_currentHashString = details.hashString;
    m_currentMagnetLink = details.magnetLink;

    updateGeneralFields(details);

    if (m_pieceProgressController && m_currentPieces.key == details.key)
        m_pieceProgressController->update(m_currentPieces);

    if (m_detailsTabController)
        m_detailsTabController->update(m_currentDetailsCache);

    emit currentTorrentDetailsChanged(
        m_currentTorrentKey,
        m_currentHashString,
        m_currentMagnetLink
        );
}

void TorrentGeneralController::updatePieces(const TorrentPieces &pieces)
{
    if (pieces.key != m_currentTorrentKey)
        return;

    m_currentPieces = pieces;
    m_currentDetailsCache.insert(QStringLiteral("pieceCount"), pieces.pieceCount);
    m_currentDetailsCache.insert(QStringLiteral("pieces"),
                                 QString::fromLatin1(pieces.completedPieces.toBase64()));
    m_currentDetailsCache.insert(QStringLiteral("percentDone"), pieces.percentDone);

    if (m_pieceProgressController)
        m_pieceProgressController->update(pieces);

    if (m_detailsTabController)
        m_detailsTabController->update(m_currentDetailsCache);
}

TorrentKey TorrentGeneralController::currentTorrentKey() const
{
    return m_currentTorrentKey;
}

QString TorrentGeneralController::currentHashString() const
{
    return m_currentHashString;
}

QString TorrentGeneralController::currentMagnetLink() const
{
    return m_currentMagnetLink;
}

QString TorrentGeneralController::currentDownloadDir() const
{
    if (!m_widgets.downloadDirLabel)
        return QString();

    return m_widgets.downloadDirLabel->text();
}

bool TorrentGeneralController::wantsLiveTorrentDetails(QWidget *currentTab) const
{
    return currentTab == m_widgets.generalTab
           || (m_detailsTabController && currentTab == m_detailsTabController->widget());
}

bool TorrentGeneralController::looksLikeUrl(const QString &text)
{
    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty())
        return false;

    const QUrl url = QUrl::fromUserInput(trimmed);

    return url.isValid()
           && !url.scheme().isEmpty()
           && !url.host().isEmpty();
}

void TorrentGeneralController::configureMagnetLabel()
{
    if (!m_widgets.magnetLabel)
        return;

    m_widgets.magnetLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse);
    // A long magnet URI must never become the layout's minimum width. Keep
    // the complete value in the label/tooltip, but let the surrounding form
    // decide how much horizontal space is available.
    m_widgets.magnetLabel->setSizePolicy(
        QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    m_widgets.magnetLabel->setMinimumWidth(0);
    m_widgets.magnetLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_widgets.magnetLabel, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &position) {
                if (!m_widgets.magnetLabel
                    || m_widgets.magnetLabel->text().isEmpty())
                    return;

                QMenu menu(m_widgets.magnetLabel);
                QAction *copy = menu.addAction(tr("Copy"));
                if (menu.exec(m_widgets.magnetLabel->mapToGlobal(position))
                    == copy) {
                    QApplication::clipboard()->setText(m_currentMagnetLink);
                }
            });
}

void TorrentGeneralController::updateGeneralFields(const TorrentDetails &details)
{
    if (m_widgets.nameLabel)
        m_widgets.nameLabel->setText(details.name);

    if (m_widgets.creatorLabel)
        m_widgets.creatorLabel->setText(details.creator);

    if (m_widgets.downloadDirLabel)
        m_widgets.downloadDirLabel->setText(details.downloadDirectory);

    if (m_widgets.hashLabel)
        m_widgets.hashLabel->setText(m_currentHashString);

    if (m_widgets.magnetLabel) {
        // Keep the layout's size hint bounded by showing a compact preview;
        // the complete URI remains available via the tooltip and Copy action.
        const QString preview = QFontMetrics(m_widgets.magnetLabel->font())
                                    .elidedText(m_currentMagnetLink,
                                                Qt::ElideMiddle,
                                                360);
        m_widgets.magnetLabel->setText(preview);
        m_widgets.magnetLabel->setToolTip(m_currentMagnetLink);
    }

    if (m_widgets.commentLabel) {
        const QString trimmedComment = details.comment.trimmed();

        if (trimmedComment.isEmpty()) {
            m_widgets.commentLabel->setText(tr("None"));
        } else if (looksLikeUrl(trimmedComment)) {
            const QUrl url = QUrl::fromUserInput(trimmedComment);

            m_widgets.commentLabel->setText(
                QStringLiteral("<a href=\"%1\">%2</a>")
                    .arg(url.toString().toHtmlEscaped(),
                         trimmedComment.toHtmlEscaped())
                );
        } else {
            m_widgets.commentLabel->setText(trimmedComment.toHtmlEscaped());
        }
    }

    if (m_widgets.totalSizeLabel) {
        m_widgets.totalSizeLabel->setText(
            QLocale().formattedDataSize(
                details.totalSize,
                1,
                QLocale::DataSizeIecFormat
                )
            );
    }

    if (m_widgets.createdLabel) {
        if (details.creationTime > 0) {
            const QDateTime created =
                QDateTime::fromSecsSinceEpoch(details.creationTime);
            m_widgets.createdLabel->setText(
                QLocale().toString(created, QLocale::ShortFormat)
                );
        } else {
            m_widgets.createdLabel->setText(tr("Unknown"));
        }
    }
}
