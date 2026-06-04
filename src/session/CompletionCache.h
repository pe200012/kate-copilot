/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionCache

    Stores local inline-completion results keyed by stable request traits.
*/

#pragma once

#include "session/CompletionCandidate.h"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

namespace KateAiInlineCompletion
{

struct CompletionSettings;
struct CompletionStrategy;
struct PromptContext;

struct CompletionCacheKey {
    QString providerId;
    QString model;
    QString promptTemplate;
    QString languageId;
    QString filePath;
    QString endpointHash;
    QString copilotNwoHash;
    QString prefixTailHash;
    QString suffixHeadHash;
    QString assembledPromptHash;
    QString requestShapeHash;
    int requestedCandidateCount = 1;
    bool requestMultiline = false;
    QString strategyMode;

    [[nodiscard]] bool operator==(const CompletionCacheKey &other) const;
};

struct CompletionCacheValue {
    QVector<CompletionCandidate> candidates;
    QDateTime createdAt;
    int hitCount = 0;
};

struct CompletionCacheOptions {
    bool enabled = true;
    int maxEntries = 128;
    int ttlMs = 120000;
    int prefixTailChars = 1200;
    int suffixHeadChars = 600;
    int maxStoredCandidates = 8;
};

struct CompletionCacheDocumentText {
    QString prefix;
    QString suffix;
};

struct CompletionCacheRequestFingerprints {
    QString assembledPrompt;
    QString requestShape;
};

class CompletionCache final
{
public:
    explicit CompletionCache(CompletionCacheOptions options = {});

    void setOptions(CompletionCacheOptions options);
    [[nodiscard]] CompletionCacheOptions options() const;

    void insert(const CompletionCacheKey &key, const CompletionCacheValue &value);
    [[nodiscard]] std::optional<CompletionCacheValue> lookupExact(const CompletionCacheKey &key);
    [[nodiscard]] std::optional<CompletionCacheValue> lookupTypingAsSuggested(const CompletionCacheKey &key, const QString &typedPrefixDelta);

    void clear();
    [[nodiscard]] int size();

    [[nodiscard]] static CompletionCacheKey makeKey(const CompletionSettings &settings,
                                                    const CompletionStrategy &strategy,
                                                    const PromptContext &promptCtx,
                                                    const CompletionCacheDocumentText &documentText,
                                                    int requestedCandidateCount,
                                                    const CompletionCacheRequestFingerprints &fingerprints);

private:
    struct Entry {
        CompletionCacheKey key;
        CompletionCacheValue value;
        qint64 lastAccessOrder = 0;
    };

    void pruneExpired();
    void enforceMaxEntries();
    [[nodiscard]] int indexOf(const CompletionCacheKey &key) const;
    [[nodiscard]] bool isExpired(const CompletionCacheValue &value, const QDateTime &now) const;

    CompletionCacheOptions m_options;
    QVector<Entry> m_entries;
    qint64 m_accessCounter = 0;
};

} // namespace KateAiInlineCompletion
