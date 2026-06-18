#ifndef TORRENTADDDIALOG_H
#define TORRENTADDDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class TorrentAddDialog;
}

class TorrentAddDialog : public QDialog
{
    Q_OBJECT

public:
    enum class SourceType {
        TorrentFile,
        MagnetLink
    };

    explicit TorrentAddDialog(QWidget *parent = nullptr);
    ~TorrentAddDialog() override;

    void setSource(SourceType type, const QString &source);
    void setDownloadDir(const QString &downloadDir);
    void setStartPaused(bool paused);
    void setRememberOptions(bool remember);

    SourceType sourceType() const;
    QString source() const;
    QString downloadDir() const;
    bool startPaused() const;
    bool rememberOptions() const;

private:
    Ui::TorrentAddDialog *ui = nullptr;

    SourceType m_sourceType = SourceType::TorrentFile;
    QString m_source;
};

#endif // TORRENTADDDIALOG_H