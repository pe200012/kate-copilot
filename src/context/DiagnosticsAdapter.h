/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticsAdapter

    Best-effort bridge from public Kate/KTextEditor diagnostic markers into DiagnosticStore.
*/

#pragma once

#include "context/DiagnosticItem.h"

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QVector>

namespace KTextEditor
{
class Document;
class MainWindow;
class View;
}

namespace KateAiInlineCompletion
{

class DiagnosticStore;

class DiagnosticsAdapter final : public QObject
{
    Q_OBJECT

public:
    explicit DiagnosticsAdapter(QObject *parent = nullptr);

    void attach(KTextEditor::MainWindow *mainWindow, DiagnosticStore *store);

    [[nodiscard]] static QVector<DiagnosticItem> diagnosticsFromLspMarks(KTextEditor::Document *document);

private Q_SLOTS:
    void scheduleRescan();

private:
    void trackView(KTextEditor::View *view);
    void trackDocument(KTextEditor::Document *document);
    void connectLspDiagnosticProviderSignals(QObject *pluginView);
    void rescanOpenDocuments();
    void clearOwnedDiagnostics();
    void disconnectTrackedConnections();

    [[nodiscard]] bool lspDiagnosticsAvailable() const;

    QPointer<KTextEditor::MainWindow> m_mainWindow;
    QPointer<DiagnosticStore> m_store;
    QSet<KTextEditor::Document *> m_documents;
    QSet<QObject *> m_connectedDiagnosticProviders;
    QVector<QMetaObject::Connection> m_connections;
    QSet<QString> m_ownedUris;
    QTimer m_rescanTimer;
};

} // namespace KateAiInlineCompletion
