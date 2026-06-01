/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: EditorSession
*/

#include "session/EditorSession.h"

#include "auth/CopilotAuthManager.h"
#include "context/ContextProviderRegistry.h"
#include "context/DiagnosticsContextProvider.h"
#include "context/DiagnosticStore.h"
#include "context/OpenTabsContextProvider.h"
#include "context/ProjectTraitsContextProvider.h"
#include "context/RecentEditsContextProvider.h"
#include "context/RecentEditsTracker.h"
#include "context/RelatedFilesContextProvider.h"
#include "network/AbstractAIProvider.h"
#include "network/CopilotCodexProvider.h"
#include "network/OpenAICompatibleProvider.h"
#include "plugin/KateAiInlineCompletionPlugin.h"
#include "prompt/CopilotCodexPromptBuilder.h"
#include "prompt/PromptAssembler.h"
#include "prompt/PromptTemplate.h"
#include "render/GhostTextInlineNoteProvider.h"
#include "render/GhostTextOverlayWidget.h"
#include "session/CompletionStrategyEngine.h"
#include "session/SuggestionPostProcessor.h"
#include "settings/CompletionSettings.h"
#include "settings/KWalletSecretStore.h"

#include <KLocalizedString>

#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QClipboard>
#include <QDateTime>
#include <QEvent>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QKeySequence>
#include <QNetworkAccessManager>
#include <QUuid>
#include <QVariantMap>
#include <QWidget>

#include <limits>
#include <memory>
#include <utility>

namespace KateAiInlineCompletion
{

static bool cursorEquals(const KTextEditor::Cursor &a, const SuggestionAnchor &b)
{
    return a.line() == b.line && a.column() == b.column;
}

static QString documentDisplayPath(KTextEditor::Document *doc)
{
    if (!doc) {
        return {};
    }

    if (doc->url().isValid() && !doc->url().isEmpty()) {
        return doc->url().toDisplayString(QUrl::PreferLocalFile);
    }

    static constexpr const char *kStableUntitledDocumentIdProperty = "_kate_ai_inline_completion_stable_untitled_document_id";
    const QVariant existing = doc->property(kStableUntitledDocumentIdProperty);
    if (existing.isValid() && !existing.toString().isEmpty()) {
        return existing.toString();
    }

    const QString stableId = QStringLiteral("untitled:%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    doc->setProperty(kStableUntitledDocumentIdProperty, stableId);
    return stableId;
}

static PromptAssemblyOptions promptAssemblyOptionsFromSettings(const CompletionSettings &settings)
{
    PromptAssemblyOptions options;
    options.enabled = settings.enableContextualPrompt;
    options.maxContextItems = settings.maxContextItems;
    options.maxContextChars = settings.maxContextChars;
    return options;
}

static RecentEditsContextOptions recentEditsContextOptionsFromSettings(const CompletionSettings &settings)
{
    RecentEditsContextOptions options;
    options.enabled = settings.enableRecentEditsContext;
    options.maxEdits = settings.recentEditsMaxEdits;
    options.maxCharsPerEdit = settings.recentEditsMaxCharsPerEdit;
    options.activeDocDistanceLimitFromCursor = settings.recentEditsActiveDocDistanceLimitFromCursor;
    return options;
}

static DiagnosticsContextOptions diagnosticsContextOptionsFromSettings(const CompletionSettings &settings)
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

static RelatedFilesContextOptions relatedFilesContextOptionsFromSettings(const CompletionSettings &settings)
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

static QVector<ContextItem> collectContextItemsForRequest(KTextEditor::View *view,
                                                          KTextEditor::Document *doc,
                                                          RecentEditsTracker *recentEditsTracker,
                                                          DiagnosticStore *diagnosticStore,
                                                          const CompletionSettings &settings,
                                                          const PromptContext &promptCtx,
                                                          const KTextEditor::Cursor &cursor,
                                                          quint64 generation)
{
    if (!view || !doc || !settings.enableContextualPrompt || settings.maxContextItems <= 0 || settings.maxContextChars <= 0) {
        return {};
    }

    ContextResolveRequest contextRequest;
    contextRequest.completionId = QString::number(generation);
    contextRequest.opportunityId = QStringLiteral("view:%1:%2:%3")
                                       .arg(static_cast<qulonglong>(reinterpret_cast<quintptr>(view)))
                                       .arg(cursor.line())
                                       .arg(cursor.column());
    contextRequest.uri = promptCtx.filePath;
    contextRequest.languageId = promptCtx.language;
    contextRequest.version = static_cast<int>(qBound<qint64>(0, doc->revision(), static_cast<qint64>(std::numeric_limits<int>::max())));
    contextRequest.position = cursor;
    contextRequest.timeBudgetMs = 120;

    ContextProviderRegistry registry;
    registry.addProvider(std::make_unique<ProjectTraitsContextProvider>());
    if (settings.enableRecentEditsContext && recentEditsTracker) {
        registry.addProvider(std::make_unique<RecentEditsContextProvider>(recentEditsTracker, recentEditsContextOptionsFromSettings(settings)));
    }
    if (settings.enableDiagnosticsContext && diagnosticStore) {
        registry.addProvider(std::make_unique<DiagnosticsContextProvider>(diagnosticStore, diagnosticsContextOptionsFromSettings(settings)));
    }
    if (settings.enableRelatedFilesContext) {
        registry.addProvider(std::make_unique<RelatedFilesContextProvider>(view->mainWindow(), view, relatedFilesContextOptionsFromSettings(settings)));
    }
    if (settings.enableOpenTabsContext) {
        registry.addProvider(std::make_unique<OpenTabsContextProvider>(view->mainWindow(), view));
    }

    return registry.resolve(contextRequest, settings.maxContextItems);
}

static QString nextNonEmptyLineAfter(KTextEditor::Document *doc, int line)
{
    if (!doc) {
        return {};
    }

    for (int i = line + 1; i < doc->lines(); ++i) {
        const QString text = doc->line(i);
        if (!text.trimmed().isEmpty()) {
            return text;
        }
    }

    return {};
}

static QString safeDocumentLine(KTextEditor::Document *doc, int line)
{
    if (!doc || line < 0 || line >= doc->lines()) {
        return {};
    }

    return doc->line(line);
}

static QString leftOfCursorOnLine(KTextEditor::Document *doc, const KTextEditor::Cursor &cursor)
{
    const QString line = safeDocumentLine(doc, cursor.line());
    const int column = qBound(0, cursor.column(), line.size());
    return line.left(column);
}

static QString rightOfCursorOnLine(KTextEditor::Document *doc, const KTextEditor::Cursor &cursor)
{
    const QString line = safeDocumentLine(doc, cursor.line());
    const int column = qBound(0, cursor.column(), line.size());
    return line.mid(column);
}

static CompletionStrategyRequest completionStrategyRequestFromDocument(KTextEditor::Document *doc,
                                                                       const KTextEditor::Cursor &cursor,
                                                                       const CompletionSettings &settings,
                                                                       const QString &filePath,
                                                                       const QString &language,
                                                                       const QString &prefix,
                                                                       const QString &suffix,
                                                                       bool manualTrigger,
                                                                       bool afterPartialAccept,
                                                                       bool afterFullAccept)
{
    CompletionStrategyRequest request;
    request.providerId = settings.provider;
    request.languageId = language;
    request.filePath = filePath;
    request.prefix = prefix;
    request.suffix = suffix;
    request.currentLinePrefix = leftOfCursorOnLine(doc, cursor);
    request.currentLineSuffix = rightOfCursorOnLine(doc, cursor);
    request.previousLine = safeDocumentLine(doc, cursor.line() - 1);
    request.nextLine = safeDocumentLine(doc, cursor.line() + 1);
    request.cursor = cursor;
    request.manualTrigger = manualTrigger;
    request.afterPartialAccept = afterPartialAccept;
    request.afterFullAccept = afterFullAccept;
    return request;
}

static void appendStrategyStopSequences(QStringList &target, const QStringList &additional, int maxStopSequences = 4)
{
    for (const QString &stop : additional) {
        if (stop.isEmpty() || target.contains(stop)) {
            continue;
        }

        if (target.size() < maxStopSequences) {
            target.push_back(stop);
            continue;
        }

        const int codeFenceIndex = target.indexOf(QStringLiteral("```"));
        if (codeFenceIndex >= 0) {
            target[codeFenceIndex] = stop;
        }
    }
}

static QString textInsertedByKeyEvent(const QKeyEvent *event)
{
    if (!event) {
        return {};
    }

    if (event->matches(QKeySequence::Paste)) {
        return QGuiApplication::clipboard() ? QGuiApplication::clipboard()->text() : QString();
    }

    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return {};
    }

    return event->text();
}

static SuggestionProcessingContext suggestionProcessingContext(KTextEditor::Document *doc, const KTextEditor::Cursor &cursor)
{
    SuggestionProcessingContext ctx;
    ctx.cursor = cursor;

    if (doc && cursor.isValid() && cursor.line() >= 0 && cursor.line() < doc->lines()) {
        ctx.currentLineSuffix = doc->line(cursor.line()).mid(cursor.column());
        ctx.nextNonEmptyLine = nextNonEmptyLineAfter(doc, cursor.line());
    }

    return ctx;
}

static KTextEditor::Cursor cursorAfterInsertedText(const KTextEditor::Cursor &cursor, const QString &text)
{
    if (!cursor.isValid()) {
        return cursor;
    }

    const QString normalized = QString(text).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const int newlineCount = normalized.count(QLatin1Char('\n'));
    if (newlineCount == 0) {
        return KTextEditor::Cursor(cursor.line(), cursor.column() + normalized.size());
    }

    const int lastNewline = normalized.lastIndexOf(QLatin1Char('\n'));
    return KTextEditor::Cursor(cursor.line() + newlineCount, normalized.size() - lastNewline - 1);
}

static QString rightBounded(QString text, int maxChars)
{
    if (maxChars <= 0) {
        return {};
    }

    if (text.size() > maxChars) {
        return text.right(maxChars);
    }

    return text;
}

static QString assembledPromptFingerprint(const CompletionRequest &request, bool providerIsCopilot)
{
    if (providerIsCopilot) {
        return request.prompt + QChar(0x1f) + request.suffix + QChar(0x1f) + QString::fromUtf8(QJsonDocument(request.extra).toJson(QJsonDocument::Compact));
    }

    return request.systemPrompt + QChar(0x1f) + request.userPrompt;
}

static QString requestShapeFingerprint(const CompletionRequest &request)
{
    return QStringLiteral("max=%1\u001ftemp=%2\u001fn=%3\u001fstops=%4")
        .arg(QString::number(request.maxTokens), QString::number(request.temperature, 'g', 12), QString::number(request.n), request.stopSequences.join(QChar(0x1e)));
}

EditorSession::EditorSession(KTextEditor::View *view,
                             KateAiInlineCompletionPlugin *plugin,
                             KWalletSecretStore *secretStore,
                             QNetworkAccessManager *networkManager,
                             CopilotAuthManager *copilotAuthManager,
                             RecentEditsTracker *recentEditsTracker,
                             DiagnosticStore *diagnosticStore,
                             CompletionCache *completionCache,
                             QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_plugin(plugin)
    , m_secretStore(secretStore)
    , m_networkManager(networkManager)
    , m_copilotAuthManager(copilotAuthManager)
    , m_recentEditsTracker(recentEditsTracker)
    , m_diagnosticStore(diagnosticStore)
    , m_completionCache(completionCache)
{
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout, this, &EditorSession::onDebounceTimeout);

    m_speculativeTimer.setSingleShot(true);
    connect(&m_speculativeTimer, &QTimer::timeout, this, &EditorSession::startSpeculativeRequest);

    if (m_view) {
        m_inlineNoteProvider = std::make_unique<GhostTextInlineNoteProvider>();
        m_view->registerInlineNoteProvider(m_inlineNoteProvider.get());

        if (QWidget *w = m_view->editorWidget()) {
            m_overlay = new GhostTextOverlayWidget(m_view, w);
        }

        connect(m_view, &KTextEditor::View::textInserted, this, &EditorSession::onTextInserted);
        connect(m_view, &KTextEditor::View::cursorPositionChanged, this, &EditorSession::onCursorPositionChanged);
        connect(m_view, &KTextEditor::View::selectionChanged, this, &EditorSession::onSelectionChanged);
        connect(m_view, &KTextEditor::View::focusOut, this, &EditorSession::onFocusOut);
        connect(m_view, &KTextEditor::View::verticalScrollPositionChanged, this, [this] {
            if (m_overlay) {
                m_overlay->update();
            }
        });
        connect(m_view, &KTextEditor::View::horizontalScrollPositionChanged, this, [this] {
            if (m_overlay) {
                m_overlay->update();
            }
        });
        connect(m_view, &KTextEditor::View::displayRangeChanged, this, [this] {
            if (m_overlay) {
                m_overlay->update();
            }
        });
        connect(m_view, &KTextEditor::View::configChanged, this, [this] {
            if (m_overlay) {
                m_overlay->update();
            }
        });

        if (KTextEditor::Document *doc = m_view->document()) {
            connect(doc, &KTextEditor::Document::textChanged, this, &EditorSession::onDocumentTextChanged);
        }

        connect(m_view, &QObject::destroyed, this, [this] {
            m_view = nullptr;
            m_overlay = nullptr;
        });

        if (QWidget *w = m_view->editorWidget()) {
            w->installEventFilter(this);
        }
    }

    clearSuggestion();
}

EditorSession::~EditorSession()
{
    if (m_view) {
        if (QWidget *w = m_view->editorWidget()) {
            w->removeEventFilter(this);
        }

        if (m_inlineNoteProvider) {
            m_view->unregisterInlineNoteProvider(m_inlineNoteProvider.get());
        }
    }
}

bool EditorSession::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_view || !event) {
        return QObject::eventFilter(watched, event);
    }

    if (watched != m_view->editorWidget()) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() != QEvent::KeyPress) {
        return QObject::eventFilter(watched, event);
    }

    const auto *keyEvent = static_cast<QKeyEvent *>(event);

    const bool suggestionVisible = hasVisibleSuggestion();
    if (!suggestionVisible) {
        return QObject::eventFilter(watched, event);
    }

    if (keyEvent->key() == Qt::Key_Tab && keyEvent->modifiers() == Qt::NoModifier) {
        acceptSuggestion();
        return true;
    }

    if (keyEvent->key() == Qt::Key_Escape && keyEvent->modifiers() == Qt::NoModifier) {
        bumpGeneration();
        return true;
    }

    const Qt::KeyboardModifiers mods = keyEvent->modifiers();

    if (keyEvent->key() == Qt::Key_Right
        && (mods == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)
            || mods == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::KeypadModifier))) {
        acceptNextWord();
        return true;
    }

    if (keyEvent->key() == Qt::Key_L && mods == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)) {
        acceptNextLine();
        return true;
    }

    const CompletionSettings settings = m_plugin ? m_plugin->settings().validated() : CompletionSettings::defaults();
    const QString insertedText = textInsertedByKeyEvent(keyEvent);
    if (settings.enableTypingAsSuggested && !insertedText.isEmpty() && m_state.visibleText.startsWith(insertedText)) {
        m_pendingTypingReuseText = insertedText;
    } else {
        m_pendingTypingReuseText.clear();
        m_skippedCursorMoveBeforeTextInsert = false;
    }

    return QObject::eventFilter(watched, event);
}

void EditorSession::onTextInserted(KTextEditor::View *view, KTextEditor::Cursor position, const QString &text)
{
    Q_UNUSED(position);

    if (view != m_view) {
        return;
    }

    if (m_ignoreNextViewSignals > 0) {
        --m_ignoreNextViewSignals;
        return;
    }

    if (m_ignoreNextTextInsertedAfterTypingReuse) {
        m_ignoreNextTextInsertedAfterTypingReuse = false;
        return;
    }

    if (tryReuseVisibleSuggestionForTypedText(text)) {
        return;
    }

    bumpGeneration();
    scheduleCompletion();
}

void EditorSession::onCursorPositionChanged(KTextEditor::View *view, KTextEditor::Cursor newPosition)
{
    Q_UNUSED(newPosition);

    if (view != m_view) {
        return;
    }

    if (m_ignoreNextViewSignals > 0) {
        --m_ignoreNextViewSignals;
        return;
    }

    if (!m_pendingTypingReuseText.isEmpty() && hasVisibleSuggestion()) {
        if (tryReuseVisibleSuggestionForTypedText(m_pendingTypingReuseText)) {
            m_ignoreNextTextInsertedAfterTypingReuse = true;
        }
        return;
    }

    if (m_ignoreNextCursorMoveAfterTypingReuse && hasVisibleSuggestion()) {
        m_ignoreNextCursorMoveAfterTypingReuse = false;
        return;
    }
    m_ignoreNextCursorMoveAfterTypingReuse = false;

    bumpGeneration();
    scheduleCompletion();
}

void EditorSession::onSelectionChanged(KTextEditor::View *view)
{
    if (view != m_view) {
        return;
    }

    bumpGeneration();
}

void EditorSession::onFocusOut(KTextEditor::View *view)
{
    if (view != m_view) {
        return;
    }

    bumpGeneration();
}

void EditorSession::onDebounceTimeout()
{
    startRequest();
}

void EditorSession::onDeltaReceived(quint64 requestId, const QString &delta)
{
    if (requestId != m_activeRequestId) {
        return;
    }

    if (m_state.anchor.generation != m_generation) {
        return;
    }

    if (!syncAnchorFromTracker()) {
        clearSuggestion();
        return;
    }

    m_activeCandidateRawByIndex[0] += delta;

    QString textToProcess = m_activeCandidateRawByIndex.value(0);
    if (!m_acceptedFromSuggestion.isEmpty()) {
        const QString full = PromptTemplate::sanitizeCompletion(m_activeCandidateRawByIndex.value(0));
        if (full.startsWith(m_acceptedFromSuggestion)) {
            textToProcess = full.mid(m_acceptedFromSuggestion.size());
        } else {
            bumpGeneration();
            return;
        }
    }

    KTextEditor::Document *doc = m_view ? m_view->document() : nullptr;
    const KTextEditor::Cursor anchorCursor(m_state.anchor.line, m_state.anchor.column);
    const ProcessedSuggestion processed = SuggestionPostProcessor::process(textToProcess, suggestionProcessingContext(doc, anchorCursor));

    if (processed.valid) {
        CompletionCandidate candidate = candidateFromProcessed(textToProcess, processed, QStringLiteral("network"), 0);
        candidate.id = QStringLiteral("network:0");
        const int currentIndex = m_candidates.currentIndex();
        const QString currentId = m_candidates.current().id;
        m_candidates.addCandidate(candidate);
        Q_EMIT candidateStateChanged();
        if (currentIndex <= 0 || currentId == candidate.id) {
            (void)applyCurrentCandidate();
        }
    } else if (m_candidates.currentIndex() <= 0) {
        (void)applyProcessedSuggestion({});
        m_suggestionSource = SuggestionSource::None;
    }

    applyStateToOverlay();
}

void EditorSession::onCandidateReceived(quint64 requestId, int index, const QString &fullText)
{
    if (requestId == m_speculativeRequestId) {
        m_speculativeCandidateRawByIndex[index] = fullText;
        return;
    }

    if (requestId != m_activeRequestId) {
        return;
    }

    if (m_state.anchor.generation != m_generation) {
        return;
    }

    if (!syncAnchorFromTracker()) {
        clearSuggestion();
        return;
    }

    m_activeCandidateRawByIndex[index] = fullText;

    QString textToProcess = fullText;
    if (index == 0 && !m_acceptedFromSuggestion.isEmpty()) {
        const QString full = PromptTemplate::sanitizeCompletion(fullText);
        if (full.startsWith(m_acceptedFromSuggestion)) {
            textToProcess = full.mid(m_acceptedFromSuggestion.size());
        } else {
            return;
        }
    }

    KTextEditor::Document *doc = m_view ? m_view->document() : nullptr;
    const KTextEditor::Cursor anchorCursor(m_state.anchor.line, m_state.anchor.column);
    const ProcessedSuggestion processed = SuggestionPostProcessor::process(textToProcess, suggestionProcessingContext(doc, anchorCursor));
    if (!processed.valid) {
        return;
    }

    CompletionCandidate candidate = candidateFromProcessed(textToProcess, processed, QStringLiteral("network"), index);
    candidate.id = QStringLiteral("network:%1").arg(index);

    const QString currentId = m_candidates.current().id;
    const bool applyAfterAdd = m_candidates.isEmpty() || currentId == candidate.id;
    m_candidates.addCandidate(candidate);
    Q_EMIT candidateStateChanged();
    if (applyAfterAdd) {
        (void)applyCurrentCandidate();
        applyStateToOverlay();
    }
}

void EditorSession::onRequestFinished(quint64 requestId)
{
    if (requestId == m_speculativeRequestId) {
        finishSpeculativeRequest();
        m_speculativeRequestId = 0;
        m_speculativeCandidateRawByIndex.clear();
        m_speculativeCacheKey = CompletionCacheKey{};
        return;
    }

    if (requestId != m_activeRequestId) {
        return;
    }

    if (!syncAnchorFromTracker()) {
        clearSuggestion();
        return;
    }

    m_activeRequestId = 0;
    m_state.streaming = false;

    if (m_completionCache && m_hasActiveCacheKey && m_suggestionSource == SuggestionSource::Network && m_acceptedFromSuggestion.isEmpty() && !m_candidates.isEmpty()) {
        CompletionCacheValue value;
        value.candidates = m_candidates.candidates();
        const CompletionCandidate current = m_candidates.current();
        value.rawCompletion = current.rawCompletion;
        value.processedInsertText = current.insertText;
        value.processedDisplayText = current.displayText;
        value.suffixCoverage = current.suffixCoverage;
        value.createdAt = QDateTime::currentDateTimeUtc();
        m_completionCache->insert(m_activeCacheKey, value);
    }

    const CompletionSettings settings = m_plugin ? m_plugin->settings().validated() : CompletionSettings::defaults();
    if (m_suggestionSource == SuggestionSource::Network) {
        scheduleSpeculativeRequestIfEnabled(settings);
    }

    applyStateToOverlay();
}

void EditorSession::onRequestFailed(quint64 requestId, const QString &message)
{
    if (requestId == m_speculativeRequestId) {
        m_speculativeRequestId = 0;
        m_speculativeCandidateRawByIndex.clear();
        m_speculativeCacheKey = CompletionCacheKey{};
        return;
    }

    if (requestId != m_activeRequestId) {
        return;
    }

    m_activeRequestId = 0;
    m_state.streaming = false;

    clearSuggestion();
    showError(i18n("AI completion request failed: %1", message));
}

void EditorSession::onDocumentTextChanged(KTextEditor::Document *document)
{
    if (!m_view || document != m_view->document()) {
        return;
    }

    if (!m_state.anchorTracked || m_state.visibleText.isEmpty()) {
        return;
    }

    if (m_state.anchor.generation != m_generation) {
        return;
    }

    const CompletionSettings settings = m_plugin ? m_plugin->settings().validated() : CompletionSettings::defaults();
    if (settings.enableTypingAsSuggested && m_anchorTracker.isValid()) {
        const KTextEditor::Cursor oldAnchor(m_state.anchor.line, m_state.anchor.column);
        const KTextEditor::Cursor newAnchor = m_anchorTracker.position();
        if (oldAnchor.isValid() && newAnchor.isValid() && oldAnchor.line() == newAnchor.line() && newAnchor.column() > oldAnchor.column()) {
            const QString typedText = document->text(KTextEditor::Range(oldAnchor, newAnchor));
            if (!typedText.isEmpty() && m_state.visibleText.startsWith(typedText)) {
                (void)tryReuseVisibleSuggestionForTypedText(typedText);
                return;
            }
        }
    }

    if (!syncAnchorFromTracker()) {
        clearSuggestion();
        return;
    }

    applyStateToOverlay();
}

bool EditorSession::applyProcessedSuggestion(const ProcessedSuggestion &processed)
{
    if (processed.valid) {
        m_state.visibleText = processed.displayText;
        m_state.insertText = processed.insertText;
        m_state.replaceRange = processed.replaceRange;
        m_state.suffixCoverage = processed.suffixCoverage;
        return true;
    }

    m_state.visibleText.clear();
    m_state.insertText.clear();
    m_state.replaceRange = KTextEditor::Range::invalid();
    m_state.suffixCoverage = 0;
    return false;
}

CompletionCandidate EditorSession::candidateFromProcessed(const QString &raw, const ProcessedSuggestion &processed, const QString &source, int index) const
{
    CompletionCandidate candidate;
    if (!processed.valid) {
        return candidate;
    }

    candidate.rawCompletion = raw;
    candidate.insertText = processed.insertText;
    candidate.displayText = processed.displayText;
    candidate.replaceRange = processed.replaceRange;
    candidate.suffixCoverage = processed.suffixCoverage;
    candidate.source = source;
    candidate.id = QStringLiteral("%1:%2").arg(source, QString::number(index));
    return candidate;
}

void EditorSession::setCandidatesForCurrentAnchor(QVector<CompletionCandidate> candidates)
{
    m_candidates.setCandidates(std::move(candidates));
    (void)applyCurrentCandidate();
    Q_EMIT candidateStateChanged();
}

bool EditorSession::applyCurrentCandidate()
{
    if (m_candidates.isEmpty()) {
        (void)applyProcessedSuggestion({});
        return false;
    }

    if (!syncAnchorFromTracker()) {
        clearSuggestion();
        return false;
    }

    const CompletionCandidate candidate = m_candidates.current();
    m_state.visibleText = candidate.displayText;
    m_state.insertText = candidate.insertText;
    m_state.replaceRange = candidate.replaceRange;
    m_state.suffixCoverage = candidate.suffixCoverage;
    m_rawSuggestionText = candidate.rawCompletion;

    if (candidate.source == QStringLiteral("network")) {
        m_suggestionSource = SuggestionSource::Network;
    } else if (candidate.source == QStringLiteral("cache")) {
        m_suggestionSource = SuggestionSource::Cache;
    } else if (candidate.source == QStringLiteral("typing-as-suggested")) {
        m_suggestionSource = SuggestionSource::TypingAsSuggested;
    }

    return true;
}

void EditorSession::replaceCurrentCandidateWithProcessed(const QString &raw, const ProcessedSuggestion &processed, const QString &source)
{
    const CompletionCandidate current = m_candidates.current();
    const int index = qMax(0, m_candidates.currentIndex());
    CompletionCandidate candidate = candidateFromProcessed(raw, processed, source, index);
    if (!current.id.isEmpty()) {
        candidate.id = current.id;
    }
    setCandidatesForCurrentAnchor({candidate});
}

void EditorSession::filterAndShrinkCandidatesByPrefix(const QString &prefix)
{
    KTextEditor::Document *doc = m_view ? m_view->document() : nullptr;
    const KTextEditor::Cursor cursor = m_view ? m_view->cursorPosition() : KTextEditor::Cursor::invalid();

    QVector<CompletionCandidate> currentOut;
    QVector<CompletionCandidate> alternativesOut;
    const QString currentId = m_candidates.current().id;
    const QVector<CompletionCandidate> candidates = m_candidates.candidates();
    for (const CompletionCandidate &candidate : candidates) {
        if (!candidate.insertText.startsWith(prefix) || !candidate.displayText.startsWith(prefix)) {
            continue;
        }

        const QString remainingRaw = candidate.rawCompletion.startsWith(prefix) ? candidate.rawCompletion.mid(prefix.size()) : candidate.insertText.mid(prefix.size());
        const ProcessedSuggestion processed = SuggestionPostProcessor::process(remainingRaw, suggestionProcessingContext(doc, cursor));
        if (!processed.valid) {
            continue;
        }

        CompletionCandidate shrunk = candidateFromProcessed(remainingRaw, processed, QStringLiteral("typing-as-suggested"), currentOut.size() + alternativesOut.size());
        shrunk.id = candidate.id;
        if (!currentId.isEmpty() && candidate.id == currentId) {
            currentOut.push_back(shrunk);
        } else {
            alternativesOut.push_back(shrunk);
        }
    }

    QVector<CompletionCandidate> out = currentOut;
    out += alternativesOut;
    setCandidatesForCurrentAnchor(out);
}

void EditorSession::scheduleSpeculativeRequestIfEnabled(const CompletionSettings &settings)
{
    const CompletionSettings validated = settings.validated();
    if (!validated.enableSpeculativeRequests || !validated.enableCompletionCache || !m_completionCache || !m_provider || !m_view || !m_view->document()) {
        return;
    }

    if (!hasVisibleSuggestion() || m_candidates.isEmpty() || m_activeRequestId != 0 || m_speculativeRequestId != 0) {
        return;
    }

    m_speculativeTimer.start(validated.speculativeRequestDelayMs);
}

void EditorSession::startSpeculativeRequest()
{
    if (!m_view || !m_view->document() || !m_plugin || !m_completionCache || !m_provider || m_activeRequestId != 0 || m_speculativeRequestId != 0) {
        return;
    }

    const CompletionSettings settings = m_plugin->settings().validated();
    if (!settings.enabled || !settings.enableSpeculativeRequests || !settings.enableCompletionCache || !hasVisibleSuggestion() || m_candidates.isEmpty()) {
        return;
    }

    KTextEditor::Document *doc = m_view->document();
    const KTextEditor::Cursor cursor = m_view->cursorPosition();
    if (!cursorEquals(cursor, m_state.anchor)) {
        return;
    }

    const CompletionCandidate current = m_candidates.current();
    if (current.insertText.isEmpty()) {
        return;
    }

    const QUrl endpoint = settings.endpoint;
    QString apiKey;
    if (m_secretStore) {
        apiKey = m_secretStore->readApiKey();
    }

    const bool providerIsOllama = settings.provider == QString::fromLatin1(CompletionSettings::kProviderOllama);
    const bool providerIsCopilot = settings.provider == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex);
    const bool apiKeyRequired = !providerIsOllama && !providerIsCopilot && !isLocalEndpoint(endpoint);
    if (apiKeyRequired && apiKey.trimmed().isEmpty()) {
        return;
    }

    if (providerIsCopilot && (!m_secretStore || !m_secretStore->hasGitHubOAuthToken())) {
        return;
    }

    const QString prefix = rightBounded(extractPrefix(settings.maxPrefixChars) + current.insertText, settings.maxPrefixChars);
    const KTextEditor::Cursor suffixCursor = current.replaceRange.isValid() ? current.replaceRange.end() : cursor;
    const QString suffix = extractSuffixFromCursor(suffixCursor, settings.maxSuffixChars);
    const QString language = doc->highlightingMode();
    const QString filePath = documentDisplayPath(doc);
    const KTextEditor::Cursor virtualCursor = cursorAfterInsertedText(cursor, current.insertText);

    PromptContext promptCtx;
    promptCtx.filePath = filePath;
    promptCtx.language = language;
    promptCtx.cursorLine1 = virtualCursor.line() + 1;
    promptCtx.cursorColumn1 = virtualCursor.column() + 1;
    promptCtx.prefix = prefix;
    promptCtx.suffix = suffix;

    CompletionStrategy strategy = CompletionStrategyEngine::choose(completionStrategyRequestFromDocument(doc,
                                                                                                         cursor,
                                                                                                         settings,
                                                                                                         filePath,
                                                                                                         language,
                                                                                                         prefix,
                                                                                                         suffix,
                                                                                                         false,
                                                                                                         false,
                                                                                                         true),
                                                                   settings);
    strategy.maxTokens = qMin(strategy.maxTokens, settings.speculativeRequestMaxTokens);

    const QVector<ContextItem> contextItems = collectContextItemsForRequest(m_view, doc, m_recentEditsTracker, m_diagnosticStore, settings, promptCtx, cursor, m_generation);
    const PromptAssemblyOptions assemblyOptions = promptAssemblyOptionsFromSettings(settings);

    CompletionRequest request;
    request.endpoint = endpoint;
    request.model = settings.model;
    request.maxTokens = strategy.maxTokens;
    request.temperature = strategy.temperature;
    request.n = 1;

    if (providerIsCopilot) {
        CopilotCodexPrompt built = CopilotCodexPromptBuilder::build(promptCtx, doc, cursor);
        built.prompt = PromptAssembler::renderCopilotContextPrefix(promptCtx, contextItems, assemblyOptions) + built.prompt;

        request.prompt = built.prompt;
        request.suffix = built.suffix;
        request.nwo = settings.copilotNwo;
        request.stopSequences = {QStringLiteral("```")};
        appendStrategyStopSequences(request.stopSequences, strategy.stopSequences);

        QJsonObject extra;
        extra[QStringLiteral("language")] = built.languageId;
        extra[QStringLiteral("next_indent")] = built.nextIndent;
        extra[QStringLiteral("trim_by_indentation")] = true;
        request.extra = extra;
    } else {
        const BuiltPrompt built = PromptAssembler::build(settings.promptTemplate, promptCtx, contextItems, assemblyOptions);

        request.apiKey = apiKey;
        request.systemPrompt = built.systemPrompt;
        request.userPrompt = built.userPrompt;
        request.stopSequences = built.stopSequences;
        appendStrategyStopSequences(request.stopSequences, strategy.stopSequences);
    }

    m_speculativeCacheKey = CompletionCache::makeKey(settings,
                                                     strategy,
                                                     promptCtx,
                                                     prefix,
                                                     suffix,
                                                     request.n,
                                                     assembledPromptFingerprint(request, providerIsCopilot),
                                                     requestShapeFingerprint(request));
    if (m_completionCache->lookupExact(m_speculativeCacheKey).has_value()) {
        return;
    }

    m_speculativeCandidateRawByIndex.clear();
    m_speculativeRequestId = m_provider->start(request);
}

void EditorSession::cancelSpeculativeRequest()
{
    m_speculativeTimer.stop();
    if (m_speculativeRequestId != 0 && m_provider) {
        m_provider->cancel(m_speculativeRequestId);
    }
    m_speculativeRequestId = 0;
    m_speculativeCandidateRawByIndex.clear();
    m_speculativeCacheKey = CompletionCacheKey{};
}

void EditorSession::finishSpeculativeRequest()
{
    if (!m_completionCache || m_speculativeCandidateRawByIndex.isEmpty()) {
        return;
    }

    CompletionCacheValue value;
    QList<int> indexes = m_speculativeCandidateRawByIndex.keys();
    std::sort(indexes.begin(), indexes.end());
    for (int index : indexes) {
        const QString raw = PromptTemplate::sanitizeCompletion(m_speculativeCandidateRawByIndex.value(index));
        if (raw.trimmed().isEmpty()) {
            continue;
        }

        CompletionCandidate candidate;
        candidate.rawCompletion = raw;
        candidate.insertText = raw;
        candidate.displayText = raw;
        candidate.source = QStringLiteral("speculative");
        candidate.id = QStringLiteral("speculative:%1").arg(index);
        value.candidates.push_back(candidate);
    }

    if (!value.candidates.isEmpty()) {
        value.createdAt = QDateTime::currentDateTimeUtc();
        m_completionCache->insert(m_speculativeCacheKey, value);
    }
}

bool EditorSession::tryReuseVisibleSuggestionForTypedText(const QString &text)
{
    const QString typedText = text.isEmpty() ? m_pendingTypingReuseText : text;

    if (!m_view || !m_plugin) {
        m_pendingTypingReuseText.clear();
        m_skippedCursorMoveBeforeTextInsert = false;
        return false;
    }

    const CompletionSettings settings = m_plugin->settings().validated();
    if (!settings.enableTypingAsSuggested) {
        m_pendingTypingReuseText.clear();
        m_skippedCursorMoveBeforeTextInsert = false;
        return false;
    }

    if (typedText.isEmpty() || !hasVisibleSuggestion()) {
        m_pendingTypingReuseText.clear();
        m_skippedCursorMoveBeforeTextInsert = false;
        return false;
    }

    if (!m_state.visibleText.startsWith(typedText)) {
        m_pendingTypingReuseText.clear();
        m_skippedCursorMoveBeforeTextInsert = false;
        return false;
    }

    if (m_activeRequestId == 0) {
        m_state.streaming = false;
    }

    m_acceptedFromSuggestion += typedText;
    m_typedPrefixFromSuggestion += typedText;

    if (!syncAnchorFromTracker()) {
        clearSuggestion();
        return true;
    }

    filterAndShrinkCandidatesByPrefix(typedText);
    m_suggestionSource = hasVisibleSuggestion() ? SuggestionSource::TypingAsSuggested : SuggestionSource::None;
    m_ignoreNextCursorMoveAfterTypingReuse = !m_skippedCursorMoveBeforeTextInsert;
    m_skippedCursorMoveBeforeTextInsert = false;
    m_pendingTypingReuseText.clear();

    applyStateToOverlay();
    return true;
}

void EditorSession::bumpGeneration()
{
    ++m_generation;
    m_debounceTimer.stop();
    cancelSpeculativeRequest();
    m_nextRequestManualTrigger = false;
    m_nextRequestAfterPartialAccept = false;
    m_nextRequestAfterFullAccept = false;
    m_ignoreNextCursorMoveAfterTypingReuse = false;

    if (m_activeRequestId != 0 && m_provider) {
        m_provider->cancel(m_activeRequestId);
        m_activeRequestId = 0;
    }

    clearSuggestion();
}

void EditorSession::scheduleCompletion()
{
    if (!m_view || !m_plugin) {
        return;
    }

    const CompletionSettings settings = m_plugin->settings().validated();
    if (!settings.enabled) {
        return;
    }

    if (m_view->selection()) {
        return;
    }

    if (settings.suppressWhenCompletionPopupVisible && m_view->isCompletionActive()) {
        setSuppressed(true);
        return;
    }

    setSuppressed(false);

    m_debounceTimer.start(settings.debounceMs);
}

void EditorSession::startRequest()
{
    if (!m_view || !m_plugin) {
        return;
    }

    KTextEditor::Document *doc = m_view->document();
    if (!doc) {
        return;
    }

    const bool manualTrigger = std::exchange(m_nextRequestManualTrigger, false);
    const bool afterPartialAccept = std::exchange(m_nextRequestAfterPartialAccept, false);
    const bool afterFullAccept = std::exchange(m_nextRequestAfterFullAccept, false);

    const CompletionSettings settings = m_plugin->settings().validated();
    if (!settings.enabled) {
        return;
    }

    if (m_view->selection()) {
        return;
    }

    if (settings.suppressWhenCompletionPopupVisible && m_view->isCompletionActive()) {
        setSuppressed(true);
        return;
    }

    setSuppressed(false);

    ensureProvider(settings.provider);
    if (!m_provider) {
        showError(i18n("AI provider is unavailable"));
        return;
    }

    const KTextEditor::Cursor cursor = m_view->cursorPosition();

    const QString prefix = extractPrefix(settings.maxPrefixChars);
    const QString suffix = extractSuffix(settings.maxSuffixChars);
    const QString language = doc->highlightingMode();

    const QUrl endpoint = settings.endpoint;

    QString apiKey;
    if (m_secretStore) {
        apiKey = m_secretStore->readApiKey();
    }

    const bool providerIsOllama = settings.provider == QString::fromLatin1(CompletionSettings::kProviderOllama);
    const bool providerIsCopilot = settings.provider == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex);

    const bool apiKeyRequired = !providerIsOllama && !providerIsCopilot && !isLocalEndpoint(endpoint);
    if (apiKeyRequired && apiKey.trimmed().isEmpty()) {
        showError(i18n("AI completion requires an API key for endpoint: %1", endpoint.toString()));
        return;
    }

    if (providerIsCopilot && (!m_secretStore || !m_secretStore->hasGitHubOAuthToken())) {
        showError(i18n("GitHub Copilot requires OAuth sign-in. Open the plugin settings and sign in."));
        return;
    }

    const QString filePath = documentDisplayPath(doc);

    PromptContext promptCtx;
    promptCtx.filePath = filePath;
    promptCtx.language = language;
    promptCtx.cursorLine1 = cursor.line() + 1;
    promptCtx.cursorColumn1 = cursor.column() + 1;
    promptCtx.prefix = prefix;
    promptCtx.suffix = suffix;

    const CompletionStrategy strategy = CompletionStrategyEngine::choose(completionStrategyRequestFromDocument(doc,
                                                                                                               cursor,
                                                                                                               settings,
                                                                                                               filePath,
                                                                                                               language,
                                                                                                               prefix,
                                                                                                               suffix,
                                                                                                               manualTrigger,
                                                                                                               afterPartialAccept,
                                                                                                               afterFullAccept),
                                                                         settings);
    const QVector<ContextItem> contextItems = collectContextItemsForRequest(m_view, doc, m_recentEditsTracker, m_diagnosticStore, settings, promptCtx, cursor, m_generation);
    const PromptAssemblyOptions assemblyOptions = promptAssemblyOptionsFromSettings(settings);

    CompletionRequest request;
    request.endpoint = endpoint;
    request.model = settings.model;
    request.maxTokens = strategy.maxTokens;
    request.temperature = strategy.temperature;
    request.n = (manualTrigger && settings.enableCandidateCycling) ? settings.manualCandidateCount : 1;

    if (providerIsCopilot) {
        CopilotCodexPrompt built = CopilotCodexPromptBuilder::build(promptCtx, doc, cursor);
        built.prompt = PromptAssembler::renderCopilotContextPrefix(promptCtx, contextItems, assemblyOptions) + built.prompt;

        request.prompt = built.prompt;
        request.suffix = built.suffix;
        request.nwo = settings.copilotNwo;
        request.stopSequences = {QStringLiteral("```")};
        appendStrategyStopSequences(request.stopSequences, strategy.stopSequences);

        QJsonObject extra;
        extra[QStringLiteral("language")] = built.languageId;
        extra[QStringLiteral("next_indent")] = built.nextIndent;
        extra[QStringLiteral("trim_by_indentation")] = true;
        request.extra = extra;
    } else {
        const BuiltPrompt built = PromptAssembler::build(settings.promptTemplate, promptCtx, contextItems, assemblyOptions);

        request.apiKey = apiKey;
        request.systemPrompt = built.systemPrompt;
        request.userPrompt = built.userPrompt;
        request.stopSequences = built.stopSequences;
        appendStrategyStopSequences(request.stopSequences, strategy.stopSequences);
    }

    const bool cacheEnabled = settings.enableCompletionCache && m_completionCache;
    const CompletionCacheKey cacheKey = CompletionCache::makeKey(settings,
                                                                 strategy,
                                                                 promptCtx,
                                                                 prefix,
                                                                 suffix,
                                                                 request.n,
                                                                 assembledPromptFingerprint(request, providerIsCopilot),
                                                                 requestShapeFingerprint(request));

    if (cacheEnabled) {
        const std::optional<CompletionCacheValue> cached = m_completionCache->lookupExact(cacheKey);
        if (cached.has_value()) {
            QVector<CompletionCandidate> candidates;
            const QVector<CompletionCandidate> cachedCandidates = cached->candidates.isEmpty()
                ? QVector<CompletionCandidate>{candidateFromProcessed(cached->rawCompletion,
                                                                       SuggestionPostProcessor::process(cached->rawCompletion, suggestionProcessingContext(doc, cursor)),
                                                                       QStringLiteral("cache"),
                                                                       0)}
                : cached->candidates;

            for (int i = 0; i < cachedCandidates.size(); ++i) {
                const CompletionCandidate &cachedCandidate = cachedCandidates.at(i);
                const ProcessedSuggestion processed = SuggestionPostProcessor::process(cachedCandidate.rawCompletion, suggestionProcessingContext(doc, cursor));
                if (processed.valid) {
                    CompletionCandidate candidate = candidateFromProcessed(cachedCandidate.rawCompletion, processed, QStringLiteral("cache"), i);
                    candidate.id = QStringLiteral("cache:%1").arg(i);
                    candidates.push_back(candidate);
                }
            }

            if (!candidates.isEmpty()) {
                if (m_activeRequestId != 0) {
                    m_provider->cancel(m_activeRequestId);
                    m_activeRequestId = 0;
                }

                m_anchorTracker.attach(doc, cursor);
                m_state.anchor.generation = m_generation;
                m_state.anchorTracked = m_anchorTracker.isValid();
                (void)syncAnchorFromTracker();

                m_acceptedFromSuggestion.clear();
                m_typedPrefixFromSuggestion.clear();
                m_activeCandidateRawByIndex.clear();
                m_activeCacheKey = cacheKey;
                m_hasActiveCacheKey = true;
                m_state.streaming = false;
                m_state.suppressed = false;
                setCandidatesForCurrentAnchor(candidates);
                m_suggestionSource = SuggestionSource::Cache;

                applyStateToOverlay();
                return;
            }
        }
    }

    if (m_activeRequestId != 0) {
        m_provider->cancel(m_activeRequestId);
        m_activeRequestId = 0;
    }

    m_anchorTracker.attach(doc, cursor);

    m_state.anchor.generation = m_generation;
    m_state.anchorTracked = m_anchorTracker.isValid();
    (void)syncAnchorFromTracker();

    m_state.visibleText.clear();
    m_state.insertText.clear();
    m_state.replaceRange = KTextEditor::Range::invalid();
    m_state.suffixCoverage = 0;
    const bool hadCandidates = !m_candidates.isEmpty();
    m_candidates.clear();
    if (hadCandidates) {
        Q_EMIT candidateStateChanged();
    }
    m_activeCandidateRawByIndex.clear();
    m_rawSuggestionText.clear();
    m_acceptedFromSuggestion.clear();
    m_typedPrefixFromSuggestion.clear();
    m_activeCacheKey = cacheKey;
    m_hasActiveCacheKey = cacheEnabled;
    m_suggestionSource = SuggestionSource::Network;
    m_state.streaming = true;
    m_state.suppressed = false;

    applyStateToOverlay();

    m_activeRequestId = m_provider->start(request);
}

void EditorSession::clearSuggestion()
{
    m_anchorTracker.clear();

    GhostTextState cleared;
    cleared.anchor.generation = m_generation;
    cleared.anchorTracked = false;
    m_state = cleared;
    const bool hadCandidates = !m_candidates.isEmpty();
    m_candidates.clear();
    m_activeCandidateRawByIndex.clear();
    m_rawSuggestionText.clear();
    m_acceptedFromSuggestion.clear();
    m_typedPrefixFromSuggestion.clear();
    m_pendingTypingReuseText.clear();
    m_skippedCursorMoveBeforeTextInsert = false;
    m_ignoreNextTextInsertedAfterTypingReuse = false;
    m_ignoreNextCursorMoveAfterTypingReuse = false;
    m_hasActiveCacheKey = false;
    m_suggestionSource = SuggestionSource::None;
    if (hadCandidates) {
        Q_EMIT candidateStateChanged();
    }

    applyStateToOverlay();
}

bool EditorSession::hasVisibleSuggestion() const
{
    return m_state.anchorTracked && !m_state.suppressed && !m_state.visibleText.isEmpty() && m_state.anchor.generation == m_generation;
}

void EditorSession::acceptFullSuggestion()
{
    acceptSuggestion();
}

void EditorSession::dismissSuggestion()
{
    bumpGeneration();
}

void EditorSession::triggerSuggestion()
{
    const bool afterPartialAccept = m_nextRequestAfterPartialAccept;
    const bool afterFullAccept = m_nextRequestAfterFullAccept;
    bumpGeneration();
    m_nextRequestManualTrigger = true;
    m_nextRequestAfterPartialAccept = afterPartialAccept;
    m_nextRequestAfterFullAccept = afterFullAccept;
    startRequest();
}

void EditorSession::nextCandidate()
{
    if (m_candidates.size() <= 1) {
        triggerSuggestion();
        return;
    }

    if (!syncAnchorFromTracker()) {
        bumpGeneration();
        return;
    }

    if (m_candidates.next()) {
        (void)applyCurrentCandidate();
        applyStateToOverlay();
    }
}

void EditorSession::previousCandidate()
{
    if (m_candidates.size() <= 1) {
        triggerSuggestion();
        return;
    }

    if (!syncAnchorFromTracker()) {
        bumpGeneration();
        return;
    }

    if (m_candidates.previous()) {
        (void)applyCurrentCandidate();
        applyStateToOverlay();
    }
}

int EditorSession::candidateCount() const
{
    return m_candidates.size();
}

void EditorSession::acceptSuggestion()
{
    if (!m_view) {
        return;
    }

    if (m_state.visibleText.isEmpty()) {
        return;
    }

    if (!syncAnchorFromTracker()) {
        bumpGeneration();
        return;
    }

    KTextEditor::Document *doc = m_view->document();
    if (!doc) {
        return;
    }

    const KTextEditor::Cursor cursor = m_view->cursorPosition();
    if (!cursorEquals(cursor, m_state.anchor)) {
        bumpGeneration();
        return;
    }

    const QString toInsert = m_state.insertText.isEmpty() ? m_state.visibleText : m_state.insertText;
    const KTextEditor::Range replaceRange = m_state.replaceRange.isValid() ? m_state.replaceRange : KTextEditor::Range(cursor, cursor);

    if (replaceRange.start() != cursor || replaceRange.end().line() != cursor.line() || replaceRange.end().column() < cursor.column()) {
        bumpGeneration();
        return;
    }

    bumpGeneration();
    m_ignoreNextViewSignals = 4;
    KTextEditor::Document::EditingTransaction transaction(doc);
    const bool ok = doc->replaceText(replaceRange, toInsert);
    if (!ok) {
        m_ignoreNextViewSignals = 0;
        showError(i18n("Failed to insert AI completion into document"));
    } else {
        m_view->setCursorPosition(cursorAfterInsertedText(cursor, toInsert));
        m_nextRequestAfterFullAccept = true;
        m_nextRequestAfterPartialAccept = false;
    }
}

void EditorSession::acceptNextWord()
{
    acceptPartial(takeNextWordChunk(m_state.visibleText));
}

void EditorSession::acceptNextLine()
{
    acceptPartial(takeNextLineChunk(m_state.visibleText));
}

void EditorSession::acceptPartial(const QString &chunk)
{
    if (!m_view) {
        return;
    }

    if (chunk.isEmpty()) {
        return;
    }

    if (m_state.visibleText.isEmpty()) {
        return;
    }

    if (!syncAnchorFromTracker()) {
        bumpGeneration();
        return;
    }

    KTextEditor::Document *doc = m_view->document();
    if (!doc) {
        return;
    }

    const KTextEditor::Cursor cursor = m_view->cursorPosition();
    if (!cursorEquals(cursor, m_state.anchor)) {
        bumpGeneration();
        return;
    }

    if (!m_state.visibleText.startsWith(chunk)) {
        bumpGeneration();
        return;
    }

    m_acceptedFromSuggestion += chunk;
    const QString remaining = m_state.visibleText.mid(chunk.size());

    m_ignoreNextViewSignals = qMax(m_ignoreNextViewSignals, 4);
    KTextEditor::Document::EditingTransaction transaction(doc);
    const bool ok = m_view->insertText(chunk);
    if (!ok) {
        m_ignoreNextViewSignals = 0;
        showError(i18n("Failed to insert AI completion into document"));
        bumpGeneration();
        return;
    }

    m_nextRequestAfterPartialAccept = true;
    m_nextRequestAfterFullAccept = false;

    if (!syncAnchorFromTracker()) {
        bumpGeneration();
        return;
    }

    Q_UNUSED(remaining);
    filterAndShrinkCandidatesByPrefix(chunk);
    m_suggestionSource = hasVisibleSuggestion() ? SuggestionSource::TypingAsSuggested : SuggestionSource::None;

    applyStateToOverlay();
}

QString EditorSession::takeNextWordChunk(const QString &remaining) const
{
    if (remaining.isEmpty()) {
        return {};
    }

    const auto isWordChar = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_');
    };

    int i = 0;

    const QChar first = remaining.at(0);
    if (first == QLatin1Char('\n')) {
        return QString(first);
    }

    if (first.isSpace()) {
        while (i < remaining.size() && remaining.at(i).isSpace() && remaining.at(i) != QLatin1Char('\n')) {
            ++i;
        }
        return remaining.left(i);
    }

    if (isWordChar(first)) {
        while (i < remaining.size() && isWordChar(remaining.at(i))) {
            ++i;
        }
        while (i < remaining.size() && remaining.at(i).isSpace() && remaining.at(i) != QLatin1Char('\n')) {
            ++i;
        }
        return remaining.left(i);
    }

    i = 1;
    while (i < remaining.size() && remaining.at(i).isSpace() && remaining.at(i) != QLatin1Char('\n')) {
        ++i;
    }
    return remaining.left(i);
}

QString EditorSession::takeNextLineChunk(const QString &remaining) const
{
    if (remaining.isEmpty()) {
        return {};
    }

    const int idx = remaining.indexOf(QLatin1Char('\n'));
    if (idx < 0) {
        return remaining;
    }

    return remaining.left(idx + 1);
}

void EditorSession::setSuppressed(bool suppressed)
{
    if (m_state.suppressed == suppressed) {
        return;
    }

    m_state.suppressed = suppressed;
    applyStateToOverlay();
}

bool EditorSession::syncAnchorFromTracker()
{
    if (!m_state.anchorTracked) {
        return false;
    }

    if (!m_anchorTracker.isValid()) {
        m_state.anchorTracked = false;
        m_state.anchor.line = -1;
        m_state.anchor.column = -1;
        return false;
    }

    const KTextEditor::Cursor cursor = m_anchorTracker.position();
    m_state.anchor.line = cursor.line();
    m_state.anchor.column = cursor.column();
    return true;
}

bool EditorSession::shouldRenderInlineNote(const GhostTextState &state) const
{
    return state.anchorTracked && !state.suppressed && !state.visibleText.isEmpty() && !state.visibleText.contains(QLatin1Char('\n'));
}

bool EditorSession::shouldRenderOverlay(const GhostTextState &state) const
{
    return state.anchorTracked && !state.suppressed && state.visibleText.contains(QLatin1Char('\n'));
}

GhostTextState EditorSession::clearedRenderState() const
{
    GhostTextState cleared;
    cleared.anchor.generation = m_generation;
    return cleared;
}

void EditorSession::applyStateToOverlay()
{
    if (m_state.anchorTracked) {
        (void)syncAnchorFromTracker();
    }

    const GhostTextState cleared = clearedRenderState();
    const bool inlineNote = shouldRenderInlineNote(m_state);
    const bool overlay = shouldRenderOverlay(m_state);

    if (m_inlineNoteProvider) {
        m_inlineNoteProvider->setState(inlineNote ? m_state : cleared);
    }

    if (m_overlay) {
        m_overlay->setState(overlay ? m_state : cleared);
    }

    const bool visible = hasVisibleSuggestion();
    if (visible != m_lastSuggestionVisible) {
        m_lastSuggestionVisible = visible;
        Q_EMIT suggestionVisibilityChanged(visible);
    }
}

void EditorSession::showInfo(const QString &text)
{
    if (!m_view) {
        return;
    }

    KTextEditor::MainWindow *mw = m_view->mainWindow();
    if (!mw) {
        return;
    }

    QVariantMap msg;
    msg[QStringLiteral("text")] = text;
    msg[QStringLiteral("type")] = QStringLiteral("Info");
    msg[QStringLiteral("category")] = i18n("AI");
    msg[QStringLiteral("token")] = QStringLiteral("kate-ai-inline-completion-session");
    mw->showMessage(msg);
}

void EditorSession::showError(const QString &text)
{
    if (!m_view) {
        return;
    }

    KTextEditor::MainWindow *mw = m_view->mainWindow();
    if (!mw) {
        return;
    }

    QVariantMap msg;
    msg[QStringLiteral("text")] = text;
    msg[QStringLiteral("type")] = QStringLiteral("Error");
    msg[QStringLiteral("category")] = i18n("AI");
    msg[QStringLiteral("token")] = QStringLiteral("kate-ai-inline-completion-session");
    mw->showMessage(msg);
}

void EditorSession::ensureProvider(const QString &providerId)
{
    const QString normalized = providerId.trimmed().toLower();

    if (m_provider && normalized == m_providerId) {
        return;
    }

    cancelSpeculativeRequest();
    m_provider.reset();
    m_providerId = normalized;

    if (m_providerId == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex)) {
        m_provider = std::make_unique<CopilotCodexProvider>(m_networkManager, m_copilotAuthManager);
    } else {
        m_provider = std::make_unique<OpenAICompatibleProvider>(m_networkManager);
    }

    connect(m_provider.get(), &AbstractAIProvider::deltaReceived, this, &EditorSession::onDeltaReceived);
    connect(m_provider.get(), &AbstractAIProvider::candidateReceived, this, &EditorSession::onCandidateReceived);
    connect(m_provider.get(), &AbstractAIProvider::requestFinished, this, &EditorSession::onRequestFinished);
    connect(m_provider.get(), &AbstractAIProvider::requestFailed, this, &EditorSession::onRequestFailed);
}

QString EditorSession::extractPrefix(int maxChars) const
{
    if (!m_view || !m_view->document() || maxChars <= 0) {
        return {};
    }

    KTextEditor::Document *doc = m_view->document();
    const KTextEditor::Cursor cursor = m_view->cursorPosition();

    QString prefix;
    int remaining = maxChars;

    const QString currentLine = doc->line(cursor.line()).left(cursor.column());
    const QString currentPart = (currentLine.size() > remaining) ? currentLine.right(remaining) : currentLine;

    prefix.prepend(currentPart);
    remaining -= currentPart.size();

    for (int line = cursor.line() - 1; line >= 0 && remaining > 0; --line) {
        const QString l = doc->line(line);

        const int need = l.size() + 1;
        if (need <= remaining) {
            prefix.prepend(QLatin1Char('\n'));
            prefix.prepend(l);
            remaining -= need;
            continue;
        }

        if (remaining <= 1) {
            break;
        }

        const int take = remaining - 1;
        prefix.prepend(QLatin1Char('\n'));
        prefix.prepend(l.right(take));
        remaining = 0;
        break;
    }

    return prefix;
}

QString EditorSession::extractSuffix(int maxChars) const
{
    const KTextEditor::Cursor cursor = m_view ? m_view->cursorPosition() : KTextEditor::Cursor::invalid();
    return extractSuffixFromCursor(cursor, maxChars);
}

QString EditorSession::extractSuffixFromCursor(const KTextEditor::Cursor &cursor, int maxChars) const
{
    if (!m_view || !m_view->document() || maxChars <= 0 || !cursor.isValid()) {
        return {};
    }

    KTextEditor::Document *doc = m_view->document();
    if (cursor.line() < 0 || cursor.line() >= doc->lines()) {
        return {};
    }

    QString suffix;
    int remaining = maxChars;

    const QString currentLine = doc->line(cursor.line()).mid(cursor.column());
    const QString currentPart = (currentLine.size() > remaining) ? currentLine.left(remaining) : currentLine;

    suffix.append(currentPart);
    remaining -= currentPart.size();

    for (int line = cursor.line() + 1; line < doc->lines() && remaining > 0; ++line) {
        const QString l = doc->line(line);

        const int need = l.size() + 1;
        if (need <= remaining) {
            suffix.append(QLatin1Char('\n'));
            suffix.append(l);
            remaining -= need;
            continue;
        }

        if (remaining <= 1) {
            break;
        }

        const int take = remaining - 1;
        suffix.append(QLatin1Char('\n'));
        suffix.append(l.left(take));
        remaining = 0;
        break;
    }

    return suffix;
}

bool EditorSession::isLocalEndpoint(const QUrl &url) const
{
    if (!url.isValid()) {
        return false;
    }

    const QString host = url.host().toLower();
    return host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1") || host == QStringLiteral("::1");
}

} // namespace KateAiInlineCompletion
