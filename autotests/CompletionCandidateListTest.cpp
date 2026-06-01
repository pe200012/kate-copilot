/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionCandidateListTest
*/

#include "session/CompletionCandidateList.h"

#include <QtTest>

using KateAiInlineCompletion::CompletionCandidate;
using KateAiInlineCompletion::CompletionCandidateList;

namespace
{
CompletionCandidate candidate(QString text, QString id = {})
{
    CompletionCandidate c;
    c.rawCompletion = text;
    c.insertText = text;
    c.displayText = text;
    c.source = QStringLiteral("test");
    c.id = id.isEmpty() ? text : id;
    return c;
}
}

class CompletionCandidateListTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyListBehavior();
    void setCandidatesResetsCurrentIndex();
    void nextWraps();
    void previousWraps();
    void deduplicatedRemovesNormalizedDuplicates();
    void deduplicatedPreservesFirstOccurrence();
    void deduplicatedDropsEmptyCandidates();
};

void CompletionCandidateListTest::emptyListBehavior()
{
    CompletionCandidateList list;

    QVERIFY(list.isEmpty());
    QCOMPARE(list.size(), 0);
    QCOMPARE(list.currentIndex(), -1);
    QVERIFY(!list.next());
    QVERIFY(!list.previous());
    QVERIFY(list.current().displayText.isEmpty());
    QVERIFY(list.candidates().isEmpty());
}

void CompletionCandidateListTest::setCandidatesResetsCurrentIndex()
{
    CompletionCandidateList list;
    list.setCandidates({candidate(QStringLiteral("alpha")), candidate(QStringLiteral("beta"))});
    QVERIFY(list.next());
    QCOMPARE(list.currentIndex(), 1);

    list.setCandidates({candidate(QStringLiteral("gamma"))});

    QCOMPARE(list.size(), 1);
    QCOMPARE(list.currentIndex(), 0);
    QCOMPARE(list.current().displayText, QStringLiteral("gamma"));
}

void CompletionCandidateListTest::nextWraps()
{
    CompletionCandidateList list;
    list.setCandidates({candidate(QStringLiteral("alpha")), candidate(QStringLiteral("beta"))});

    QCOMPARE(list.current().displayText, QStringLiteral("alpha"));
    QVERIFY(list.next());
    QCOMPARE(list.current().displayText, QStringLiteral("beta"));
    QVERIFY(list.next());
    QCOMPARE(list.current().displayText, QStringLiteral("alpha"));
}

void CompletionCandidateListTest::previousWraps()
{
    CompletionCandidateList list;
    list.setCandidates({candidate(QStringLiteral("alpha")), candidate(QStringLiteral("beta"))});

    QCOMPARE(list.current().displayText, QStringLiteral("alpha"));
    QVERIFY(list.previous());
    QCOMPARE(list.current().displayText, QStringLiteral("beta"));
    QVERIFY(list.previous());
    QCOMPARE(list.current().displayText, QStringLiteral("alpha"));
}

void CompletionCandidateListTest::deduplicatedRemovesNormalizedDuplicates()
{
    QVector<CompletionCandidate> out = CompletionCandidateList::deduplicated({
        candidate(QStringLiteral("alpha  \r\n beta   "), QStringLiteral("first")),
        candidate(QStringLiteral(" alpha\n beta"), QStringLiteral("second")),
        candidate(QStringLiteral("gamma"), QStringLiteral("third")),
    });

    QCOMPARE(out.size(), 2);
    QCOMPARE(out.at(0).id, QStringLiteral("first"));
    QCOMPARE(out.at(1).id, QStringLiteral("third"));
}

void CompletionCandidateListTest::deduplicatedPreservesFirstOccurrence()
{
    QVector<CompletionCandidate> out = CompletionCandidateList::deduplicated({
        candidate(QStringLiteral("same"), QStringLiteral("first")),
        candidate(QStringLiteral("same  "), QStringLiteral("second")),
        candidate(QStringLiteral("other"), QStringLiteral("third")),
    });

    QCOMPARE(out.size(), 2);
    QCOMPARE(out.at(0).id, QStringLiteral("first"));
    QCOMPARE(out.at(1).id, QStringLiteral("third"));
}

void CompletionCandidateListTest::deduplicatedDropsEmptyCandidates()
{
    CompletionCandidate emptyDisplay = candidate(QStringLiteral("alpha"));
    emptyDisplay.displayText.clear();

    CompletionCandidate emptyInsert = candidate(QStringLiteral("beta"));
    emptyInsert.insertText = QStringLiteral("   ");

    QVector<CompletionCandidate> out = CompletionCandidateList::deduplicated({
        emptyDisplay,
        emptyInsert,
        candidate(QStringLiteral("gamma")),
    });

    QCOMPARE(out.size(), 1);
    QCOMPARE(out.constFirst().displayText, QStringLiteral("gamma"));
}

QTEST_MAIN(CompletionCandidateListTest)

#include "CompletionCandidateListTest.moc"
