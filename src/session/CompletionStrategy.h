/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionStrategy
*/

#pragma once

#include <KTextEditor/Cursor>

#include <QString>
#include <QStringList>

namespace KateAiInlineCompletion
{

struct CompletionStrategy {
    enum class Mode {
        SingleLine,
        ParseBlock,
        MoreMultiline,
        AfterAccept,
    };

    Mode mode = Mode::SingleLine;
    bool requestMultiline = false;
    int maxTokens = 64;
    double temperature = 0.2;
    QStringList stopSequences;
    QString reason;
};

struct CompletionStrategyRequest {
    QString providerId;
    QString languageId;
    QString filePath;
    QString prefix;
    QString suffix;
    QString currentLinePrefix;
    QString currentLineSuffix;
    QString previousLine;
    QString nextLine;
    KTextEditor::Cursor cursor;
    bool manualTrigger = false;
    bool afterPartialAccept = false;
    bool afterFullAccept = false;
};

[[nodiscard]] QString completionStrategyModeName(CompletionStrategy::Mode mode);

} // namespace KateAiInlineCompletion
