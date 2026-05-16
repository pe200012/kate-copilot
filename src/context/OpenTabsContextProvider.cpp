/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: OpenTabsContextProvider
*/

#include "context/OpenTabsContextProvider.h"

#include "context/ContextFileFilter.h"
#include "context/ProjectContextResolver.h"

#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QUrl>

namespace KateAiInlineCompletion
{

namespace
{
constexpr int kMaxFiles = 3;
constexpr int kMaxDocumentChars = 50000;
constexpr int kMaxSnippetChars = 1200;
constexpr int kMaxTotalSnippetChars = 3000;

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
    const QString currentLocalPath = ProjectContextResolver::localPathFromUri(currentUri);
    const QString projectRoot = ProjectContextResolver::findProjectRoot(currentLocalPath);
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
        const QString localPath = ProjectContextResolver::localPathFromUri(uri);
        if ((!currentLocalPath.isEmpty() && ProjectContextResolver::canonicalPath(localPath) == ProjectContextResolver::canonicalPath(currentLocalPath))
            || (!currentUri.isEmpty() && uri == currentUri)) {
            continue;
        }

        if (!requestedLanguage.isEmpty() && doc->highlightingMode().compare(requestedLanguage, Qt::CaseInsensitive) != 0) {
            continue;
        }

        ContextFileFilterOptions filterOptions;
        const QString filterPath = localPath.isEmpty() ? uri : localPath;
        if (!ContextFileFilter::isAllowedPath(filterPath, filterOptions)) {
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

        const QString path = ProjectContextResolver::relativeDisplayPath(uri, projectRoot);
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
