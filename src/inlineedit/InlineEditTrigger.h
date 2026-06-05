/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditTrigger

    Describes why an inline edit request was started.
*/

#pragma once

#include "context/DiagnosticItem.h"
#include "context/RecentEdit.h"

#include <KTextEditor/Cursor>
#include <KTextEditor/Range>

#include <QString>
#include <QVector>

#include <cstdint>

namespace KateAiInlineCompletion
{

enum class InlineEditTriggerKind : std::uint8_t {
    Manual,
    DiagnosticRepair,
    RecentEditContinuation,
    SelectionRepair,
};

struct InlineEditTrigger {
    InlineEditTriggerKind kind = InlineEditTriggerKind::Manual;
    QString reason;
    QString sourceUri;
    KTextEditor::Range targetRange = KTextEditor::Range::invalid();
    int priority = 0;
    QString diagnosticMessage;
    QString recentEditSummary;
};

struct InlineEditTriggerRequest {
    QString filePath;
    QString languageId;
    KTextEditor::Cursor cursor;
    KTextEditor::Range selectionRange = KTextEditor::Range::invalid();
    bool hasSelection = false;
    QVector<DiagnosticItem> diagnostics;
    QVector<RecentEdit> recentEdits;
    int documentRevision = 0;
};

[[nodiscard]] QString inlineEditTriggerKindName(InlineEditTriggerKind kind);

} // namespace KateAiInlineCompletion
