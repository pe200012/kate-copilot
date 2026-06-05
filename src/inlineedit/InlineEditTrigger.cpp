/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditTrigger
*/

#include "inlineedit/InlineEditTrigger.h"

namespace KateAiInlineCompletion
{

QString inlineEditTriggerKindName(InlineEditTriggerKind kind)
{
    switch (kind) {
    case InlineEditTriggerKind::Manual:
        return QStringLiteral("Manual");
    case InlineEditTriggerKind::DiagnosticRepair:
        return QStringLiteral("DiagnosticRepair");
    case InlineEditTriggerKind::RecentEditContinuation:
        return QStringLiteral("RecentEditContinuation");
    case InlineEditTriggerKind::SelectionRepair:
        return QStringLiteral("SelectionRepair");
    }

    return QStringLiteral("Manual");
}

} // namespace KateAiInlineCompletion
