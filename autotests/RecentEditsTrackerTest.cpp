/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RecentEditsTrackerTest
*/

#include "context/RecentEditsTracker.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>

#include <QScopedPointer>
#include <QTest>

using KateAiInlineCompletion::RecentEdit;
using KateAiInlineCompletion::RecentEditsTracker;
using KateAiInlineCompletion::RecentEditsTrackerOptions;

namespace
{

KTextEditor::Document *createDocument(const QString &text)
{
    auto *editor = KTextEditor::Editor::instance();
    Q_ASSERT(editor);

    KTextEditor::Document *doc = editor->createDocument(nullptr);
    Q_ASSERT(doc);
    doc->setText(text);
    return doc;
}

RecentEdit syntheticEdit(const QString &uri, int line, const QString &summary)
{
    RecentEdit edit;
    edit.uri = uri;
    edit.timestamp = QDateTime::currentDateTimeUtc();
    edit.startLine = line;
    edit.endLine = line;
    edit.beforeText = QStringLiteral("before");
    edit.afterText = QStringLiteral("after");
    edit.summary = summary;
    return edit;
}

} // namespace

class RecentEditsTrackerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tracksLineBasedDocumentEdits();
    void retrackingDocumentKeepsPendingEditSnapshot();
    void lineInsertionSummaryOnlyShowsInsertedLine();
    void lineDeletionSummaryOnlyShowsDeletedLine();
    void untrackFlushesPendingEdit();
    void mergesNearbyEditsForSameFile();
    void boundsHistoryByMaxEditsAndFiles();
};

void RecentEditsTrackerTest::tracksLineBasedDocumentEdits()
{
    QScopedPointer<KTextEditor::Document> doc(createDocument(QStringLiteral("alpha\nbeta\ngamma\n")));

    RecentEditsTrackerOptions options;
    options.diffContextLines = 0;
    options.maxLinesPerEdit = 10;
    options.maxCharsPerEdit = 2000;

    RecentEditsTracker tracker;
    tracker.setOptions(options);
    tracker.trackDocument(doc.data(), QStringLiteral("/repo/src/foo.cpp"));

    QVERIFY(doc->replaceText(KTextEditor::Range(1, 0, 1, 4), QStringLiteral("BETA")));
    tracker.flushPendingEdits();

    const QVector<RecentEdit> edits = tracker.recentEdits();
    QCOMPARE(edits.size(), 1);
    QCOMPARE(edits.constFirst().uri, QStringLiteral("/repo/src/foo.cpp"));
    QCOMPARE(edits.constFirst().startLine, 1);
    QCOMPARE(edits.constFirst().endLine, 1);
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("@@ lines 2-2")));
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("- beta")));
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("+ BETA")));
}

void RecentEditsTrackerTest::retrackingDocumentKeepsPendingEditSnapshot()
{
    QScopedPointer<KTextEditor::Document> doc(createDocument(QStringLiteral("alpha\nbeta\n")));

    RecentEditsTrackerOptions options;
    options.diffContextLines = 0;

    RecentEditsTracker tracker;
    tracker.setOptions(options);
    tracker.trackDocument(doc.data(), QStringLiteral("/repo/src/foo.cpp"));

    QVERIFY(doc->replaceText(KTextEditor::Range(1, 0, 1, 4), QStringLiteral("BETA")));
    tracker.trackDocument(doc.data(), QStringLiteral("/repo/src/foo.cpp"));
    tracker.flushPendingEdits();

    const QVector<RecentEdit> edits = tracker.recentEdits();
    QCOMPARE(edits.size(), 1);
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("- beta")));
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("+ BETA")));
}

void RecentEditsTrackerTest::lineInsertionSummaryOnlyShowsInsertedLine()
{
    QScopedPointer<KTextEditor::Document> doc(createDocument(QStringLiteral("a\nc\n")));

    RecentEditsTrackerOptions options;
    options.diffContextLines = 0;

    RecentEditsTracker tracker;
    tracker.setOptions(options);
    tracker.trackDocument(doc.data(), QStringLiteral("/repo/src/foo.cpp"));

    QVERIFY(doc->insertLine(1, QStringLiteral("b")));
    tracker.flushPendingEdits();

    const QVector<RecentEdit> edits = tracker.recentEdits();
    QCOMPARE(edits.size(), 1);
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("+ b")));
    QVERIFY(!edits.constFirst().summary.contains(QStringLiteral("- c")));
}

void RecentEditsTrackerTest::lineDeletionSummaryOnlyShowsDeletedLine()
{
    QScopedPointer<KTextEditor::Document> doc(createDocument(QStringLiteral("a\nb\nc\n")));

    RecentEditsTrackerOptions options;
    options.diffContextLines = 0;

    RecentEditsTracker tracker;
    tracker.setOptions(options);
    tracker.trackDocument(doc.data(), QStringLiteral("/repo/src/foo.cpp"));

    QVERIFY(doc->removeLine(1));
    tracker.flushPendingEdits();

    const QVector<RecentEdit> edits = tracker.recentEdits();
    QCOMPARE(edits.size(), 1);
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("- b")));
    QVERIFY(!edits.constFirst().summary.contains(QStringLiteral("+ c")));
}

void RecentEditsTrackerTest::untrackFlushesPendingEdit()
{
    QScopedPointer<KTextEditor::Document> doc(createDocument(QStringLiteral("alpha\nbeta\n")));

    RecentEditsTrackerOptions options;
    options.diffContextLines = 0;
    options.debounceMs = 500;

    RecentEditsTracker tracker;
    tracker.setOptions(options);
    tracker.trackDocument(doc.data(), QStringLiteral("/repo/src/foo.cpp"));

    QVERIFY(doc->replaceText(KTextEditor::Range(1, 0, 1, 4), QStringLiteral("BETA")));
    tracker.untrackDocument(doc.data());

    const QVector<RecentEdit> edits = tracker.recentEdits();
    QCOMPARE(edits.size(), 1);
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("+ BETA")));
}

void RecentEditsTrackerTest::mergesNearbyEditsForSameFile()
{
    QScopedPointer<KTextEditor::Document> doc(createDocument(QStringLiteral("a\nb\nc\nd\n")));

    RecentEditsTrackerOptions options;
    options.diffContextLines = 1;
    options.maxLinesPerEdit = 10;

    RecentEditsTracker tracker;
    tracker.setOptions(options);
    tracker.trackDocument(doc.data(), QStringLiteral("/repo/src/foo.cpp"));

    QVERIFY(doc->replaceText(KTextEditor::Range(1, 0, 1, 1), QStringLiteral("B")));
    tracker.flushPendingEdits();
    QVERIFY(doc->replaceText(KTextEditor::Range(2, 0, 2, 1), QStringLiteral("C")));
    tracker.flushPendingEdits();

    const QVector<RecentEdit> edits = tracker.recentEdits();
    QCOMPARE(edits.size(), 1);
    QCOMPARE(edits.constFirst().startLine, 1);
    QCOMPARE(edits.constFirst().endLine, 2);
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("+ B")));
    QVERIFY(edits.constFirst().summary.contains(QStringLiteral("+ C")));
}

void RecentEditsTrackerTest::boundsHistoryByMaxEditsAndFiles()
{
    RecentEditsTrackerOptions options;
    options.maxEdits = 2;
    options.maxFiles = 1;

    RecentEditsTracker tracker;
    tracker.setOptions(options);
    tracker.addRecentEdit(syntheticEdit(QStringLiteral("/repo/src/one.cpp"), 1, QStringLiteral("one")));
    tracker.addRecentEdit(syntheticEdit(QStringLiteral("/repo/src/two.cpp"), 20, QStringLiteral("two-a")));
    tracker.addRecentEdit(syntheticEdit(QStringLiteral("/repo/src/two.cpp"), 100, QStringLiteral("two-b")));

    const QVector<RecentEdit> edits = tracker.recentEdits();
    QCOMPARE(edits.size(), 2);
    QCOMPARE(edits.at(0).uri, QStringLiteral("/repo/src/two.cpp"));
    QCOMPARE(edits.at(1).uri, QStringLiteral("/repo/src/two.cpp"));
    QVERIFY(edits.at(0).summary.contains(QStringLiteral("two-b")));
}

QTEST_MAIN(RecentEditsTrackerTest)

#include "RecentEditsTrackerTest.moc"
