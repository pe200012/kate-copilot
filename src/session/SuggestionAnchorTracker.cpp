/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SuggestionAnchorTracker
*/

#include "session/SuggestionAnchorTracker.h"

#include <KTextEditor/Document>

namespace KateAiInlineCompletion
{

void SuggestionAnchorTracker::attach(KTextEditor::Document *document,
                                     const KTextEditor::Cursor &cursor,
                                     KTextEditor::MovingCursor::InsertBehavior insertBehavior)
{
    clear();

    if (!document || !cursor.isValid()) {
        return;
    }

    m_cursor.reset(document->newMovingCursor(cursor, insertBehavior));
}

void SuggestionAnchorTracker::clear()
{
    m_cursor.reset();
}

bool SuggestionAnchorTracker::isValid() const
{
    return m_cursor && m_cursor->isValid();
}

KTextEditor::Cursor SuggestionAnchorTracker::position() const
{
    if (!isValid()) {
        return KTextEditor::Cursor::invalid();
    }

    return m_cursor->toCursor();
}

} // namespace KateAiInlineCompletion
