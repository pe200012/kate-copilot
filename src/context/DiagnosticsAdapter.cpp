/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticsAdapter
*/

#include "context/DiagnosticsAdapter.h"

#include "context/DiagnosticStore.h"

#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <algorithm>

#include <QDateTime>
#include <QMetaMethod>
#include <QUrl>

namespace KateAiInlineCompletion
{

namespace
{
constexpr KTextEditor::Document::MarkTypes kKateLspDiagnosticMark = KTextEditor::Document::markType31;

[[nodiscard]] QString uriForDocument(KTextEditor::Document *document)
{
    if (!document || !document->url().isValid() || document->url().isEmpty()) {
        return {};
    }

    return document->url().toDisplayString(QUrl::PreferLocalFile);
}

[[nodiscard]] QList<QObject *> diagnosticProviderChildren(QObject *root)
{
    if (!root) {
        return {};
    }
    return root->findChildren<QObject *>(QStringLiteral("LSPDiagnosticProvider"));
}
} // namespace

DiagnosticsAdapter::DiagnosticsAdapter(QObject *parent)
    : QObject(parent)
{
    m_rescanTimer.setSingleShot(true);
    m_rescanTimer.setInterval(50);
    connect(&m_rescanTimer, &QTimer::timeout, this, &DiagnosticsAdapter::rescanOpenDocuments);
}

void DiagnosticsAdapter::attach(KTextEditor::MainWindow *mainWindow, DiagnosticStore *store)
{
    disconnectTrackedConnections();
    clearOwnedDiagnostics();
    m_documents.clear();
    m_connectedDiagnosticProviders.clear();
    m_mainWindow = mainWindow;
    m_store = store;

    if (!m_mainWindow || !m_store) {
        return;
    }

    m_connections.push_back(connect(m_mainWindow.data(), &KTextEditor::MainWindow::viewCreated, this, &DiagnosticsAdapter::trackView, Qt::UniqueConnection));
    m_connections.push_back(connect(m_mainWindow.data(), &KTextEditor::MainWindow::pluginViewCreated, this, [this](const QString &name, QObject *pluginView) {
        if (name.contains(QStringLiteral("lspclient"), Qt::CaseInsensitive)) {
            connectLspDiagnosticProviderSignals(pluginView);
            scheduleRescan();
        }
    }, Qt::UniqueConnection));
    m_connections.push_back(connect(m_mainWindow.data(), &KTextEditor::MainWindow::pluginViewDeleted, this, [this](const QString &name, QObject *) {
        if (name.contains(QStringLiteral("lspclient"), Qt::CaseInsensitive)) {
            scheduleRescan();
        }
    }, Qt::UniqueConnection));

    for (KTextEditor::View *view : m_mainWindow->views()) {
        trackView(view);
    }

    connectLspDiagnosticProviderSignals(m_mainWindow->pluginView(QStringLiteral("lspclientplugin")));
    connectLspDiagnosticProviderSignals(m_mainWindow->pluginView(QStringLiteral("lspclient")));
    scheduleRescan();
}

QVector<DiagnosticItem> DiagnosticsAdapter::diagnosticsFromLspMarks(KTextEditor::Document *document)
{
    QVector<DiagnosticItem> out;
    const QString uri = uriForDocument(document);
    if (!document || uri.isEmpty()) {
        return out;
    }

    const QHash<int, KTextEditor::Mark *> &marks = document->marks();
    out.reserve(marks.size());
    const QDateTime timestamp = QDateTime::currentDateTimeUtc();
    for (auto it = marks.constBegin(); it != marks.constEnd(); ++it) {
        const KTextEditor::Mark *mark = it.value();
        if (!mark || !(mark->type & kKateLspDiagnosticMark)) {
            continue;
        }

        const int line = qMax(0, mark->line);
        DiagnosticItem item;
        item.uri = uri;
        item.severity = DiagnosticItem::Severity::Warning;
        item.startLine = line;
        item.endLine = line;
        item.startColumn = 0;
        item.endColumn = (line >= 0 && line < document->lines()) ? document->line(line).size() : 0;
        item.source = QStringLiteral("Kate LSP");
        item.code = QStringLiteral("markType31");
        item.message = QStringLiteral("Diagnostic reported by Kate LSP");
        item.timestamp = timestamp;
        out.push_back(item);
    }

    std::stable_sort(out.begin(), out.end(), [](const DiagnosticItem &a, const DiagnosticItem &b) {
        if (a.uri != b.uri) {
            return a.uri < b.uri;
        }
        return a.startLine < b.startLine;
    });
    return out;
}

void DiagnosticsAdapter::scheduleRescan()
{
    if (!m_rescanTimer.isActive()) {
        m_rescanTimer.start();
    }
}

void DiagnosticsAdapter::trackView(KTextEditor::View *view)
{
    if (!view) {
        return;
    }
    trackDocument(view->document());
}

void DiagnosticsAdapter::trackDocument(KTextEditor::Document *document)
{
    if (!document || m_documents.contains(document)) {
        return;
    }

    m_documents.insert(document);
    m_connections.push_back(connect(document, &KTextEditor::Document::marksChanged, this, [this](KTextEditor::Document *) {
        scheduleRescan();
    }, Qt::UniqueConnection));
    m_connections.push_back(connect(document, &KTextEditor::Document::documentUrlChanged, this, [this](KTextEditor::Document *) {
        scheduleRescan();
    }, Qt::UniqueConnection));
    m_connections.push_back(connect(document, &KTextEditor::Document::aboutToClose, this, [this, document](KTextEditor::Document *) {
        const QString uri = uriForDocument(document);
        if (m_store && !uri.isEmpty() && m_ownedUris.contains(uri)) {
            m_store->clearDiagnostics(uri);
            m_ownedUris.remove(uri);
        }
        m_documents.remove(document);
    }, Qt::UniqueConnection));
    m_connections.push_back(connect(document, &QObject::destroyed, this, [this, document] {
        m_documents.remove(document);
    }, Qt::UniqueConnection));
}

void DiagnosticsAdapter::connectLspDiagnosticProviderSignals(QObject *pluginView)
{
    for (QObject *provider : diagnosticProviderChildren(pluginView)) {
        if (!provider || m_connectedDiagnosticProviders.contains(provider)) {
            continue;
        }

        m_connectedDiagnosticProviders.insert(provider);
        m_connections.push_back(connect(provider, &QObject::destroyed, this, [this, provider] {
            m_connectedDiagnosticProviders.remove(provider);
            scheduleRescan();
        }, Qt::UniqueConnection));

        const QMetaObject *meta = provider->metaObject();
        const int slotIndex = metaObject()->indexOfSlot("scheduleRescan()");
        if (!meta || slotIndex < 0) {
            continue;
        }
        const QMetaMethod slot = metaObject()->method(slotIndex);
        for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
            const QMetaMethod method = meta->method(i);
            if (method.methodType() == QMetaMethod::Signal && method.name() == QByteArray("diagnosticsAdded")) {
                m_connections.push_back(QObject::connect(provider, method, this, slot, Qt::UniqueConnection));
            }
        }
    }
}

void DiagnosticsAdapter::rescanOpenDocuments()
{
    if (!m_store) {
        return;
    }

    if (!m_mainWindow || !lspDiagnosticsAvailable()) {
        clearOwnedDiagnostics();
        return;
    }

    QSet<QString> seenUris;
    for (KTextEditor::View *view : m_mainWindow->views()) {
        trackView(view);
    }

    for (KTextEditor::Document *document : std::as_const(m_documents)) {
        if (!document) {
            continue;
        }
        const QString uri = uriForDocument(document);
        if (uri.isEmpty()) {
            continue;
        }
        seenUris.insert(uri);
        const QVector<DiagnosticItem> diagnostics = diagnosticsFromLspMarks(document);
        if (diagnostics.isEmpty()) {
            if (m_ownedUris.contains(uri)) {
                m_store->clearDiagnostics(uri);
                m_ownedUris.remove(uri);
            }
            continue;
        }
        m_store->setDiagnostics(uri, diagnostics);
        m_ownedUris.insert(uri);
    }

    const QSet<QString> previousUris = m_ownedUris;
    for (const QString &uri : previousUris) {
        if (!seenUris.contains(uri)) {
            m_store->clearDiagnostics(uri);
            m_ownedUris.remove(uri);
        }
    }
}

void DiagnosticsAdapter::clearOwnedDiagnostics()
{
    if (!m_store) {
        m_ownedUris.clear();
        return;
    }

    const QSet<QString> uris = m_ownedUris;
    for (const QString &uri : uris) {
        m_store->clearDiagnostics(uri);
    }
    m_ownedUris.clear();
}

void DiagnosticsAdapter::disconnectTrackedConnections()
{
    for (const QMetaObject::Connection &connection : std::as_const(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}

bool DiagnosticsAdapter::lspDiagnosticsAvailable() const
{
    if (!m_mainWindow) {
        return false;
    }

    for (const QString &name : {QStringLiteral("lspclientplugin"), QStringLiteral("lspclient")}) {
        QObject *pluginView = m_mainWindow->pluginView(name);
        if (pluginView) {
            if (!diagnosticProviderChildren(pluginView).isEmpty()) {
                return true;
            }
            return true;
        }
    }

    return !diagnosticProviderChildren(m_mainWindow.data()).isEmpty();
}

} // namespace KateAiInlineCompletion
