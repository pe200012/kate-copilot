/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticStore
*/

#include "context/DiagnosticStore.h"

#include <algorithm>

#include <QtGlobal>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] int severityOrder(DiagnosticItem::Severity severity)
{
    switch (severity) {
    case DiagnosticItem::Severity::Error:
        return 0;
    case DiagnosticItem::Severity::Warning:
        return 1;
    case DiagnosticItem::Severity::Information:
        return 2;
    case DiagnosticItem::Severity::Hint:
        return 3;
    }

    return 4;
}

[[nodiscard]] bool diagnosticLess(const DiagnosticItem &a, const DiagnosticItem &b)
{
    if (a.uri != b.uri) {
        return a.uri < b.uri;
    }
    if (a.startLine != b.startLine) {
        return a.startLine < b.startLine;
    }
    if (a.startColumn != b.startColumn) {
        return a.startColumn < b.startColumn;
    }
    if (severityOrder(a.severity) != severityOrder(b.severity)) {
        return severityOrder(a.severity) < severityOrder(b.severity);
    }
    if (a.source != b.source) {
        return a.source < b.source;
    }
    if (a.code != b.code) {
        return a.code < b.code;
    }
    return a.message < b.message;
}
} // namespace

DiagnosticStore::DiagnosticStore(QObject *parent)
    : QObject(parent)
{
}

void DiagnosticStore::setDiagnostics(const QString &uri, const QVector<DiagnosticItem> &diagnostics)
{
    const QString normalizedUri = uri.trimmed();
    if (normalizedUri.isEmpty()) {
        return;
    }

    QVector<DiagnosticItem> normalized;
    normalized.reserve(diagnostics.size());
    for (const DiagnosticItem &diagnostic : diagnostics) {
        if (diagnostic.message.trimmed().isEmpty()) {
            continue;
        }
        normalized.push_back(normalizeDiagnostic(normalizedUri, diagnostic));
    }

    std::stable_sort(normalized.begin(), normalized.end(), diagnosticLess);

    if (normalized.isEmpty()) {
        m_diagnosticsByUri.remove(normalizedUri);
        return;
    }

    m_diagnosticsByUri.insert(normalizedUri, normalized);
}

void DiagnosticStore::clearDiagnostics(const QString &uri)
{
    m_diagnosticsByUri.remove(uri.trimmed());
}

void DiagnosticStore::clear()
{
    m_diagnosticsByUri.clear();
}

QVector<DiagnosticItem> DiagnosticStore::diagnostics(const QString &uri) const
{
    return m_diagnosticsByUri.value(uri.trimmed());
}

QVector<DiagnosticItem> DiagnosticStore::allDiagnostics() const
{
    QVector<DiagnosticItem> out;
    for (const QVector<DiagnosticItem> &items : m_diagnosticsByUri) {
        out += items;
    }

    std::stable_sort(out.begin(), out.end(), diagnosticLess);
    return out;
}

bool DiagnosticStore::isEmpty() const
{
    return m_diagnosticsByUri.isEmpty();
}

DiagnosticItem DiagnosticStore::normalizeDiagnostic(const QString &uri, const DiagnosticItem &diagnostic) const
{
    DiagnosticItem out = diagnostic;
    out.uri = uri;
    out.startLine = qMax(0, out.startLine);
    out.startColumn = qMax(0, out.startColumn);
    out.endLine = qMax(out.startLine, out.endLine);
    out.endColumn = qMax(0, out.endColumn);
    if (out.endLine == out.startLine) {
        out.endColumn = qMax(out.startColumn, out.endColumn);
    }
    out.source = out.source.trimmed();
    out.code = out.code.trimmed();
    out.message = out.message.trimmed();
    if (!out.timestamp.isValid()) {
        out.timestamp = QDateTime::currentDateTimeUtc();
    }
    return out;
}

} // namespace KateAiInlineCompletion
