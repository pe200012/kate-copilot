/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionSettings

    Stores user-facing configuration for AI inline completion.
    The type provides:
    - stable defaults
    - validation and normalization
    - KConfigGroup serialization
*/

#pragma once

#include <KConfigGroup>

#include <QUrl>

namespace KateAiInlineCompletion
{

struct CompletionSettings {
    // Persistence keys
    static constexpr const char *kConfigGroupName = "KateAiInlineCompletion";

    // Validation bounds
    static constexpr int kDebounceMinMs = 50;
    static constexpr int kDebounceMaxMs = 1000;
    static constexpr int kPrefixMinChars = 1000;
    static constexpr int kPrefixMaxChars = 20000;
    static constexpr int kSuffixMinChars = 0;
    static constexpr int kSuffixMaxChars = 8000;
    static constexpr int kContextItemsMin = 0;
    static constexpr int kContextItemsMax = 20;
    static constexpr int kContextCharsMin = 0;
    static constexpr int kContextCharsMax = 30000;

    // Provider identifiers
    static constexpr const char *kProviderOpenAICompatible = "openai-compatible";
    static constexpr const char *kProviderOllama = "ollama";
    static constexpr const char *kProviderGitHubCopilotCodex = "github-copilot-codex";

    // Prompt template identifiers
    static constexpr const char *kPromptTemplateFimV1 = "fim_v1";
    static constexpr const char *kPromptTemplateFimV2 = "fim_v2";
    static constexpr const char *kPromptTemplateFimV3 = "fim_v3";

    bool enabled = true;
    int debounceMs = 180;
    int maxPrefixChars = 12000;
    int maxSuffixChars = 3000;

    QString provider = QString::fromLatin1(kProviderOpenAICompatible);
    QUrl endpoint = QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions"));
    QString model = QStringLiteral("gpt-4o-mini");
    QString promptTemplate = QString::fromLatin1(kPromptTemplateFimV3);

    bool enableContextualPrompt = true;
    int maxContextItems = 6;
    int maxContextChars = 6000;

    // GitHub Copilot (OAuth) provider options
    QString copilotClientId = QStringLiteral("Iv1.b507a08c87ecfe98");
    QString copilotNwo = QStringLiteral("github/copilot.vim");

    bool suppressWhenCompletionPopupVisible = true;

    [[nodiscard]] static CompletionSettings defaults();

    [[nodiscard]] CompletionSettings validated() const;

    [[nodiscard]] static CompletionSettings load(const KConfigGroup &group);
    void save(KConfigGroup &group) const;
};

} // namespace KateAiInlineCompletion
