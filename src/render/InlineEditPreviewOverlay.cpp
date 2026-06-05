/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditPreviewOverlay
*/

#include "render/InlineEditPreviewOverlay.h"

#include <KTextEditor/View>

#include <QColor>
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QStringList>
#include <QVariant>
#include <QtGlobal>

#include <algorithm>

namespace KateAiInlineCompletion
{
namespace
{
[[nodiscard]] QStringList boundedPreviewLines(const ProposedEdit &edit, int index, int totalEdits, int maxLines)
{
    const int boundedMaxLines = qBound(1, maxLines, 50);
    QString text = edit.newText;
    if (text.isEmpty()) {
        text = QStringLiteral("delete selected text");
    }

    QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    if (lines.isEmpty()) {
        lines.push_back(text);
    }

    if (lines.size() > boundedMaxLines) {
        lines = lines.mid(0, qMax(0, boundedMaxLines - 1));
        lines.push_back(QStringLiteral("…"));
    }

    const QString prefix = totalEdits > 1 ? QStringLiteral("#%1: ").arg(index + 1) : QString();
    lines[0].prepend(prefix);
    return lines;
}

[[nodiscard]] QVector<ProposedEdit> editsInDocumentOrder(const QVector<ProposedEdit> &edits)
{
    QVector<ProposedEdit> ordered = edits;
    std::sort(ordered.begin(), ordered.end(), [](const ProposedEdit &a, const ProposedEdit &b) {
        if (a.range.start().line() != b.range.start().line()) {
            return a.range.start().line() < b.range.start().line();
        }
        return a.range.start().column() < b.range.start().column();
    });
    return ordered;
}
} // namespace

InlineEditPreviewOverlay::InlineEditPreviewOverlay(KTextEditor::View *view, QWidget *editorWidget)
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
        hide();
    }
}

InlineEditPreviewOverlay::~InlineEditPreviewOverlay()
{
    if (m_editorWidget) {
        m_editorWidget->removeEventFilter(this);
    }
}

void InlineEditPreviewOverlay::setSuggestion(const InlineEditSuggestion &suggestion)
{
    const bool wasActive = isActive();
    m_suggestion = suggestion;
    const bool active = isActive();

    if (wasActive != active) {
        if (active) {
            show();
            raise();
        } else {
            hide();
        }
    }

    update();
}

void InlineEditPreviewOverlay::clear()
{
    setSuggestion({});
}

void InlineEditPreviewOverlay::setPreviewMaxLines(int maxLines)
{
    m_previewMaxLines = qBound(1, maxLines, 50);
    update();
}

void InlineEditPreviewOverlay::refresh()
{
    updateGeometryFromParent();
    if (isActive()) {
        show();
        raise();
        update();
    }
}

InlineEditSuggestion InlineEditPreviewOverlay::suggestion() const
{
    return m_suggestion;
}

bool InlineEditPreviewOverlay::isActive() const
{
    return m_suggestion.valid && !m_suggestion.edits.isEmpty() && m_view && m_editorWidget;
}

QFont InlineEditPreviewOverlay::effectiveTextFont() const
{
    if (m_view) {
        const QVariant v = m_view->configValue(QStringLiteral("font"));
        if (v.isValid() && v.canConvert<QFont>()) {
            return v.value<QFont>();
        }
    }

    return m_editorWidget ? m_editorWidget->font() : font();
}

void InlineEditPreviewOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (!isActive()) {
        return;
    }

    const QRect clipRect = textAreaRectInEditorWidget();
    if (!clipRect.isValid()) {
        return;
    }

    const int lineHeight = lineHeightPx();
    if (lineHeight <= 0) {
        return;
    }

    QPainter painter(this);
    painter.setClipRect(clipRect);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QPalette pal = m_editorWidget ? m_editorWidget->palette() : palette();
    QColor preview = pal.color(QPalette::Highlight);
    preview.setAlpha(130);
    QColor previewText = pal.color(QPalette::Text);
    QColor background = pal.color(QPalette::Highlight);
    background.setAlpha(35);

    const QFont textFont = effectiveTextFont();
    const QFontMetrics metrics(textFont);
    painter.setFont(textFont);

    const int centeredTopOffset = qMax(0, (lineHeight - metrics.height()) / 2);
    const QVector<ProposedEdit> orderedEdits = editsInDocumentOrder(m_suggestion.edits);
    const int totalEdits = orderedEdits.size();

    painter.setPen(previewText);
    for (int editIndex = 0; editIndex < orderedEdits.size(); ++editIndex) {
        const ProposedEdit &edit = orderedEdits.at(editIndex);
        const QPoint anchor = cursorToEditorWidget(edit.range.start());
        QStringList lines = boundedPreviewLines(edit, editIndex, totalEdits, m_previewMaxLines);
        if (editIndex == 0 && totalEdits > 1) {
            lines.prepend(m_suggestion.displayText.isEmpty() ? QStringLiteral("AI Inline Edit: %1 edits").arg(totalEdits) : m_suggestion.displayText);
        }

        const int renderedLines = qMin(lines.size(), qBound(1, m_previewMaxLines, 50));
        const int startX = qMax(clipRect.left(), anchor.x());
        const int touchedLineCount = qMax(1, edit.range.end().line() - edit.range.start().line() + 1);
        const int highlightHeight = qMax(lineHeight, touchedLineCount * lineHeight);
        painter.fillRect(QRect(clipRect.left(), anchor.y(), clipRect.width(), highlightHeight).intersected(clipRect), background);

        painter.setPen(previewText);
        for (int i = 0; i < renderedLines; ++i) {
            const int y = anchor.y() + i * lineHeight;
            if (y + lineHeight <= clipRect.top()) {
                continue;
            }
            if (y > clipRect.bottom()) {
                break;
            }

            const int x = (i == 0) ? startX : clipRect.left();
            const int available = qMax(0, clipRect.right() - x + 1);
            if (available <= 0) {
                continue;
            }

            const QString line = metrics.elidedText(lines.at(i), Qt::ElideRight, available);
            painter.drawText(QPoint(x, y + centeredTopOffset + metrics.ascent()), line);
        }

        painter.setPen(preview);
        painter.drawRect(QRect(clipRect.left(), anchor.y(), clipRect.width() - 1, qMax(lineHeight, renderedLines * lineHeight) - 1).intersected(clipRect));
    }
}

bool InlineEditPreviewOverlay::eventFilter(QObject *watched, QEvent *event)
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

void InlineEditPreviewOverlay::updateGeometryFromParent()
{
    if (!m_editorWidget) {
        return;
    }

    setGeometry(m_editorWidget->rect());
    raise();
}

QPoint InlineEditPreviewOverlay::cursorToEditorWidget(const KTextEditor::Cursor &cursor) const
{
    if (!m_view || !m_editorWidget) {
        return QPoint();
    }

    const QPoint inView = m_view->cursorToCoordinate(cursor);
    return m_editorWidget->mapFrom(m_view, inView);
}

QRect InlineEditPreviewOverlay::textAreaRectInEditorWidget() const
{
    if (!m_view || !m_editorWidget) {
        return QRect();
    }

    const QRect inView = m_view->textAreaRect();
    const QPoint tl = m_editorWidget->mapFrom(m_view, inView.topLeft());
    const QPoint br = m_editorWidget->mapFrom(m_view, inView.bottomRight());
    return QRect(tl, br).normalized();
}

int InlineEditPreviewOverlay::lineHeightPx() const
{
    if (!m_view || !m_editorWidget) {
        return fontMetrics().height();
    }

    const int line = m_suggestion.edits.isEmpty() ? 0 : qMax(0, m_suggestion.edits.constFirst().range.start().line());
    const QPoint p0 = cursorToEditorWidget(KTextEditor::Cursor(line, 0));
    const QPoint p1 = cursorToEditorWidget(KTextEditor::Cursor(line + 1, 0));
    const int diff = p1.y() - p0.y();
    if (diff > 0) {
        return diff;
    }

    return QFontMetrics(effectiveTextFont()).height();
}

} // namespace KateAiInlineCompletion
