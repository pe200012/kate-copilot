/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionStrategyEngine
*/

#include "CompletionStrategyEngine.h"

#include "settings/CompletionSettings.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include <utility>

namespace KateAiInlineCompletion
{
namespace
{
QString normalizedLanguageId(const CompletionStrategyRequest &request)
{
    return request.languageId.toLower();
}

QString normalizedFilePath(const CompletionStrategyRequest &request)
{
    return request.filePath.toLower();
}

QString normalizedSuffix(const CompletionStrategyRequest &request)
{
    return QFileInfo(request.filePath).suffix().toLower();
}

bool hasAnyWordPrefix(const QString &line, const QStringList &words)
{
    for (const QString &word : words) {
        const QRegularExpression expression(QStringLiteral("^%1\\b").arg(QRegularExpression::escape(word)));
        if (expression.match(line).hasMatch()) {
            return true;
        }
    }
    return false;
}

bool isPythonLike(const CompletionStrategyRequest &request)
{
    const QString language = normalizedLanguageId(request);
    const QString suffix = normalizedSuffix(request);
    return language.contains(QStringLiteral("python")) || suffix == QStringLiteral("py") || suffix == QStringLiteral("pyw");
}

bool isShellLike(const CompletionStrategyRequest &request)
{
    const QString language = normalizedLanguageId(request);
    const QString suffix = normalizedSuffix(request);
    return language.contains(QStringLiteral("shell")) || language.contains(QStringLiteral("bash")) || language.contains(QStringLiteral("zsh"))
        || suffix == QStringLiteral("sh") || suffix == QStringLiteral("bash") || suffix == QStringLiteral("zsh");
}

bool isHaskellLike(const CompletionStrategyRequest &request)
{
    const QString language = normalizedLanguageId(request);
    const QString suffix = normalizedSuffix(request);
    const QString filePath = normalizedFilePath(request);
    return language.contains(QStringLiteral("haskell")) || suffix == QStringLiteral("hs") || suffix == QStringLiteral("lhs")
        || suffix == QStringLiteral("hs-boot") || filePath.endsWith(QStringLiteral(".hs-boot"));
}

bool isCStyleBlockStart(const QString &line)
{
    if (line.endsWith(QLatin1Char('{'))) {
        return true;
    }

    if (line.endsWith(QLatin1Char(':'))) {
        return true;
    }

    static const QRegularExpression opener(QStringLiteral(R"(^(if|for|while|switch|catch|try|else|class|struct|enum|namespace|fn|function|impl|match|loop)\b.*$)"));
    return opener.match(line).hasMatch();
}

bool isPythonBlockStart(const QString &line)
{
    if (!line.endsWith(QLatin1Char(':'))) {
        return false;
    }

    static const QStringList words = {
        QStringLiteral("async def"),
        QStringLiteral("def"),
        QStringLiteral("class"),
        QStringLiteral("if"),
        QStringLiteral("elif"),
        QStringLiteral("else"),
        QStringLiteral("for"),
        QStringLiteral("async for"),
        QStringLiteral("while"),
        QStringLiteral("try"),
        QStringLiteral("except"),
        QStringLiteral("finally"),
        QStringLiteral("with"),
        QStringLiteral("async with"),
        QStringLiteral("match"),
        QStringLiteral("case"),
    };

    return hasAnyWordPrefix(line, words);
}

bool isShellBlockStart(const QString &line)
{
    return line.endsWith(QStringLiteral(" then")) || line == QStringLiteral("then") || line.endsWith(QStringLiteral(" do"))
        || line == QStringLiteral("do") || line.endsWith(QLatin1Char('{'));
}

bool isHaskellBlockStart(const QString &line)
{
    return line.endsWith(QStringLiteral(" where")) || line == QStringLiteral("where") || line.endsWith(QStringLiteral(" do"))
        || line == QStringLiteral("do") || line.endsWith(QStringLiteral(" of")) || line == QStringLiteral("of")
        || line.contains(QStringLiteral("= do"));
}

bool previousLineStartsBlock(const CompletionStrategyRequest &request)
{
    const QString line = request.previousLine.trimmed().toLower();
    if (line.isEmpty()) {
        return false;
    }

    if (isHaskellLike(request)) {
        return isHaskellBlockStart(line);
    }

    if (isPythonLike(request)) {
        return isPythonBlockStart(line);
    }

    if (isShellLike(request)) {
        return isShellBlockStart(line);
    }

    return isCStyleBlockStart(line);
}

bool currentLineIsBlank(const CompletionStrategyRequest &request)
{
    return request.currentLinePrefix.trimmed().isEmpty() && request.currentLineSuffix.trimmed().isEmpty();
}

QStringList uniqueStops(std::initializer_list<QString> stops)
{
    QStringList out;
    for (const QString &stop : stops) {
        if (!stop.isEmpty() && !out.contains(stop)) {
            out.push_back(stop);
        }
    }
    return out;
}

CompletionStrategy makeStrategy(CompletionStrategy::Mode mode, bool multiline, int maxTokens, double temperature, QStringList stops, QString reason)
{
    CompletionStrategy strategy;
    strategy.mode = mode;
    strategy.requestMultiline = multiline;
    strategy.maxTokens = maxTokens;
    strategy.temperature = temperature;
    strategy.stopSequences = std::move(stops);
    strategy.reason = std::move(reason);
    return strategy;
}
}

CompletionStrategy CompletionStrategyEngine::choose(const CompletionStrategyRequest &request, const CompletionSettings &settings)
{
    const CompletionSettings validated = settings.validated();
    if (!validated.enableCompletionStrategy) {
        return makeStrategy(CompletionStrategy::Mode::MoreMultiline,
                            true,
                            512,
                            0.2,
                            {},
                            QStringLiteral("legacy strategy disabled"));
    }

    const double temperature = validated.completionTemperature;
    const bool blankLine = currentLineIsBlank(request);

    if (request.afterPartialAccept || request.afterFullAccept) {
        return makeStrategy(CompletionStrategy::Mode::AfterAccept,
                            true,
                            validated.afterAcceptMaxTokens,
                            temperature,
                            uniqueStops({QStringLiteral("\n\n")}),
                            QStringLiteral("after accept continuation"));
    }

    if (request.manualTrigger && blankLine) {
        return makeStrategy(CompletionStrategy::Mode::MoreMultiline,
                            true,
                            validated.manualMultilineMaxTokens,
                            temperature,
                            uniqueStops({QStringLiteral("\n\n\n")}),
                            QStringLiteral("manual multiline trigger"));
    }

    if (blankLine && previousLineStartsBlock(request)) {
        return makeStrategy(CompletionStrategy::Mode::ParseBlock,
                            true,
                            validated.multilineMaxTokens,
                            temperature,
                            uniqueStops({QStringLiteral("\n\n\n")}),
                            QStringLiteral("parse block opener"));
    }

    return makeStrategy(CompletionStrategy::Mode::SingleLine,
                        false,
                        validated.singleLineMaxTokens,
                        temperature,
                        uniqueStops({validated.singleLineStopAtNewline ? QStringLiteral("\n") : QString()}),
                        QStringLiteral("single-line inline completion"));
}

} // namespace KateAiInlineCompletion
