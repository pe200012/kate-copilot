/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CompletionCandidateList
*/

#include "session/CompletionCandidateList.h"

#include <QSet>
#include <QStringList>
#include <QtGlobal>

#include <utility>

namespace KateAiInlineCompletion
{
namespace
{
QString normalizedCandidateKey(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        while (!line.isEmpty() && line.back().isSpace()) {
            line.chop(1);
        }
    }

    return lines.join(QLatin1Char('\n')).trimmed();
}

bool candidateHasText(const CompletionCandidate &candidate)
{
    return !candidate.insertText.trimmed().isEmpty() && !candidate.displayText.trimmed().isEmpty();
}
}

void CompletionCandidateList::setCandidates(QVector<CompletionCandidate> candidates)
{
    m_candidates = deduplicated(std::move(candidates));
    m_currentIndex = m_candidates.isEmpty() ? -1 : 0;
}

void CompletionCandidateList::addCandidate(const CompletionCandidate &candidate)
{
    QVector<CompletionCandidate> next = m_candidates;
    const int previousIndex = m_currentIndex;

    if (!candidate.id.isEmpty()) {
        for (CompletionCandidate &existing : next) {
            if (existing.id == candidate.id) {
                existing = candidate;
                m_candidates = deduplicated(std::move(next));
                m_currentIndex = m_candidates.isEmpty() ? -1 : qBound(0, previousIndex, m_candidates.size() - 1);
                return;
            }
        }
    }

    next.push_back(candidate);
    m_candidates = deduplicated(std::move(next));
    if (m_candidates.isEmpty()) {
        m_currentIndex = -1;
        return;
    }

    m_currentIndex = qBound(0, previousIndex, m_candidates.size() - 1);
}

void CompletionCandidateList::clear()
{
    m_candidates.clear();
    m_currentIndex = -1;
}

bool CompletionCandidateList::isEmpty() const
{
    return m_candidates.isEmpty();
}

int CompletionCandidateList::size() const
{
    return m_candidates.size();
}

int CompletionCandidateList::currentIndex() const
{
    return m_currentIndex;
}

CompletionCandidate CompletionCandidateList::current() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_candidates.size()) {
        return {};
    }

    return m_candidates.at(m_currentIndex);
}

bool CompletionCandidateList::next()
{
    if (m_candidates.isEmpty()) {
        return false;
    }

    m_currentIndex = (m_currentIndex + 1) % m_candidates.size();
    return true;
}

bool CompletionCandidateList::previous()
{
    if (m_candidates.isEmpty()) {
        return false;
    }

    m_currentIndex = (m_currentIndex - 1 + m_candidates.size()) % m_candidates.size();
    return true;
}

QVector<CompletionCandidate> CompletionCandidateList::candidates() const
{
    return m_candidates;
}

QVector<CompletionCandidate> CompletionCandidateList::deduplicated(QVector<CompletionCandidate> candidates)
{
    QVector<CompletionCandidate> out;
    QSet<QString> seen;

    for (const CompletionCandidate &candidate : std::as_const(candidates)) {
        if (!candidateHasText(candidate)) {
            continue;
        }

        const QString key = normalizedCandidateKey(candidate.insertText);
        if (key.isEmpty() || seen.contains(key)) {
            continue;
        }

        seen.insert(key);
        out.push_back(candidate);
    }

    return out;
}

} // namespace KateAiInlineCompletion
