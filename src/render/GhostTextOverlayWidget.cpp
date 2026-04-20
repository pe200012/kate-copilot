/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextOverlayWidget
*/

#include "render/GhostTextOverlayWidget.h"

#include <KTextEditor/View>

#include <QColor>
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QVariant>

namespace KateAiInlineCompletion
{

static bool hasRenderableText(const GhostTextState &s)
{
    return s.anchorTracked && !s.suppressed && !s.visibleText.isEmpty() && s.anchor.line >= 0 && s.anchor.column >= 0;
}

GhostTextOverlayWidget::GhostTextOverlayWidget(KTextEditor::View *view, QWidget *editorWidget)
    : QWidget(editorWidget)
    , m_view(view)
    , m_editorWidget(editorWidget)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);

    if (m_editorWidget) {
        m_editorWidget->installEventFilter(this);
        updateGeometryFromParent();
        raise();
        show();
    }
}

void GhostTextOverlayWidget::setState(const GhostTextState &state)
{
    const bool prevActive = isActive();
    m_state = state;
    const bool nextActive = isActive();

    if (prevActive != nextActive) {
        if (nextActive) {
            show();
            raise();
        } else {
            hide();
        }
    }

    update();
}

GhostTextState GhostTextOverlayWidget::state() const
{
    return m_state;
}

bool GhostTextOverlayWidget::isActive() const
{
    return hasRenderableText(m_state) && m_view && m_editorWidget;
}

QFont GhostTextOverlayWidget::effectiveTextFont() const
{
    if (m_view) {
        const QVariant v = m_view->configValue(QStringLiteral("font"));
        if (v.isValid() && v.canConvert<QFont>()) {
            return v.value<QFont>();
        }
    }

    return m_editorWidget ? m_editorWidget->font() : font();
}

void GhostTextOverlayWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (!isActive()) {
        return;
    }

    const QStringList lines = m_state.visibleText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    if (lines.isEmpty()) {
        return;
    }

    const QRect clipRect = textAreaRectInEditorWidget();
    if (!clipRect.isValid()) {
        return;
    }

    const int lh = lineHeightPx();
    if (lh <= 0) {
        return;
    }

    const QPoint anchorPos = cursorToEditorWidget(KTextEditor::Cursor(m_state.anchor.line, m_state.anchor.column));
    const int firstVisibleLine = qMax(0, (clipRect.top() - anchorPos.y()) / lh);

    QPainter painter(this);
    painter.setClipRect(clipRect);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QPalette pal = m_editorWidget ? m_editorWidget->palette() : palette();
    const QColor fg = pal.color(QPalette::Text);
    const QColor bg = pal.color(QPalette::Base);

    QColor ghost = QColor::fromRgbF((fg.redF() + bg.redF()) * 0.5,
                                    (fg.greenF() + bg.greenF()) * 0.5,
                                    (fg.blueF() + bg.blueF()) * 0.5);
    ghost.setAlpha(255);

    const QFont textFont = effectiveTextFont();
    const QFontMetrics metrics(textFont);
    const int centeredTopOffset = qMax(0, (lh - metrics.height()) / 2);

    painter.setFont(textFont);
    painter.setPen(ghost);

    for (int i = firstVisibleLine; i < lines.size(); ++i) {
        const int lineTop = anchorPos.y() + (i * lh);
        if (lineTop + lh <= clipRect.top()) {
            continue;
        }
        if (lineTop > clipRect.bottom()) {
            break;
        }

        const int x = (i == 0) ? qMax(clipRect.left(), anchorPos.x()) : clipRect.left();
        const int availableWidth = (i == 0) ? qMax(0, clipRect.right() - x + 1) : clipRect.width();
        if (availableWidth <= 0) {
            continue;
        }

        const QRect lineClip(x, lineTop, availableWidth, lh);
        const QString displayLine = metrics.elidedText(lines.at(i), Qt::ElideRight, availableWidth);
        const QPoint baseline(x, lineTop + centeredTopOffset + metrics.ascent());

        painter.save();
        painter.setClipRect(lineClip.intersected(clipRect));
        painter.drawText(baseline, displayLine);
        painter.restore();
    }
}

bool GhostTextOverlayWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (!event || !m_editorWidget || watched != m_editorWidget) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
        updateGeometryFromParent();
        if (isActive()) {
            update();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void GhostTextOverlayWidget::updateGeometryFromParent()
{
    if (!m_editorWidget) {
        return;
    }

    setGeometry(m_editorWidget->rect());
    raise();
}

QPoint GhostTextOverlayWidget::cursorToEditorWidget(const KTextEditor::Cursor &cursor) const
{
    if (!m_view || !m_editorWidget) {
        return QPoint();
    }

    const QPoint inView = m_view->cursorToCoordinate(cursor);
    return m_editorWidget->mapFrom(m_view, inView);
}

QRect GhostTextOverlayWidget::textAreaRectInEditorWidget() const
{
    if (!m_view || !m_editorWidget) {
        return QRect();
    }

    const QRect inView = m_view->textAreaRect();
    const QPoint tl = m_editorWidget->mapFrom(m_view, inView.topLeft());
    const QPoint br = m_editorWidget->mapFrom(m_view, inView.bottomRight());
    return QRect(tl, br).normalized();
}

int GhostTextOverlayWidget::lineHeightPx() const
{
    if (!m_view || !m_editorWidget) {
        return fontMetrics().height();
    }

    const int line = qMax(0, m_state.anchor.line);
    const QPoint p0 = cursorToEditorWidget(KTextEditor::Cursor(line, 0));
    const QPoint p1 = cursorToEditorWidget(KTextEditor::Cursor(line + 1, 0));
    const int diff = p1.y() - p0.y();
    if (diff > 0) {
        return diff;
    }

    return QFontMetrics(effectiveTextFont()).height();
}

} // namespace KateAiInlineCompletion
