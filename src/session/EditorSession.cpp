/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: EditorSession
*/

#include "session/EditorSession.h"

#include "auth/CopilotAuthManager.h"
#include "context/ContextProviderRegistry.h"
#include "context/CurrentFileContextProvider.h"
#include "context/OpenTabsContextProvider.h"
#include "context/ProjectTraitsContextProvider.h"
#include "context/RecentEditsContextProvider.h"
#include "context/RecentEditsTracker.h"
#include "network/AbstractAIProvider.h"
#include "network/CopilotCodexProvider.h"
#include "network/OpenAICompatibleProvider.h"
#include "plugin/KateAiInlineCompletionPlugin.h"
#include "prompt/CopilotCodexPromptBuilder.h"
#include "prompt/PromptAssembler.h"
#include "prompt/PromptTemplate.h"
#include "render/GhostTextInlineNoteProvider.h"
#include "render/GhostTextOverlayWidget.h"
#include "session/SuggestionPostProcessor.h"
#include "settings/CompletionSettings.h"
#include "settings/KWalletSecretStore.h"

#include <KLocalizedString>

#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QEvent>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QVariantMap>
#include <QWidget>

#include <limits>
#include <memory>

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

    return doc->documentName();
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

static QVector<ContextItem> collectContextItemsForRequest(KTextEditor::View *view,
                                                          KTextEditor::Document *doc,
                                                          RecentEditsTracker *recentEditsTracker,
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
    registry.addProvider(std::make_unique<CurrentFileContextProvider>());
    registry.addProvider(std::make_unique<ProjectTraitsContextProvider>());
    if (settings.enableRecentEditsContext && recentEditsTracker) {
        registry.addProvider(std::make_unique<RecentEditsContextProvider>(recentEditsTracker, recentEditsContextOptionsFromSettings(settings)));
    }
    registry.addProvider(std::make_unique<OpenTabsContextProvider>(view->mainWindow(), view));

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

EditorSession::EditorSession(KTextEditor::View *view,
                             KateAiInlineCompletionPlugin *plugin,
                             KWalletSecretStore *secretStore,
                             QNetworkAccessManager *networkManager,
                             CopilotAuthManager *copilotAuthManager,
                             RecentEditsTracker *recentEditsTracker,
                             QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_plugin(plugin)
    , m_secretStore(secretStore)
    , m_networkManager(networkManager)
    , m_copilotAuthManager(copilotAuthManager)
    , m_recentEditsTracker(recentEditsTracker)
{
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout, this, &EditorSession::onDebounceTimeout);

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

    return QObject::eventFilter(watched, event);
}

void EditorSession::onTextInserted(KTextEditor::View *view, KTextEditor::Cursor position, const QString &text)
{
    Q_UNUSED(position);
    Q_UNUSED(text);

    if (view != m_view) {
        return;
    }

    if (m_ignoreNextViewSignals > 0) {
        --m_ignoreNextViewSignals;
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

    m_rawSuggestionText += delta;

    QString textToProcess = m_rawSuggestionText;
    if (!m_acceptedFromSuggestion.isEmpty()) {
        const QString full = PromptTemplate::sanitizeCompletion(m_rawSuggestionText);
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
        m_state.visibleText = processed.displayText;
        m_state.insertText = processed.insertText;
        m_state.replaceRange = processed.replaceRange;
        m_state.suffixCoverage = processed.suffixCoverage;
    } else {
        m_state.visibleText.clear();
        m_state.insertText.clear();
        m_state.replaceRange = KTextEditor::Range::invalid();
        m_state.suffixCoverage = 0;
    }

    applyStateToOverlay();
}

void EditorSession::onRequestFinished(quint64 requestId)
{
    if (requestId != m_activeRequestId) {
        return;
    }

    if (!syncAnchorFromTracker()) {
        clearSuggestion();
        return;
    }

    m_activeRequestId = 0;
    m_state.streaming = false;

    applyStateToOverlay();
}

void EditorSession::onRequestFailed(quint64 requestId, const QString &message)
{
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

    if (!syncAnchorFromTracker()) {
        clearSuggestion();
        return;
    }

    applyStateToOverlay();
}

void EditorSession::bumpGeneration()
{
    ++m_generation;

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

    const QVector<ContextItem> contextItems = collectContextItemsForRequest(m_view, doc, m_recentEditsTracker, settings, promptCtx, cursor, m_generation);
    const PromptAssemblyOptions assemblyOptions = promptAssemblyOptionsFromSettings(settings);

    CompletionRequest request;
    request.endpoint = endpoint;
    request.model = settings.model;
    request.maxTokens = 512;
    request.temperature = 0.2;

    if (providerIsCopilot) {
        CopilotCodexPrompt built = CopilotCodexPromptBuilder::build(promptCtx, doc, cursor);
        built.prompt = PromptAssembler::renderContextPrefix(promptCtx, contextItems, assemblyOptions) + built.prompt;

        request.prompt = built.prompt;
        request.suffix = built.suffix;
        request.nwo = settings.copilotNwo;
        request.stopSequences = {QStringLiteral("```")};

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
    m_rawSuggestionText.clear();
    m_acceptedFromSuggestion.clear();
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
    m_rawSuggestionText.clear();
    m_acceptedFromSuggestion.clear();

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
    bumpGeneration();
    startRequest();
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

    if (!syncAnchorFromTracker()) {
        bumpGeneration();
        return;
    }

    const KTextEditor::Cursor anchorCursor(m_state.anchor.line, m_state.anchor.column);
    const ProcessedSuggestion processed = SuggestionPostProcessor::process(remaining, suggestionProcessingContext(doc, anchorCursor));
    if (processed.valid) {
        m_state.visibleText = processed.displayText;
        m_state.insertText = processed.insertText;
        m_state.replaceRange = processed.replaceRange;
        m_state.suffixCoverage = processed.suffixCoverage;
    } else {
        m_state.visibleText.clear();
        m_state.insertText.clear();
        m_state.replaceRange = KTextEditor::Range::invalid();
        m_state.suffixCoverage = 0;
    }

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

    m_provider.reset();
    m_providerId = normalized;

    if (m_providerId == QString::fromLatin1(CompletionSettings::kProviderGitHubCopilotCodex)) {
        m_provider = std::make_unique<CopilotCodexProvider>(m_networkManager, m_copilotAuthManager);
    } else {
        m_provider = std::make_unique<OpenAICompatibleProvider>(m_networkManager);
    }

    connect(m_provider.get(), &AbstractAIProvider::deltaReceived, this, &EditorSession::onDeltaReceived);
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
    if (!m_view || !m_view->document() || maxChars < 0) {
        return {};
    }

    if (maxChars == 0) {
        return {};
    }

    KTextEditor::Document *doc = m_view->document();
    const KTextEditor::Cursor cursor = m_view->cursorPosition();

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
