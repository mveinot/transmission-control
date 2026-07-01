#include "tableplaceholdercontroller.h"

#include <QAbstractScrollArea>
#include <QEvent>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QPalette>

TablePlaceholderController::TablePlaceholderController(QAbstractScrollArea *view,
                                                       QObject *parent)
    : QObject(parent)
    , m_view(view)
{
    if (!m_view || !m_view->viewport())
        return;

    m_label = new QLabel(m_view->viewport());
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    m_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_label->setAutoFillBackground(false);
    m_label->setMargin(16);

    QFont font = m_label->font();
    font.setItalic(true);
    m_label->setFont(font);

    const QColor textColor = m_view->palette().color(QPalette::Disabled, QPalette::Text);
    m_label->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(textColor.name()));

    m_view->viewport()->installEventFilter(this);
    updateGeometry();
    updateVisibility();
}

void TablePlaceholderController::setMessage(const QString &message)
{
    if (m_message == message)
        return;

    m_message = message;

    if (m_label)
        m_label->setText(m_message);

    updateVisibility();
}

void TablePlaceholderController::clearMessage()
{
    setMessage(QString());
}

QString TablePlaceholderController::message() const
{
    return m_message;
}

bool TablePlaceholderController::eventFilter(QObject *watched, QEvent *event)
{
    if (m_view && watched == m_view->viewport()) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
            updateGeometry();
            updateVisibility();
            break;
        default:
            break;
        }
    }

    return QObject::eventFilter(watched, event);
}

void TablePlaceholderController::updateGeometry()
{
    if (!m_view || !m_view->viewport() || !m_label)
        return;

    m_label->setGeometry(m_view->viewport()->rect());
}

void TablePlaceholderController::updateVisibility()
{
    if (!m_label)
        return;

    const bool visible = !m_message.trimmed().isEmpty();
    m_label->setVisible(visible);

    if (visible) {
        updateGeometry();
        m_label->raise();
    }
}
