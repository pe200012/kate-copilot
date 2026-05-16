/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionStrategyEngine
*/

#pragma once

#include "CompletionStrategy.h"

namespace KateAiInlineCompletion
{

struct CompletionSettings;

class CompletionStrategyEngine
{
public:
    [[nodiscard]] static CompletionStrategy choose(const CompletionStrategyRequest &request, const CompletionSettings &settings);
};

} // namespace KateAiInlineCompletion
