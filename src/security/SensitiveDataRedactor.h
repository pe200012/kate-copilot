/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SensitiveDataRedactor

    Small header-only helper for user-visible network/auth error details.
*/

#pragma once

#include <QRegularExpression>
#include <QString>

namespace KateAiInlineCompletion
{

inline constexpr int kDefaultMaxRedactedDetailChars = 400;

[[nodiscard]] inline QString redactSensitiveData(QString detail, int maxChars = kDefaultMaxRedactedDetailChars)
{
    detail = detail.simplified();
    if (detail.isEmpty()) {
        return {};
    }

    static const QRegularExpression bearerToken(QStringLiteral("\\bBearer\\s+[-A-Za-z0-9._~+/=]+"));
    detail.replace(bearerToken, QStringLiteral("Bearer <redacted>"));

    static const QRegularExpression jsonStringField(
        QStringLiteral("(?i)\"(token|access_token|oauth_token|refresh_token|session_token|api[_-]?key|authorization|password|secret)\"\\s*:\\s*\"[^\"]*\""));
    detail.replace(jsonStringField, QStringLiteral("\"\\1\":\"<redacted>\""));

    static const QRegularExpression jsonBareField(
        QStringLiteral("(?i)\"(token|access_token|oauth_token|refresh_token|session_token|api[_-]?key|authorization|password|secret)\"\\s*:\\s*(?!\")[^\\s,}\\]]+"));
    detail.replace(jsonBareField, QStringLiteral("\"\\1\":<redacted>"));

    static const QRegularExpression quotedAssignment(
        QStringLiteral("(?i)\\b(token|access_token|oauth_token|refresh_token|session_token|api[_-]?key|authorization|password|secret)\\b(\\s*[:=]\\s*\")[^\"]*(\")"));
    detail.replace(quotedAssignment, QStringLiteral("\\1\\2<redacted>\\3"));

    static const QRegularExpression bareAssignment(
        QStringLiteral("(?i)\\b(token|access_token|oauth_token|refresh_token|session_token|api[_-]?key|authorization|password|secret)\\b(\\s*[:=]\\s*)[^\\s,}\\]]+"));
    detail.replace(bareAssignment, QStringLiteral("\\1\\2<redacted>"));

    if (maxChars > 0 && detail.size() > maxChars) {
        detail = detail.left(maxChars - 1) + QStringLiteral("…");
    }

    return detail;
}

} // namespace KateAiInlineCompletion
