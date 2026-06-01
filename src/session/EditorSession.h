/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: EditorSession

    Per-view controller that:
    - debounces text input
    - requests streaming completion
    - renders ghost text via overlay attached to editorWidget()
    - accepts suggestion on Tab
*/

#pragma once

#include "session/CompletionCandidateList.h"
#include "session/CompletionCache.h"
#include "session/GhostTextState.h"
#include "session/SuggestionAnchorTracker.h"

#include <KTextEditor/Cursor>
#include <KTextEditor/Document>

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QUrl>

#include <memory>

class QNetworkAccessManager;

namespace KTextEditor
{
class View;
}

class KateAiInlineCompletionPlugin;

namespace KateAiInlineCompletion
{

class AbstractAIProvider;
class CopilotAuthManager;
class CompletionCache;
class DiagnosticStore;
class GhostTextInlineNoteProvider;
class GhostTextOverlayWidget;
class KWalletSecretStore;
class RecentEditsTracker;
struct ProcessedSuggestion;

class EditorSession final : public QObject
{
    Q_OBJECT

public:
    EditorSession(KTextEditor::View *view,
                  KateAiInlineCompletionPlugin *plugin,
                  KWalletSecretStore *secretStore,
                  QNetworkAccessManager *networkManager,
                  CopilotAuthManager *copilotAuthManager,
                  RecentEditsTracker *recentEditsTracker,
                  DiagnosticStore *diagnosticStore,
                  CompletionCache *completionCache,
                  QObject *parent = nullptr);

    ~EditorSession() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    [[nodiscard]] bool hasVisibleSuggestion() const;

    void acceptFullSuggestion();
    void acceptNextWord();
    void acceptNextLine();
    void dismissSuggestion();
    void triggerSuggestion();
    void nextCandidate();
    void previousCandidate();

    [[nodiscard]] int candidateCount() const;

Q_SIGNALS:
    void suggestionVisibilityChanged(bool visible);
    void candidateStateChanged();

private Q_SLOTS:
    void onTextInserted(KTextEditor::View *view, KTextEditor::Cursor position, const QString &text);
    void onCursorPositionChanged(KTextEditor::View *view, KTextEditor::Cursor newPosition);
    void onSelectionChanged(KTextEditor::View *view);
    void onFocusOut(KTextEditor::View *view);

    void onDebounceTimeout();

    void onDeltaReceived(quint64 requestId, const QString &delta);
    void onCandidateReceived(quint64 requestId, int index, const QString &fullText);
    void onRequestFinished(quint64 requestId);
    void onRequestFailed(quint64 requestId, const QString &message);
    void onDocumentTextChanged(KTextEditor::Document *document);

private:
    void bumpGeneration();
    void scheduleCompletion();
    void startRequest();

    void clearSuggestion();
    void acceptSuggestion();

    void acceptPartial(const QString &chunk);
    [[nodiscard]] bool tryReuseVisibleSuggestionForTypedText(const QString &text);
    [[nodiscard]] bool applyProcessedSuggestion(const ProcessedSuggestion &processed);
    [[nodiscard]] CompletionCandidate candidateFromProcessed(const QString &raw, const ProcessedSuggestion &processed, const QString &source, int index) const;
    void setCandidatesForCurrentAnchor(QVector<CompletionCandidate> candidates);
    [[nodiscard]] bool applyCurrentCandidate();
    void replaceCurrentCandidateWithProcessed(const QString &raw, const ProcessedSuggestion &processed, const QString &source);
    void filterAndShrinkCandidatesByPrefix(const QString &prefix);
    void scheduleSpeculativeRequestIfEnabled(const CompletionSettings &settings);
    void startSpeculativeRequest();
    void cancelSpeculativeRequest();
    void finishSpeculativeRequest();

    void setSuppressed(bool suppressed);

    [[nodiscard]] bool syncAnchorFromTracker();
    [[nodiscard]] bool shouldRenderInlineNote(const GhostTextState &state) const;
    [[nodiscard]] bool shouldRenderOverlay(const GhostTextState &state) const;
    [[nodiscard]] GhostTextState clearedRenderState() const;
    void applyStateToOverlay();

    [[nodiscard]] QString takeNextWordChunk(const QString &remaining) const;
    [[nodiscard]] QString takeNextLineChunk(const QString &remaining) const;

    void showInfo(const QString &text);
    void showError(const QString &text);

    void ensureProvider(const QString &providerId);

    [[nodiscard]] QString extractPrefix(int maxChars) const;
    [[nodiscard]] QString extractSuffix(int maxChars) const;
    [[nodiscard]] QString extractSuffixFromCursor(const KTextEditor::Cursor &cursor, int maxChars) const;
    [[nodiscard]] bool isLocalEndpoint(const QUrl &url) const;

    QPointer<KTextEditor::View> m_view;
    KateAiInlineCompletionPlugin *m_plugin = nullptr;
    KWalletSecretStore *m_secretStore = nullptr;

    QNetworkAccessManager *m_networkManager = nullptr;
    CopilotAuthManager *m_copilotAuthManager = nullptr;
    RecentEditsTracker *m_recentEditsTracker = nullptr;
    DiagnosticStore *m_diagnosticStore = nullptr;
    CompletionCache *m_completionCache = nullptr;

    QPointer<GhostTextOverlayWidget> m_overlay;
    std::unique_ptr<GhostTextInlineNoteProvider> m_inlineNoteProvider;

    std::unique_ptr<AbstractAIProvider> m_provider;
    QString m_providerId;

    QTimer m_debounceTimer;

    GhostTextState m_state;
    SuggestionAnchorTracker m_anchorTracker;
    CompletionCandidateList m_candidates;
    QHash<int, QString> m_activeCandidateRawByIndex;
    QTimer m_speculativeTimer;
    quint64 m_speculativeRequestId = 0;
    CompletionCacheKey m_speculativeCacheKey;
    QHash<int, QString> m_speculativeCandidateRawByIndex;
    QString m_rawSuggestionText;
    QString m_acceptedFromSuggestion;
    QString m_typedPrefixFromSuggestion;
    QString m_pendingTypingReuseText;
    CompletionCacheKey m_activeCacheKey;
    bool m_hasActiveCacheKey = false;

    enum class SuggestionSource {
        None,
        Network,
        Cache,
        TypingAsSuggested,
    };
    SuggestionSource m_suggestionSource = SuggestionSource::None;

    bool m_nextRequestManualTrigger = false;
    bool m_nextRequestAfterPartialAccept = false;
    bool m_nextRequestAfterFullAccept = false;

    quint64 m_generation = 0;
    quint64 m_activeRequestId = 0;

    int m_ignoreNextViewSignals = 0;
    bool m_ignoreNextTextInsertedAfterTypingReuse = false;
    bool m_ignoreNextCursorMoveAfterTypingReuse = false;
    bool m_skippedCursorMoveBeforeTextInsert = false;
    bool m_lastSuggestionVisible = false;
};

} // namespace KateAiInlineCompletion
