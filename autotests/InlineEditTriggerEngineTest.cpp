/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditTriggerEngineTest
*/

#include "inlineedit/InlineEditTriggerEngine.h"

#include "context/DiagnosticItem.h"
#include "context/RecentEdit.h"
#include "settings/CompletionSettings.h"

#include <QtTest>

using KateAiInlineCompletion::CompletionSettings;
using KateAiInlineCompletion::DiagnosticItem;
using KateAiInlineCompletion::InlineEditTriggerEngine;
using KateAiInlineCompletion::InlineEditTriggerKind;
using KateAiInlineCompletion::InlineEditTriggerRequest;
using KateAiInlineCompletion::RecentEdit;

namespace
{
CompletionSettings automaticSettings()
{
    CompletionSettings settings = CompletionSettings::defaults();
    settings.enableAutomaticInlineEdits = true;
    settings.enableDiagnosticsContext = true;
    settings.enableRecentEditsContext = true;
    settings.autoInlineEditDiagnostics = true;
    settings.autoInlineEditRecentEdits = true;
    settings.autoInlineEditDiagnosticLineDistance = 5;
    settings.autoInlineEditRecentEditWindowMs = 300000;
    return settings.validated();
}

InlineEditTriggerRequest baseRequest()
{
    InlineEditTriggerRequest request;
    request.filePath = QStringLiteral("/repo/src/foo.cpp");
    request.languageId = QStringLiteral("C++");
    request.cursor = KTextEditor::Cursor(9, 4);
    request.documentRevision = 7;
    return request;
}

DiagnosticItem diagnostic(DiagnosticItem::Severity severity, int line, QString message = QStringLiteral("expected ';'"))
{
    DiagnosticItem item;
    item.uri = QStringLiteral("/repo/src/foo.cpp");
    item.severity = severity;
    item.startLine = line;
    item.startColumn = 2;
    item.endLine = line;
    item.endColumn = 8;
    item.message = std::move(message);
    item.timestamp = QDateTime::currentDateTimeUtc();
    return item;
}

RecentEdit recentEdit(QString uri = QStringLiteral("/repo/src/foo_test.cpp"), QDateTime timestamp = QDateTime::currentDateTimeUtc())
{
    RecentEdit edit;
    edit.uri = std::move(uri);
    edit.timestamp = timestamp;
    edit.startLine = 3;
    edit.endLine = 3;
    edit.beforeText = QStringLiteral("oldName();");
    edit.afterText = QStringLiteral("newName();");
    edit.summary = QStringLiteral("Renamed oldName to newName");
    return edit;
}
} // namespace

class InlineEditTriggerEngineTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void choosesDiagnosticRepairForErrorOnCursorLine();
    void choosesDiagnosticRepairForErrorWithinConfiguredLineDistance();
    void skipsWarningWhenWarningsDisabled();
    void choosesWarningWhenWarningsEnabled();
    void skipsDiagnosticOutsideConfiguredDistance();
    void choosesRecentEditContinuationForSameLanguageRecentEdit();
    void skipsStaleRecentEditOutsideWindow();
    void choosesSelectionRepairForExplicitSelectedRangeWhenEnabled();
    void returnsNoTriggerWhenAutomaticInlineEditsDisabled();
    void priorityOrderChoosesDiagnosticOverRecentEdit();
};

void InlineEditTriggerEngineTest::choosesDiagnosticRepairForErrorOnCursorLine()
{
    InlineEditTriggerRequest request = baseRequest();
    request.diagnostics = {diagnostic(DiagnosticItem::Severity::Error, 9)};

    const auto trigger = InlineEditTriggerEngine::choose(request, automaticSettings());

    QVERIFY(trigger.has_value());
    QCOMPARE(trigger->kind, InlineEditTriggerKind::DiagnosticRepair);
    QCOMPARE(trigger->targetRange, KTextEditor::Range(9, 2, 9, 8));
    QVERIFY(trigger->reason.contains(QStringLiteral("Fix the diagnostic near the cursor")));
    QCOMPARE(trigger->diagnosticMessage, QStringLiteral("expected ';'"));
}

void InlineEditTriggerEngineTest::choosesDiagnosticRepairForErrorWithinConfiguredLineDistance()
{
    InlineEditTriggerRequest request = baseRequest();
    request.diagnostics = {diagnostic(DiagnosticItem::Severity::Error, 12)};

    const auto trigger = InlineEditTriggerEngine::choose(request, automaticSettings());

    QVERIFY(trigger.has_value());
    QCOMPARE(trigger->kind, InlineEditTriggerKind::DiagnosticRepair);
    QCOMPARE(trigger->targetRange.start().line(), 12);
}

void InlineEditTriggerEngineTest::skipsWarningWhenWarningsDisabled()
{
    InlineEditTriggerRequest request = baseRequest();
    request.diagnostics = {diagnostic(DiagnosticItem::Severity::Warning, 9)};

    const auto trigger = InlineEditTriggerEngine::choose(request, automaticSettings());

    QVERIFY(!trigger.has_value());
}

void InlineEditTriggerEngineTest::choosesWarningWhenWarningsEnabled()
{
    InlineEditTriggerRequest request = baseRequest();
    request.diagnostics = {diagnostic(DiagnosticItem::Severity::Warning, 9, QStringLiteral("unused variable"))};
    CompletionSettings settings = automaticSettings();
    settings.autoInlineEditWarnings = true;

    const auto trigger = InlineEditTriggerEngine::choose(request, settings.validated());

    QVERIFY(trigger.has_value());
    QCOMPARE(trigger->kind, InlineEditTriggerKind::DiagnosticRepair);
    QCOMPARE(trigger->diagnosticMessage, QStringLiteral("unused variable"));
}

void InlineEditTriggerEngineTest::skipsDiagnosticOutsideConfiguredDistance()
{
    InlineEditTriggerRequest request = baseRequest();
    request.diagnostics = {diagnostic(DiagnosticItem::Severity::Error, 30)};

    const auto trigger = InlineEditTriggerEngine::choose(request, automaticSettings());

    QVERIFY(!trigger.has_value());
}

void InlineEditTriggerEngineTest::choosesRecentEditContinuationForSameLanguageRecentEdit()
{
    InlineEditTriggerRequest request = baseRequest();
    request.recentEdits = {recentEdit()};

    const auto trigger = InlineEditTriggerEngine::choose(request, automaticSettings());

    QVERIFY(trigger.has_value());
    QCOMPARE(trigger->kind, InlineEditTriggerKind::RecentEditContinuation);
    QCOMPARE(trigger->targetRange, KTextEditor::Range(9, 0, 9, 0));
    QVERIFY(trigger->reason.contains(QStringLiteral("Continue the recent edit pattern")));
    QCOMPARE(trigger->recentEditSummary, QStringLiteral("Renamed oldName to newName"));
}

void InlineEditTriggerEngineTest::skipsStaleRecentEditOutsideWindow()
{
    InlineEditTriggerRequest request = baseRequest();
    request.recentEdits = {recentEdit(QStringLiteral("/repo/src/foo_test.cpp"), QDateTime::currentDateTimeUtc().addMSecs(-600000))};

    const auto trigger = InlineEditTriggerEngine::choose(request, automaticSettings());

    QVERIFY(!trigger.has_value());
}

void InlineEditTriggerEngineTest::choosesSelectionRepairForExplicitSelectedRangeWhenEnabled()
{
    InlineEditTriggerRequest request = baseRequest();
    request.hasSelection = true;
    request.selectionRange = KTextEditor::Range(4, 1, 6, 3);
    CompletionSettings settings = automaticSettings();
    settings.autoInlineEditSelections = true;

    const auto trigger = InlineEditTriggerEngine::choose(request, settings.validated());

    QVERIFY(trigger.has_value());
    QCOMPARE(trigger->kind, InlineEditTriggerKind::SelectionRepair);
    QCOMPARE(trigger->targetRange, KTextEditor::Range(4, 1, 6, 3));
}

void InlineEditTriggerEngineTest::returnsNoTriggerWhenAutomaticInlineEditsDisabled()
{
    InlineEditTriggerRequest request = baseRequest();
    request.diagnostics = {diagnostic(DiagnosticItem::Severity::Error, 9)};
    CompletionSettings settings = automaticSettings();
    settings.enableAutomaticInlineEdits = false;

    const auto trigger = InlineEditTriggerEngine::choose(request, settings.validated());

    QVERIFY(!trigger.has_value());
}

void InlineEditTriggerEngineTest::priorityOrderChoosesDiagnosticOverRecentEdit()
{
    InlineEditTriggerRequest request = baseRequest();
    request.diagnostics = {diagnostic(DiagnosticItem::Severity::Error, 9)};
    request.recentEdits = {recentEdit()};

    const auto trigger = InlineEditTriggerEngine::choose(request, automaticSettings());

    QVERIFY(trigger.has_value());
    QCOMPARE(trigger->kind, InlineEditTriggerKind::DiagnosticRepair);
}

QTEST_MAIN(InlineEditTriggerEngineTest)

#include "InlineEditTriggerEngineTest.moc"
