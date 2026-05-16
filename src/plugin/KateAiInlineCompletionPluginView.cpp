/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiInlineCompletionPluginView
*/

#include "plugin/KateAiInlineCompletionPluginView.h"

#include "plugin/KateAiInlineCompletionPlugin.h"

#include "auth/CopilotAuthManager.h"
#include "context/DiagnosticStore.h"
#include "context/RecentEditsTracker.h"
#include "session/EditorSession.h"
#include "settings/KWalletSecretStore.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <KXMLGUIFactory>

#include <KTextEditor/Document>
#include <KTextEditor/View>

#include <QAction>
#include <QKeySequence>
#include <QNetworkAccessManager>

KateAiInlineCompletionPluginView::KateAiInlineCompletionPluginView(KateAiInlineCompletionPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_plugin(plugin)
    , m_mainWindow(mainWindow)
{
    m_networkManager = new QNetworkAccessManager(this);

    const WId wid = m_mainWindow->window() ? m_mainWindow->window()->winId() : 0;
    m_secretStore = new KateAiInlineCompletion::KWalletSecretStore(wid, this);
    m_copilotAuthManager = new KateAiInlineCompletion::CopilotAuthManager(m_secretStore, m_networkManager, this);
    m_recentEditsTracker = new KateAiInlineCompletion::RecentEditsTracker(this);
    m_diagnosticStore = new KateAiInlineCompletion::DiagnosticStore(this);
    applyRecentEditsSettings();

    setupActions();
    if (KXMLGUIFactory *factory = m_mainWindow->guiFactory()) {
        factory->addClient(this);
    }

    connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &KateAiInlineCompletionPluginView::onViewChanged);
    connect(m_plugin, &KateAiInlineCompletionPlugin::settingsChanged, this, [this] {
        applyRecentEditsSettings();
        trackKnownDocuments();
    });

    trackKnownDocuments();

    const QList<KTextEditor::View *> views = m_mainWindow->views();
    if (!views.isEmpty()) {
        onViewChanged(views.constFirst());
    }
}

KateAiInlineCompletionPluginView::~KateAiInlineCompletionPluginView()
{
    const auto sessions = m_sessions.values();
    m_sessions.clear();
    for (KateAiInlineCompletion::EditorSession *session : sessions) {
        delete session;
    }

    if (m_mainWindow) {
        if (KXMLGUIFactory *factory = m_mainWindow->guiFactory()) {
            factory->removeClient(this);
        }
    }
}

void KateAiInlineCompletionPluginView::onViewChanged(KTextEditor::View *view)
{
    if (!view || !view->document()) {
        return;
    }

    if (m_recentEditsTracker && m_plugin && m_plugin->settings().validated().enableRecentEditsContext) {
        m_recentEditsTracker->trackDocument(view->document());
    }

    ensureSession(view);
    updateActionState();
}

void KateAiInlineCompletionPluginView::applyRecentEditsSettings()
{
    if (!m_plugin || !m_recentEditsTracker) {
        return;
    }

    const KateAiInlineCompletion::CompletionSettings settings = m_plugin->settings().validated();
    KateAiInlineCompletion::RecentEditsTrackerOptions options;
    options.maxFiles = settings.recentEditsMaxFiles;
    options.maxEdits = settings.recentEditsMaxEdits;
    options.diffContextLines = settings.recentEditsDiffContextLines;
    options.maxCharsPerEdit = settings.recentEditsMaxCharsPerEdit;
    options.debounceMs = settings.recentEditsDebounceMs;
    options.maxLinesPerEdit = settings.recentEditsMaxLinesPerEdit;
    m_recentEditsTracker->setOptions(options);
    if (!settings.enableRecentEditsContext) {
        m_recentEditsTracker->clear();
    }
}

void KateAiInlineCompletionPluginView::trackKnownDocuments()
{
    if (!m_mainWindow || !m_recentEditsTracker || !m_plugin) {
        return;
    }

    if (!m_plugin->settings().validated().enableRecentEditsContext) {
        m_recentEditsTracker->clear();
        return;
    }

    const QList<KTextEditor::View *> views = m_mainWindow->views();
    for (KTextEditor::View *view : views) {
        if (view && view->document()) {
            m_recentEditsTracker->trackDocument(view->document());
        }
    }
}

void KateAiInlineCompletionPluginView::setupActions()
{
    setComponentName(QStringLiteral("kate-ai-inline-completion"), i18n("AI Inline Completion"));

    m_acceptFullAction = actionCollection()->addAction(QStringLiteral("kate_ai_inline_completion_accept_full"));
    m_acceptFullAction->setText(i18n("Accept AI Inline Suggestion"));
    connect(m_acceptFullAction, &QAction::triggered, this, [this] {
        if (auto *session = activeSession()) {
            session->acceptFullSuggestion();
        }
    });

    m_acceptNextWordAction = actionCollection()->addAction(QStringLiteral("kate_ai_inline_completion_accept_next_word"));
    m_acceptNextWordAction->setText(i18n("Accept Next AI Suggestion Word"));
    actionCollection()->setDefaultShortcut(m_acceptNextWordAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Right));
    connect(m_acceptNextWordAction, &QAction::triggered, this, [this] {
        if (auto *session = activeSession()) {
            session->acceptNextWord();
        }
    });

    m_acceptNextLineAction = actionCollection()->addAction(QStringLiteral("kate_ai_inline_completion_accept_next_line"));
    m_acceptNextLineAction->setText(i18n("Accept Next AI Suggestion Line"));
    actionCollection()->setDefaultShortcut(m_acceptNextLineAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_L));
    connect(m_acceptNextLineAction, &QAction::triggered, this, [this] {
        if (auto *session = activeSession()) {
            session->acceptNextLine();
        }
    });

    m_dismissAction = actionCollection()->addAction(QStringLiteral("kate_ai_inline_completion_dismiss"));
    m_dismissAction->setText(i18n("Dismiss AI Inline Suggestion"));
    connect(m_dismissAction, &QAction::triggered, this, [this] {
        if (auto *session = activeSession()) {
            session->dismissSuggestion();
        }
    });

    m_triggerAction = actionCollection()->addAction(QStringLiteral("kate_ai_inline_completion_trigger"));
    m_triggerAction->setText(i18n("Trigger AI Inline Suggestion"));
    actionCollection()->setDefaultShortcut(m_triggerAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Space));
    connect(m_triggerAction, &QAction::triggered, this, [this] {
        if (auto *session = activeSession()) {
            session->triggerSuggestion();
        }
    });

    updateActionState();
}

void KateAiInlineCompletionPluginView::ensureSession(KTextEditor::View *view)
{
    if (!view) {
        return;
    }

    if (m_sessions.contains(view)) {
        return;
    }

    if (m_recentEditsTracker && view->document() && m_plugin && m_plugin->settings().validated().enableRecentEditsContext) {
        m_recentEditsTracker->trackDocument(view->document());
    }

    auto *session = new KateAiInlineCompletion::EditorSession(view,
                                                             m_plugin,
                                                             m_secretStore,
                                                             m_networkManager,
                                                             m_copilotAuthManager,
                                                             m_recentEditsTracker,
                                                             m_diagnosticStore,
                                                             view);
    m_sessions.insert(view, session);

    connect(session, &KateAiInlineCompletion::EditorSession::suggestionVisibilityChanged, this, [this] {
        updateActionState();
    });

    connect(view, &QObject::destroyed, this, [this, view] {
        m_sessions.remove(view);
        updateActionState();
    });
}

KateAiInlineCompletion::EditorSession *KateAiInlineCompletionPluginView::activeSession() const
{
    if (!m_mainWindow) {
        return nullptr;
    }

    KTextEditor::View *view = m_mainWindow->activeView();
    if (!view) {
        return nullptr;
    }

    return m_sessions.value(view, nullptr);
}

void KateAiInlineCompletionPluginView::updateActionState()
{
    const bool hasSession = activeSession() != nullptr;
    const bool hasSuggestion = hasSession && activeSession()->hasVisibleSuggestion();

    if (m_acceptFullAction) {
        m_acceptFullAction->setEnabled(hasSuggestion);
    }
    if (m_acceptNextWordAction) {
        m_acceptNextWordAction->setEnabled(hasSuggestion);
    }
    if (m_acceptNextLineAction) {
        m_acceptNextLineAction->setEnabled(hasSuggestion);
    }
    if (m_dismissAction) {
        m_dismissAction->setEnabled(hasSuggestion);
    }
    if (m_triggerAction) {
        m_triggerAction->setEnabled(hasSession);
    }
}
