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
    static constexpr int kRecentEditsMaxFilesMin = 1;
    static constexpr int kRecentEditsMaxFilesMax = 100;
    static constexpr int kRecentEditsMaxEditsMin = 0;
    static constexpr int kRecentEditsMaxEditsMax = 50;
    static constexpr int kRecentEditsDiffContextLinesMin = 0;
    static constexpr int kRecentEditsDiffContextLinesMax = 20;
    static constexpr int kRecentEditsMaxCharsPerEditMin = 200;
    static constexpr int kRecentEditsMaxCharsPerEditMax = 20000;
    static constexpr int kRecentEditsDebounceMinMs = 50;
    static constexpr int kRecentEditsDebounceMaxMs = 5000;
    static constexpr int kRecentEditsMaxLinesPerEditMin = 1;
    static constexpr int kRecentEditsMaxLinesPerEditMax = 100;
    static constexpr int kRecentEditsActiveDocDistanceLimitMin = 0;
    static constexpr int kRecentEditsActiveDocDistanceLimitMax = 10000;
    static constexpr int kDiagnosticsMaxItemsMin = 0;
    static constexpr int kDiagnosticsMaxItemsMax = 50;
    static constexpr int kDiagnosticsMaxCharsMin = 0;
    static constexpr int kDiagnosticsMaxCharsMax = 30000;
    static constexpr int kDiagnosticsMaxLineDistanceMin = 0;
    static constexpr int kDiagnosticsMaxLineDistanceMax = 10000;

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

    bool enableRecentEditsContext = true;
    int recentEditsMaxFiles = 20;
    int recentEditsMaxEdits = 8;
    int recentEditsDiffContextLines = 3;
    int recentEditsMaxCharsPerEdit = 2000;
    int recentEditsDebounceMs = 500;
    int recentEditsMaxLinesPerEdit = 10;
    int recentEditsActiveDocDistanceLimitFromCursor = 100;

    bool enableDiagnosticsContext = true;
    int diagnosticsMaxItems = 8;
    int diagnosticsMaxChars = 3000;
    int diagnosticsMaxLineDistance = 120;
    bool diagnosticsIncludeWarnings = true;
    bool diagnosticsIncludeInformation = false;
    bool diagnosticsIncludeHints = false;

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
