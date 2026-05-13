/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: OpenTabsContextProvider
*/

#include "context/OpenTabsContextProvider.h"

#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace KateAiInlineCompletion
{

namespace
{
constexpr int kMaxFiles = 3;
constexpr int kMaxDocumentChars = 50000;
constexpr int kMaxSnippetChars = 1200;
constexpr int kMaxTotalSnippetChars = 3000;

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

[[nodiscard]] QString displayUriForDocument(KTextEditor::Document *doc)
{
    if (!doc) {
        return {};
    }

    if (doc->url().isValid() && !doc->url().isEmpty()) {
        return doc->url().toDisplayString(QUrl::PreferLocalFile);
    }

    return doc->documentName();
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

    return uri;
}

[[nodiscard]] ContextItem snippetItem(const QString &providerId,
                                      const QString &uri,
                                      const QString &path,
                                      const QString &value,
                                      int importance)
{
    ContextItem item;
    item.kind = ContextItem::Kind::CodeSnippet;
    item.providerId = providerId;
    item.id = path;
    item.uri = uri;
    item.name = path;
    item.value = value;
    item.importance = importance;
    return item;
}
} // namespace

OpenTabsContextProvider::OpenTabsContextProvider(KTextEditor::MainWindow *mainWindow, KTextEditor::View *activeView)
    : m_mainWindow(mainWindow)
    , m_activeView(activeView)
{
}

QString OpenTabsContextProvider::id() const
{
    return QStringLiteral("open-tabs");
}

int OpenTabsContextProvider::matchScore(const ContextResolveRequest &request) const
{
    Q_UNUSED(request);
    return m_mainWindow ? 60 : 0;
}

QVector<ContextItem> OpenTabsContextProvider::resolve(const ContextResolveRequest &request)
{
    QVector<ContextItem> items;
    if (!m_mainWindow) {
        return items;
    }

    const QString currentUri = request.uri.trimmed();
    const QString currentLocalPath = localPathFromUri(currentUri);
    const QString projectRoot = findProjectRoot(currentLocalPath);
    const QString requestedLanguage = request.languageId.trimmed();

    int acceptedFiles = 0;
    int totalChars = 0;

    const QList<KTextEditor::View *> views = m_mainWindow->views();
    for (KTextEditor::View *view : views) {
        if (!view || view == m_activeView) {
            continue;
        }

        KTextEditor::Document *doc = view->document();
        if (!doc) {
            continue;
        }

        const QString uri = displayUriForDocument(doc);
        const QString localPath = localPathFromUri(uri);
        if ((!currentLocalPath.isEmpty() && QFileInfo(localPath).absoluteFilePath() == QFileInfo(currentLocalPath).absoluteFilePath())
            || (!currentUri.isEmpty() && uri == currentUri)) {
            continue;
        }

        if (!requestedLanguage.isEmpty() && doc->highlightingMode().compare(requestedLanguage, Qt::CaseInsensitive) != 0) {
            continue;
        }

        if (isPrivateLookingPath(uri) || isPrivateLookingPath(localPath)) {
            continue;
        }

        const QString text = doc->text();
        if (text.trimmed().isEmpty() || text.size() > kMaxDocumentChars) {
            continue;
        }

        QString snippet = text.left(kMaxSnippetChars);
        if (text.size() > snippet.size()) {
            snippet += QStringLiteral("\n...");
        }

        if (totalChars + snippet.size() > kMaxTotalSnippetChars) {
            break;
        }

        const QString path = relativeDisplayPath(uri, projectRoot);
        items.push_back(snippetItem(id(), uri, path, snippet, 60 - acceptedFiles));

        totalChars += snippet.size();
        ++acceptedFiles;
        if (acceptedFiles >= kMaxFiles) {
            break;
        }
    }

    return items;
}

} // namespace KateAiInlineCompletion
