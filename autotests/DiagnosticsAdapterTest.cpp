/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticsAdapterTest
*/

#include "context/DiagnosticsAdapter.h"

#include "context/DiagnosticStore.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QDir>
#include <QFile>
#include <QPointer>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QUrl>
#include <QWidget>
#include <QtTest>

using KateAiInlineCompletion::DiagnosticItem;
using KateAiInlineCompletion::DiagnosticStore;
using KateAiInlineCompletion::DiagnosticsAdapter;

namespace
{
class FakeDiagnosticProvider final : public QObject
{
    Q_OBJECT

public:
    explicit FakeDiagnosticProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
        setObjectName(QStringLiteral("LSPDiagnosticProvider"));
    }

    void publishDiagnostics()
    {
        Q_EMIT diagnosticsAdded();
    }

Q_SIGNALS:
    void diagnosticsAdded();
};

class FakeMainWindowHost final : public QObject
{
    Q_OBJECT

public:
    explicit FakeMainWindowHost(QObject *parent = nullptr)
        : QObject(parent)
        , m_provider(&m_lspPluginView)
    {
    }

    void setViews(const QList<KTextEditor::View *> &views)
    {
        m_views = views;
    }

    FakeDiagnosticProvider *provider()
    {
        return &m_provider;
    }

public Q_SLOTS:
    QWidget *window()
    {
        return nullptr;
    }

    QList<KTextEditor::View *> views()
    {
        return m_views;
    }

    KTextEditor::View *activeView()
    {
        return m_views.isEmpty() ? nullptr : m_views.constFirst();
    }

    QObject *pluginView(const QString &name)
    {
        return name.contains(QStringLiteral("lspclient"), Qt::CaseInsensitive) ? &m_lspPluginView : nullptr;
    }

    QWidget *createViewBar(KTextEditor::View *)
    {
        return nullptr;
    }

    void deleteViewBar(KTextEditor::View *)
    {
    }

private:
    QObject m_lspPluginView;
    FakeDiagnosticProvider m_provider;
    QList<KTextEditor::View *> m_views;
};

void writeText(const QString &path, const QString &text)
{
    QFileInfo info(path);
    QVERIFY(QDir().mkpath(info.absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray bytes = text.toUtf8();
    QCOMPARE(file.write(bytes), bytes.size());
}

KTextEditor::Document *createOpenedDocument(const QString &path, const QString &text)
{
    writeText(path, text);
    auto *editor = KTextEditor::Editor::instance();
    if (!editor) {
        QTest::qFail("KTextEditor editor instance is unavailable", __FILE__, __LINE__);
        return nullptr;
    }

    auto *document = editor->createDocument(nullptr);
    if (!document) {
        QTest::qFail("Failed to create KTextEditor document", __FILE__, __LINE__);
        return nullptr;
    }

    const bool opened = document->openUrl(QUrl::fromLocalFile(path));
    if (!opened) {
        QTest::qFail("Failed to open KTextEditor document URL", __FILE__, __LINE__);
        document->deleteLater();
        return nullptr;
    }

    return document;
}
}

class DiagnosticsAdapterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void lspMarksBecomeLineDiagnostics();
    void documentsWithoutLspMarksProduceNoDiagnostics();
    void attachTracksDocumentsWithoutInvalidUniqueConnectionWarnings();
    void destroyedDocumentIsRemovedBeforePendingRescan();
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

void DiagnosticsAdapterTest::attachTracksDocumentsWithoutInvalidUniqueConnectionWarnings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QWidget widget;
    FakeMainWindowHost host;
    KTextEditor::MainWindow mainWindow(&host);
    DiagnosticStore store;
    DiagnosticsAdapter adapter;

    const QString path = dir.filePath(QStringLiteral("src/Foo.cpp"));
    QScopedPointer<KTextEditor::Document> document(createOpenedDocument(path, QStringLiteral("int a;\nint b;\n")));
    document->addMark(1, KTextEditor::Document::markType31);
    QScopedPointer<KTextEditor::View> view(document->createView(&widget, &mainWindow));
    QVERIFY(view);
    host.setViews({view.data()});

    QTest::failOnWarning(QRegularExpression(QStringLiteral(".*unique connections require a pointer to member function.*")));

    adapter.attach(&mainWindow, &store);
    host.provider()->publishDiagnostics();

    QTRY_COMPARE_WITH_TIMEOUT(store.diagnostics(path).size(), 1, 1000);
}

void DiagnosticsAdapterTest::destroyedDocumentIsRemovedBeforePendingRescan()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QWidget widget;
    FakeMainWindowHost host;
    KTextEditor::MainWindow mainWindow(&host);
    DiagnosticStore store;
    DiagnosticsAdapter adapter;

    const QString path = dir.filePath(QStringLiteral("src/Foo.cpp"));
    QScopedPointer<KTextEditor::Document> document(createOpenedDocument(path, QStringLiteral("int a;\nint b;\n")));
    document->addMark(1, KTextEditor::Document::markType31);
    QScopedPointer<KTextEditor::View> view(document->createView(&widget, &mainWindow));
    QVERIFY(view);
    host.setViews({view.data()});

    adapter.attach(&mainWindow, &store);
    host.provider()->publishDiagnostics();
    QTRY_COMPARE_WITH_TIMEOUT(store.diagnostics(path).size(), 1, 1000);

    host.setViews({});
    view.reset();
    document.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&adapter, "scheduleRescan", Qt::DirectConnection));
    QTest::qWait(75);
    QCoreApplication::processEvents();

    QVERIFY(store.diagnostics(path).isEmpty());
}

QTEST_MAIN(DiagnosticsAdapterTest)

#include "DiagnosticsAdapterTest.moc"
