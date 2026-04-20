/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextState

    Holds the current AI suggestion state for a single editor view.
*/

#pragma once

#include <QString>

#include <cstdint>

namespace KateAiInlineCompletion
{

struct SuggestionAnchor {
    int line = -1;
    int column = -1;
    std::uint64_t generation = 0;
};

struct GhostTextState {
    SuggestionAnchor anchor;
    bool anchorTracked = false;
    QString visibleText;
    bool streaming = false;
    bool suppressed = false;
};

} // namespace KateAiInlineCompletion
