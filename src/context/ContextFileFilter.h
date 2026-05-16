/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ContextFileFilter

    Applies deterministic privacy, size, generated-file, and binary filters
    before related files are included in prompts.
*/

#pragma once

#include <QString>
#include <QStringList>

namespace KateAiInlineCompletion
{

struct ContextFileFilterOptions {
    int maxFileChars = 4000;
    QStringList excludePatterns;
};

class ContextFileFilter
{
public:
    [[nodiscard]] static bool isAllowedPath(const QString &path, const ContextFileFilterOptions &options);
    [[nodiscard]] static bool isAllowedFile(const QString &path, const ContextFileFilterOptions &options);
    [[nodiscard]] static QString readTextFile(const QString &path, const ContextFileFilterOptions &options, bool *ok = nullptr);
};

} // namespace KateAiInlineCompletion
