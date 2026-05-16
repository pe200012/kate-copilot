/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticStoreTest
*/

#include "context/DiagnosticStore.h"

#include <QtTest>

using KateAiInlineCompletion::DiagnosticItem;
using KateAiInlineCompletion::DiagnosticStore;

namespace
{

DiagnosticItem diagnostic(QString uri, DiagnosticItem::Severity severity, int line, QString message)
{
    DiagnosticItem item;
    item.uri = std::move(uri);
    item.severity = severity;
    item.startLine = line;
    item.startColumn = 2;
    item.endLine = line;
    item.endColumn = 4;
    item.source = QStringLiteral("CLANG");
    item.code = QStringLiteral("E%1").arg(line);
    item.message = std::move(message);
    return item;
}

} // namespace

class DiagnosticStoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void replacesDiagnosticsPerUriAndSkipsEmptyMessages();
    void normalizesRangesAndTimestamps();
    void allDiagnosticsAreDeterministic();
};

void DiagnosticStoreTest::replacesDiagnosticsPerUriAndSkipsEmptyMessages()
{
    DiagnosticStore store;

    store.setDiagnostics(QStringLiteral("/repo/a.cpp"),
                         QVector<DiagnosticItem>{diagnostic(QStringLiteral("/repo/a.cpp"), DiagnosticItem::Severity::Error, 4, QStringLiteral("missing semicolon")),
                                                 diagnostic(QStringLiteral("/repo/a.cpp"), DiagnosticItem::Severity::Warning, 5, QString())});

    QCOMPARE(store.diagnostics(QStringLiteral("/repo/a.cpp")).size(), 1);
    QCOMPARE(store.diagnostics(QStringLiteral("/repo/a.cpp")).constFirst().message, QStringLiteral("missing semicolon"));

    store.setDiagnostics(QStringLiteral("/repo/a.cpp"),
                         QVector<DiagnosticItem>{diagnostic(QStringLiteral("/repo/a.cpp"), DiagnosticItem::Severity::Warning, 8, QStringLiteral("unused variable"))});

    QCOMPARE(store.diagnostics(QStringLiteral("/repo/a.cpp")).size(), 1);
    QCOMPARE(store.diagnostics(QStringLiteral("/repo/a.cpp")).constFirst().message, QStringLiteral("unused variable"));
}

void DiagnosticStoreTest::normalizesRangesAndTimestamps()
{
    DiagnosticStore store;

    DiagnosticItem item = diagnostic(QString(), DiagnosticItem::Severity::Error, -5, QStringLiteral("bad range"));
    item.startColumn = -3;
    item.endLine = -2;
    item.endColumn = -1;

    store.setDiagnostics(QStringLiteral("/repo/a.cpp"), QVector<DiagnosticItem>{item});

    const QVector<DiagnosticItem> items = store.diagnostics(QStringLiteral("/repo/a.cpp"));
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.constFirst().uri, QStringLiteral("/repo/a.cpp"));
    QCOMPARE(items.constFirst().startLine, 0);
    QCOMPARE(items.constFirst().startColumn, 0);
    QCOMPARE(items.constFirst().endLine, 0);
    QCOMPARE(items.constFirst().endColumn, 0);
    QVERIFY(items.constFirst().timestamp.isValid());
}

void DiagnosticStoreTest::allDiagnosticsAreDeterministic()
{
    DiagnosticStore store;
    store.setDiagnostics(QStringLiteral("/repo/b.cpp"),
                         QVector<DiagnosticItem>{diagnostic(QStringLiteral("/repo/b.cpp"), DiagnosticItem::Severity::Warning, 3, QStringLiteral("b"))});
    store.setDiagnostics(QStringLiteral("/repo/a.cpp"),
                         QVector<DiagnosticItem>{diagnostic(QStringLiteral("/repo/a.cpp"), DiagnosticItem::Severity::Error, 5, QStringLiteral("a2")),
                                                 diagnostic(QStringLiteral("/repo/a.cpp"), DiagnosticItem::Severity::Error, 1, QStringLiteral("a1"))});

    const QVector<DiagnosticItem> items = store.allDiagnostics();

    QCOMPARE(items.size(), 3);
    QCOMPARE(items.at(0).uri, QStringLiteral("/repo/a.cpp"));
    QCOMPARE(items.at(0).startLine, 1);
    QCOMPARE(items.at(1).uri, QStringLiteral("/repo/a.cpp"));
    QCOMPARE(items.at(1).startLine, 5);
    QCOMPARE(items.at(2).uri, QStringLiteral("/repo/b.cpp"));
}

QTEST_MAIN(DiagnosticStoreTest)

#include "DiagnosticStoreTest.moc"
