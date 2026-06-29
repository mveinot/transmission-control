#ifndef DIALOGABOUT_H
#define DIALOGABOUT_H

#include <QDialog>

class GeoIpService;

namespace Ui {
class DialogAbout;
}

class DialogAbout : public QDialog
{
    Q_OBJECT

public:
    explicit DialogAbout(GeoIpService *geoIpService = nullptr, QWidget *parent = nullptr);
    ~DialogAbout();

private:
    Ui::DialogAbout *ui;
    GeoIpService *geoIpService = nullptr;

    QString buildAboutHtml() const;
    QString buildGeoIpHtml() const;
    void triggerEasterEgg();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // DIALOGABOUT_H
