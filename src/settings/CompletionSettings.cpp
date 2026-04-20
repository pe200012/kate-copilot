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
