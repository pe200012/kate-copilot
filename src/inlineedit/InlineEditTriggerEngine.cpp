/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditTriggerEngine
*/

#include "inlineedit/InlineEditTriggerEngine.h"

#include <QDateTime>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace KateAiInlineCompletion
{
namespace
{
[[nodiscard]] bool isError(const DiagnosticItem &diagnostic)
{
    return diagnostic.severity == DiagnosticItem::Severity::Error;
}

[[nodiscard]] bool isWarning(const DiagnosticItem &diagnostic)
{
    return diagnostic.severity == DiagnosticItem::Severity::Warning;
}

[[nodiscard]] bool diagnosticSeverityAllowed(const DiagnosticItem &diagnostic, const CompletionSettings &settings)
{
    return isError(diagnostic) || (settings.autoInlineEditWarnings && isWarning(diagnostic));
}

[[nodiscard]] int diagnosticLineDistance(const InlineEditTriggerRequest &request, const DiagnosticItem &diagnostic)
{
    return std::abs(diagnostic.startLine - request.cursor.line());
}

[[nodiscard]] bool nonEmptyDiagnosticRange(const DiagnosticItem &diagnostic)
{
    const KTextEditor::Cursor start(diagnostic.startLine, diagnostic.startColumn);
    const KTextEditor::Cursor end(diagnostic.endLine, diagnostic.endColumn);
    return start.isValid() && end.isValid() && start < end;
}

[[nodiscard]] KTextEditor::Range currentLineSentinel(const InlineEditTriggerRequest &request)
{
    const int line = qMax(0, request.cursor.line());
    return KTextEditor::Range(line, 0, line, 0);
}

[[nodiscard]] KTextEditor::Range diagnosticTargetRange(const InlineEditTriggerRequest &request, const DiagnosticItem &diagnostic)
{
    if (nonEmptyDiagnosticRange(diagnostic)) {
        return KTextEditor::Range(diagnostic.startLine, diagnostic.startColumn, diagnostic.endLine, diagnostic.endColumn);
    }

    return currentLineSentinel(request);
}

[[nodiscard]] QString fileSuffix(const QString &path)
{
    return QFileInfo(path).suffix().toLower();
}

[[nodiscard]] bool languageAcceptsSuffix(const QString &languageId, const QString &suffix)
{
    const QString language = languageId.trimmed().toLower();
    if (language.contains(QStringLiteral("c++")) || language.contains(QStringLiteral("cpp"))) {
        static const QStringList cppSuffixes = {QStringLiteral("cpp"), QStringLiteral("cc"), QStringLiteral("cxx"), QStringLiteral("hpp"), QStringLiteral("hh"), QStringLiteral("hxx"), QStringLiteral("h")};
        return cppSuffixes.contains(suffix);
    }

    if (language.contains(QStringLiteral("haskell"))) {
        return suffix == QStringLiteral("hs") || suffix == QStringLiteral("lhs");
    }

    if (language.contains(QStringLiteral("python"))) {
        return suffix == QStringLiteral("py");
    }

    if (language.contains(QStringLiteral("javascript")) || language.contains(QStringLiteral("typescript"))) {
        static const QStringList jsSuffixes = {QStringLiteral("js"), QStringLiteral("jsx"), QStringLiteral("ts"), QStringLiteral("tsx")};
        return jsSuffixes.contains(suffix);
    }

    return suffix.isEmpty() || language.contains(suffix);
}

[[nodiscard]] bool recentEditLanguageMatches(const InlineEditTriggerRequest &request, const RecentEdit &edit)
{
    const QString suffix = fileSuffix(edit.uri);
    return languageAcceptsSuffix(request.languageId, suffix);
}

[[nodiscard]] QStringList basenameTokens(const QString &path)
{
    const QString base = QFileInfo(path).completeBaseName().toLower();
    QStringList tokens = base.split(QRegularExpression(QStringLiteral("[^a-z0-9]+")), Qt::SkipEmptyParts);
    if (tokens.isEmpty() && !base.isEmpty()) {
        tokens.push_back(base);
    }
    return tokens;
}

[[nodiscard]] bool sharesBasenameToken(const QString &left, const QString &right)
{
    const QStringList leftTokens = basenameTokens(left);
    const QStringList rightTokens = basenameTokens(right);
    for (const QString &token : leftTokens) {
        if (token.size() >= 3 && rightTokens.contains(token)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool recentEditPathMatches(const InlineEditTriggerRequest &request, const RecentEdit &edit)
{
    const QFileInfo currentInfo(request.filePath);
    const QFileInfo editInfo(edit.uri);
    if (currentInfo.absolutePath() == editInfo.absolutePath()) {
        return true;
    }

    return sharesBasenameToken(request.filePath, edit.uri);
}

[[nodiscard]] bool recentEditFresh(const RecentEdit &edit, const CompletionSettings &settings)
{
    if (!edit.timestamp.isValid()) {
        return false;
    }

    const qint64 ageMs = edit.timestamp.msecsTo(QDateTime::currentDateTimeUtc());
    return ageMs >= 0 && ageMs <= settings.autoInlineEditRecentEditWindowMs;
}

[[nodiscard]] QString boundedReasonText(QString text, int maxChars)
{
    if (maxChars <= 0) {
        return {};
    }
    if (text.size() > maxChars) {
        text.truncate(maxChars);
    }
    return text.trimmed();
}

[[nodiscard]] std::optional<InlineEditTrigger> chooseSelectionRepair(const InlineEditTriggerRequest &request, const CompletionSettings &settings)
{
    if (!request.hasSelection || !request.selectionRange.isValid() || !settings.autoInlineEditSelections) {
        return std::nullopt;
    }

    InlineEditTrigger trigger;
    trigger.kind = InlineEditTriggerKind::SelectionRepair;
    trigger.reason = QStringLiteral("Improve or repair the selected code.");
    trigger.targetRange = request.selectionRange;
    trigger.sourceUri = request.filePath;
    trigger.priority = 90;
    return trigger;
}

[[nodiscard]] std::optional<InlineEditTrigger> chooseDiagnosticRepair(const InlineEditTriggerRequest &request, const CompletionSettings &settings)
{
    if (!settings.enableDiagnosticsContext || !settings.autoInlineEditDiagnostics || request.hasSelection) {
        return std::nullopt;
    }

    std::optional<InlineEditTrigger> best;
    int bestDistance = std::numeric_limits<int>::max();
    for (const DiagnosticItem &diagnostic : request.diagnostics) {
        if (!diagnostic.uri.isEmpty() && diagnostic.uri != request.filePath) {
            continue;
        }
        if (!diagnosticSeverityAllowed(diagnostic, settings)) {
            continue;
        }

        const int distance = diagnosticLineDistance(request, diagnostic);
        if (distance > settings.autoInlineEditDiagnosticLineDistance) {
            continue;
        }

        InlineEditTrigger trigger;
        trigger.kind = InlineEditTriggerKind::DiagnosticRepair;
        trigger.diagnosticMessage = boundedReasonText(diagnostic.message, settings.autoInlineEditMaxPromptChars);
        trigger.reason = QStringLiteral("Fix the diagnostic near the cursor:\n%1").arg(trigger.diagnosticMessage);
        trigger.sourceUri = diagnostic.uri.isEmpty() ? request.filePath : diagnostic.uri;
        trigger.targetRange = diagnosticTargetRange(request, diagnostic);
        trigger.priority = (isError(diagnostic) ? 100 : 70) - distance;

        if (!best || trigger.priority > best->priority || (trigger.priority == best->priority && distance < bestDistance)) {
            best = std::move(trigger);
            bestDistance = distance;
        }
    }

    return best;
}

[[nodiscard]] std::optional<InlineEditTrigger> chooseRecentEditContinuation(const InlineEditTriggerRequest &request, const CompletionSettings &settings)
{
    if (!settings.enableRecentEditsContext || !settings.autoInlineEditRecentEdits || request.hasSelection) {
        return std::nullopt;
    }

    std::optional<InlineEditTrigger> best;
    QDateTime bestTimestamp;
    for (const RecentEdit &edit : request.recentEdits) {
        if (edit.uri == request.filePath) {
            continue;
        }
        if (!recentEditFresh(edit, settings)) {
            continue;
        }
        if (!recentEditLanguageMatches(request, edit) || !recentEditPathMatches(request, edit)) {
            continue;
        }

        InlineEditTrigger trigger;
        trigger.kind = InlineEditTriggerKind::RecentEditContinuation;
        trigger.recentEditSummary = boundedReasonText(edit.summary.isEmpty() ? edit.afterText : edit.summary, settings.autoInlineEditMaxPromptChars);
        trigger.reason = QStringLiteral("Continue the recent edit pattern:\n%1").arg(trigger.recentEditSummary);
        trigger.sourceUri = edit.uri;
        trigger.targetRange = currentLineSentinel(request);
        trigger.priority = 40;

        if (!best || edit.timestamp > bestTimestamp) {
            best = std::move(trigger);
            bestTimestamp = edit.timestamp;
        }
    }

    return best;
}
} // namespace

std::optional<InlineEditTrigger> InlineEditTriggerEngine::choose(const InlineEditTriggerRequest &request, const CompletionSettings &settings)
{
    const CompletionSettings v = settings.validated();
    if (!v.enableAutomaticInlineEdits) {
        return std::nullopt;
    }

    if (const std::optional<InlineEditTrigger> selection = chooseSelectionRepair(request, v)) {
        return selection;
    }

    const std::optional<InlineEditTrigger> diagnostic = chooseDiagnosticRepair(request, v);
    const std::optional<InlineEditTrigger> recent = chooseRecentEditContinuation(request, v);

    if (diagnostic && recent) {
        return diagnostic->priority >= recent->priority ? diagnostic : recent;
    }

    return diagnostic ? diagnostic : recent;
}

} // namespace KateAiInlineCompletion
