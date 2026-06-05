/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditSession
*/

#include "inlineedit/InlineEditSession.h"

#include "auth/CopilotAuthManager.h"
#include "context/ContextFileFilter.h"
#include "context/ContextProviderRegistry.h"
#include "context/CurrentFileContextProvider.h"
#include "context/DiagnosticStore.h"
#include "context/DiagnosticsContextProvider.h"
#include "context/OpenTabsContextProvider.h"
#include "context/ProjectTraitsContextProvider.h"
#include "context/RecentEditsContextProvider.h"
#include "context/RelatedFilesContextProvider.h"
#include "inlineedit/InlineEditApplier.h"
#include "inlineedit/InlineEditParser.h"
#include "inlineedit/InlineEditPromptBuilder.h"
#include "inlineedit/InlineEditValidator.h"
#include "network/AbstractAIProvider.h"
#include "network/CopilotCodexProvider.h"
#include "network/OpenAICompatibleProvider.h"
#include "plugin/KateAiInlineCompletionPlugin.h"
#include "render/InlineEditPreviewOverlay.h"
#include "settings/CompletionSettings.h"
#include "settings/KWalletSecretStore.h"
#include "security/SensitiveDataRedactor.h"

#include <KLocalizedString>

#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QNetworkAccessManager>
#include <QScopedValueRollback>
#include <QUuid>
#include <QVariantMap>
#include <QWidget>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace KateAiInlineCompletion
{
namespace
{
[[nodiscard]] RecentEditsContextOptions recentEditsOptionsFromSettings(const CompletionSettings &settings)
{
    RecentEditsContextOptions options;
    options.enabled = settings.enableRecentEditsContext;
    options.maxEdits = settings.recentEditsMaxEdits;
    options.maxCharsPerEdit = settings.recentEditsMaxCharsPerEdit;
    options.activeDocDistanceLimitFromCursor = settings.recentEditsActiveDocDistanceLimitFromCursor;
    return options;
}

[[nodiscard]] DiagnosticsContextOptions diagnosticsOptionsFromSettings(const CompletionSettings &settings)
{
    DiagnosticsContextOptions options;
    options.enabled = settings.enableDiagnosticsContext;
    options.maxItems = settings.diagnosticsMaxItems;
    options.maxChars = settings.diagnosticsMaxChars;
    options.maxLineDistance = settings.diagnosticsMaxLineDistance;
    options.includeWarnings = settings.diagnosticsIncludeWarnings;
    options.includeInformation = settings.diagnosticsIncludeInformation;
    options.includeHints = settings.diagnosticsIncludeHints;
    return options;
}

[[nodiscard]] RelatedFilesContextOptions relatedFilesOptionsFromSettings(const CompletionSettings &settings)
{
    RelatedFilesContextOptions options;
    options.enabled = settings.enableRelatedFilesContext;
    options.maxFiles = settings.relatedFilesMaxFiles;
    options.maxChars = settings.relatedFilesMaxChars;
    options.maxCharsPerFile = settings.relatedFilesMaxCharsPerFile;
    options.preferOpenTabs = settings.relatedFilesPreferOpenTabs;
    options.excludePatterns = settings.contextExcludePatterns;
    return options;
}

[[nodiscard]] InlineEditValidationOptions inlineEditValidationOptionsFromSettings(const CompletionSettings &settings, qint64 expectedDocumentRevision)
{
    InlineEditValidationOptions options;
    options.maxEdits = settings.inlineEditMaxEdits;
    options.maxNewTextChars = settings.inlineEditMaxNewTextChars;
    options.maxTotalNewTextChars = settings.inlineEditMaxTotalNewTextChars;
    options.allowDeletion = settings.inlineEditAllowDeletion;
    options.expectedDocumentRevision = expectedDocumentRevision;
    return options;
}

[[nodiscard]] int inlineEditMaxTokens(const CompletionSettings &settings)
{
    return qBound(64, settings.inlineEditMaxNewTextChars / 4, 2048);
}

[[nodiscard]] QString safeDisplayUrl(QUrl url)
{
    url.setUserInfo(QString());
    url.setQuery({});
    url.setFragment({});
    return url.toDisplayString(QUrl::PreferLocalFile | QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
}

[[nodiscard]] int boundedSize(qsizetype value)
{
    return static_cast<int>(qMin<qsizetype>(value, std::numeric_limits<int>::max()));
}

[[nodiscard]] QString sanitizedErrorDetail(QString detail)
{
    return redactSensitiveData(std::move(detail));
}
} // namespace

InlineEditSession::InlineEditSession(KTextEditor::View *view,
                                     KateAiInlineCompletionPlugin *plugin,
                                     KWalletSecretStore *secretStore,
                                     QNetworkAccessManager *networkManager,
                                     CopilotAuthManager *copilotAuthManager,
                                     RecentEditsTracker *recentEditsTracker,
                                     DiagnosticStore *diagnosticStore,
                                     QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_plugin(plugin)
    , m_secretStore(secretStore)
    , m_networkManager(networkManager)
    , m_copilotAuthManager(copilotAuthManager)
    , m_recentEditsTracker(recentEditsTracker)
    , m_diagnosticStore(diagnosticStore)
{
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
    }

    if (m_view) {
        if (QWidget *widget = m_view->editorWidget()) {
            m_overlay = new InlineEditPreviewOverlay(m_view, widget);
        }

        connect(m_view, &KTextEditor::View::cursorPositionChanged, this, &InlineEditSession::onCursorPositionChanged);
        connect(m_view, &KTextEditor::View::selectionChanged, this, &InlineEditSession::onSelectionChanged);
        connect(m_view, &KTextEditor::View::focusOut, this, &InlineEditSession::onFocusOut);
        connect(m_view, &KTextEditor::View::verticalScrollPositionChanged, this, [this] {
            if (m_overlay) {
                m_overlay->refresh();
            }
        });
        connect(m_view, &KTextEditor::View::horizontalScrollPositionChanged, this, [this] {
            if (m_overlay) {
                m_overlay->refresh();
            }
        });
        connect(m_view, &KTextEditor::View::displayRangeChanged, this, [this] {
            if (m_overlay) {
                m_overlay->refresh();
            }
        });
        connect(m_view, &KTextEditor::View::configChanged, this, [this] {
            if (m_overlay) {
                m_overlay->refresh();
            }
        });
        connect(m_view, &QObject::destroyed, this, [this] {
            m_view = nullptr;
            m_overlay = nullptr;
        });

        if (KTextEditor::Document *document = m_view->document()) {
            connect(document, &KTextEditor::Document::textChanged, this, &InlineEditSession::onDocumentTextChanged);
        }
    }
}

InlineEditSession::~InlineEditSession()
{
    cancelActiveRequest();
    if (m_overlay) {
        m_overlay->clear();
        std::unique_ptr<InlineEditPreviewOverlay> overlay(m_overlay.data());
        overlay->setParent(nullptr);
        m_overlay = nullptr;
    }
}

void InlineEditSession::triggerInlineEdit()
{
    if (!m_view || !m_plugin || !m_view->document()) {
        return;
    }

    const CompletionSettings settings = m_plugin->settings().validated();
    if (!settings.enableInlineEdits) {
        showInfo(i18n("AI inline edits are disabled"));
        return;
    }

    ensureProvider(settings.provider);
    if (!m_provider || !providerAllowedForInlineEdits(settings.provider)) {
        showError(i18n("AI inline edits are unavailable for the selected provider"));
        return;
    }

    const QUrl endpoint = settings.endpoint;
    const bool providerIsOllama = settings.provider == QString::fromLatin1(CompletionSettings::kProviderOllama);
    const bool providerIsCopilot = settings.provider == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex);
    QString apiKey;
    if (!providerIsCopilot && m_secretStore) {
        apiKey = m_secretStore->readApiKey();
    }

    if (!providerIsOllama && !providerIsCopilot && !isLocalEndpoint(endpoint) && apiKey.trimmed().isEmpty()) {
        showError(i18n("AI inline edit requires an API key for endpoint: %1", safeDisplayUrl(endpoint)));
        return;
    }

    if (providerIsCopilot && (!m_copilotAuthManager || !m_secretStore || !m_secretStore->hasGitHubOAuthToken())) {
        showError(i18n("GitHub Copilot requires OAuth sign-in. Open the plugin settings and sign in."));
        return;
    }

    const KTextEditor::Range targetRange = targetRangeForCurrentState();
    if (!targetRange.isValid()) {
        showError(i18n("AI inline edit target is invalid"));
        return;
    }

    cancelActiveRequest();
    clearPreview();

    const InlineEditRequestContext context = buildRequestContext(targetRange);
    InlineEditPromptOptions promptOptions;
    promptOptions.useContext = settings.inlineEditUseContext;
    promptOptions.maxContextChars = settings.maxContextChars;
    promptOptions.maxEdits = settings.inlineEditMaxEdits;
    const InlineEditPrompt prompt = InlineEditPromptBuilder::build(context, promptOptions);

    CompletionRequest request;
    request.endpoint = endpoint;
    request.model = settings.model;
    request.systemPrompt = prompt.systemPrompt;
    request.userPrompt = prompt.userPrompt;
    request.temperature = 0.0;
    request.maxTokens = inlineEditMaxTokens(settings);
    request.n = 1;

    if (providerIsCopilot) {
        request.prompt = prompt.systemPrompt + QStringLiteral("\n\n") + prompt.userPrompt;
        request.suffix.clear();
        request.nwo = settings.copilotNwo;
    } else {
        request.apiKey = apiKey;
    }

    m_activeResponse.clear();
    m_activeTargetRange = targetRange;
    m_activeDocumentRevision = m_view->document()->revision();
    m_activeRequestId = m_provider->start(request);
    Q_EMIT requestStateChanged(true);
}

void InlineEditSession::acceptInlineEdit()
{
    if (!m_view || !m_view->document() || !m_currentSuggestion.valid || m_currentSuggestion.edits.isEmpty()) {
        return;
    }

    KTextEditor::Document *document = m_view->document();
    const CompletionSettings settings = m_plugin ? m_plugin->settings().validated() : CompletionSettings::defaults();
    const InlineEditValidationOptions options = inlineEditValidationOptionsFromSettings(settings, m_previewDocumentRevision);

    QScopedValueRollback<bool> ignoreDocumentChange(m_ignoreDocumentChange, true);
    const InlineEditApplyResult result = InlineEditApplier::apply(document, m_currentSuggestion, options);
    if (!result.ok) {
        clearPreview();
        showError(i18n("Failed to apply AI inline edit: %1", sanitizedErrorDetail(result.message)));
        return;
    }

    clearPreview();
}

void InlineEditSession::dismissInlineEdit()
{
    cancelActiveRequest();
    clearPreview();
}

bool InlineEditSession::hasPreview() const
{
    return m_currentSuggestion.valid && !m_currentSuggestion.edits.isEmpty();
}

bool InlineEditSession::hasActiveRequest() const
{
    return m_activeRequestId != 0;
}

InlineEditSuggestion InlineEditSession::currentSuggestion() const
{
    return m_currentSuggestion;
}

void InlineEditSession::onDeltaReceived(quint64 requestId, const QString &delta)
{
    if (requestId != m_activeRequestId) {
        return;
    }

    m_activeResponse += delta;
}

void InlineEditSession::onRequestFinished(quint64 requestId)
{
    if (requestId != m_activeRequestId) {
        return;
    }

    m_activeRequestId = 0;
    Q_EMIT requestStateChanged(false);

    const CompletionSettings settings = m_plugin ? m_plugin->settings().validated() : CompletionSettings::defaults();
    InlineEditParserOptions options;
    options.maxNewTextChars = settings.inlineEditMaxNewTextChars;
    options.maxTotalNewTextChars = settings.inlineEditMaxTotalNewTextChars;
    options.maxEdits = settings.inlineEditMaxEdits;
    options.allowDeletion = settings.inlineEditAllowDeletion;
    options.expectedRange = m_activeTargetRange;

    KTextEditor::Document *document = m_view ? m_view->document() : nullptr;
    InlineEditSuggestion parsed = InlineEditParser::parse(m_activeResponse, document, options);
    const InlineEditValidationOptions validationOptions = inlineEditValidationOptionsFromSettings(settings, m_activeDocumentRevision);
    const InlineEditValidationResult validation = InlineEditValidator::validate(document, parsed, validationOptions);
    if (!validation.ok) {
        m_activeResponse.clear();
        m_activeTargetRange = KTextEditor::Range::invalid();
        m_activeDocumentRevision = -1;
        clearPreview();
        showError(i18n("AI inline edit response did not contain a valid edit"));
        return;
    }

    parsed.edits = validation.edits;
    parsed.source = QStringLiteral("manual");
    m_previewDocumentRevision = m_activeDocumentRevision;
    m_activeResponse.clear();
    m_activeTargetRange = KTextEditor::Range::invalid();
    m_activeDocumentRevision = -1;
    setPreview(parsed);
}

void InlineEditSession::onRequestFailed(quint64 requestId, const QString &message)
{
    if (requestId != m_activeRequestId) {
        return;
    }

    m_activeRequestId = 0;
    m_activeResponse.clear();
    m_activeTargetRange = KTextEditor::Range::invalid();
    m_activeDocumentRevision = -1;
    Q_EMIT requestStateChanged(false);
    clearPreview();
    showError(i18n("AI inline edit request failed: %1", sanitizedErrorDetail(message)));
}

void InlineEditSession::onCursorPositionChanged(KTextEditor::View *view, KTextEditor::Cursor cursor)
{
    Q_UNUSED(cursor);
    if (view == m_view) {
        dismissInlineEdit();
    }
}

void InlineEditSession::onDocumentTextChanged(KTextEditor::Document *document)
{
    if (m_ignoreDocumentChange) {
        return;
    }

    if (m_view && document == m_view->document()) {
        dismissInlineEdit();
    }
}

void InlineEditSession::onFocusOut(KTextEditor::View *view)
{
    if (view == m_view) {
        dismissInlineEdit();
    }
}

void InlineEditSession::onSelectionChanged(KTextEditor::View *view)
{
    if (view == m_view) {
        dismissInlineEdit();
    }
}

void InlineEditSession::ensureProvider(const QString &providerId)
{
    const QString normalized = providerId.trimmed().toLower();
    if (m_provider && m_providerId == normalized) {
        return;
    }

    cancelActiveRequest();
    m_provider.reset();
    m_providerId = normalized;

    if (m_providerId == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex)) {
        m_provider = std::make_unique<CopilotCodexProvider>(m_networkManager, m_copilotAuthManager);
    } else {
        m_provider = std::make_unique<OpenAICompatibleProvider>(m_networkManager);
    }

    connect(m_provider.get(), &AbstractAIProvider::deltaReceived, this, &InlineEditSession::onDeltaReceived);
    connect(m_provider.get(), &AbstractAIProvider::requestFinished, this, &InlineEditSession::onRequestFinished);
    connect(m_provider.get(), &AbstractAIProvider::requestFailed, this, &InlineEditSession::onRequestFailed);
}

void InlineEditSession::cancelActiveRequest()
{
    const quint64 requestId = m_activeRequestId;
    if (requestId != 0) {
        m_activeRequestId = 0;
        Q_EMIT requestStateChanged(false);
    }

    m_activeResponse.clear();
    m_activeTargetRange = KTextEditor::Range::invalid();
    m_activeDocumentRevision = -1;

    if (requestId != 0 && m_provider) {
        m_provider->cancel(requestId);
    }
}

void InlineEditSession::clearPreview()
{
    const bool wasActive = hasPreview();
    m_previewDocumentRevision = -1;
    m_currentSuggestion = {};
    if (m_overlay) {
        m_overlay->clear();
    }
    if (wasActive) {
        Q_EMIT previewStateChanged(false);
    }
}

void InlineEditSession::setPreview(const InlineEditSuggestion &suggestion)
{
    const bool wasActive = hasPreview();
    m_currentSuggestion = suggestion;
    if (m_overlay) {
        const CompletionSettings settings = m_plugin ? m_plugin->settings().validated() : CompletionSettings::defaults();
        m_overlay->setPreviewMaxLines(settings.inlineEditPreviewMaxLines);
        m_overlay->setSuggestion(suggestion);
    }
    if (wasActive != hasPreview()) {
        Q_EMIT previewStateChanged(hasPreview());
    }
}

InlineEditRequestContext InlineEditSession::buildRequestContext(const KTextEditor::Range &targetRange) const
{
    InlineEditRequestContext context;
    if (!m_view || !m_view->document()) {
        return context;
    }

    KTextEditor::Document *document = m_view->document();
    const CompletionSettings settings = m_plugin ? m_plugin->settings().validated() : CompletionSettings::defaults();

    context.filePath = documentDisplayPath(document);
    context.languageId = document->highlightingMode();
    context.cursor = m_view->cursorPosition();
    context.targetRange = targetRange;
    context.selectedText = m_view->selection() ? m_view->selectionText() : QString();
    context.currentTargetText = document->text(targetRange);
    context.prefixExcerpt = extractPrefixBefore(targetRange.start(), settings.inlineEditMaxPrefixChars);
    context.suffixExcerpt = extractSuffixAfter(targetRange.end(), settings.inlineEditMaxSuffixChars);
    context.contextItems = collectContextItems(context);
    return context;
}

KTextEditor::Range InlineEditSession::targetRangeForCurrentState() const
{
    if (!m_view || !m_view->document()) {
        return KTextEditor::Range::invalid();
    }

    if (m_view->selection()) {
        return m_view->selectionRange();
    }

    const int line = m_view->cursorPosition().line();
    if (line < 0 || line >= m_view->document()->lines()) {
        return KTextEditor::Range::invalid();
    }

    return KTextEditor::Range(line, 0, line, boundedSize(m_view->document()->line(line).size()));
}

QString InlineEditSession::documentDisplayPath(KTextEditor::Document *document) const
{
    if (!document) {
        return {};
    }

    if (document->url().isValid() && !document->url().isEmpty()) {
        return safeDisplayUrl(document->url());
    }

    static constexpr const char *kStableUntitledDocumentIdProperty = "_kate_ai_inline_edit_stable_untitled_document_id";
    const QVariant existing = document->property(kStableUntitledDocumentIdProperty);
    if (existing.isValid() && !existing.toString().isEmpty()) {
        return existing.toString();
    }

    const QString stableId = QStringLiteral("untitled:%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    document->setProperty(kStableUntitledDocumentIdProperty, stableId);
    return stableId;
}

QString InlineEditSession::extractPrefixBefore(const KTextEditor::Cursor &cursor, int maxChars) const
{
    if (!m_view || !m_view->document() || maxChars <= 0 || !cursor.isValid()) {
        return {};
    }

    KTextEditor::Document *document = m_view->document();
    QString prefix;
    int remaining = maxChars;

    const QString currentLine = document->line(cursor.line()).left(cursor.column());
    const QString currentPart = currentLine.size() > remaining ? currentLine.right(remaining) : currentLine;
    prefix.prepend(currentPart);
    remaining -= boundedSize(currentPart.size());

    for (int line = cursor.line() - 1; line >= 0 && remaining > 0; --line) {
        const QString text = document->line(line);
        const int need = boundedSize(text.size()) + 1;
        if (need <= remaining) {
            prefix.prepend(QLatin1Char('\n'));
            prefix.prepend(text);
            remaining -= need;
        } else if (remaining > 1) {
            prefix.prepend(QLatin1Char('\n'));
            prefix.prepend(text.right(remaining - 1));
            break;
        }
    }

    return prefix;
}

QString InlineEditSession::extractSuffixAfter(const KTextEditor::Cursor &cursor, int maxChars) const
{
    if (!m_view || !m_view->document() || maxChars <= 0 || !cursor.isValid()) {
        return {};
    }

    KTextEditor::Document *document = m_view->document();
    QString suffix;
    int remaining = maxChars;

    const QString currentLine = document->line(cursor.line()).mid(cursor.column());
    const QString currentPart = currentLine.size() > remaining ? currentLine.left(remaining) : currentLine;
    suffix.append(currentPart);
    remaining -= boundedSize(currentPart.size());

    for (int line = cursor.line() + 1; line < document->lines() && remaining > 0; ++line) {
        const QString text = document->line(line);
        const int need = boundedSize(text.size()) + 1;
        if (need <= remaining) {
            suffix.append(QLatin1Char('\n'));
            suffix.append(text);
            remaining -= need;
        } else if (remaining > 1) {
            suffix.append(QLatin1Char('\n'));
            suffix.append(text.left(remaining - 1));
            break;
        }
    }

    return suffix;
}

QVector<ContextItem> InlineEditSession::collectContextItems(const InlineEditRequestContext &context) const
{
    if (!m_view || !m_view->document() || !m_plugin) {
        return {};
    }

    const CompletionSettings settings = m_plugin->settings().validated();
    if (!settings.inlineEditUseContext || !settings.enableContextualPrompt || settings.maxContextItems <= 0 || settings.maxContextChars <= 0) {
        return {};
    }

    ContextResolveRequest request;
    request.completionId = QStringLiteral("inline-edit");
    request.opportunityId = QStringLiteral("inline-edit:%1:%2").arg(context.cursor.line()).arg(context.cursor.column());
    request.uri = context.filePath;
    request.languageId = context.languageId;
    request.version = static_cast<int>(qBound<qint64>(0LL, m_view->document()->revision(), static_cast<qint64>(std::numeric_limits<int>::max())));
    request.position = context.cursor;
    request.timeBudgetMs = 120;

    ContextProviderRegistry registry;
    registry.addProvider(std::make_unique<ProjectTraitsContextProvider>());
    registry.addProvider(std::make_unique<CurrentFileContextProvider>());
    if (settings.enableRecentEditsContext && m_recentEditsTracker) {
        registry.addProvider(std::make_unique<RecentEditsContextProvider>(m_recentEditsTracker, recentEditsOptionsFromSettings(settings)));
    }
    if (settings.enableDiagnosticsContext && m_diagnosticStore) {
        registry.addProvider(std::make_unique<DiagnosticsContextProvider>(m_diagnosticStore, diagnosticsOptionsFromSettings(settings)));
    }
    if (settings.enableRelatedFilesContext) {
        registry.addProvider(std::make_unique<RelatedFilesContextProvider>(m_view->mainWindow(), m_view, relatedFilesOptionsFromSettings(settings)));
    }
    if (settings.enableOpenTabsContext) {
        registry.addProvider(std::make_unique<OpenTabsContextProvider>(m_view->mainWindow(), m_view));
    }

    return registry.resolve(request, settings.maxContextItems);
}

bool InlineEditSession::providerAllowedForInlineEdits(const QString &providerId) const
{
    const CompletionSettings settings = m_plugin ? m_plugin->settings().validated() : CompletionSettings::defaults();
    if (providerId == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex)) {
        return settings.inlineEditCopilotExperimental;
    }
    return true;
}

bool InlineEditSession::isLocalEndpoint(const QUrl &url) const
{
    if (!url.isValid()) {
        return false;
    }

    const QString host = url.host().toLower();
    return host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1") || host == QStringLiteral("::1");
}

void InlineEditSession::showInfo(const QString &text)
{
    if (!m_view || !m_view->mainWindow()) {
        return;
    }

    QVariantMap message;
    message[QStringLiteral("text")] = text;
    message[QStringLiteral("type")] = QStringLiteral("Information");
    message[QStringLiteral("category")] = i18n("AI Inline Edit");
    message[QStringLiteral("token")] = QStringLiteral("kate-ai-inline-edit-session");
    m_view->mainWindow()->showMessage(message);
}

void InlineEditSession::showError(const QString &text)
{
    if (!m_view || !m_view->mainWindow()) {
        return;
    }

    QVariantMap message;
    message[QStringLiteral("text")] = text;
    message[QStringLiteral("type")] = QStringLiteral("Error");
    message[QStringLiteral("category")] = i18n("AI Inline Edit");
    message[QStringLiteral("token")] = QStringLiteral("kate-ai-inline-edit-session");
    m_view->mainWindow()->showMessage(message);
}

} // namespace KateAiInlineCompletion
