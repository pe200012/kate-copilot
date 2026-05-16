/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticsContextProviderTest
*/

#include "context/DiagnosticsContextProvider.h"
#include "context/DiagnosticStore.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

using KateAiInlineCompletion::ContextItem;
using KateAiInlineCompletion::ContextResolveRequest;
using KateAiInlineCompletion::DiagnosticItem;
using KateAiInlineCompletion::DiagnosticsContextOptions;
using KateAiInlineCompletion::DiagnosticsContextProvider;
using KateAiInlineCompletion::DiagnosticStore;

namespace
{

DiagnosticItem diagnostic(QString uri, DiagnosticItem::Severity severity, int line, QString message, QString code = {})
{
    DiagnosticItem item;
    item.uri = std::move(uri);
    item.severity = severity;
    item.startLine = line;
    item.startColumn = 12;
    item.endLine = line;
    item.endColumn = 16;
    item.source = QStringLiteral("CLANG");
    item.code = std::move(code);
    item.message = std::move(message);
    item.timestamp = QDateTime::currentDateTimeUtc();
    return item;
}

} // namespace

class DiagnosticsContextProviderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void matchScorePrioritizesDiagnosticsAboveOtherSecondaryContext();
    void emitsActiveFileDiagnosticsFirstAndSortsByDistanceAndSeverity();
    void filtersSeverityByOptions();
    void respectsItemAndCharacterBudgets();
    void rendersRelativeFileNames();
};

void DiagnosticsContextProviderTest::matchScorePrioritizesDiagnosticsAboveOtherSecondaryContext()
{
    DiagnosticStore store;
    store.setDiagnostics(QStringLiteral("/repo/src/main.cpp"),
                         QVector<DiagnosticItem>{diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Error, 4, QStringLiteral("error"))});

    DiagnosticsContextProvider provider(&store, DiagnosticsContextOptions{});

    ContextResolveRequest request;
    request.uri = QStringLiteral("/repo/src/main.cpp");
    request.position = KTextEditor::Cursor(4, 1);

    QVERIFY(provider.matchScore(request) > 85);
}

void DiagnosticsContextProviderTest::emitsActiveFileDiagnosticsFirstAndSortsByDistanceAndSeverity()
{
    DiagnosticStore store;
    store.setDiagnostics(QStringLiteral("/repo/src/main.cpp"),
                         QVector<DiagnosticItem>{diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Warning, 70, QStringLiteral("far warning")),
                                                 diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Error, 43, QStringLiteral("near error")),
                                                 diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Warning, 42, QStringLiteral("near warning"))});
    store.setDiagnostics(QStringLiteral("/repo/src/other.cpp"),
                         QVector<DiagnosticItem>{diagnostic(QStringLiteral("/repo/src/other.cpp"), DiagnosticItem::Severity::Error, 1, QStringLiteral("other error"))});

    DiagnosticsContextOptions options;
    options.maxItems = 8;
    options.maxLineDistance = 120;
    DiagnosticsContextProvider provider(&store, options);

    ContextResolveRequest request;
    request.uri = QStringLiteral("/repo/src/main.cpp");
    request.position = KTextEditor::Cursor(41, 4);

    const QVector<ContextItem> items = provider.resolve(request);

    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).providerId, QStringLiteral("diagnostics"));
    QCOMPARE(items.at(0).kind, ContextItem::Kind::DiagnosticBag);
    QVERIFY(items.at(0).value.indexOf(QStringLiteral("near warning")) < items.at(0).value.indexOf(QStringLiteral("near error")));
    QVERIFY(items.at(0).value.indexOf(QStringLiteral("near error")) < items.at(0).value.indexOf(QStringLiteral("far warning")));
    QVERIFY(items.at(1).value.contains(QStringLiteral("other error")));
}

void DiagnosticsContextProviderTest::filtersSeverityByOptions()
{
    DiagnosticStore store;
    store.setDiagnostics(QStringLiteral("/repo/src/main.cpp"),
                         QVector<DiagnosticItem>{diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Error, 5, QStringLiteral("error")),
                                                 diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Warning, 6, QStringLiteral("warning")),
                                                 diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Information, 7, QStringLiteral("info")),
                                                 diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Hint, 8, QStringLiteral("hint"))});

    DiagnosticsContextOptions options;
    options.includeWarnings = false;
    options.includeInformation = true;
    options.includeHints = false;
    DiagnosticsContextProvider provider(&store, options);

    ContextResolveRequest request;
    request.uri = QStringLiteral("/repo/src/main.cpp");
    request.position = KTextEditor::Cursor(5, 1);

    const QVector<ContextItem> items = provider.resolve(request);

    QCOMPARE(items.size(), 1);
    QVERIFY(items.constFirst().value.contains(QStringLiteral("error")));
    QVERIFY(items.constFirst().value.contains(QStringLiteral("info")));
    QVERIFY(!items.constFirst().value.contains(QStringLiteral("warning")));
    QVERIFY(!items.constFirst().value.contains(QStringLiteral("hint")));
}

void DiagnosticsContextProviderTest::respectsItemAndCharacterBudgets()
{
    DiagnosticStore store;
    store.setDiagnostics(QStringLiteral("/repo/src/main.cpp"),
                         QVector<DiagnosticItem>{diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Error, 1, QStringLiteral("first diagnostic message")),
                                                 diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Error, 2, QStringLiteral("second diagnostic message")),
                                                 diagnostic(QStringLiteral("/repo/src/main.cpp"), DiagnosticItem::Severity::Error, 3, QStringLiteral("third diagnostic message"))});

    DiagnosticsContextOptions options;
    options.maxItems = 2;
    options.maxChars = 140;
    DiagnosticsContextProvider provider(&store, options);

    ContextResolveRequest request;
    request.uri = QStringLiteral("/repo/src/main.cpp");
    request.position = KTextEditor::Cursor(1, 1);

    const QVector<ContextItem> items = provider.resolve(request);

    QCOMPARE(items.size(), 1);
    QVERIFY(items.constFirst().value.contains(QStringLiteral("first diagnostic message")));
    QVERIFY(items.constFirst().value.contains(QStringLiteral("second diagnostic message")));
    QVERIFY(!items.constFirst().value.contains(QStringLiteral("third diagnostic message")));
    QVERIFY(items.constFirst().value.size() <= options.maxChars);
}

void DiagnosticsContextProviderTest::rendersRelativeFileNames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral(".git")));
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("src")));

    const QString requestPath = dir.filePath(QStringLiteral("src/main.cpp"));
    const QString diagnosticPath = dir.filePath(QStringLiteral("src/foo.cpp"));

    DiagnosticStore store;
    store.setDiagnostics(diagnosticPath,
                         QVector<DiagnosticItem>{diagnostic(diagnosticPath,
                                                             DiagnosticItem::Severity::Warning,
                                                             56,
                                                             QStringLiteral("unused variable 'x'"),
                                                             QStringLiteral("Wunused-variable"))});

    DiagnosticsContextProvider provider(&store, DiagnosticsContextOptions{});

    ContextResolveRequest request;
    request.uri = requestPath;
    request.position = KTextEditor::Cursor(1, 1);

    const QVector<ContextItem> items = provider.resolve(request);

    QCOMPARE(items.size(), 1);
    QCOMPARE(items.constFirst().name, QStringLiteral("src/foo.cpp"));
    QVERIFY(items.constFirst().value.contains(QStringLiteral("57:13 - warning CLANG-Wunused-variable: unused variable 'x'")));
}

QTEST_MAIN(DiagnosticsContextProviderTest)

#include "DiagnosticsContextProviderTest.moc"
