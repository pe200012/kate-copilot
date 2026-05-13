/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: OpenTabsContextProvider

    Emits bounded snippets from same-language open Kate views.
*/

#pragma once

#include "context/ContextProvider.h"

#include <QPointer>

namespace KTextEditor
{
class MainWindow;
class View;
}

namespace KateAiInlineCompletion
{

class OpenTabsContextProvider final : public ContextProvider
{
public:
    OpenTabsContextProvider(KTextEditor::MainWindow *mainWindow, KTextEditor::View *activeView);

    [[nodiscard]] QString id() const override;
    [[nodiscard]] int matchScore(const ContextResolveRequest &request) const override;
    [[nodiscard]] QVector<ContextItem> resolve(const ContextResolveRequest &request) override;

private:
    QPointer<KTextEditor::MainWindow> m_mainWindow;
    QPointer<KTextEditor::View> m_activeView;
};

} // namespace KateAiInlineCompletion
