/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RecentEditsContextProviderTest
*/

#include "context/RecentEditsContextProvider.h"
#include "context/RecentEditsTracker.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using KateAiInlineCompletion::ContextItem;
using KateAiInlineCompletion::ContextResolveRequest;
using KateAiInlineCompletion::RecentEdit;
using KateAiInlineCompletion::RecentEditsContextOptions;
using KateAiInlineCompletion::RecentEditsContextProvider;
using KateAiInlineCompletion::RecentEditsTracker;

namespace
{

RecentEdit editForPath(const QString &uri, int startLine, int endLine, const QString &summary)
{
    RecentEdit edit;
    edit.uri = uri;
    edit.timestamp = QDateTime::currentDateTimeUtc();
    edit.startLine = startLine;
    edit.endLine = endLine;
    edit.beforeText = QStringLiteral("before");
    edit.afterText = QStringLiteral("after");
    edit.summary = summary;
    return edit;
}

} // namespace

class RecentEditsContextProviderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emitsRecentEditsAsContextItems();
    void filtersActiveFileEditsNearCursor();
};

void RecentEditsContextProviderTest::emitsRecentEditsAsContextItems()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral(".git")));
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("src")));

    const QString editPath = dir.filePath(QStringLiteral("src/foo.cpp"));
    const QString requestPath = dir.filePath(QStringLiteral("src/main.cpp"));

    RecentEditsTracker tracker;
    tracker.addRecentEdit(editForPath(editPath, 39, 46, QStringLiteral("@@ lines 40-47\n- oldName();\n+ newName();")));

    RecentEditsContextOptions options;
    options.maxEdits = 8;
    options.maxCharsPerEdit = 2000;

    RecentEditsContextProvider provider(&tracker, options);

    ContextResolveRequest request;
    request.uri = requestPath;
    request.languageId = QStringLiteral("C++");
    request.position = KTextEditor::Cursor(3, 4);

    const QVector<ContextItem> items = provider.resolve(request);

    QCOMPARE(items.size(), 1);
    QCOMPARE(items.constFirst().providerId, QStringLiteral("recent-edits"));
    QCOMPARE(items.constFirst().kind, ContextItem::Kind::CodeSnippet);
    QVERIFY(items.constFirst().value.contains(QStringLiteral("File: src/foo.cpp")));
    QVERIFY(items.constFirst().value.contains(QStringLiteral("@@ lines 40-47")));
    QVERIFY(items.constFirst().value.contains(QStringLiteral("+ newName();")));
}

void RecentEditsContextProviderTest::filtersActiveFileEditsNearCursor()
{
    const QString path = QStringLiteral("/repo/src/main.cpp");

    RecentEditsTracker tracker;
    tracker.addRecentEdit(editForPath(path, 48, 52, QStringLiteral("@@ lines 49-53\n- near\n+ nearChanged")));
    tracker.addRecentEdit(editForPath(path, 220, 222, QStringLiteral("@@ lines 221-223\n- far\n+ farChanged")));

    RecentEditsContextOptions options;
    options.activeDocDistanceLimitFromCursor = 100;
    options.maxEdits = 8;

    RecentEditsContextProvider provider(&tracker, options);

    ContextResolveRequest request;
    request.uri = path;
    request.languageId = QStringLiteral("C++");
    request.position = KTextEditor::Cursor(50, 1);

    const QVector<ContextItem> items = provider.resolve(request);

    QCOMPARE(items.size(), 1);
    QVERIFY(items.constFirst().value.contains(QStringLiteral("farChanged")));
    QVERIFY(!items.constFirst().value.contains(QStringLiteral("nearChanged")));
}

QTEST_MAIN(RecentEditsContextProviderTest)

#include "RecentEditsContextProviderTest.moc"
