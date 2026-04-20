/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextInlineNoteProvider

    Renders GhostTextState using KTextEditor::InlineNoteProvider.
*/

#pragma once

#include "session/GhostTextState.h"

#include <KTextEditor/InlineNoteProvider>

#include <QObject>

namespace KateAiInlineCompletion
{

class GhostTextInlineNoteProvider final : public KTextEditor::InlineNoteProvider
{
    Q_OBJECT

public:
    explicit GhostTextInlineNoteProvider(QObject *parent = nullptr);

    void setState(const GhostTextState &state);
    [[nodiscard]] GhostTextState state() const;

    QList<int> inlineNotes(int line) const override;
    QSize inlineNoteSize(const KTextEditor::InlineNote &note) const override;
    void paintInlineNote(const KTextEditor::InlineNote &note,
                         QPainter &painter,
                         Qt::LayoutDirection direction) const override;

private:
    [[nodiscard]] QStringList visibleLines() const;

    GhostTextState m_state;
};

} // namespace KateAiInlineCompletion
