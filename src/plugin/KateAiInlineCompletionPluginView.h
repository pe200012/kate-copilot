/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiInlineCompletionPluginView

    MainWindow-scoped plugin instance.
    Integrates with the active view lifecycle.
*/

#pragma once

#include <KTextEditor/MainWindow>

#include <KXMLGUIClient>

#include <QObject>

#include <QHash>

class KateAiInlineCompletionPlugin;
class QAction;
class QNetworkAccessManager;

namespace KateAiInlineCompletion
{
class CopilotAuthManager;
class DiagnosticsAdapter;
class DiagnosticStore;
class EditorSession;
class KWalletSecretStore;
class RecentEditsTracker;
}

namespace KTextEditor
{
class View;
}

class KateAiInlineCompletionPluginView final : public QObject, public KXMLGUIClient
{
    Q_OBJECT

public:
    KateAiInlineCompletionPluginView(KateAiInlineCompletionPlugin *plugin, KTextEditor::MainWindow *mainWindow);
    ~KateAiInlineCompletionPluginView() override;

private Q_SLOTS:
    void onViewChanged(KTextEditor::View *view);

private:
    void setupActions();
    void applyRecentEditsSettings();
    void trackKnownDocuments();
    void ensureSession(KTextEditor::View *view);
    [[nodiscard]] KateAiInlineCompletion::EditorSession *activeSession() const;
    void updateActionState();

    KateAiInlineCompletionPlugin *m_plugin = nullptr;
    KTextEditor::MainWindow *m_mainWindow = nullptr;

    QNetworkAccessManager *m_networkManager = nullptr;
    KateAiInlineCompletion::KWalletSecretStore *m_secretStore = nullptr;
    KateAiInlineCompletion::CopilotAuthManager *m_copilotAuthManager = nullptr;
    KateAiInlineCompletion::RecentEditsTracker *m_recentEditsTracker = nullptr;
    KateAiInlineCompletion::DiagnosticStore *m_diagnosticStore = nullptr;
    KateAiInlineCompletion::DiagnosticsAdapter *m_diagnosticsAdapter = nullptr;

    QAction *m_acceptFullAction = nullptr;
    QAction *m_acceptNextWordAction = nullptr;
    QAction *m_acceptNextLineAction = nullptr;
    QAction *m_dismissAction = nullptr;
    QAction *m_triggerAction = nullptr;

    QHash<KTextEditor::View *, KateAiInlineCompletion::EditorSession *> m_sessions;
};
