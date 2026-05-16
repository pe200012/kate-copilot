/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticsContextProvider
*/

#include "context/DiagnosticsContextProvider.h"

#include "context/DiagnosticStore.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QtGlobal>

namespace KateAiInlineCompletion
{

namespace
{
struct RankedDiagnostic {
    DiagnosticItem diagnostic;
    bool activeFile = false;
    int distance = 0;
    int originalIndex = 0;
};

[[nodiscard]] QString localPathFromUri(const QString &uri)
{
    const QString trimmed = uri.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl url(trimmed);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }

    const QFileInfo info(trimmed);
    if (info.exists() || info.isAbsolute()) {
        return info.absoluteFilePath();
    }

    return {};
}

[[nodiscard]] QString directoryForPath(const QString &path)
{
    const QFileInfo info(path);
    if (info.isDir()) {
        return info.absoluteFilePath();
    }
    return info.absoluteDir().absolutePath();
}

[[nodiscard]] QString findProjectRoot(const QString &localPath)
{
    if (localPath.trimmed().isEmpty()) {
        return {};
    }

    QDir dir(directoryForPath(localPath));
    QString markerRoot;

    while (true) {
        if (dir.exists(QStringLiteral(".git"))) {
            return dir.absolutePath();
        }

        if (markerRoot.isEmpty()
            && (dir.exists(QStringLiteral("CMakeLists.txt")) || dir.exists(QStringLiteral("package.json"))
                || dir.exists(QStringLiteral("pyproject.toml")) || dir.exists(QStringLiteral("Cargo.toml")))) {
            markerRoot = dir.absolutePath();
        }

        if (!dir.cdUp()) {
            break;
        }
    }

    return markerRoot;
}

[[nodiscard]] bool sameUri(const QString &a, const QString &b)
{
    const QString localA = localPathFromUri(a);
    const QString localB = localPathFromUri(b);
    if (!localA.isEmpty() && !localB.isEmpty()) {
        return QFileInfo(localA).absoluteFilePath() == QFileInfo(localB).absoluteFilePath();
    }

    return a.trimmed() == b.trimmed();
}

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

[[nodiscard]] QString severityLabel(DiagnosticItem::Severity severity)
{
    switch (severity) {
    case DiagnosticItem::Severity::Error:
        return QStringLiteral("error");
    case DiagnosticItem::Severity::Warning:
        return QStringLiteral("warning");
    case DiagnosticItem::Severity::Information:
        return QStringLiteral("information");
    case DiagnosticItem::Severity::Hint:
        return QStringLiteral("hint");
    }

    return QStringLiteral("information");
}

[[nodiscard]] bool severityAllowed(const DiagnosticItem &diagnostic, const DiagnosticsContextOptions &options)
{
    switch (diagnostic.severity) {
    case DiagnosticItem::Severity::Error:
        return true;
    case DiagnosticItem::Severity::Warning:
        return options.includeWarnings;
    case DiagnosticItem::Severity::Information:
        return options.includeInformation;
    case DiagnosticItem::Severity::Hint:
        return options.includeHints;
    }

    return false;
}

[[nodiscard]] int lineDistanceToCursor(const DiagnosticItem &diagnostic, const KTextEditor::Cursor &cursor)
{
    if (!cursor.isValid()) {
        return 0;
    }

    const int line = cursor.line();
    if (line < diagnostic.startLine) {
        return diagnostic.startLine - line;
    }
    if (line > diagnostic.endLine) {
        return line - diagnostic.endLine;
    }
    return 0;
}

[[nodiscard]] bool rankedDiagnosticLess(const RankedDiagnostic &a, const RankedDiagnostic &b)
{
    if (a.activeFile != b.activeFile) {
        return a.activeFile;
    }
    if (a.distance != b.distance) {
        return a.distance < b.distance;
    }
    if (severityOrder(a.diagnostic.severity) != severityOrder(b.diagnostic.severity)) {
        return severityOrder(a.diagnostic.severity) < severityOrder(b.diagnostic.severity);
    }
    if (a.diagnostic.uri != b.diagnostic.uri) {
        return a.diagnostic.uri < b.diagnostic.uri;
    }
    if (a.diagnostic.startLine != b.diagnostic.startLine) {
        return a.diagnostic.startLine < b.diagnostic.startLine;
    }
    if (a.diagnostic.startColumn != b.diagnostic.startColumn) {
        return a.diagnostic.startColumn < b.diagnostic.startColumn;
    }
    if (a.diagnostic.message != b.diagnostic.message) {
        return a.diagnostic.message < b.diagnostic.message;
    }
    return a.originalIndex < b.originalIndex;
}

[[nodiscard]] QString relativeDisplayPath(const QString &uri, const QString &projectRoot)
{
    const QString localPath = localPathFromUri(uri);
    if (!localPath.isEmpty() && !projectRoot.isEmpty()) {
        const QString relative = QDir(projectRoot).relativeFilePath(localPath);
        if (!relative.startsWith(QStringLiteral(".."))) {
            return relative;
        }
    }

    if (!localPath.isEmpty()) {
        return QFileInfo(localPath).fileName();
    }

    return uri.trimmed();
}

[[nodiscard]] QString diagnosticLine(const DiagnosticItem &diagnostic)
{
    QString label = severityLabel(diagnostic.severity);
    QString sourceAndCode = diagnostic.source.trimmed();
    if (!diagnostic.code.trimmed().isEmpty()) {
        sourceAndCode += diagnostic.source.trimmed().isEmpty() ? diagnostic.code.trimmed() : QStringLiteral("-%1").arg(diagnostic.code.trimmed());
    }

    if (!sourceAndCode.isEmpty()) {
        label += QLatin1Char(' ') + sourceAndCode;
    }

    return QStringLiteral("%1:%2 - %3: %4")
        .arg(diagnostic.startLine + 1)
        .arg(diagnostic.startColumn + 1)
        .arg(label, diagnostic.message.trimmed());
}
} // namespace

DiagnosticsContextProvider::DiagnosticsContextProvider(DiagnosticStore *store, DiagnosticsContextOptions options)
    : m_store(store)
    , m_options(options)
{
    m_options.maxItems = qBound(0, m_options.maxItems, 50);
    m_options.maxChars = qBound(0, m_options.maxChars, 30000);
    m_options.maxLineDistance = qBound(0, m_options.maxLineDistance, 10000);
}

QString DiagnosticsContextProvider::id() const
{
    return QStringLiteral("diagnostics");
}

int DiagnosticsContextProvider::matchScore(const ContextResolveRequest &request) const
{
    Q_UNUSED(request);
    return (m_options.enabled && m_store && !m_store->isEmpty()) ? 95 : 0;
}

QVector<ContextItem> DiagnosticsContextProvider::resolve(const ContextResolveRequest &request)
{
    QVector<ContextItem> items;
    if (!m_options.enabled || !m_store || m_options.maxItems <= 0 || m_options.maxChars <= 0) {
        return items;
    }

    const QVector<DiagnosticItem> diagnostics = m_store->allDiagnostics();
    QVector<RankedDiagnostic> ranked;
    ranked.reserve(diagnostics.size());

    for (int i = 0; i < diagnostics.size(); ++i) {
        const DiagnosticItem &diagnostic = diagnostics.at(i);
        if (!severityAllowed(diagnostic, m_options) || diagnostic.message.trimmed().isEmpty()) {
            continue;
        }

        const bool active = sameUri(diagnostic.uri, request.uri);
        const int distance = active ? lineDistanceToCursor(diagnostic, request.position) : 0;
        if (active && request.position.isValid() && distance > m_options.maxLineDistance) {
            continue;
        }

        ranked.push_back(RankedDiagnostic{diagnostic, active, distance, i});
    }

    std::stable_sort(ranked.begin(), ranked.end(), rankedDiagnosticLess);

    const QString requestLocalPath = localPathFromUri(request.uri);
    const QString projectRoot = findProjectRoot(requestLocalPath);

    QHash<QString, int> itemIndexByUri;
    int emittedDiagnostics = 0;
    int emittedChars = 0;

    for (const RankedDiagnostic &entry : std::as_const(ranked)) {
        if (emittedDiagnostics >= m_options.maxItems) {
            break;
        }

        const QString line = diagnosticLine(entry.diagnostic);
        const int extraChars = line.size() + 1;
        if (emittedChars + extraChars > m_options.maxChars) {
            continue;
        }

        const QString uri = entry.diagnostic.uri.trimmed();
        int itemIndex = itemIndexByUri.value(uri, -1);
        if (itemIndex < 0) {
            ContextItem item;
            item.kind = ContextItem::Kind::DiagnosticBag;
            item.providerId = id();
            item.id = uri;
            item.importance = 65;
            item.uri = uri;
            item.name = relativeDisplayPath(uri, projectRoot);
            item.value.clear();
            items.push_back(item);
            itemIndex = items.size() - 1;
            itemIndexByUri.insert(uri, itemIndex);
        }

        if (!items[itemIndex].value.isEmpty()) {
            items[itemIndex].value += QLatin1Char('\n');
        }
        items[itemIndex].value += line;
        emittedChars += extraChars;
        ++emittedDiagnostics;
    }

    return items;
}

} // namespace KateAiInlineCompletion
