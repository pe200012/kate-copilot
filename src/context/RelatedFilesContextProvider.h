/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RelatedFilesContextProvider

    Converts repository-local related files into snippet context items.
*/

#pragma once

#include "context/ContextProvider.h"

#include <QHash>
#include <QPointer>
#include <QStringList>

namespace KTextEditor
{
class MainWindow;
class View;
}

namespace KateAiInlineCompletion
{

struct RelatedFilesContextOptions {
    bool enabled = true;
    int maxFiles = 6;
    int maxChars = 12000;
    int maxCharsPerFile = 4000;
    bool preferOpenTabs = true;
    QStringList excludePatterns;
    QHash<QString, QString> openDocuments;
};

class RelatedFilesContextProvider final : public ContextProvider
{
public:
    explicit RelatedFilesContextProvider(RelatedFilesContextOptions options = {});
    RelatedFilesContextProvider(KTextEditor::MainWindow *mainWindow, KTextEditor::View *activeView, RelatedFilesContextOptions options = {});

    [[nodiscard]] QString id() const override;
    [[nodiscard]] int matchScore(const ContextResolveRequest &request) const override;
    [[nodiscard]] QVector<ContextItem> resolve(const ContextResolveRequest &request) override;

private:
    [[nodiscard]] QHash<QString, QString> openDocuments() const;
    [[nodiscard]] QString currentFileText(const QString &path) const;

    QPointer<KTextEditor::MainWindow> m_mainWindow;
    QPointer<KTextEditor::View> m_activeView;
    RelatedFilesContextOptions m_options;
};

} // namespace KateAiInlineCompletion
