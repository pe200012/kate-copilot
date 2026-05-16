/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RecentEditsTracker
*/

#include "context/RecentEditsTracker.h"

#include <QFileInfo>
#include <QSet>
#include <QUrl>
#include <QtGlobal>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] RecentEditsTrackerOptions normalizedOptions(RecentEditsTrackerOptions options)
{
    options.maxFiles = qBound(1, options.maxFiles, 100);
    options.maxEdits = qBound(0, options.maxEdits, 50);
    options.diffContextLines = qBound(0, options.diffContextLines, 20);
    options.maxCharsPerEdit = qBound(200, options.maxCharsPerEdit, 20000);
    options.debounceMs = qBound(0, options.debounceMs, 5000);
    options.maxLinesPerEdit = qBound(1, options.maxLinesPerEdit, 100);
    options.maxDocumentChars = qBound(1000, options.maxDocumentChars, 2'000'000);
    return options;
}

[[nodiscard]] QString joinLines(const QStringList &lines)
{
    return lines.join(QLatin1Char('\n'));
}

[[nodiscard]] QStringList sliceLines(const QStringList &lines, int start, int endExclusive)
{
    QStringList out;
    const int safeStart = qBound(0, start, lines.size());
    const int safeEnd = qBound(safeStart, endExclusive, lines.size());
    out.reserve(safeEnd - safeStart);
    for (int i = safeStart; i < safeEnd; ++i) {
        out.push_back(lines.at(i));
    }
    return out;
}

[[nodiscard]] QString truncateText(QString text, int maxChars)
{
    if (maxChars > 0 && text.size() > maxChars) {
        text = text.left(qMax(0, maxChars - 4));
        text += QStringLiteral("\n...");
    }
    return text;
}

[[nodiscard]] int lineDistance(const RecentEdit &a, const RecentEdit &b)
{
    if (a.endLine < b.startLine) {
        return b.startLine - a.endLine;
    }
    if (b.endLine < a.startLine) {
        return a.startLine - b.endLine;
    }
    return 0;
}
} // namespace

RecentEditsTracker::RecentEditsTracker(QObject *parent)
    : QObject(parent)
{
}

RecentEditsTracker::~RecentEditsTracker()
{
    clear();
}

void RecentEditsTracker::setOptions(const RecentEditsTrackerOptions &options)
{
    m_options = normalizedOptions(options);
    for (DocumentState *state : std::as_const(m_documents)) {
        if (state && state->timer) {
            state->timer->setInterval(m_options.debounceMs);
        }
    }
    pruneHistory();
}

RecentEditsTrackerOptions RecentEditsTracker::options() const
{
    return m_options;
}

void RecentEditsTracker::trackDocument(KTextEditor::Document *document, const QString &uriOverride)
{
    if (!document) {
        return;
    }

    if (DocumentState *existing = m_documents.value(document, nullptr)) {
        if (!uriOverride.trimmed().isEmpty()) {
            existing->uriOverride = uriOverride.trimmed();
        }
        return;
    }

    auto *state = new DocumentState;
    state->document = document;
    state->uriOverride = uriOverride.trimmed();
    state->snapshot = captureLines(document);
    state->timer = new QTimer(this);
    state->timer->setSingleShot(true);
    state->timer->setInterval(m_options.debounceMs);

    connect(state->timer, &QTimer::timeout, this, [this, document] {
        processPendingEdit(document);
    });

    state->textChangedConnection = connect(document, &KTextEditor::Document::textChanged, this, [this, document](KTextEditor::Document *changed) {
        if (changed == document) {
            onDocumentTextChanged(document);
        }
    });

    state->destroyedConnection = connect(document, &QObject::destroyed, this, [this, document] {
        untrackDocument(document);
    });

    m_documents.insert(document, state);
}

void RecentEditsTracker::untrackDocument(KTextEditor::Document *document)
{
    DocumentState *state = m_documents.value(document, nullptr);
    if (!state) {
        return;
    }

    if (state->timer) {
        state->timer->stop();
    }
    if (state->pending && state->document) {
        processPendingEdit(document);
    }

    state = m_documents.take(document);
    if (!state) {
        return;
    }

    QObject::disconnect(state->textChangedConnection);
    QObject::disconnect(state->destroyedConnection);
    if (state->timer) {
        state->timer->deleteLater();
    }
    delete state;
}

void RecentEditsTracker::addRecentEdit(const RecentEdit &edit)
{
    if (m_options.maxEdits <= 0 || edit.uri.trimmed().isEmpty() || edit.summary.trimmed().isEmpty()) {
        return;
    }

    RecentEdit incoming = edit;
    incoming.uri = incoming.uri.trimmed();
    incoming.startLine = qMax(0, incoming.startLine);
    incoming.endLine = qMax(incoming.startLine, incoming.endLine);
    incoming.summary = boundedText(incoming.summary.trimmed());
    incoming.beforeText = boundedText(incoming.beforeText);
    incoming.afterText = boundedText(incoming.afterText);
    if (!incoming.timestamp.isValid()) {
        incoming.timestamp = QDateTime::currentDateTimeUtc();
    }

    for (int i = 0; i < m_edits.size(); ++i) {
        if (!shouldMerge(m_edits.at(i), incoming)) {
            continue;
        }

        RecentEdit merged = m_edits.at(i);
        merged.timestamp = qMax(merged.timestamp, incoming.timestamp);
        merged.startLine = qMin(merged.startLine, incoming.startLine);
        merged.endLine = qMax(merged.endLine, incoming.endLine);
        if (!incoming.beforeText.trimmed().isEmpty() && !merged.beforeText.contains(incoming.beforeText)) {
            merged.beforeText = boundedText(merged.beforeText + QLatin1Char('\n') + incoming.beforeText);
        }
        if (!incoming.afterText.trimmed().isEmpty() && !merged.afterText.contains(incoming.afterText)) {
            merged.afterText = boundedText(merged.afterText + QLatin1Char('\n') + incoming.afterText);
        }
        if (!merged.summary.contains(incoming.summary)) {
            merged.summary = boundedText(merged.summary + QLatin1Char('\n') + incoming.summary);
        }

        m_edits.removeAt(i);
        m_edits.prepend(merged);
        pruneHistory();
        return;
    }

    m_edits.prepend(incoming);
    pruneHistory();
}

QVector<RecentEdit> RecentEditsTracker::recentEdits() const
{
    return m_edits;
}

void RecentEditsTracker::flushPendingEdits()
{
    const QList<KTextEditor::Document *> docs = m_documents.keys();
    for (KTextEditor::Document *document : docs) {
        if (DocumentState *state = m_documents.value(document, nullptr)) {
            if (state->timer) {
                state->timer->stop();
            }
            if (state->pending) {
                processPendingEdit(document);
            }
        }
    }
}

void RecentEditsTracker::clear()
{
    const QList<KTextEditor::Document *> docs = m_documents.keys();
    for (KTextEditor::Document *document : docs) {
        untrackDocument(document);
    }
    m_edits.clear();
}

void RecentEditsTracker::onDocumentTextChanged(KTextEditor::Document *document)
{
    DocumentState *state = m_documents.value(document, nullptr);
    if (!state || !state->document) {
        return;
    }

    if (state->document->totalCharacters() > m_options.maxDocumentChars) {
        state->snapshot = captureLines(state->document);
        state->pending = false;
        if (state->timer) {
            state->timer->stop();
        }
        return;
    }

    state->pending = true;
    if (state->timer) {
        state->timer->start(m_options.debounceMs);
    }
}

void RecentEditsTracker::processPendingEdit(KTextEditor::Document *document)
{
    DocumentState *state = m_documents.value(document, nullptr);
    if (!state || !state->document || !state->pending) {
        return;
    }

    state->pending = false;
    const QStringList before = state->snapshot;
    const QStringList after = captureLines(state->document);
    state->snapshot = after;

    RecentEdit edit = buildEdit(documentUri(*state), before, after);
    addRecentEdit(edit);
}

void RecentEditsTracker::pruneHistory()
{
    if (m_options.maxEdits <= 0) {
        m_edits.clear();
        return;
    }

    QVector<RecentEdit> kept;
    kept.reserve(qMin(m_options.maxEdits, m_edits.size()));
    QSet<QString> files;

    for (const RecentEdit &edit : std::as_const(m_edits)) {
        if (!files.contains(edit.uri) && files.size() >= m_options.maxFiles) {
            continue;
        }

        files.insert(edit.uri);
        kept.push_back(edit);
        if (kept.size() >= m_options.maxEdits) {
            break;
        }
    }

    m_edits = kept;
}

QStringList RecentEditsTracker::captureLines(KTextEditor::Document *document) const
{
    QStringList lines;
    if (!document || document->totalCharacters() > m_options.maxDocumentChars) {
        return lines;
    }

    lines.reserve(document->lines());
    for (int i = 0; i < document->lines(); ++i) {
        lines.push_back(document->line(i));
    }
    return lines;
}

RecentEdit RecentEditsTracker::buildEdit(const QString &uri, const QStringList &before, const QStringList &after) const
{
    RecentEdit edit;
    edit.uri = uri.trimmed();
    edit.timestamp = QDateTime::currentDateTimeUtc();

    if (edit.uri.isEmpty() || before == after) {
        return edit;
    }

    int prefix = 0;
    while (prefix < before.size() && prefix < after.size() && before.at(prefix) == after.at(prefix)) {
        ++prefix;
    }

    int suffix = 0;
    while (suffix < before.size() - prefix && suffix < after.size() - prefix
           && before.at(before.size() - 1 - suffix) == after.at(after.size() - 1 - suffix)) {
        ++suffix;
    }

    const int oldEndExclusive = before.size() - suffix;
    const int newEndExclusive = after.size() - suffix;

    edit.startLine = qMax(0, prefix);
    edit.endLine = qMax(edit.startLine, qMax(oldEndExclusive, newEndExclusive) - 1);

    const int beforeContextStart = qMax(0, prefix - m_options.diffContextLines);
    const int afterContextEnd = qMin(after.size(), newEndExclusive + m_options.diffContextLines);

    const QStringList leadingContext = sliceLines(after, beforeContextStart, prefix);
    const QStringList removedLines = sliceLines(before, prefix, oldEndExclusive);
    const QStringList addedLines = sliceLines(after, prefix, newEndExclusive);
    const QStringList trailingContext = sliceLines(after, newEndExclusive, afterContextEnd);

    QStringList beforeWindow = leadingContext;
    beforeWindow.append(removedLines);
    beforeWindow.append(trailingContext);

    QStringList afterWindow = leadingContext;
    afterWindow.append(addedLines);
    afterWindow.append(trailingContext);

    edit.beforeText = boundedText(joinLines(beforeWindow));
    edit.afterText = boundedText(joinLines(afterWindow));

    QStringList summaryLines;
    summaryLines.push_back(QStringLiteral("@@ lines %1-%2").arg(edit.startLine + 1).arg(edit.endLine + 1));

    const int maxSummaryLines = qMax(1, m_options.maxLinesPerEdit);
    const auto appendSummaryLine = [&summaryLines, maxSummaryLines](const QString &prefixText, const QString &line) {
        if (summaryLines.size() - 1 >= maxSummaryLines) {
            return;
        }
        summaryLines.push_back(prefixText + line);
    };

    for (const QString &line : leadingContext) {
        appendSummaryLine(QStringLiteral("  "), line);
    }
    for (const QString &line : removedLines) {
        appendSummaryLine(QStringLiteral("- "), line);
    }
    for (const QString &line : addedLines) {
        appendSummaryLine(QStringLiteral("+ "), line);
    }
    for (const QString &line : trailingContext) {
        appendSummaryLine(QStringLiteral("  "), line);
    }

    edit.summary = boundedText(summaryLines.join(QLatin1Char('\n')));
    return edit;
}

QString RecentEditsTracker::documentUri(const DocumentState &state) const
{
    if (!state.uriOverride.trimmed().isEmpty()) {
        return state.uriOverride.trimmed();
    }

    KTextEditor::Document *document = state.document;
    if (!document) {
        return {};
    }

    if (document->url().isValid() && !document->url().isEmpty()) {
        return document->url().toDisplayString(QUrl::PreferLocalFile);
    }

    return document->documentName();
}

QString RecentEditsTracker::boundedText(const QString &text) const
{
    return truncateText(text, m_options.maxCharsPerEdit);
}

bool RecentEditsTracker::shouldMerge(const RecentEdit &existing, const RecentEdit &incoming) const
{
    if (existing.uri != incoming.uri) {
        return false;
    }

    return lineDistance(existing, incoming) <= qMax(1, m_options.diffContextLines);
}

} // namespace KateAiInlineCompletion
