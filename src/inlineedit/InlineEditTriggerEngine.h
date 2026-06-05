/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditTriggerEngine

    Chooses a bounded automatic inline edit trigger from editor signals.
*/

#pragma once

#include "inlineedit/InlineEditTrigger.h"
#include "settings/CompletionSettings.h"

#include <optional>

namespace KateAiInlineCompletion
{

class InlineEditTriggerEngine final
{
public:
    [[nodiscard]] static std::optional<InlineEditTrigger> choose(const InlineEditTriggerRequest &request,
                                                                 const CompletionSettings &settings);
};

} // namespace KateAiInlineCompletion
