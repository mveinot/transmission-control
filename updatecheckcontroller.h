#ifndef UPDATECHECKCONTROLLER_H
#define UPDATECHECKCONTROLLER_H

#include <QObject>

class QWidget;
class UpdateChecker;

class UpdateCheckController : public QObject
{
    Q_OBJECT

public:
    explicit UpdateCheckController(QWidget *parentWidget, QObject *parent = nullptr);

    void setup();
    void checkNow();
    void maybeCheckAutomatically();

signals:
    void statusMessageRequested(const QString &message, int timeoutMs);

private:
    QWidget *m_parentWidget = nullptr;
    UpdateChecker *m_updateChecker = nullptr;

    static QString displayVersion(QString version);
};

#endif // UPDATECHECKCONTROLLER_H
