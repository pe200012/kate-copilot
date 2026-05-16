/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: DiagnosticStore

    In-memory URI-keyed diagnostic store for contextual prompt providers.
*/

#pragma once

#include "context/DiagnosticItem.h"

#include <QHash>
#include <QObject>
#include <QVector>

namespace KateAiInlineCompletion
{

class DiagnosticStore final : public QObject
{
    Q_OBJECT

public:
    explicit DiagnosticStore(QObject *parent = nullptr);

    void setDiagnostics(const QString &uri, const QVector<DiagnosticItem> &diagnostics);
    void clearDiagnostics(const QString &uri);
    void clear();

    [[nodiscard]] QVector<DiagnosticItem> diagnostics(const QString &uri) const;
    [[nodiscard]] QVector<DiagnosticItem> allDiagnostics() const;
    [[nodiscard]] bool isEmpty() const;

private:
    [[nodiscard]] DiagnosticItem normalizeDiagnostic(const QString &uri, const DiagnosticItem &diagnostic) const;

    QHash<QString, QVector<DiagnosticItem>> m_diagnosticsByUri;
};

} // namespace KateAiInlineCompletion
