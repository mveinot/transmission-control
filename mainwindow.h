#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QAction>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void showAbout();

private slots:
    void updateTorrentList();
    void drawTorrentList();

    void on_tableWidget_cellClicked(int row, int column);

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    QMenu *mainMenu;
    QAction *aboutAction;
    QMenuBar *mainMenuBar;
};
#endif // MAINWINDOW_H
