/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ProjectContextResolver

    Shared local-path, canonical-path, project-root, and display-path helpers for context providers.
*/

#pragma once

#include <QString>

namespace KateAiInlineCompletion
{

class ProjectContextResolver final
{
public:
    [[nodiscard]] static QString localPathFromUri(const QString &uriOrPath);
    [[nodiscard]] static QString canonicalPath(const QString &path);
    [[nodiscard]] static QString findProjectRoot(const QString &path);
    [[nodiscard]] static QString relativeDisplayPath(const QString &uriOrPath, const QString &projectRoot);
    [[nodiscard]] static bool isWithinRoot(const QString &path, const QString &projectRoot);
};

} // namespace KateAiInlineCompletion
