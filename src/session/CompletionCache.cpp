/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionCache
*/

#include "session/CompletionCache.h"

#include "prompt/PromptTemplate.h"
#include "session/CompletionCandidateList.h"
#include "session/CompletionStrategy.h"
#include "settings/CompletionSettings.h"

#include <QCryptographicHash>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>
#include <utility>

namespace KateAiInlineCompletion
{
namespace
{
[[nodiscard]] QString sha256Hex(const QString &text)
{
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] QString prefixTailForHash(const QString &prefix, int maxChars)
{
    return maxChars <= 0 ? QString() : prefix.right(maxChars);
}

[[nodiscard]] QString suffixHeadForHash(const QString &suffix, int maxChars)
{
    return maxChars <= 0 ? QString() : suffix.left(maxChars);
}

[[nodiscard]] CompletionCacheOptions validatedOptions(CompletionCacheOptions options)
{
    options.maxEntries = qBound(CompletionSettings::kCompletionCacheMaxEntriesMin,
                                options.maxEntries,
                                CompletionSettings::kCompletionCacheMaxEntriesMax);
    options.ttlMs = qBound(CompletionSettings::kCompletionCacheTtlMinMs,
                           options.ttlMs,
                           CompletionSettings::kCompletionCacheTtlMaxMs);
    options.prefixTailChars = qBound(CompletionSettings::kCompletionCachePrefixTailCharsMin,
                                     options.prefixTailChars,
                                     CompletionSettings::kCompletionCachePrefixTailCharsMax);
    options.suffixHeadChars = qBound(CompletionSettings::kCompletionCacheSuffixHeadCharsMin,
                                     options.suffixHeadChars,
                                     CompletionSettings::kCompletionCacheSuffixHeadCharsMax);
    options.maxStoredCandidates = qBound(CompletionSettings::kMaxStoredCandidatesMin,
                                         options.maxStoredCandidates,
                                         CompletionSettings::kMaxStoredCandidatesMax);
    return options;
}

[[nodiscard]] QVector<CompletionCandidate> normalizedCandidatesForStorage(const CompletionCacheValue &value, int maxStoredCandidates)
{
    QVector<CompletionCandidate> candidates = value.candidates;
    candidates = CompletionCandidateList::deduplicated(std::move(candidates));
    if (candidates.size() > maxStoredCandidates) {
        candidates.resize(maxStoredCandidates);
    }
    return candidates;
}

[[nodiscard]] CompletionCandidate shrinkCandidateByTypedPrefix(CompletionCandidate candidate, const QString &typedPrefixDelta)
{
    candidate.rawCompletion = candidate.rawCompletion.startsWith(typedPrefixDelta) ? candidate.rawCompletion.mid(typedPrefixDelta.size()) : candidate.rawCompletion;
    candidate.insertText = candidate.insertText.mid(typedPrefixDelta.size());
    candidate.displayText = candidate.displayText.mid(typedPrefixDelta.size());
    candidate.suffixCoverage = 0;
    return candidate;
}
}

bool CompletionCacheKey::operator==(const CompletionCacheKey &other) const
{
    return providerId == other.providerId && model == other.model && promptTemplate == other.promptTemplate && languageId == other.languageId
        && filePath == other.filePath && endpointHash == other.endpointHash && copilotNwoHash == other.copilotNwoHash
        && prefixTailHash == other.prefixTailHash && suffixHeadHash == other.suffixHeadHash && assembledPromptHash == other.assembledPromptHash
        && requestShapeHash == other.requestShapeHash && requestedCandidateCount == other.requestedCandidateCount && requestMultiline == other.requestMultiline
        && strategyMode == other.strategyMode;
}

CompletionCache::CompletionCache(CompletionCacheOptions options)
{
    setOptions(options);
}

void CompletionCache::setOptions(CompletionCacheOptions options)
{
    m_options = validatedOptions(options);
    if (!m_options.enabled || m_options.maxEntries == 0) {
        clear();
        return;
    }

    pruneExpired();
    enforceMaxEntries();
}

CompletionCacheOptions CompletionCache::options() const
{
    return m_options;
}

void CompletionCache::insert(const CompletionCacheKey &key, const CompletionCacheValue &value)
{
    if (!m_options.enabled || m_options.maxEntries <= 0) {
        return;
    }

    CompletionCacheValue stored = value;
    stored.candidates = normalizedCandidatesForStorage(value, m_options.maxStoredCandidates);

    if (stored.candidates.isEmpty()) {
        return;
    }

    pruneExpired();

    if (!stored.createdAt.isValid()) {
        stored.createdAt = QDateTime::currentDateTimeUtc();
    }

    const int existingIndex = indexOf(key);
    if (existingIndex >= 0) {
        m_entries[existingIndex].value = stored;
        m_entries[existingIndex].lastAccessOrder = ++m_accessCounter;
        return;
    }

    Entry entry;
    entry.key = key;
    entry.value = stored;
    entry.lastAccessOrder = ++m_accessCounter;
    m_entries.push_back(entry);
    enforceMaxEntries();
}

std::optional<CompletionCacheValue> CompletionCache::lookupExact(const CompletionCacheKey &key)
{
    if (!m_options.enabled || m_options.maxEntries <= 0) {
        return std::nullopt;
    }

    pruneExpired();

    const int idx = indexOf(key);
    if (idx < 0) {
        return std::nullopt;
    }

    m_entries[idx].lastAccessOrder = ++m_accessCounter;
    ++m_entries[idx].value.hitCount;
    return m_entries[idx].value;
}

std::optional<CompletionCacheValue> CompletionCache::lookupTypingAsSuggested(const CompletionCacheKey &key, const QString &typedPrefixDelta)
{
    if (typedPrefixDelta.isEmpty()) {
        return lookupExact(key);
    }

    if (!m_options.enabled || m_options.maxEntries <= 0) {
        return std::nullopt;
    }

    pruneExpired();

    const int idx = indexOf(key);
    if (idx < 0) {
        return std::nullopt;
    }

    const CompletionCacheValue &stored = m_entries.at(idx).value;
    QVector<CompletionCandidate> candidates;
    for (const CompletionCandidate &candidate : stored.candidates) {
        if (candidate.insertText.startsWith(typedPrefixDelta) && candidate.displayText.startsWith(typedPrefixDelta)) {
            candidates.push_back(shrinkCandidateByTypedPrefix(candidate, typedPrefixDelta));
        }
    }

    candidates = CompletionCandidateList::deduplicated(std::move(candidates));
    if (candidates.isEmpty()) {
        return std::nullopt;
    }

    m_entries[idx].lastAccessOrder = ++m_accessCounter;
    ++m_entries[idx].value.hitCount;

    CompletionCacheValue hit = m_entries.at(idx).value;
    hit.candidates = candidates;

    return hit;
}

void CompletionCache::clear()
{
    m_entries.clear();
}

int CompletionCache::size()
{
    pruneExpired();
    return m_entries.size();
}

CompletionCacheKey CompletionCache::makeKey(const CompletionSettings &settings,
                                            const CompletionStrategy &strategy,
                                            const PromptContext &promptCtx,
                                            const QString &prefix,
                                            const QString &suffix,
                                            int requestedCandidateCount,
                                            const QString &assembledPromptFingerprint,
                                            const QString &requestShapeFingerprint)
{
    const CompletionSettings validated = settings.validated();

    CompletionCacheKey key;
    key.providerId = validated.provider;
    key.model = validated.model;
    key.promptTemplate = validated.promptTemplate;
    key.languageId = promptCtx.language;
    key.filePath = promptCtx.filePath;
    key.endpointHash = sha256Hex(validated.endpoint.toString(QUrl::RemoveUserInfo));
    key.copilotNwoHash = validated.provider == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex) ? sha256Hex(validated.copilotNwo) : QString();
    key.prefixTailHash = sha256Hex(prefixTailForHash(prefix, validated.completionCachePrefixTailChars));
    key.suffixHeadHash = sha256Hex(suffixHeadForHash(suffix, validated.completionCacheSuffixHeadChars));
    key.assembledPromptHash = sha256Hex(assembledPromptFingerprint);
    key.requestShapeHash = sha256Hex(requestShapeFingerprint);
    key.requestedCandidateCount = qMax(1, requestedCandidateCount);
    key.requestMultiline = strategy.requestMultiline;
    key.strategyMode = completionStrategyModeName(strategy.mode);
    return key;
}

void CompletionCache::pruneExpired()
{
    if (!m_options.enabled || m_entries.isEmpty()) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [this, &now](const Entry &entry) {
                        return isExpired(entry.value, now);
                    }),
                    m_entries.end());
}

void CompletionCache::enforceMaxEntries()
{
    if (m_options.maxEntries <= 0) {
        clear();
        return;
    }

    while (m_entries.size() > m_options.maxEntries) {
        int oldestIndex = 0;
        for (int i = 1; i < m_entries.size(); ++i) {
            if (m_entries.at(i).lastAccessOrder < m_entries.at(oldestIndex).lastAccessOrder) {
                oldestIndex = i;
            }
        }
        m_entries.removeAt(oldestIndex);
    }
}

int CompletionCache::indexOf(const CompletionCacheKey &key) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).key == key) {
            return i;
        }
    }
    return -1;
}

bool CompletionCache::isExpired(const CompletionCacheValue &value, const QDateTime &now) const
{
    return value.createdAt.isValid() && value.createdAt.msecsTo(now) > m_options.ttlMs;
}

} // namespace KateAiInlineCompletion
