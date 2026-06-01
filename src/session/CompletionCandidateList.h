/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionCandidateList

    Stores and cycles deduplicated inline-completion candidates.
*/

#pragma once

#include "session/CompletionCandidate.h"

#include <QVector>

namespace KateAiInlineCompletion
{

class CompletionCandidateList final
{
public:
    void setCandidates(QVector<CompletionCandidate> candidates);
    void addCandidate(const CompletionCandidate &candidate);
    void clear();

    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] int size() const;
    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] CompletionCandidate current() const;

    [[nodiscard]] bool next();
    [[nodiscard]] bool previous();

    [[nodiscard]] QVector<CompletionCandidate> candidates() const;

    [[nodiscard]] static QVector<CompletionCandidate> deduplicated(QVector<CompletionCandidate> candidates);

private:
    QVector<CompletionCandidate> m_candidates;
    int m_currentIndex = -1;
};

} // namespace KateAiInlineCompletion
