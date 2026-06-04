#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

#include <QDialog>

namespace Ui {
class ServerConfig;
}

class ServerConfig : public QDialog
{
    Q_OBJECT

public:
    explicit ServerConfig(QWidget *parent = nullptr);
    ~ServerConfig();

private:
    Ui::ServerConfig *ui;
};

#endif // SERVERCONFIG_H
