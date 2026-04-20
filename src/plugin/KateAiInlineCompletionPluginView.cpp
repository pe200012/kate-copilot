/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiInlineCompletionPluginView
*/

#include "plugin/KateAiInlineCompletionPluginView.h"

#include "plugin/KateAiInlineCompletionPlugin.h"

#include "auth/CopilotAuthManager.h"
#include "session/EditorSession.h"
#include "settings/KWalletSecretStore.h"

#include <KLocalizedString>

#include <KTextEditor/Document>
#include <KTextEditor/View>

#include <QNetworkAccessManager>
#include <QVariantMap>

KateAiInlineCompletionPluginView::KateAiInlineCompletionPluginView(KateAiInlineCompletionPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_plugin(plugin)
    , m_mainWindow(mainWindow)
{
    m_networkManager = new QNetworkAccessManager(this);

    const WId wid = m_mainWindow->window() ? m_mainWindow->window()->winId() : 0;
    m_secretStore = new KateAiInlineCompletion::KWalletSecretStore(wid, this);
    m_copilotAuthManager = new KateAiInlineCompletion::CopilotAuthManager(m_secretStore, m_networkManager, this);

    QVariantMap loaded;
    loaded[QStringLiteral("text")] = i18n("AI Inline Completion plugin loaded");
    loaded[QStringLiteral("type")] = QStringLiteral("Info");
    loaded[QStringLiteral("category")] = i18n("AI");
    loaded[QStringLiteral("token")] = QStringLiteral("kate-ai-inline-completion-loaded");
    m_mainWindow->showMessage(loaded);

    connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &KateAiInlineCompletionPluginView::onViewChanged);

    const QList<KTextEditor::View *> views = m_mainWindow->views();
    if (!views.isEmpty()) {
        onViewChanged(views.constFirst());
    }
}

void KateAiInlineCompletionPluginView::onViewChanged(KTextEditor::View *view)
{
    if (!view || !view->document()) {
        return;
    }

    ensureSession(view);

    const QUrl url = view->document()->url();
    const QString title = url.isValid() ? url.toDisplayString() : i18n("Untitled document");

    QVariantMap active;
    active[QStringLiteral("text")] = i18n("AI Inline Completion active in: %1", title);
    active[QStringLiteral("type")] = QStringLiteral("Log");
    active[QStringLiteral("category")] = i18n("AI");
    active[QStringLiteral("token")] = QStringLiteral("kate-ai-inline-completion-active-view");
    m_mainWindow->showMessage(active);
}

void KateAiInlineCompletionPluginView::ensureSession(KTextEditor::View *view)
{
    if (!view) {
        return;
    }

    if (m_sessions.contains(view)) {
        return;
    }

    auto *session = new KateAiInlineCompletion::EditorSession(view, m_plugin, m_secretStore, m_networkManager, m_copilotAuthManager, view);
    m_sessions.insert(view, session);

    connect(view, &QObject::destroyed, this, [this, view] {
        m_sessions.remove(view);
    });
}
