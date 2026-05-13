/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: PromptAssembler

    Prepends bounded context blocks to existing FIM prompt templates.
*/

#pragma once

#include "context/ContextItem.h"
#include "prompt/PromptTemplate.h"

#include <QVector>

namespace KateAiInlineCompletion
{

struct PromptAssemblyOptions {
    bool enabled = true;
    int maxContextItems = 6;
    int maxContextChars = 6000;
};

class PromptAssembler
{
public:
    [[nodiscard]] static BuiltPrompt build(const QString &templateId,
                                           const PromptContext &ctx,
                                           const QVector<ContextItem> &items,
                                           const PromptAssemblyOptions &options);

    [[nodiscard]] static QString renderContextPrefix(const PromptContext &ctx,
                                                     const QVector<ContextItem> &items,
                                                     const PromptAssemblyOptions &options);
};

} // namespace KateAiInlineCompletion
