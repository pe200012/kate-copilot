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
class QNetworkAccessManager;

namespace KateAiInlineCompletion
{
class CopilotAuthManager;
class EditorSession;
class KWalletSecretStore;
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

private Q_SLOTS:
    void onViewChanged(KTextEditor::View *view);

private:
    void ensureSession(KTextEditor::View *view);

    KateAiInlineCompletionPlugin *m_plugin = nullptr;
    KTextEditor::MainWindow *m_mainWindow = nullptr;

    QNetworkAccessManager *m_networkManager = nullptr;
    KateAiInlineCompletion::KWalletSecretStore *m_secretStore = nullptr;
    KateAiInlineCompletion::CopilotAuthManager *m_copilotAuthManager = nullptr;

    QHash<KTextEditor::View *, KateAiInlineCompletion::EditorSession *> m_sessions;
};
