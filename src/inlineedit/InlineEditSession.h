/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditSession

    Per-view controller for manual inline edit suggestions.
*/

#pragma once

#include "inlineedit/InlineEdit.h"

#include <KTextEditor/Cursor>
#include <KTextEditor/Range>

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QUrl>

#include <memory>

class KateAiInlineCompletionPlugin;
class QNetworkAccessManager;

namespace KTextEditor
{
class Document;
class View;
}

namespace KateAiInlineCompletion
{

class AbstractAIProvider;
class CopilotAuthManager;
class DiagnosticStore;
class InlineEditPreviewOverlay;
class KWalletSecretStore;
class RecentEditsTracker;

class InlineEditSession final : public QObject
{
    Q_OBJECT

public:
    InlineEditSession(KTextEditor::View *view,
                      KateAiInlineCompletionPlugin *plugin,
                      KWalletSecretStore *secretStore,
                      QNetworkAccessManager *networkManager,
                      CopilotAuthManager *copilotAuthManager,
                      RecentEditsTracker *recentEditsTracker,
                      DiagnosticStore *diagnosticStore,
                      QObject *parent = nullptr);
    ~InlineEditSession() override;

    void triggerInlineEdit();
    void acceptInlineEdit();
    void dismissInlineEdit();

    [[nodiscard]] bool hasPreview() const;
    [[nodiscard]] bool hasActiveRequest() const;
    [[nodiscard]] InlineEditSuggestion currentSuggestion() const;

Q_SIGNALS:
    void previewStateChanged(bool active);
    void requestStateChanged(bool active);

private Q_SLOTS:
    void onDeltaReceived(quint64 requestId, const QString &delta);
    void onRequestFinished(quint64 requestId);
    void onRequestFailed(quint64 requestId, const QString &message);
    void onCursorPositionChanged(KTextEditor::View *view, KTextEditor::Cursor cursor);
    void onDocumentTextChanged(KTextEditor::Document *document);
    void onFocusOut(KTextEditor::View *view);
    void onSelectionChanged(KTextEditor::View *view);

private:
    void ensureProvider(const QString &providerId);
    void cancelActiveRequest();
    void clearPreview();
    void setPreview(const InlineEditSuggestion &suggestion);

    [[nodiscard]] InlineEditRequestContext buildRequestContext(const KTextEditor::Range &targetRange) const;
    [[nodiscard]] KTextEditor::Range targetRangeForCurrentState() const;
    [[nodiscard]] QString documentDisplayPath(KTextEditor::Document *document) const;
    [[nodiscard]] QString extractPrefixBefore(const KTextEditor::Cursor &cursor, int maxChars) const;
    [[nodiscard]] QString extractSuffixAfter(const KTextEditor::Cursor &cursor, int maxChars) const;
    [[nodiscard]] QVector<ContextItem> collectContextItems(const InlineEditRequestContext &context) const;
    [[nodiscard]] bool providerAllowedForInlineEdits(const QString &providerId) const;
    [[nodiscard]] bool isLocalEndpoint(const QUrl &url) const;

    void showInfo(const QString &text);
    void showError(const QString &text);

    QPointer<KTextEditor::View> m_view;
    KateAiInlineCompletionPlugin *m_plugin = nullptr;
    KWalletSecretStore *m_secretStore = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    CopilotAuthManager *m_copilotAuthManager = nullptr;
    RecentEditsTracker *m_recentEditsTracker = nullptr;
    DiagnosticStore *m_diagnosticStore = nullptr;

    QPointer<InlineEditPreviewOverlay> m_overlay;
    std::unique_ptr<AbstractAIProvider> m_provider;
    QString m_providerId;

    quint64 m_activeRequestId = 0;
    QString m_activeResponse;
    KTextEditor::Range m_activeTargetRange = KTextEditor::Range::invalid();
    qint64 m_activeDocumentRevision = -1;
    qint64 m_previewDocumentRevision = -1;
    InlineEditSuggestion m_currentSuggestion;
    bool m_ignoreDocumentChange = false;
};

} // namespace KateAiInlineCompletion
