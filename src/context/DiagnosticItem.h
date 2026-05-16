/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticItem

    Stable diagnostic data model used by contextual prompt providers.
*/

#pragma once

#include <QDateTime>
#include <QString>

namespace KateAiInlineCompletion
{

struct DiagnosticItem {
    enum class Severity { Error, Warning, Information, Hint };

    QString uri;
    Severity severity = Severity::Information;
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    QString source;
    QString code;
    QString message;
    QDateTime timestamp;
};

} // namespace KateAiInlineCompletion
