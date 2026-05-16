/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RelatedFilesResolver

    Cheap deterministic related-file discovery heuristics.
*/

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace KateAiInlineCompletion
{

struct RelatedFileCandidate {
    QString path;
    int score = 0;
    bool fromOpenDocument = false;
};

struct RelatedFilesResolveRequest {
    QString currentFilePath;
    QString currentText;
    QString languageId;
    QString projectRoot;
    QHash<QString, QString> openDocuments;
    bool preferOpenTabs = true;
    int maxCharsPerFile = 4000;
    QStringList excludePatterns;
};

class RelatedFilesResolver
{
public:
    [[nodiscard]] static QVector<RelatedFileCandidate> resolve(const RelatedFilesResolveRequest &request);
    [[nodiscard]] static QString findProjectRoot(const QString &path);
};

} // namespace KateAiInlineCompletion
