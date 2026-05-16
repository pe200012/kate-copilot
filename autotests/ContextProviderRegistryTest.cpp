/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ContextProviderRegistryTest
*/

#include "context/ContextProviderRegistry.h"

#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include <memory>

using KateAiInlineCompletion::ContextItem;
using KateAiInlineCompletion::ContextProvider;
using KateAiInlineCompletion::ContextProviderRegistry;
using KateAiInlineCompletion::ContextResolveRequest;

namespace
{

class FakeProvider final : public ContextProvider
{
public:
    FakeProvider(QString providerId, int score, QVector<ContextItem> items)
        : m_id(std::move(providerId))
        , m_score(score)
        , m_items(std::move(items))
    {
    }

    QString id() const override
    {
        return m_id;
    }

    int matchScore(const ContextResolveRequest &request) const override
    {
        Q_UNUSED(request);
        return m_score;
    }

    QVector<ContextItem> resolve(const ContextResolveRequest &request) override
    {
        Q_UNUSED(request);
        return m_items;
    }

private:
    QString m_id;
    int m_score = 0;
    QVector<ContextItem> m_items;
};

ContextItem trait(QString providerId, QString id, int importance)
{
    ContextItem item;
    item.kind = ContextItem::Kind::Trait;
    item.providerId = std::move(providerId);
    item.id = std::move(id);
    item.importance = importance;
    item.name = item.id;
    item.value = QStringLiteral("value-%1").arg(item.id);
    return item;
}

ContextItem snippet(QString providerId, QString uri, QString value, int importance)
{
    ContextItem item;
    item.kind = ContextItem::Kind::CodeSnippet;
    item.providerId = std::move(providerId);
    item.id = uri;
    item.uri = std::move(uri);
    item.name = item.uri;
    item.value = std::move(value);
    item.importance = importance;
    return item;
}

void writeText(const QString &path, const QString &text)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(text.toUtf8());
}

} // namespace

class ContextProviderRegistryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resolvesItemsByProviderScoreAndImportance();
    void ignoresNonMatchingProviders();
    void fillsMissingProviderId();
    void deduplicatesSnippetItemsByCanonicalPathKeepingRelatedFileSemantics();
};

void ContextProviderRegistryTest::resolvesItemsByProviderScoreAndImportance()
{
    ContextProviderRegistry registry;
    registry.addProvider(std::make_unique<FakeProvider>(QStringLiteral("low-score"),
                                                        10,
                                                        QVector<ContextItem>{trait(QStringLiteral("low-score"), QStringLiteral("low"), 100)}));
    registry.addProvider(std::make_unique<FakeProvider>(QStringLiteral("high-score"),
                                                        90,
                                                        QVector<ContextItem>{trait(QStringLiteral("high-score"), QStringLiteral("middle"), 50),
                                                                             trait(QStringLiteral("high-score"), QStringLiteral("top"), 90)}));

    const QVector<ContextItem> items = registry.resolve(ContextResolveRequest{}, 3);

    QCOMPARE(items.size(), 3);
    QCOMPARE(items.at(0).id, QStringLiteral("top"));
    QCOMPARE(items.at(1).id, QStringLiteral("middle"));
    QCOMPARE(items.at(2).id, QStringLiteral("low"));
}

void ContextProviderRegistryTest::ignoresNonMatchingProviders()
{
    ContextProviderRegistry registry;
    registry.addProvider(std::make_unique<FakeProvider>(QStringLiteral("disabled"),
                                                        0,
                                                        QVector<ContextItem>{trait(QStringLiteral("disabled"), QStringLiteral("hidden"), 100)}));
    registry.addProvider(std::make_unique<FakeProvider>(QStringLiteral("enabled"),
                                                        1,
                                                        QVector<ContextItem>{trait(QStringLiteral("enabled"), QStringLiteral("visible"), 1)}));

    const QVector<ContextItem> items = registry.resolve(ContextResolveRequest{}, 6);

    QCOMPARE(items.size(), 1);
    QCOMPARE(items.constFirst().id, QStringLiteral("visible"));
}

void ContextProviderRegistryTest::fillsMissingProviderId()
{
    ContextItem item = trait(QString(), QStringLiteral("path"), 20);
    item.providerId.clear();

    ContextProviderRegistry registry;
    registry.addProvider(std::make_unique<FakeProvider>(QStringLiteral("current-file"), 50, QVector<ContextItem>{item}));

    const QVector<ContextItem> items = registry.resolve(ContextResolveRequest{}, 6);

    QCOMPARE(items.size(), 1);
    QCOMPARE(items.constFirst().providerId, QStringLiteral("current-file"));
}

void ContextProviderRegistryTest::deduplicatesSnippetItemsByCanonicalPathKeepingRelatedFileSemantics()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString header = dir.filePath(QStringLiteral("Foo.h"));
    writeText(header, QStringLiteral("class Foo {};\n"));

    ContextProviderRegistry registry;
    registry.addProvider(std::make_unique<FakeProvider>(QStringLiteral("open-tabs"),
                                                        95,
                                                        QVector<ContextItem>{snippet(QStringLiteral("open-tabs"),
                                                                                     QUrl::fromLocalFile(header).toString(),
                                                                                     QStringLiteral("class FooFromOpenTab {};\n"),
                                                                                     95)}));
    registry.addProvider(std::make_unique<FakeProvider>(QStringLiteral("related-files"),
                                                        80,
                                                        QVector<ContextItem>{snippet(QStringLiteral("related-files"),
                                                                                     header,
                                                                                     QStringLiteral("class FooFromRelatedFile {};\n"),
                                                                                     80)}));

    const QVector<ContextItem> items = registry.resolve(ContextResolveRequest{}, 6);

    QCOMPARE(items.size(), 1);
    QCOMPARE(items.constFirst().providerId, QStringLiteral("related-files"));
    QVERIFY(items.constFirst().value.contains(QStringLiteral("FooFromRelatedFile")));
}

QTEST_MAIN(ContextProviderRegistryTest)

#include "ContextProviderRegistryTest.moc"
