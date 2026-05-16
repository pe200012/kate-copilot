/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RecentEditsContextProvider

    Converts recent edit history into prompt context items.
*/

#pragma once

#include "context/ContextProvider.h"

#include <QPointer>

namespace KateAiInlineCompletion
{

class RecentEditsTracker;

struct RecentEditsContextOptions {
    bool enabled = true;
    int maxEdits = 8;
    int maxCharsPerEdit = 2000;
    int activeDocDistanceLimitFromCursor = 100;
};

class RecentEditsContextProvider final : public ContextProvider
{
public:
    RecentEditsContextProvider(RecentEditsTracker *tracker, RecentEditsContextOptions options = {});

    [[nodiscard]] QString id() const override;
    [[nodiscard]] int matchScore(const ContextResolveRequest &request) const override;
    [[nodiscard]] QVector<ContextItem> resolve(const ContextResolveRequest &request) override;

private:
    QPointer<RecentEditsTracker> m_tracker;
    RecentEditsContextOptions m_options;
};

} // namespace KateAiInlineCompletion
