/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticsContextProvider

    Converts stored diagnostics into bounded contextual prompt items.
*/

#pragma once

#include "context/ContextProvider.h"

#include <QPointer>

namespace KateAiInlineCompletion
{

class DiagnosticStore;

struct DiagnosticsContextOptions {
    bool enabled = true;
    int maxItems = 8;
    int maxChars = 3000;
    int maxLineDistance = 120;
    bool includeWarnings = true;
    bool includeInformation = false;
    bool includeHints = false;
};

class DiagnosticsContextProvider final : public ContextProvider
{
public:
    DiagnosticsContextProvider(DiagnosticStore *store, DiagnosticsContextOptions options = {});

    [[nodiscard]] QString id() const override;
    [[nodiscard]] int matchScore(const ContextResolveRequest &request) const override;
    [[nodiscard]] QVector<ContextItem> resolve(const ContextResolveRequest &request) override;

private:
    QPointer<DiagnosticStore> m_store;
    DiagnosticsContextOptions m_options;
};

} // namespace KateAiInlineCompletion
