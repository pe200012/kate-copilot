/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CopilotCodexPromptBuilder
*/

#include "prompt/CopilotCodexPromptBuilder.h"

#include <KTextEditor/Document>

#include <QRegularExpression>

namespace KateAiInlineCompletion
{

CopilotCodexPrompt CopilotCodexPromptBuilder::build(const PromptContext &ctx, KTextEditor::Document *doc, const KTextEditor::Cursor &cursor)
{
    CopilotCodexPrompt out;
    out.suffix = ctx.suffix;
    out.languageId = normalizeLanguageId(ctx.language);
    out.nextIndent = computeNextIndent(doc, cursor);

    const QString comment = commentPrefixForLanguage(ctx.language);
    QString header = comment + QStringLiteral(" Path: ") + ctx.filePath;

    if (!header.endsWith(QLatin1Char('\n'))) {
        header += QLatin1Char('\n');
    }

    out.prompt = header + ctx.prefix;
    return out;
}

QString CopilotCodexPromptBuilder::commentPrefixForLanguage(const QString &language)
{
    const QString l = language.toLower();

    if (l.contains(QStringLiteral("python")) || l.contains(QStringLiteral("shell")) || l.contains(QStringLiteral("bash"))
        || l.contains(QStringLiteral("ruby")) || l.contains(QStringLiteral("perl")) || l.contains(QStringLiteral("yaml"))
        || l.contains(QStringLiteral("toml")) || l.contains(QStringLiteral("make")) || l.contains(QStringLiteral("docker"))) {
        return QStringLiteral("#");
    }

    return QStringLiteral("//");
}

int CopilotCodexPromptBuilder::computeNextIndent(KTextEditor::Document *doc, const KTextEditor::Cursor &cursor)
{
    if (!doc || cursor.line() < 0) {
        return 0;
    }

    int tabWidth = doc->configValue(QStringLiteral("tab-width")).toInt();
    if (tabWidth <= 0) {
        tabWidth = 4;
    }

    const QString line = doc->line(cursor.line());

    int indent = 0;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);

        if (ch == QLatin1Char(' ')) {
            indent += 1;
            continue;
        }

        if (ch == QLatin1Char('\t')) {
            const int rem = indent % tabWidth;
            indent += (rem == 0) ? tabWidth : (tabWidth - rem);
            continue;
        }

        break;
    }

    return indent;
}

QString CopilotCodexPromptBuilder::normalizeLanguageId(QString language)
{
    language = language.trimmed().toLower();
    language.replace(QRegularExpression(QStringLiteral("[^a-z0-9_+.-]")), QStringLiteral("_"));
    return language;
}

} // namespace KateAiInlineCompletion
