#ifndef UPDATECHECKCONTROLLER_H
#define UPDATECHECKCONTROLLER_H

#include <QObject>

class QWidget;
class UpdateChecker;

// Converts UpdateChecker results into user-facing dialogs and status messages,
// including automatic-check scheduling policy.
class UpdateCheckController : public QObject
{
    Q_OBJECT

public:
    explicit UpdateCheckController(QWidget *parentWidget, QObject *parent = nullptr);

    void setup();
    void checkNow(bool beta = false);
    void maybeCheckAutomatically();

signals:
    void statusMessageRequested(const QString &message, int timeoutMs);

private:
    QWidget *m_parentWidget = nullptr;
    UpdateChecker *m_updateChecker = nullptr;
    bool m_betaCheckInFlight = false;

    static QString displayVersion(QString version);
};

#endif // UPDATECHECKCONTROLLER_H
