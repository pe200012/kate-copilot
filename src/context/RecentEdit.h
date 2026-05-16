/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RecentEdit

    Line-based record of an edit used by recent-edit context providers.
*/

#pragma once

#include <QDateTime>
#include <QString>

namespace KateAiInlineCompletion
{

struct RecentEdit {
    QString uri;
    QDateTime timestamp;
    int startLine = 0;
    int endLine = 0;
    QString beforeText;
    QString afterText;
    QString summary;
};

} // namespace KateAiInlineCompletion
