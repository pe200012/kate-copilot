/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextInlineNoteProviderTest
*/

#include "render/GhostTextInlineNoteProvider.h"

#include <QSignalSpy>
#include <QtTest>

using KateAiInlineCompletion::GhostTextInlineNoteProvider;
using KateAiInlineCompletion::GhostTextState;

namespace
{

GhostTextState makeState(int line, int column, const QString &visibleText)
{
    GhostTextState state;
    state.anchor.line = line;
    state.anchor.column = column;
    state.anchor.generation = 1;
    state.visibleText = visibleText;
    return state;
}

} // namespace

class GhostTextInlineNoteProviderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void multilineSuggestionUsesSingleAnchorNote();
    void stateChangeRefreshesAnchorLine();
};

void GhostTextInlineNoteProviderTest::multilineSuggestionUsesSingleAnchorNote()
{
    GhostTextInlineNoteProvider provider;
    provider.setState(makeState(7, 5, QStringLiteral("first\n    second\nthird")));

    QCOMPARE(provider.inlineNotes(6), QList<int>{});
    QCOMPARE(provider.inlineNotes(7), QList<int>{5});
    QCOMPARE(provider.inlineNotes(8), QList<int>{});
    QCOMPARE(provider.inlineNotes(9), QList<int>{});
    QCOMPARE(provider.inlineNotes(10), QList<int>{});
}

void GhostTextInlineNoteProviderTest::stateChangeRefreshesAnchorLine()
{
    GhostTextInlineNoteProvider provider;
    provider.setState(makeState(4, 2, QStringLiteral("first")));

    QSignalSpy lineSpy(&provider, SIGNAL(inlineNotesChanged(int)));
    QSignalSpy resetSpy(&provider, SIGNAL(inlineNotesReset()));

    provider.setState(makeState(4, 2, QStringLiteral("first\nsecond\nthird")));

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(lineSpy.count(), 1);
    QCOMPARE(lineSpy.at(0).at(0).toInt(), 4);
}

QTEST_MAIN(GhostTextInlineNoteProviderTest)

#include "GhostTextInlineNoteProviderTest.moc"
