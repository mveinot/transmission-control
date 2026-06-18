#include "torrentadddialog.h"
#include "ui_torrentadddialog.h"

TorrentAddDialog::TorrentAddDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TorrentAddDialog)
{
    ui->setupUi(this);

    ui->editSource->setTextMargins(0, 0, 0, 0);
    ui->editSource->setCursorPosition(0);

    QPalette sourcePalette = ui->editSource->palette();
    sourcePalette.setColor(QPalette::Base, sourcePalette.color(QPalette::Window));
    sourcePalette.setColor(QPalette::Text, sourcePalette.color(QPalette::WindowText));
    ui->editSource->setPalette(sourcePalette);

    ui->editSource->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0px;"
        "}"
        ));
}

TorrentAddDialog::~TorrentAddDialog()
{
    delete ui;
}

void TorrentAddDialog::setSource(SourceType type, const QString &source)
{
    m_sourceType = type;
    m_source = source;

    switch (type) {
    case SourceType::TorrentFile:
        ui->labelSourceTypeValue->setText(QStringLiteral("Torrent file"));
        break;

    case SourceType::MagnetLink:
        ui->labelSourceTypeValue->setText(QStringLiteral("Magnet link"));
        break;
    }

    ui->editSource->setText(source);
    ui->editSource->setCursorPosition(0);
}

void TorrentAddDialog::setDownloadDir(const QString &downloadDir)
{
    ui->editDownloadDir->setText(downloadDir);
}

void TorrentAddDialog::setStartPaused(bool paused)
{
    ui->checkStartPaused->setChecked(paused);
}

void TorrentAddDialog::setRememberOptions(bool remember)
{
    ui->checkRememberOptions->setChecked(remember);
}

TorrentAddDialog::SourceType TorrentAddDialog::sourceType() const
{
    return m_sourceType;
}

QString TorrentAddDialog::source() const
{
    return m_source;
}

QString TorrentAddDialog::downloadDir() const
{
    return ui->editDownloadDir->text().trimmed();
}

bool TorrentAddDialog::startPaused() const
{
    return ui->checkStartPaused->isChecked();
}

bool TorrentAddDialog::rememberOptions() const
{
    return ui->checkRememberOptions->isChecked();
}