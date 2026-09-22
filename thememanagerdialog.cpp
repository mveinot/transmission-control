#include "thememanagerdialog.h"

#include "appcolors.h"
#include "icontheme.h"
#include "themeregistry.h"
#include "theme.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString componentText(const AppThemes::Theme &theme)
{
    QStringList components;
    if (theme.hasIconTheme())
        components << QObject::tr("Icons");
    if (theme.hasColorTheme())
        components << QObject::tr("Colours");
    if (theme.hasStyleSheet())
        components << QObject::tr("Stylesheet");
    return components.join(QStringLiteral("  •  "));
}

void clearLayout(QLayout *layout)
{
    if (!layout)
        return;
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }
}

} // namespace

ThemeManagerDialog::ThemeManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage Themes"));
    resize(900, 650);

    auto *root = new QVBoxLayout(this);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    m_themeList = new QListWidget(splitter);
    m_themeList->setMinimumWidth(230);
    m_themeList->setAlternatingRowColors(true);
    m_themeList->setFrameShape(QFrame::StyledPanel);
    m_themeList->setFrameShadow(QFrame::Sunken);
    m_themeList->setLineWidth(2);
    m_themeList->setObjectName(QStringLiteral("themeListPanel"));
    m_themeList->setStyleSheet(QStringLiteral(
        "#themeListPanel { border: 2px solid palette(mid); }"));
    m_themeList->setToolTip(tr("Installed built-in and external themes"));

    auto *preview = new QFrame(splitter);
    preview->setFrameShape(QFrame::StyledPanel);
    preview->setFrameShadow(QFrame::Sunken);
    preview->setLineWidth(2);
    preview->setObjectName(QStringLiteral("themePreviewPanel"));
    preview->setStyleSheet(QStringLiteral(
        "#themePreviewPanel { border: 2px solid palette(mid); }"));
    auto *previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(14, 8, 8, 8);

    m_nameLabel = new QLabel(preview);
    QFont titleFont = m_nameLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    m_nameLabel->setFont(titleFont);
    previewLayout->addWidget(m_nameLabel);

    m_componentsLabel = new QLabel(preview);
    previewLayout->addWidget(m_componentsLabel);
    m_descriptionLabel = new QLabel(preview);
    m_descriptionLabel->setWordWrap(true);
    previewLayout->addWidget(m_descriptionLabel);

    auto *scroll = new QScrollArea(preview);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::StyledPanel);
    scroll->setFrameShadow(QFrame::Sunken);
    scroll->setLineWidth(1);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);

    auto *iconTitle = new QLabel(tr("Icon preview"), content);
    QFont sectionFont = iconTitle->font();
    sectionFont.setBold(true);
    iconTitle->setFont(sectionFont);
    contentLayout->addWidget(iconTitle);
    m_iconPreview = new QWidget(content);
    m_iconPreview->setMinimumHeight(52);
    auto *iconLayout = new QHBoxLayout(m_iconPreview);
    iconLayout->setContentsMargins(0, 4, 0, 8);
    iconLayout->setSpacing(12);
    contentLayout->addWidget(m_iconPreview);

    auto *colourTitle = new QLabel(tr("Colour preview"), content);
    colourTitle->setFont(sectionFont);
    contentLayout->addWidget(colourTitle);
    m_colourPreview = new QWidget(content);
    m_colourPreview->setMinimumHeight(48);
    auto *colourLayout = new QHBoxLayout(m_colourPreview);
    colourLayout->setContentsMargins(0, 4, 0, 8);
    colourLayout->setSpacing(8);
    contentLayout->addWidget(m_colourPreview);
    auto *uiTitle = new QLabel(tr("Application preview"), content);
    uiTitle->setFont(sectionFont);
    contentLayout->addWidget(uiTitle);
    m_uiPreview = new QFrame(content);
    m_uiPreview->setFrameShape(QFrame::StyledPanel);
    m_uiPreview->setFrameShadow(QFrame::Plain);
    m_uiPreview->setMinimumHeight(190);
    contentLayout->addWidget(m_uiPreview);
    contentLayout->addStretch();
    scroll->setWidget(content);
    previewLayout->addWidget(scroll, 1);

    splitter->addWidget(m_themeList);
    splitter->addWidget(preview);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    auto *actions = new QHBoxLayout;
    auto *refresh = new QPushButton(tr("Refresh Theme Packs"), this);
    refresh->setToolTip(tr("Scan the installed theme-pack directory for new or removed themes"));
    actions->addWidget(refresh);
    auto *openFolder = new QPushButton(tr("Open Themes Folder"), this);
    actions->addWidget(openFolder);
    actions->addStretch();
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    actions->addWidget(m_statusLabel, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    actions->addWidget(buttons);
    root->addLayout(actions);

    connect(m_themeList, &QListWidget::currentRowChanged,
            this, &ThemeManagerDialog::showTheme);
    connect(refresh, &QPushButton::clicked, this, [this]() {
        AppThemes::ThemeRegistry::instance().rescanExternalThemes();
        populateThemes();
        m_statusLabel->setText(tr("Theme packs refreshed."));
    });
    connect(openFolder, &QPushButton::clicked, this, [this]() {
        const QString path = AppThemes::ThemeRegistry::instance().themeDirectory();
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    populateThemes();
}

void ThemeManagerDialog::populateThemes()
{
    const QString selected = m_themeList->currentItem()
                                 ? m_themeList->currentItem()->data(Qt::UserRole).toString()
                                 : QString();
    m_themeList->clear();

    QList<AppThemes::Theme> themes = AppThemes::ThemeRegistry::instance().themes();
    std::sort(themes.begin(), themes.end(), [](const AppThemes::Theme &left,
                                               const AppThemes::Theme &right) {
        if (left.isBuiltIn() != right.isBuiltIn())
            return left.isBuiltIn();
        return QString::localeAwareCompare(left.displayName(), right.displayName()) < 0;
    });
    for (const AppThemes::Theme &theme : themes) {
        auto *item = new QListWidgetItem(theme.displayName(), m_themeList);
        item->setData(Qt::UserRole, theme.id());
        item->setToolTip(componentText(theme));
        QString suffix;
        if (theme.hasIconTheme() && theme.hasColorTheme())
            suffix = tr("  (icons + colours)");
        else if (theme.hasIconTheme())
            suffix = tr("  (icons)");
        else if (theme.hasColorTheme())
            suffix = tr("  (colours)");
        item->setText(theme.displayName() + suffix);
    }
    if (m_themeList->count() == 0) {
        clearPreview();
        return;
    }
    int row = -1;
    for (int i = 0; i < m_themeList->count(); ++i) {
        if (m_themeList->item(i)->data(Qt::UserRole).toString() == selected) {
            row = i;
            break;
        }
    }
    m_themeList->setCurrentRow(row >= 0 ? row : 0);
}

void ThemeManagerDialog::showTheme(int row)
{
    if (row < 0 || row >= m_themeList->count()) {
        clearPreview();
        return;
    }
    updatePreview(AppThemes::ThemeRegistry::instance().theme(
        m_themeList->item(row)->data(Qt::UserRole).toString()));
}

QLabel *ThemeManagerDialog::makeSwatch(const QColor &color, const QString &name)
{
    auto *label = new QLabel(m_colourPreview);
    label->setAlignment(Qt::AlignCenter);
    label->setFixedSize(48, 34);
    label->setAccessibleName(name);
    label->setToolTip(color.name(QColor::HexArgb));
    label->setAutoFillBackground(true);
    QPalette swatchPalette = label->palette();
    swatchPalette.setColor(QPalette::Window, color);
    label->setPalette(swatchPalette);
    label->setFrameShape(QFrame::StyledPanel);
    label->setFrameShadow(QFrame::Plain);
    return label;
}

void ThemeManagerDialog::updatePreview(const AppThemes::Theme &theme)
{
    m_nameLabel->setText(theme.displayName());
    m_componentsLabel->setText(componentText(theme));
    m_descriptionLabel->setText(theme.isBuiltIn()
                                    ? tr("Built-in Planetary theme.")
                                    : tr("Installed external theme pack: %1").arg(theme.id()));

    QLayout *oldIconLayout = m_iconPreview->layout();
    clearLayout(oldIconLayout);
    delete oldIconLayout;
    auto *icons = new QHBoxLayout(m_iconPreview);
    icons->setContentsMargins(0, 4, 0, 8);
    icons->setSpacing(12);
    if (theme.hasIconTheme()) {
        const QList<AppIcons::Id> samples = {
            AppIcons::Id::ActionAddTorrent, AppIcons::Id::ActionStart,
            AppIcons::Id::ActionStop, AppIcons::Id::ActionVerify,
            AppIcons::Id::ActionReannounce, AppIcons::Id::ActionDelete};
        for (const AppIcons::Id id : samples) {
            auto *iconLabel = new QLabel(m_iconPreview);
            iconLabel->setPixmap(AppThemes::ThemeRegistry::instance()
                                     .icon(theme.id(), id).pixmap(32, 32));
            iconLabel->setToolTip(AppIcons::semanticName(id));
            iconLabel->setAlignment(Qt::AlignCenter);
            iconLabel->setFixedSize(38, 38);
            icons->addWidget(iconLabel);
        }
    } else {
        icons->addWidget(new QLabel(tr("This theme does not provide icons."), m_iconPreview));
    }
    icons->addStretch();

    QLayout *oldColourLayout = m_colourPreview->layout();
    clearLayout(oldColourLayout);
    delete oldColourLayout;
    auto *colours = new QHBoxLayout(m_colourPreview);
    colours->setContentsMargins(0, 4, 0, 8);
    colours->setSpacing(8);
    QPalette palette = theme.hasColorTheme()
                           ? theme.colorTheme().appliedTo(qApp->palette())
                           : qApp->palette();
    // QLabel and button text use different palette roles on different
    // platforms. Keep the preview's foreground roles coherent so a theme's
    // text colour is visible throughout the miniature application UI.
    QColor previewText = palette.color(QPalette::Text);
    if (!previewText.isValid())
        previewText = palette.color(QPalette::WindowText);
    palette.setColor(QPalette::WindowText, previewText);
    palette.setColor(QPalette::ButtonText, previewText);
    palette.setColor(QPalette::Text, previewText);
    m_uiPreview->setAutoFillBackground(true);
    m_uiPreview->setPalette(palette);
    const QList<AppColors::Role> roles = {
        AppColors::Role::Download, AppColors::Role::Upload,
        AppColors::Role::Success, AppColors::Role::Warning,
        AppColors::Role::Error, AppColors::Role::Inactive};
    for (const AppColors::Role role : roles) {
        const QColor color = theme.hasColorTheme()
                                 ? theme.colorTheme().color(role, palette)
                                 : AppColors::defaultColor(role, palette);
        colours->addWidget(makeSwatch(color, AppColors::semanticName(role)));
    }
    colours->addStretch();

    QLayout *oldUiLayout = m_uiPreview->layout();
    clearLayout(oldUiLayout);
    delete oldUiLayout;
    auto *uiLayout = new QVBoxLayout(m_uiPreview);
    uiLayout->setContentsMargins(10, 8, 10, 8);
    uiLayout->setSpacing(6);

    auto *menuBar = new QLabel(tr("File    Edit    View    Transfers    Help"), m_uiPreview);
    menuBar->setStyleSheet(QStringLiteral("font-weight:600;"));
    menuBar->setPalette(palette);
    uiLayout->addWidget(menuBar);

    auto *toolbar = new QWidget(m_uiPreview);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(4);
    const QList<AppIcons::Id> toolbarIcons = {
        AppIcons::Id::ActionAddTorrent, AppIcons::Id::ActionStart,
        AppIcons::Id::ActionStop, AppIcons::Id::ActionVerify};
    for (const AppIcons::Id id : toolbarIcons) {
        auto *button = new QPushButton(toolbar);
        button->setPalette(palette);
        button->setIcon(AppThemes::ThemeRegistry::instance().icon(theme.id(), id));
        button->setIconSize(QSize(20, 20));
        button->setFixedSize(34, 32);
        button->setToolTip(AppIcons::semanticName(id));
        button->setEnabled(false);
        toolbarLayout->addWidget(button);
    }
    toolbarLayout->addStretch();
    uiLayout->addWidget(toolbar);

    auto *filters = new QLabel(tr("Torrents     All     Downloading     Seeding"), m_uiPreview);
    filters->setAutoFillBackground(true);
    QPalette filterPalette = filters->palette();
    filterPalette.setColor(QPalette::Window, palette.color(QPalette::AlternateBase));
    filterPalette.setColor(QPalette::WindowText, palette.color(QPalette::Text));
    filters->setPalette(filterPalette);
    filters->setMargin(4);
    uiLayout->addWidget(filters);

    const QColor downloading = theme.hasColorTheme()
                                   ? theme.colorTheme().color(AppColors::Role::Download, palette)
                                   : AppColors::defaultColor(AppColors::Role::Download, palette);
    const QColor seeding = theme.hasColorTheme()
                               ? theme.colorTheme().color(AppColors::Role::Success, palette)
                               : AppColors::defaultColor(AppColors::Role::Success, palette);
    const auto addTorrentRow = [this, uiLayout, &palette](const QString &name,
                                                           const QString &state,
                                                           const QColor &stateColor) {
        auto *row = new QFrame(m_uiPreview);
        row->setFrameShape(QFrame::NoFrame);
        row->setAutoFillBackground(true);
        QPalette rowPalette = row->palette();
        rowPalette.setColor(QPalette::Window, palette.color(QPalette::Base));
        rowPalette.setColor(QPalette::WindowText, palette.color(QPalette::Text));
        row->setPalette(rowPalette);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(7, 4, 7, 4);
        rowLayout->setSpacing(8);
        auto *nameLabel = new QLabel(name, row);
        nameLabel->setPalette(palette);
        nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *stateLabel = new QLabel(state, row);
        stateLabel->setPalette(palette);
        stateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stateLabel->setStyleSheet(QStringLiteral("font-weight:600; color:%1;")
                                      .arg(stateColor.name()));
        rowLayout->addWidget(nameLabel);
        rowLayout->addWidget(stateLabel);
        uiLayout->addWidget(row);
    };
    addTorrentRow(QStringLiteral("Ubuntu 24.04.2 LTS amd64"), tr("Downloading"), downloading);
    addTorrentRow(QStringLiteral("Fedora Workstation 42 x86_64"), tr("Seeding"), seeding);
    addTorrentRow(QStringLiteral("Linux Mint 22.1 Cinnamon"), tr("Queued"),
                  theme.hasColorTheme()
                      ? theme.colorTheme().color(AppColors::Role::Queued, palette)
                      : AppColors::defaultColor(AppColors::Role::Queued, palette));
}

void ThemeManagerDialog::clearPreview()
{
    m_nameLabel->clear();
    m_componentsLabel->clear();
    m_descriptionLabel->clear();
}
