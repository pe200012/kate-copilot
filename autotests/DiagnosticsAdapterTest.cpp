/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticsAdapterTest
*/

#include "context/DiagnosticsAdapter.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>

#include <QDir>
#include <QFile>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

using KateAiInlineCompletion::DiagnosticItem;
using KateAiInlineCompletion::DiagnosticsAdapter;

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

KTextEditor::Document *createOpenedDocument(const QString &path, const QString &text)
{
    writeText(path, text);
    auto *editor = KTextEditor::Editor::instance();
    Q_ASSERT(editor);
    auto *document = editor->createDocument(nullptr);
    Q_ASSERT(document);
    const bool opened = document->openUrl(QUrl::fromLocalFile(path));
    Q_ASSERT(opened);
    return document;
}
}

class DiagnosticsAdapterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void lspMarksBecomeLineDiagnostics();
    void documentsWithoutLspMarksProduceNoDiagnostics();
};

void DiagnosticsAdapterTest::lspMarksBecomeLineDiagnostics()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("src/Foo.cpp"));
    QScopedPointer<KTextEditor::Document> document(createOpenedDocument(path, QStringLiteral("int a;\nint b;\n")));
    document->addMark(1, KTextEditor::Document::markType31);

    const QVector<DiagnosticItem> diagnostics = DiagnosticsAdapter::diagnosticsFromLspMarks(document.data());

    QCOMPARE(diagnostics.size(), 1);
    const DiagnosticItem item = diagnostics.constFirst();
    QCOMPARE(item.uri, path);
    QCOMPARE(item.severity, DiagnosticItem::Severity::Warning);
    QCOMPARE(item.startLine, 1);
    QCOMPARE(item.endLine, 1);
    QCOMPARE(item.startColumn, 0);
    QCOMPARE(item.endColumn, QStringLiteral("int b;").size());
    QCOMPARE(item.source, QStringLiteral("Kate LSP"));
    QCOMPARE(item.code, QStringLiteral("markType31"));
    QCOMPARE(item.message, QStringLiteral("Diagnostic reported by Kate LSP"));
}

void DiagnosticsAdapterTest::documentsWithoutLspMarksProduceNoDiagnostics()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("src/Foo.cpp"));
    QScopedPointer<KTextEditor::Document> document(createOpenedDocument(path, QStringLiteral("int a;\n")));

    QVERIFY(DiagnosticsAdapter::diagnosticsFromLspMarks(document.data()).isEmpty());
}

QTEST_MAIN(DiagnosticsAdapterTest)

#include "DiagnosticsAdapterTest.moc"
