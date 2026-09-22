#ifndef THEMEMANAGERDIALOG_H
#define THEMEMANAGERDIALOG_H

#include <QDialog>
#include <QColor>

class QListWidget;
class QLabel;
class QFrame;
class QWidget;

namespace AppThemes {
class Theme;
}

class ThemeManagerDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ThemeManagerDialog(QWidget *parent = nullptr);

private:
    void populateThemes();
    void showTheme(int row);
    void updatePreview(const AppThemes::Theme &theme);
    void clearPreview();
    QLabel *makeSwatch(const QColor &color, const QString &name);

    QListWidget *m_themeList = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_descriptionLabel = nullptr;
    QLabel *m_componentsLabel = nullptr;
    QWidget *m_iconPreview = nullptr;
    QWidget *m_colourPreview = nullptr;
    QFrame *m_uiPreview = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif // THEMEMANAGERDIALOG_H
