/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditPromptBuilder

    Builds deterministic chat prompts for structured inline edits.
*/

#pragma once

#include "inlineedit/InlineEdit.h"
#include "inlineedit/InlineEditTrigger.h"

namespace KateAiInlineCompletion
{

struct InlineEditPrompt {
    QString systemPrompt;
    QString userPrompt;
};

struct InlineEditPromptOptions {
    bool useContext = true;
    int maxContextChars = 6000;
    int maxEdits = 4;
    int maxTriggerPromptChars = 16000;
    InlineEditTrigger trigger;
};

class InlineEditPromptBuilder final
{
public:
    [[nodiscard]] static InlineEditPrompt build(const InlineEditRequestContext &context,
                                                const InlineEditPromptOptions &options = {});
};

} // namespace KateAiInlineCompletion
