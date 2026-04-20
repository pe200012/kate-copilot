/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SuggestionAnchorTrackerTest
*/

#include "session/SuggestionAnchorTracker.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>

#include <QScopedPointer>
#include <QTest>

using KateAiInlineCompletion::SuggestionAnchorTracker;

class SuggestionAnchorTrackerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void movesOnInsertBeforeAndAtAnchor();
};

void SuggestionAnchorTrackerTest::movesOnInsertBeforeAndAtAnchor()
{
    auto *editor = KTextEditor::Editor::instance();
    QVERIFY(editor);

    QScopedPointer<KTextEditor::Document> doc(editor->createDocument(nullptr));
    QVERIFY(doc);
    doc->setText(QStringLiteral("abc\ndef\n"));

    SuggestionAnchorTracker tracker;
    tracker.attach(doc.data(), KTextEditor::Cursor(1, 1));

    QVERIFY(tracker.isValid());
    QCOMPARE(tracker.position(), KTextEditor::Cursor(1, 1));

    QVERIFY(doc->insertText(KTextEditor::Cursor(1, 0), QStringLiteral("XX")));
    QCOMPARE(tracker.position(), KTextEditor::Cursor(1, 3));

    QVERIFY(doc->insertText(KTextEditor::Cursor(1, 3), QStringLiteral("YY")));
    QCOMPARE(tracker.position(), KTextEditor::Cursor(1, 5));

    tracker.clear();
    QVERIFY(!tracker.isValid());
}

QTEST_MAIN(SuggestionAnchorTrackerTest)

#include "SuggestionAnchorTrackerTest.moc"
