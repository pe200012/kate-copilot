/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditApplier

    Applies validated multi-range inline edits in a single KTextEditor transaction.
*/

#pragma once

#include "inlineedit/InlineEditValidator.h"

namespace KateAiInlineCompletion
{

struct InlineEditApplyResult {
    bool ok = false;
    QString message;
};

class InlineEditApplier final
{
public:
    [[nodiscard]] static InlineEditApplyResult apply(KTextEditor::Document *document,
                                                     const InlineEditSuggestion &suggestion,
                                                     const InlineEditValidationOptions &options = {});
};

} // namespace KateAiInlineCompletion
