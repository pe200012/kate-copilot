/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SuggestionAnchorTracker

    Maintains a document-bound suggestion anchor using KTextEditor::MovingCursor.
*/

#pragma once

#include <KTextEditor/Cursor>
#include <KTextEditor/MovingCursor>

#include <memory>

namespace KTextEditor
{
class Document;
}

namespace KateAiInlineCompletion
{

class SuggestionAnchorTracker final
{
public:
    SuggestionAnchorTracker() = default;
    ~SuggestionAnchorTracker() = default;

    void attach(KTextEditor::Document *document,
                const KTextEditor::Cursor &cursor,
                KTextEditor::MovingCursor::InsertBehavior insertBehavior = KTextEditor::MovingCursor::MoveOnInsert);

    void clear();

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] KTextEditor::Cursor position() const;

private:
    std::unique_ptr<KTextEditor::MovingCursor> m_cursor;
};

} // namespace KateAiInlineCompletion
