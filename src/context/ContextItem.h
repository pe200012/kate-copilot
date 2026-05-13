/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ContextItem

    Shared data model for lightweight prompt context.
*/

#pragma once

#include <KTextEditor/Cursor>

#include <QString>

namespace KateAiInlineCompletion
{

struct ContextItem {
    enum class Kind { Trait, CodeSnippet, DiagnosticBag };

    Kind kind = Kind::Trait;
    QString providerId;
    QString id;
    int importance = 0;
    QString uri;
    QString name;
    QString value;
};

struct ContextResolveRequest {
    QString completionId;
    QString opportunityId;
    QString uri;
    QString languageId;
    int version = 0;
    KTextEditor::Cursor position;
    int timeBudgetMs = 120;
};

} // namespace KateAiInlineCompletion
