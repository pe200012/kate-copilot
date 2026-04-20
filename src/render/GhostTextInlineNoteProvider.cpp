/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextInlineNoteProvider
*/

#include "render/GhostTextInlineNoteProvider.h"

#include <KTextEditor/View>

#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QStringList>

namespace KateAiInlineCompletion
{

static bool hasRenderableText(const GhostTextState &s)
{
    return !s.suppressed && !s.visibleText.isEmpty() && s.anchor.line >= 0 && s.anchor.column >= 0;
}

static QStringList splitVisibleLines(const GhostTextState &s)
{
    if (!hasRenderableText(s)) {
        return {};
    }

    return s.visibleText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
}

static int maxVisibleLineWidth(const QStringList &lines, const QFontMetrics &fm)
{
    int width = 0;
    for (const QString &line : lines) {
        width = qMax(width, fm.horizontalAdvance(line));
    }
    return width;
}

GhostTextInlineNoteProvider::GhostTextInlineNoteProvider(QObject *parent)
    : KTextEditor::InlineNoteProvider()
{
    setParent(parent);
}

void GhostTextInlineNoteProvider::setState(const GhostTextState &state)
{
    const int prevLine = m_state.anchor.line;
    const int prevColumn = m_state.anchor.column;
    const bool prevVisible = hasRenderableText(m_state);

    m_state = state;

    const int nextLine = m_state.anchor.line;
    const int nextColumn = m_state.anchor.column;
    const bool nextVisible = hasRenderableText(m_state);

    if (!prevVisible && !nextVisible) {
        Q_EMIT inlineNotesReset();
        return;
    }

    if (prevLine != nextLine || prevColumn != nextColumn || prevVisible != nextVisible) {
        Q_EMIT inlineNotesReset();
        return;
    }

    if (nextLine >= 0) {
        Q_EMIT inlineNotesChanged(nextLine);
    } else {
        Q_EMIT inlineNotesReset();
    }
}

GhostTextState GhostTextInlineNoteProvider::state() const
{
    return m_state;
}

QList<int> GhostTextInlineNoteProvider::inlineNotes(int line) const
{
    if (visibleLines().isEmpty()) {
        return {};
    }

    return line == m_state.anchor.line ? QList<int>{m_state.anchor.column} : QList<int>{};
}

QSize GhostTextInlineNoteProvider::inlineNoteSize(const KTextEditor::InlineNote &note) const
{
    const QStringList lines = visibleLines();
    if (lines.isEmpty()) {
        return QSize(0, note.lineHeight());
    }

    const QFontMetrics fm(note.font());
    return QSize(maxVisibleLineWidth(lines, fm), note.lineHeight());
}

void GhostTextInlineNoteProvider::paintInlineNote(const KTextEditor::InlineNote &note,
                                                 QPainter &painter,
                                                 Qt::LayoutDirection direction) const
{
    const QStringList lines = visibleLines();
    if (lines.isEmpty()) {
        return;
    }

    const auto *view = note.view();
    const QColor fg = view ? view->palette().color(QPalette::Text) : painter.pen().color();
    const QColor bg = view ? view->palette().color(QPalette::Base) : QColor(Qt::white);

    QColor ghost = QColor::fromRgbF((fg.redF() + bg.redF()) * 0.5,
                                   (fg.greenF() + bg.greenF()) * 0.5,
                                   (fg.blueF() + bg.blueF()) * 0.5);
    ghost.setAlphaF(0.75);

    painter.setFont(note.font());
    painter.setPen(ghost);

    const QSize size = inlineNoteSize(note);
    const QRect rect(0, 0, size.width(), note.lineHeight());

    const Qt::Alignment hAlign = (direction == Qt::RightToLeft) ? Qt::AlignRight : Qt::AlignLeft;
    painter.drawText(rect, hAlign | Qt::AlignVCenter, m_state.visibleText);
}

QStringList GhostTextInlineNoteProvider::visibleLines() const
{
    return splitVisibleLines(m_state);
}

} // namespace KateAiInlineCompletion
