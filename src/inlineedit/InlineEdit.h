/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEdit

    Shared data models for manual inline edit requests and suggestions.
*/

#pragma once

#include "context/ContextItem.h"

#include <KTextEditor/Cursor>
#include <KTextEditor/Range>

#include <QString>
#include <QVector>

namespace KateAiInlineCompletion
{

struct ProposedEdit {
    KTextEditor::Range range;
    QString newText;
};

struct InlineEditSuggestion {
    QVector<ProposedEdit> edits;
    QString rawResponse;
    QString displayText;
    QString rationale;
    QString source = QStringLiteral("manual");
    QString id;
    bool valid = false;
};

struct InlineEditRequestContext {
    QString filePath;
    QString languageId;
    KTextEditor::Cursor cursor;
    KTextEditor::Range targetRange = KTextEditor::Range::invalid();
    QString selectedText;
    QString currentTargetText;
    QString prefixExcerpt;
    QString suffixExcerpt;
    QVector<ContextItem> contextItems;
};

} // namespace KateAiInlineCompletion
