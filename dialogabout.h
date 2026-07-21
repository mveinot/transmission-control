#ifndef DIALOGABOUT_H
#define DIALOGABOUT_H

#include <QDialog>

namespace Ui {
class DialogAbout;
}

// Presents build identity, credits, and bundled license material without
// depending on network availability.
class DialogAbout : public QDialog
{
    Q_OBJECT

public:
    explicit DialogAbout(QWidget *parent = nullptr);
    ~DialogAbout();

private:
    Ui::DialogAbout *ui;
    QString buildAboutHtml() const;
    void triggerEasterEgg();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // DIALOGABOUT_H
