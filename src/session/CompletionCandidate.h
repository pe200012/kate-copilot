/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionCandidate

    Represents one processed inline-completion candidate.
*/

#pragma once

#include <KTextEditor/Range>

#include <QString>

namespace KateAiInlineCompletion
{

struct CompletionCandidate {
    QString rawCompletion;
    QString insertText;
    QString displayText;
    KTextEditor::Range replaceRange = KTextEditor::Range::invalid();
    int suffixCoverage = 0;
    QString source;
    QString id;
};

} // namespace KateAiInlineCompletion
