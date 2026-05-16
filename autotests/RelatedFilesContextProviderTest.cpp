/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RelatedFilesContextProviderTest
*/

#include "context/ContextProviderRegistry.h"
#include "context/RelatedFilesContextProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using KateAiInlineCompletion::ContextItem;
using KateAiInlineCompletion::ContextProviderRegistry;
using KateAiInlineCompletion::ContextResolveRequest;
using KateAiInlineCompletion::RelatedFilesContextOptions;
using KateAiInlineCompletion::RelatedFilesContextProvider;

namespace
{
void writeText(const QString &path, const QString &text)
{
    QFileInfo info(path);
    QVERIFY(QDir().mkpath(info.absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(text.toUtf8());
}
}

class RelatedFilesContextProviderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emitsBoundedRelatedFileSnippets();
    void registryKeepsResolverRelevanceOrderUnderTightBudget();
    void usesOpenDocumentTextBeforeFilesystemText();
    void appliesContextFileFilter();
};

void RelatedFilesContextProviderTest::emitsBoundedRelatedFileSnippets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral(".git")));

    const QString source = dir.filePath(QStringLiteral("src/Foo.cpp"));
    const QString header = dir.filePath(QStringLiteral("src/Foo.h"));
    const QString cmake = dir.filePath(QStringLiteral("src/CMakeLists.txt"));
    writeText(source, QStringLiteral("#include \"Foo.h\"\n"));
    writeText(header, QStringLiteral("class Foo {\npublic:\n    void run();\n};\n"));
    writeText(cmake, QStringLiteral("add_library(foo Foo.cpp)\n"));

    RelatedFilesContextOptions options;
    options.maxFiles = 2;
    options.maxChars = 2000;
    options.maxCharsPerFile = 4000;

    RelatedFilesContextProvider provider(options);

    ContextResolveRequest request;
    request.uri = source;
    request.languageId = QStringLiteral("C++");
    request.position = KTextEditor::Cursor(0, 1);

    const QVector<ContextItem> items = provider.resolve(request);

    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).providerId, QStringLiteral("related-files"));
    QCOMPARE(items.at(0).kind, ContextItem::Kind::CodeSnippet);
    QCOMPARE(items.at(0).name, QStringLiteral("src/Foo.h"));
    QVERIFY(items.at(0).value.contains(QStringLiteral("class Foo")));
    QCOMPARE(items.at(1).name, QStringLiteral("src/CMakeLists.txt"));
}

void RelatedFilesContextProviderTest::registryKeepsResolverRelevanceOrderUnderTightBudget()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral(".git")));

    const QString source = dir.filePath(QStringLiteral("src/Foo.cpp"));
    const QString header = dir.filePath(QStringLiteral("src/Foo.h"));
    const QString cmake = dir.filePath(QStringLiteral("src/CMakeLists.txt"));
    writeText(source, QStringLiteral("#include \"Foo.h\"\n"));
    writeText(header, QStringLiteral("class Foo {};\n"));
    writeText(cmake, QStringLiteral("add_library(foo Foo.cpp)\n"));

    ContextProviderRegistry registry;
    registry.addProvider(std::make_unique<RelatedFilesContextProvider>(RelatedFilesContextOptions{}));

    ContextResolveRequest request;
    request.uri = source;
    request.languageId = QStringLiteral("C++");

    const QVector<ContextItem> items = registry.resolve(request, 1);

    QCOMPARE(items.size(), 1);
    QCOMPARE(items.constFirst().name, QStringLiteral("src/Foo.h"));
}

void RelatedFilesContextProviderTest::usesOpenDocumentTextBeforeFilesystemText()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString source = dir.filePath(QStringLiteral("Foo.cpp"));
    const QString header = QFileInfo(dir.filePath(QStringLiteral("Foo.h"))).absoluteFilePath();
    writeText(source, QStringLiteral("#include \"Foo.h\"\n"));
    writeText(header, QStringLiteral("class FilesystemFoo {};\n"));

    RelatedFilesContextOptions options;
    options.preferOpenTabs = true;
    options.openDocuments.insert(header, QStringLiteral("class OpenDocumentFoo {};\n"));

    RelatedFilesContextProvider provider(options);

    ContextResolveRequest request;
    request.uri = source;
    request.languageId = QStringLiteral("C++");

    const QVector<ContextItem> items = provider.resolve(request);

    QCOMPARE(items.size(), 1);
    QVERIFY(items.constFirst().value.contains(QStringLiteral("OpenDocumentFoo")));
    QVERIFY(!items.constFirst().value.contains(QStringLiteral("FilesystemFoo")));
}

void RelatedFilesContextProviderTest::appliesContextFileFilter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString source = dir.filePath(QStringLiteral("Foo.cpp"));
    const QString header = dir.filePath(QStringLiteral("Foo.h"));
    writeText(source, QStringLiteral("#include \"Foo.h\"\n"));
    writeText(header, QStringLiteral("class Foo {};\n"));

    RelatedFilesContextOptions options;
    options.excludePatterns = {QStringLiteral("*.h")};

    RelatedFilesContextProvider provider(options);

    ContextResolveRequest request;
    request.uri = source;
    request.languageId = QStringLiteral("C++");

    const QVector<ContextItem> items = provider.resolve(request);

    QVERIFY(items.isEmpty());
}

QTEST_MAIN(RelatedFilesContextProviderTest)

#include "RelatedFilesContextProviderTest.moc"
