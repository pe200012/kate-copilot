/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RecentEditsContextProvider
*/

#include "context/RecentEditsContextProvider.h"

#include "context/RecentEditsTracker.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QtGlobal>

namespace KateAiInlineCompletion
{

namespace
{
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

[[nodiscard]] bool nearCursor(const RecentEdit &edit, const KTextEditor::Cursor &cursor, int limit)
{
    if (!cursor.isValid() || limit < 0) {
        return false;
    }

    const int line = cursor.line();
    if (line < edit.startLine) {
        return edit.startLine - line <= limit;
    }
    if (line > edit.endLine) {
        return line - edit.endLine <= limit;
    }
    return true;
}

[[nodiscard]] bool isPrivateLookingPath(const QString &path)
{
    const QString p = path.toLower();
    return p.contains(QStringLiteral("/.env")) || p.endsWith(QStringLiteral(".env")) || p.contains(QStringLiteral("secret"))
        || p.contains(QStringLiteral("token")) || p.contains(QStringLiteral("credential")) || p.contains(QStringLiteral("password"))
        || p.contains(QStringLiteral("private"));
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

[[nodiscard]] QString truncateText(QString text, int maxChars)
{
    if (maxChars > 0 && text.size() > maxChars) {
        text = text.left(qMax(0, maxChars - 4));
        text += QStringLiteral("\n...");
    }
    return text;
}

[[nodiscard]] ContextItem itemForEdit(const RecentEdit &edit, const QString &path, int order, int maxChars)
{
    ContextItem item;
    item.kind = ContextItem::Kind::CodeSnippet;
    item.providerId = QStringLiteral("recent-edits");
    item.id = QStringLiteral("%1:%2:%3").arg(edit.uri, QString::number(edit.startLine), QString::number(edit.timestamp.toMSecsSinceEpoch()));
    item.importance = qBound(0, 92 - order, 100);
    item.uri = edit.uri;
    item.name = path;
    item.value = truncateText(QStringLiteral("File: %1\n%2").arg(path, edit.summary.trimmed()), maxChars);
    return item;
}
} // namespace

RecentEditsContextProvider::RecentEditsContextProvider(RecentEditsTracker *tracker, RecentEditsContextOptions options)
    : m_tracker(tracker)
    , m_options(options)
{
    m_options.maxEdits = qBound(0, m_options.maxEdits, 50);
    m_options.maxCharsPerEdit = qBound(200, m_options.maxCharsPerEdit, 20000);
    m_options.activeDocDistanceLimitFromCursor = qBound(0, m_options.activeDocDistanceLimitFromCursor, 10000);
}

QString RecentEditsContextProvider::id() const
{
    return QStringLiteral("recent-edits");
}

int RecentEditsContextProvider::matchScore(const ContextResolveRequest &request) const
{
    Q_UNUSED(request);
    return (m_options.enabled && m_tracker) ? 85 : 0;
}

QVector<ContextItem> RecentEditsContextProvider::resolve(const ContextResolveRequest &request)
{
    QVector<ContextItem> items;
    if (!m_options.enabled || !m_tracker || m_options.maxEdits <= 0) {
        return items;
    }

    const QString requestLocalPath = localPathFromUri(request.uri);
    const QString projectRoot = findProjectRoot(requestLocalPath);
    const QVector<RecentEdit> edits = m_tracker->recentEdits();

    int emitted = 0;
    for (const RecentEdit &edit : edits) {
        if (emitted >= m_options.maxEdits) {
            break;
        }

        if (edit.uri.trimmed().isEmpty() || edit.summary.trimmed().isEmpty()) {
            continue;
        }

        const QString editLocalPath = localPathFromUri(edit.uri);
        if (isPrivateLookingPath(edit.uri) || isPrivateLookingPath(editLocalPath)) {
            continue;
        }

        if (sameUri(edit.uri, request.uri) && nearCursor(edit, request.position, m_options.activeDocDistanceLimitFromCursor)) {
            continue;
        }

        const QString path = relativeDisplayPath(edit.uri, projectRoot);
        items.push_back(itemForEdit(edit, path, emitted, m_options.maxCharsPerEdit));
        ++emitted;
    }

    return items;
}

} // namespace KateAiInlineCompletion
