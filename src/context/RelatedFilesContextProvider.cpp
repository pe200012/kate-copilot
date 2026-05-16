/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RelatedFilesContextProvider
*/

#include "context/RelatedFilesContextProvider.h"

#include "context/ContextFileFilter.h"
#include "context/ProjectContextResolver.h"
#include "context/RelatedFilesResolver.h"

#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QUrl>
#include <QtGlobal>

#include <utility>

namespace KateAiInlineCompletion
{

namespace
{
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

[[nodiscard]] QString boundedOpenDocumentText(const QString &text, int maxChars)
{
    if (maxChars >= 0 && text.size() > maxChars) {
        return {};
    }
    return text;
}

[[nodiscard]] QString documentTextPrefix(KTextEditor::Document *document, int maxChars)
{
    if (!document || maxChars <= 0) {
        return {};
    }

    QString out;
    for (int line = 0; line < document->lines() && out.size() < maxChars; ++line) {
        if (!out.isEmpty()) {
            out += QLatin1Char('\n');
        }
        const QString text = document->line(line);
        const int remaining = maxChars - out.size();
        out += text.left(qMax(0, remaining));
    }
    return out;
}
} // namespace

RelatedFilesContextProvider::RelatedFilesContextProvider(RelatedFilesContextOptions options)
    : m_options(std::move(options))
{
}

RelatedFilesContextProvider::RelatedFilesContextProvider(KTextEditor::MainWindow *mainWindow,
                                                         KTextEditor::View *activeView,
                                                         RelatedFilesContextOptions options)
    : m_mainWindow(mainWindow)
    , m_activeView(activeView)
    , m_options(std::move(options))
{
}

QString RelatedFilesContextProvider::id() const
{
    return QStringLiteral("related-files");
}

int RelatedFilesContextProvider::matchScore(const ContextResolveRequest &request) const
{
    Q_UNUSED(request);
    return m_options.enabled ? 80 : 0;
}

QVector<ContextItem> RelatedFilesContextProvider::resolve(const ContextResolveRequest &request)
{
    QVector<ContextItem> items;
    if (!m_options.enabled || m_options.maxFiles <= 0 || m_options.maxChars <= 0 || m_options.maxCharsPerFile <= 0) {
        return items;
    }

    const QString currentPath = ProjectContextResolver::localPathFromUri(request.uri);
    if (currentPath.isEmpty()) {
        return items;
    }

    const QString currentText = currentFileText(currentPath);
    const QString projectRoot = RelatedFilesResolver::findProjectRoot(currentPath);
    const QHash<QString, QString> openDocs = openDocuments();

    RelatedFilesResolveRequest resolverRequest;
    resolverRequest.currentFilePath = currentPath;
    resolverRequest.currentText = currentText;
    resolverRequest.languageId = request.languageId;
    resolverRequest.projectRoot = projectRoot;
    resolverRequest.openDocuments = openDocs;
    resolverRequest.preferOpenTabs = m_options.preferOpenTabs;
    resolverRequest.maxCharsPerFile = m_options.maxCharsPerFile;
    resolverRequest.excludePatterns = m_options.excludePatterns;

    const QVector<RelatedFileCandidate> candidates = RelatedFilesResolver::resolve(resolverRequest);
    ContextFileFilterOptions filterOptions;
    filterOptions.maxFileChars = m_options.maxCharsPerFile;
    filterOptions.excludePatterns = m_options.excludePatterns;

    int totalChars = 0;
    for (const RelatedFileCandidate &candidate : candidates) {
        if (items.size() >= m_options.maxFiles) {
            break;
        }

        QString text;
        if (candidate.fromOpenDocument && openDocs.contains(candidate.path)) {
            text = boundedOpenDocumentText(openDocs.value(candidate.path), m_options.maxCharsPerFile);
        } else {
            bool ok = false;
            text = ContextFileFilter::readTextFile(candidate.path, filterOptions, &ok);
            if (!ok) {
                text.clear();
            }
        }

        if (text.trimmed().isEmpty()) {
            continue;
        }

        if (totalChars + text.size() > m_options.maxChars) {
            continue;
        }

        ContextItem item;
        item.kind = ContextItem::Kind::CodeSnippet;
        item.providerId = id();
        item.id = candidate.path;
        item.importance = qBound(0, candidate.score, 100);
        item.uri = candidate.path;
        item.name = ProjectContextResolver::relativeDisplayPath(candidate.path, projectRoot);
        item.value = text;
        items.push_back(item);
        totalChars += text.size();
    }

    return items;
}

QHash<QString, QString> RelatedFilesContextProvider::openDocuments() const
{
    QHash<QString, QString> docs;
    for (auto it = m_options.openDocuments.constBegin(); it != m_options.openDocuments.constEnd(); ++it) {
        const QString path = ProjectContextResolver::localPathFromUri(it.key());
        const QString text = boundedOpenDocumentText(it.value(), m_options.maxCharsPerFile);
        if (!path.isEmpty() && !text.isEmpty()) {
            docs.insert(path, text);
        }
    }

    if (!m_mainWindow) {
        return docs;
    }

    const QList<KTextEditor::View *> views = m_mainWindow->views();
    for (KTextEditor::View *view : views) {
        if (!view || !view->document()) {
            continue;
        }

        const QString path = ProjectContextResolver::localPathFromUri(displayUriForDocument(view->document()));
        if (path.isEmpty()) {
            continue;
        }
        if (view->document()->totalCharacters() > m_options.maxCharsPerFile) {
            continue;
        }
        docs.insert(path, documentTextPrefix(view->document(), m_options.maxCharsPerFile));
    }

    return docs;
}

QString RelatedFilesContextProvider::currentFileText(const QString &path) const
{
    const QString cleanPath = ProjectContextResolver::localPathFromUri(path);

    if (m_activeView && m_activeView->document()) {
        const QString activePath = ProjectContextResolver::localPathFromUri(displayUriForDocument(m_activeView->document()));
        if (!activePath.isEmpty() && activePath == cleanPath) {
            return documentTextPrefix(m_activeView->document(), m_options.maxCharsPerFile);
        }
    }

    const QHash<QString, QString> docs = openDocuments();
    if (docs.contains(cleanPath)) {
        return docs.value(cleanPath);
    }

    ContextFileFilterOptions filterOptions;
    filterOptions.maxFileChars = m_options.maxCharsPerFile;
    filterOptions.excludePatterns = m_options.excludePatterns;
    bool ok = false;
    const QString text = ContextFileFilter::readTextFile(cleanPath, filterOptions, &ok);
    return ok ? text : QString();
}

} // namespace KateAiInlineCompletion
