/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionCacheTest
*/

#include "session/CompletionCache.h"

#include "prompt/PromptTemplate.h"
#include "session/CompletionCandidate.h"
#include "session/CompletionStrategy.h"
#include "settings/CompletionSettings.h"

#include <QDateTime>
#include <QUrl>
#include <QtTest>

using KateAiInlineCompletion::CompletionCache;
using KateAiInlineCompletion::CompletionCacheKey;
using KateAiInlineCompletion::CompletionCacheOptions;
using KateAiInlineCompletion::CompletionCacheValue;
using KateAiInlineCompletion::CompletionCandidate;
using KateAiInlineCompletion::CompletionSettings;
using KateAiInlineCompletion::CompletionStrategy;
using KateAiInlineCompletion::PromptContext;

namespace
{
CompletionCacheKey baseKey()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    settings.provider = QString::fromLatin1(CompletionSettings::kProviderOpenAICompatible);
    settings.model = QStringLiteral("test-model");
    settings.promptTemplate = QString::fromLatin1(CompletionSettings::kPromptTemplateFimV3);
    settings.completionCachePrefixTailChars = 5;
    settings.completionCacheSuffixHeadChars = 4;

    CompletionStrategy strategy;
    strategy.mode = CompletionStrategy::Mode::SingleLine;
    strategy.requestMultiline = false;

    PromptContext ctx;
    ctx.language = QStringLiteral("C++");
    ctx.filePath = QStringLiteral("/repo/src/main.cpp");

    return CompletionCache::makeKey(settings, strategy, ctx, QStringLiteral("abcdef"), QStringLiteral("wxyz-tail"));
}

CompletionCandidate candidate(QString text, QString source = QStringLiteral("test"))
{
    CompletionCandidate c;
    c.rawCompletion = text;
    c.insertText = text;
    c.displayText = text;
    c.source = source;
    c.id = QStringLiteral("%1:%2").arg(source, text);
    return c;
}

CompletionCacheValue baseValue(QString text = QStringLiteral("ghostText"))
{
    CompletionCacheValue value;
    value.candidates = {candidate(text)};
    value.createdAt = QDateTime::currentDateTimeUtc();
    return value;
}
}

class CompletionCacheTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exactLookupReturnsInsertedValue();
    void lookupExpiresAfterTtl();
    void lruMaxEntriesEvictsOldEntries();
    void providerModelTemplateDifferencesMiss();
    void endpointAndCopilotNwoDifferencesMiss();
    void prefixTailHashDifferencesMiss();
    void suffixHeadHashDifferencesMiss();
    void assembledPromptFingerprintDifferencesMiss();
    void requestedCandidateCountDifferencesMiss();
    void disabledCacheStoresNothing();
    void clearRemovesEntries();
    void storesMultipleCandidates();
    void maxStoredCandidatesIsRespected();
    void candidateDedupAppliesOnInsert();
    void typingAsSuggestedLookupWorksWithCandidateList();
    void typingAsSuggestedLookupReturnsRemainingCompletion();
    void typingAsSuggestedRejectsNonmatchingPrefix();
};

void CompletionCacheTest::exactLookupReturnsInsertedValue()
{
    CompletionCache cache;
    const CompletionCacheKey key = baseKey();
    cache.insert(key, baseValue());

    const auto hit = cache.lookupExact(key);
    QVERIFY(hit.has_value());
    QCOMPARE(hit->candidates.size(), 1);
    QCOMPARE(hit->candidates.constFirst().rawCompletion, QStringLiteral("ghostText"));
    QCOMPARE(hit->candidates.constFirst().insertText, QStringLiteral("ghostText"));
    QCOMPARE(hit->candidates.constFirst().displayText, QStringLiteral("ghostText"));
    QCOMPARE(hit->hitCount, 1);
}

void CompletionCacheTest::lookupExpiresAfterTtl()
{
    CompletionCacheOptions options;
    options.ttlMs = CompletionSettings::kCompletionCacheTtlMinMs;
    CompletionCache cache(options);

    CompletionCacheValue stale = baseValue();
    stale.createdAt = QDateTime::currentDateTimeUtc().addMSecs(-CompletionSettings::kCompletionCacheTtlMinMs - 1);
    cache.insert(baseKey(), stale);

    QVERIFY(!cache.lookupExact(baseKey()).has_value());
    QCOMPARE(cache.size(), 0);
}

void CompletionCacheTest::lruMaxEntriesEvictsOldEntries()
{
    CompletionCacheOptions options;
    options.maxEntries = 2;
    CompletionCache cache(options);

    CompletionCacheKey first = baseKey();
    CompletionCacheKey second = baseKey();
    second.model = QStringLiteral("second");
    CompletionCacheKey third = baseKey();
    third.model = QStringLiteral("third");

    cache.insert(first, baseValue(QStringLiteral("first")));
    cache.insert(second, baseValue(QStringLiteral("second")));
    QVERIFY(cache.lookupExact(first).has_value());
    cache.insert(third, baseValue(QStringLiteral("third")));

    QVERIFY(cache.lookupExact(first).has_value());
    QVERIFY(!cache.lookupExact(second).has_value());
    QVERIFY(cache.lookupExact(third).has_value());
    QCOMPARE(cache.size(), 2);
}

void CompletionCacheTest::providerModelTemplateDifferencesMiss()
{
    CompletionCache cache;
    const CompletionCacheKey key = baseKey();
    cache.insert(key, baseValue());

    CompletionCacheKey provider = key;
    provider.providerId = QStringLiteral("ollama");
    QVERIFY(!cache.lookupExact(provider).has_value());

    CompletionCacheKey model = key;
    model.model = QStringLiteral("other-model");
    QVERIFY(!cache.lookupExact(model).has_value());

    CompletionCacheKey templ = key;
    templ.promptTemplate = QStringLiteral("fim_v2");
    QVERIFY(!cache.lookupExact(templ).has_value());
}

void CompletionCacheTest::endpointAndCopilotNwoDifferencesMiss()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    settings.completionCachePrefixTailChars = 5;
    settings.completionCacheSuffixHeadChars = 4;

    CompletionStrategy strategy;
    PromptContext ctx;
    ctx.language = QStringLiteral("C++");
    ctx.filePath = QStringLiteral("/repo/src/main.cpp");

    const CompletionCacheKey first = CompletionCache::makeKey(settings, strategy, ctx, QStringLiteral("abcdef"), QStringLiteral("wxyz-tail"));
    CompletionCache cache;
    cache.insert(first, baseValue());

    settings.endpoint = QUrl(QStringLiteral("https://example.invalid/v1/chat/completions"));
    const CompletionCacheKey endpointChanged = CompletionCache::makeKey(settings, strategy, ctx, QStringLiteral("abcdef"), QStringLiteral("wxyz-tail"));
    QVERIFY(!cache.lookupExact(endpointChanged).has_value());

    settings.provider = QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex);
    settings.endpoint = QUrl(QStringLiteral("https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions"));
    settings.copilotNwo = QStringLiteral("first/nwo");
    const CompletionCacheKey firstNwo = CompletionCache::makeKey(settings, strategy, ctx, QStringLiteral("abcdef"), QStringLiteral("wxyz-tail"));
    cache.insert(firstNwo, baseValue(QStringLiteral("copilot")));

    settings.copilotNwo = QStringLiteral("second/nwo");
    const CompletionCacheKey secondNwo = CompletionCache::makeKey(settings, strategy, ctx, QStringLiteral("abcdef"), QStringLiteral("wxyz-tail"));
    QVERIFY(!cache.lookupExact(secondNwo).has_value());
}

void CompletionCacheTest::prefixTailHashDifferencesMiss()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    settings.completionCachePrefixTailChars = 5;
    settings.completionCacheSuffixHeadChars = 4;

    CompletionStrategy strategy;
    PromptContext ctx;
    ctx.language = QStringLiteral("C++");
    ctx.filePath = QStringLiteral("/repo/src/main.cpp");

    const CompletionCacheKey first = CompletionCache::makeKey(settings, strategy, ctx, QStringLiteral("abcdef"), QStringLiteral("same-suffix"));
    const CompletionCacheKey second = CompletionCache::makeKey(settings, strategy, ctx, QStringLiteral("abcxef"), QStringLiteral("same-suffix"));

    CompletionCache cache;
    cache.insert(first, baseValue());
    QVERIFY(!cache.lookupExact(second).has_value());
}

void CompletionCacheTest::suffixHeadHashDifferencesMiss()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    settings.completionCachePrefixTailChars = 5;
    settings.completionCacheSuffixHeadChars = 4;

    CompletionStrategy strategy;
    PromptContext ctx;
    ctx.language = QStringLiteral("C++");
    ctx.filePath = QStringLiteral("/repo/src/main.cpp");

    const CompletionCacheKey first = CompletionCache::makeKey(settings, strategy, ctx, QStringLiteral("same-prefix"), QStringLiteral("abcd-tail"));
    const CompletionCacheKey second = CompletionCache::makeKey(settings, strategy, ctx, QStringLiteral("same-prefix"), QStringLiteral("abxd-tail"));

    CompletionCache cache;
    cache.insert(first, baseValue());
    QVERIFY(!cache.lookupExact(second).has_value());
}

void CompletionCacheTest::assembledPromptFingerprintDifferencesMiss()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    CompletionStrategy strategy;
    PromptContext ctx;
    ctx.language = QStringLiteral("C++");
    ctx.filePath = QStringLiteral("/repo/src/main.cpp");

    const CompletionCacheKey first = CompletionCache::makeKey(settings,
                                                              strategy,
                                                              ctx,
                                                              QStringLiteral("same-prefix"),
                                                              QStringLiteral("same-suffix"),
                                                              1,
                                                              QStringLiteral("context-a"),
                                                              QStringLiteral("shape"));
    const CompletionCacheKey second = CompletionCache::makeKey(settings,
                                                               strategy,
                                                               ctx,
                                                               QStringLiteral("same-prefix"),
                                                               QStringLiteral("same-suffix"),
                                                               1,
                                                               QStringLiteral("context-b"),
                                                               QStringLiteral("shape"));

    CompletionCache cache;
    cache.insert(first, baseValue());
    QVERIFY(!cache.lookupExact(second).has_value());
}

void CompletionCacheTest::requestedCandidateCountDifferencesMiss()
{
    CompletionSettings settings = CompletionSettings::defaults().validated();
    CompletionStrategy strategy;
    PromptContext ctx;
    ctx.language = QStringLiteral("C++");
    ctx.filePath = QStringLiteral("/repo/src/main.cpp");

    const CompletionCacheKey automatic = CompletionCache::makeKey(settings,
                                                                  strategy,
                                                                  ctx,
                                                                  QStringLiteral("same-prefix"),
                                                                  QStringLiteral("same-suffix"),
                                                                  1,
                                                                  QStringLiteral("context"),
                                                                  QStringLiteral("shape-n1"));
    const CompletionCacheKey manual = CompletionCache::makeKey(settings,
                                                               strategy,
                                                               ctx,
                                                               QStringLiteral("same-prefix"),
                                                               QStringLiteral("same-suffix"),
                                                               3,
                                                               QStringLiteral("context"),
                                                               QStringLiteral("shape-n3"));

    CompletionCache cache;
    cache.insert(automatic, baseValue());
    QVERIFY(!cache.lookupExact(manual).has_value());
}

void CompletionCacheTest::disabledCacheStoresNothing()
{
    CompletionCacheOptions options;
    options.enabled = false;
    CompletionCache cache(options);
    cache.insert(baseKey(), baseValue());

    QVERIFY(!cache.lookupExact(baseKey()).has_value());
    QCOMPARE(cache.size(), 0);
}

void CompletionCacheTest::clearRemovesEntries()
{
    CompletionCache cache;
    cache.insert(baseKey(), baseValue());
    QCOMPARE(cache.size(), 1);

    cache.clear();
    QCOMPARE(cache.size(), 0);
    QVERIFY(!cache.lookupExact(baseKey()).has_value());
}

void CompletionCacheTest::storesMultipleCandidates()
{
    CompletionCache cache;
    CompletionCacheValue value = baseValue(QStringLiteral("alpha"));
    value.candidates = {candidate(QStringLiteral("alpha")), candidate(QStringLiteral("beta"))};

    cache.insert(baseKey(), value);

    const auto hit = cache.lookupExact(baseKey());
    QVERIFY(hit.has_value());
    QCOMPARE(hit->candidates.size(), 2);
    QCOMPARE(hit->candidates.at(0).displayText, QStringLiteral("alpha"));
    QCOMPARE(hit->candidates.at(1).displayText, QStringLiteral("beta"));
    QCOMPARE(hit->candidates.constFirst().displayText, QStringLiteral("alpha"));
}

void CompletionCacheTest::maxStoredCandidatesIsRespected()
{
    CompletionCacheOptions options;
    options.maxStoredCandidates = 2;
    CompletionCache cache(options);

    CompletionCacheValue value = baseValue(QStringLiteral("alpha"));
    value.candidates = {candidate(QStringLiteral("alpha")), candidate(QStringLiteral("beta")), candidate(QStringLiteral("gamma"))};
    cache.insert(baseKey(), value);

    const auto hit = cache.lookupExact(baseKey());
    QVERIFY(hit.has_value());
    QCOMPARE(hit->candidates.size(), 2);
    QCOMPARE(hit->candidates.at(0).displayText, QStringLiteral("alpha"));
    QCOMPARE(hit->candidates.at(1).displayText, QStringLiteral("beta"));
}

void CompletionCacheTest::candidateDedupAppliesOnInsert()
{
    CompletionCache cache;
    CompletionCacheValue value = baseValue(QStringLiteral("alpha"));
    value.candidates = {candidate(QStringLiteral("alpha  \r\n beta")), candidate(QStringLiteral(" alpha\n beta  ")), candidate(QStringLiteral("gamma"))};
    cache.insert(baseKey(), value);

    const auto hit = cache.lookupExact(baseKey());
    QVERIFY(hit.has_value());
    QCOMPARE(hit->candidates.size(), 2);
    QCOMPARE(hit->candidates.at(0).displayText, QStringLiteral("alpha  \r\n beta"));
    QCOMPARE(hit->candidates.at(1).displayText, QStringLiteral("gamma"));
}

void CompletionCacheTest::typingAsSuggestedLookupWorksWithCandidateList()
{
    CompletionCache cache;
    CompletionCacheValue value = baseValue(QStringLiteral("ghostAlpha"));
    value.candidates = {candidate(QStringLiteral("ghostAlpha")), candidate(QStringLiteral("ghostBeta")), candidate(QStringLiteral("other"))};
    cache.insert(baseKey(), value);

    const auto hit = cache.lookupTypingAsSuggested(baseKey(), QStringLiteral("ghost"));
    QVERIFY(hit.has_value());
    QCOMPARE(hit->candidates.size(), 2);
    QCOMPARE(hit->candidates.at(0).displayText, QStringLiteral("Alpha"));
    QCOMPARE(hit->candidates.at(1).displayText, QStringLiteral("Beta"));
    QCOMPARE(hit->candidates.constFirst().displayText, QStringLiteral("Alpha"));
}

void CompletionCacheTest::typingAsSuggestedLookupReturnsRemainingCompletion()
{
    CompletionCache cache;
    const CompletionCacheKey key = baseKey();
    cache.insert(key, baseValue(QStringLiteral("ghostText")));

    const auto hit = cache.lookupTypingAsSuggested(key, QStringLiteral("ghost"));
    QVERIFY(hit.has_value());
    QCOMPARE(hit->candidates.size(), 1);
    QCOMPARE(hit->candidates.constFirst().rawCompletion, QStringLiteral("Text"));
    QCOMPARE(hit->candidates.constFirst().insertText, QStringLiteral("Text"));
    QCOMPARE(hit->candidates.constFirst().displayText, QStringLiteral("Text"));
    QCOMPARE(hit->hitCount, 1);
}

void CompletionCacheTest::typingAsSuggestedRejectsNonmatchingPrefix()
{
    CompletionCache cache;
    const CompletionCacheKey key = baseKey();
    cache.insert(key, baseValue(QStringLiteral("ghostText")));

    QVERIFY(!cache.lookupTypingAsSuggested(key, QStringLiteral("goat")).has_value());
}

QTEST_MAIN(CompletionCacheTest)

#include "CompletionCacheTest.moc"
