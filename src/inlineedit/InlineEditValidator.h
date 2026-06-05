/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditValidator

    Validates structured multi-range inline edit suggestions before preview or application.
*/

#pragma once

#include "inlineedit/InlineEdit.h"

namespace KTextEditor
{
class Document;
}

namespace KateAiInlineCompletion
{

struct InlineEditValidationOptions {
    int maxEdits = 4;
    int maxNewTextChars = 8000;
    int maxTotalNewTextChars = 16000;
    bool allowDeletion = true;
    qint64 expectedDocumentRevision = -1;
};

struct InlineEditValidationResult {
    bool ok = false;
    QVector<ProposedEdit> edits;
    QString message;
};

class InlineEditValidator final
{
public:
    [[nodiscard]] static InlineEditValidationResult validate(KTextEditor::Document *document,
                                                            const InlineEditSuggestion &suggestion,
                                                            const InlineEditValidationOptions &options = {});
};

} // namespace KateAiInlineCompletion
