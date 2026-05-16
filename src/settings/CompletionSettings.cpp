/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionSettings
*/

#include "settings/CompletionSettings.h"

#include <QtGlobal>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] bool isSupportedProvider(const QString &provider)
{
    return provider == QString::fromLatin1(CompletionSettings::kProviderOpenAICompatible)
        || provider == QString::fromLatin1(CompletionSettings::kProviderOllama)
        || provider == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex);
}

[[nodiscard]] bool isSupportedPromptTemplate(const QString &templateId)
{
    return templateId == QString::fromLatin1(CompletionSettings::kPromptTemplateFimV1)
        || templateId == QString::fromLatin1(CompletionSettings::kPromptTemplateFimV2)
        || templateId == QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3);
}

[[nodiscard]] QUrl defaultEndpointForProvider(const QString &provider)
{
    if (provider == QString::fromLatin1(CompletionSettings::kProviderOllama)) {
        return QUrl(QStringLiteral("http://localhost:11434/v1/chat/completions"));
    }

    if (provider == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex)) {
        return QUrl(QStringLiteral("https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions"));
    }

    return QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions"));
}

[[nodiscard]] QString defaultModelForProvider(const QString &provider)
{
    if (provider == QString::fromLatin1(CompletionSettings::kProviderOllama)) {
        return QStringLiteral("llama3.2");
    }

    if (provider == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex)) {
        return QStringLiteral("cushman-ml");
    }

    return QStringLiteral("gpt-4o-mini");
}

[[nodiscard]] bool isAbsoluteHttpUrl(const QUrl &url)
{
    if (!url.isValid()) {
        return false;
    }

    if (url.isRelative()) {
        return false;
    }

    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
}

[[nodiscard]] QStringList normalizedPatterns(const QStringList &patterns)
{
    QStringList out;
    for (const QString &pattern : patterns) {
        const QString trimmed = pattern.trimmed();
        if (!trimmed.isEmpty() && !out.contains(trimmed)) {
            out.push_back(trimmed);
        }
    }
    return out;
}

} // namespace

CompletionSettings CompletionSettings::defaults()
{
    return CompletionSettings{};
}

CompletionSettings CompletionSettings::validated() const
{
    CompletionSettings out = *this;

    out.debounceMs = qBound(kDebounceMinMs, out.debounceMs, kDebounceMaxMs);
    out.maxPrefixChars = qBound(kPrefixMinChars, out.maxPrefixChars, kPrefixMaxChars);
    out.maxSuffixChars = qBound(kSuffixMinChars, out.maxSuffixChars, kSuffixMaxChars);
    out.maxContextItems = qBound(kContextItemsMin, out.maxContextItems, kContextItemsMax);
    out.maxContextChars = qBound(kContextCharsMin, out.maxContextChars, kContextCharsMax);
    out.recentEditsMaxFiles = qBound(kRecentEditsMaxFilesMin, out.recentEditsMaxFiles, kRecentEditsMaxFilesMax);
    out.recentEditsMaxEdits = qBound(kRecentEditsMaxEditsMin, out.recentEditsMaxEdits, kRecentEditsMaxEditsMax);
    out.recentEditsDiffContextLines = qBound(kRecentEditsDiffContextLinesMin, out.recentEditsDiffContextLines, kRecentEditsDiffContextLinesMax);
    out.recentEditsMaxCharsPerEdit = qBound(kRecentEditsMaxCharsPerEditMin, out.recentEditsMaxCharsPerEdit, kRecentEditsMaxCharsPerEditMax);
    out.recentEditsDebounceMs = qBound(kRecentEditsDebounceMinMs, out.recentEditsDebounceMs, kRecentEditsDebounceMaxMs);
    out.recentEditsMaxLinesPerEdit = qBound(kRecentEditsMaxLinesPerEditMin, out.recentEditsMaxLinesPerEdit, kRecentEditsMaxLinesPerEditMax);
    out.recentEditsActiveDocDistanceLimitFromCursor = qBound(kRecentEditsActiveDocDistanceLimitMin,
                                                             out.recentEditsActiveDocDistanceLimitFromCursor,
                                                             kRecentEditsActiveDocDistanceLimitMax);
    out.diagnosticsMaxItems = qBound(kDiagnosticsMaxItemsMin, out.diagnosticsMaxItems, kDiagnosticsMaxItemsMax);
    out.diagnosticsMaxChars = qBound(kDiagnosticsMaxCharsMin, out.diagnosticsMaxChars, kDiagnosticsMaxCharsMax);
    out.diagnosticsMaxLineDistance = qBound(kDiagnosticsMaxLineDistanceMin, out.diagnosticsMaxLineDistance, kDiagnosticsMaxLineDistanceMax);
    out.relatedFilesMaxFiles = qBound(kRelatedFilesMaxFilesMin, out.relatedFilesMaxFiles, kRelatedFilesMaxFilesMax);
    out.relatedFilesMaxChars = qBound(kRelatedFilesMaxCharsMin, out.relatedFilesMaxChars, kRelatedFilesMaxCharsMax);
    out.relatedFilesMaxCharsPerFile = qBound(kRelatedFilesMaxCharsPerFileMin,
                                             out.relatedFilesMaxCharsPerFile,
                                             kRelatedFilesMaxCharsPerFileMax);
    out.contextExcludePatterns = normalizedPatterns(out.contextExcludePatterns);

    out.provider = out.provider.trimmed().toLower();
    if (!isSupportedProvider(out.provider)) {
        out.provider = QString::fromLatin1(kProviderOpenAICompatible);
    }

    if (!isAbsoluteHttpUrl(out.endpoint)) {
        out.endpoint = defaultEndpointForProvider(out.provider);
    }

    if (out.provider == QString::fromLatin1(kProviderGitHubCopilotCodex)) {
        out.endpoint = defaultEndpointForProvider(out.provider);
    }

    out.model = out.model.trimmed();
    if (out.model.isEmpty()) {
        out.model = defaultModelForProvider(out.provider);
    }

    out.promptTemplate = out.promptTemplate.trimmed().toLower();
    if (!isSupportedPromptTemplate(out.promptTemplate)) {
        out.promptTemplate = QString::fromLatin1(kPromptTemplateFimV3);
    }

    out.copilotClientId = out.copilotClientId.trimmed();
    if (out.copilotClientId.isEmpty()) {
        out.copilotClientId = CompletionSettings::defaults().copilotClientId;
    }

    out.copilotNwo = out.copilotNwo.trimmed();
    if (out.copilotNwo.isEmpty()) {
        out.copilotNwo = CompletionSettings::defaults().copilotNwo;
    }

    return out;
}

CompletionSettings CompletionSettings::load(const KConfigGroup &group)
{
    const CompletionSettings d = CompletionSettings::defaults();

    CompletionSettings out;
    out.enabled = group.readEntry("Enabled", d.enabled);
    out.debounceMs = group.readEntry("DebounceMs", d.debounceMs);
    out.maxPrefixChars = group.readEntry("MaxPrefixChars", d.maxPrefixChars);
    out.maxSuffixChars = group.readEntry("MaxSuffixChars", d.maxSuffixChars);
    out.enableContextualPrompt = group.readEntry("EnableContextualPrompt", d.enableContextualPrompt);
    out.maxContextItems = group.readEntry("MaxContextItems", d.maxContextItems);
    out.maxContextChars = group.readEntry("MaxContextChars", d.maxContextChars);
    out.enableOpenTabsContext = group.readEntry("EnableOpenTabsContext", d.enableOpenTabsContext);
    out.enableRecentEditsContext = group.readEntry("EnableRecentEditsContext", d.enableRecentEditsContext);
    out.recentEditsMaxFiles = group.readEntry("RecentEditsMaxFiles", d.recentEditsMaxFiles);
    out.recentEditsMaxEdits = group.readEntry("RecentEditsMaxEdits", d.recentEditsMaxEdits);
    out.recentEditsDiffContextLines = group.readEntry("RecentEditsDiffContextLines", d.recentEditsDiffContextLines);
    out.recentEditsMaxCharsPerEdit = group.readEntry("RecentEditsMaxCharsPerEdit", d.recentEditsMaxCharsPerEdit);
    out.recentEditsDebounceMs = group.readEntry("RecentEditsDebounceMs", d.recentEditsDebounceMs);
    out.recentEditsMaxLinesPerEdit = group.readEntry("RecentEditsMaxLinesPerEdit", d.recentEditsMaxLinesPerEdit);
    out.recentEditsActiveDocDistanceLimitFromCursor = group.readEntry("RecentEditsActiveDocDistanceLimitFromCursor",
                                                                     d.recentEditsActiveDocDistanceLimitFromCursor);
    out.enableDiagnosticsContext = group.readEntry("EnableDiagnosticsContext", d.enableDiagnosticsContext);
    out.diagnosticsMaxItems = group.readEntry("DiagnosticsMaxItems", d.diagnosticsMaxItems);
    out.diagnosticsMaxChars = group.readEntry("DiagnosticsMaxChars", d.diagnosticsMaxChars);
    out.diagnosticsMaxLineDistance = group.readEntry("DiagnosticsMaxLineDistance", d.diagnosticsMaxLineDistance);
    out.diagnosticsIncludeWarnings = group.readEntry("DiagnosticsIncludeWarnings", d.diagnosticsIncludeWarnings);
    out.diagnosticsIncludeInformation = group.readEntry("DiagnosticsIncludeInformation", d.diagnosticsIncludeInformation);
    out.diagnosticsIncludeHints = group.readEntry("DiagnosticsIncludeHints", d.diagnosticsIncludeHints);
    out.enableRelatedFilesContext = group.readEntry("EnableRelatedFilesContext", d.enableRelatedFilesContext);
    out.relatedFilesMaxFiles = group.readEntry("RelatedFilesMaxFiles", d.relatedFilesMaxFiles);
    out.relatedFilesMaxChars = group.readEntry("RelatedFilesMaxChars", d.relatedFilesMaxChars);
    out.relatedFilesMaxCharsPerFile = group.readEntry("RelatedFilesMaxCharsPerFile", d.relatedFilesMaxCharsPerFile);
    out.relatedFilesPreferOpenTabs = group.readEntry("RelatedFilesPreferOpenTabs", d.relatedFilesPreferOpenTabs);
    out.contextExcludePatterns = group.readEntry("ContextExcludePatterns", d.contextExcludePatterns);

    out.provider = group.readEntry("Provider", d.provider);
    out.endpoint = QUrl(group.readEntry("Endpoint", d.endpoint.toString()));
    out.model = group.readEntry("Model", d.model);
    out.promptTemplate = group.readEntry("PromptTemplate", d.promptTemplate);

    out.copilotClientId = group.readEntry("CopilotClientId", d.copilotClientId);
    out.copilotNwo = group.readEntry("CopilotNwo", d.copilotNwo);

    out.suppressWhenCompletionPopupVisible = group.readEntry("SuppressWhenCompletionPopupVisible", d.suppressWhenCompletionPopupVisible);

    return out.validated();
}

void CompletionSettings::save(KConfigGroup &group) const
{
    const CompletionSettings v = validated();

    group.writeEntry("Enabled", v.enabled);
    group.writeEntry("DebounceMs", v.debounceMs);
    group.writeEntry("MaxPrefixChars", v.maxPrefixChars);
    group.writeEntry("MaxSuffixChars", v.maxSuffixChars);
    group.writeEntry("EnableContextualPrompt", v.enableContextualPrompt);
    group.writeEntry("MaxContextItems", v.maxContextItems);
    group.writeEntry("MaxContextChars", v.maxContextChars);
    group.writeEntry("EnableOpenTabsContext", v.enableOpenTabsContext);
    group.writeEntry("EnableRecentEditsContext", v.enableRecentEditsContext);
    group.writeEntry("RecentEditsMaxFiles", v.recentEditsMaxFiles);
    group.writeEntry("RecentEditsMaxEdits", v.recentEditsMaxEdits);
    group.writeEntry("RecentEditsDiffContextLines", v.recentEditsDiffContextLines);
    group.writeEntry("RecentEditsMaxCharsPerEdit", v.recentEditsMaxCharsPerEdit);
    group.writeEntry("RecentEditsDebounceMs", v.recentEditsDebounceMs);
    group.writeEntry("RecentEditsMaxLinesPerEdit", v.recentEditsMaxLinesPerEdit);
    group.writeEntry("RecentEditsActiveDocDistanceLimitFromCursor", v.recentEditsActiveDocDistanceLimitFromCursor);
    group.writeEntry("EnableDiagnosticsContext", v.enableDiagnosticsContext);
    group.writeEntry("DiagnosticsMaxItems", v.diagnosticsMaxItems);
    group.writeEntry("DiagnosticsMaxChars", v.diagnosticsMaxChars);
    group.writeEntry("DiagnosticsMaxLineDistance", v.diagnosticsMaxLineDistance);
    group.writeEntry("DiagnosticsIncludeWarnings", v.diagnosticsIncludeWarnings);
    group.writeEntry("DiagnosticsIncludeInformation", v.diagnosticsIncludeInformation);
    group.writeEntry("DiagnosticsIncludeHints", v.diagnosticsIncludeHints);
    group.writeEntry("EnableRelatedFilesContext", v.enableRelatedFilesContext);
    group.writeEntry("RelatedFilesMaxFiles", v.relatedFilesMaxFiles);
    group.writeEntry("RelatedFilesMaxChars", v.relatedFilesMaxChars);
    group.writeEntry("RelatedFilesMaxCharsPerFile", v.relatedFilesMaxCharsPerFile);
    group.writeEntry("RelatedFilesPreferOpenTabs", v.relatedFilesPreferOpenTabs);
    group.writeEntry("ContextExcludePatterns", v.contextExcludePatterns);

    group.writeEntry("Provider", v.provider);
    group.writeEntry("Endpoint", v.endpoint.toString());
    group.writeEntry("Model", v.model);
    group.writeEntry("PromptTemplate", v.promptTemplate);

    group.writeEntry("CopilotClientId", v.copilotClientId);
    group.writeEntry("CopilotNwo", v.copilotNwo);

    group.writeEntry("SuppressWhenCompletionPopupVisible", v.suppressWhenCompletionPopupVisible);

    group.sync();
}

} // namespace KateAiInlineCompletion
