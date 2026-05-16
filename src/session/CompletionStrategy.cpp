/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionStrategy
*/

#include "CompletionStrategy.h"

namespace KateAiInlineCompletion
{

QString completionStrategyModeName(CompletionStrategy::Mode mode)
{
    switch (mode) {
    case CompletionStrategy::Mode::SingleLine:
        return QStringLiteral("SingleLine");
    case CompletionStrategy::Mode::ParseBlock:
        return QStringLiteral("ParseBlock");
    case CompletionStrategy::Mode::MoreMultiline:
        return QStringLiteral("MoreMultiline");
    case CompletionStrategy::Mode::AfterAccept:
        return QStringLiteral("AfterAccept");
    }

    return QStringLiteral("SingleLine");
}

} // namespace KateAiInlineCompletion
