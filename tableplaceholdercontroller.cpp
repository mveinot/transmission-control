#include "tableplaceholdercontroller.h"

#include <QAbstractScrollArea>
#include <QApplication>
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

    refreshPalette();

    m_view->viewport()->installEventFilter(this);
    updateGeometry();
    updateVisibility();
}

void TablePlaceholderController::refreshPalette()
{
    if (!m_view || !m_label)
        return;

    QPalette palette = m_view->palette();
    const QColor textColor = palette.color(QPalette::Disabled, QPalette::Text);
    palette.setColor(QPalette::WindowText, textColor);
    palette.setColor(QPalette::Text, textColor);
    m_label->setPalette(palette);
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
    // The overlay is parented to the viewport, not the outer scroll area.
    if (m_view && watched == m_view->viewport()) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
            updateGeometry();
            updateVisibility();
            break;
        case QEvent::ApplicationPaletteChange:
            refreshPalette();
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
