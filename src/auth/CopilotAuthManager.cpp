/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CopilotAuthManager
*/

#include "auth/CopilotAuthManager.h"

#include "settings/KWalletSecretStore.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

#include <utility>

namespace KateAiInlineCompletion
{

namespace
{
constexpr const char *kGitHubCopilotTokenUrl = "https://api.github.com/copilot_internal/v2/token";

struct CopilotClientHeaders {
    QByteArray userAgent;
    QByteArray editorVersion;
    QByteArray editorPluginVersion;
    QByteArray copilotIntegrationId;
};

[[nodiscard]] CopilotClientHeaders defaultCopilotHeaders()
{
    CopilotClientHeaders h;
    h.userAgent = QByteArrayLiteral("GitHubCopilotChat/0.26.7");
    h.editorVersion = QByteArrayLiteral("vscode/1.99.3");
    h.editorPluginVersion = QByteArrayLiteral("copilot-chat/0.26.7");
    h.copilotIntegrationId = QByteArrayLiteral("vscode-chat");
    return h;
}

[[nodiscard]] QString extractErrorDetail(const QByteArray &body)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return QString::fromUtf8(body).trimmed();
    }

    const QJsonObject obj = doc.object();
    const QJsonValue errVal = obj.value(QStringLiteral("error"));
    if (errVal.isObject()) {
        const QString msg = errVal.toObject().value(QStringLiteral("message")).toString().trimmed();
        if (!msg.isEmpty()) {
            return msg;
        }
    }
    if (errVal.isString()) {
        return errVal.toString().trimmed();
    }

    return obj.value(QStringLiteral("message")).toString().trimmed();
}

[[nodiscard]] QString classifyExchangeFailure(int statusCode, const QString &detail, const QString &fallback)
{
    const QString clean = detail.trimmed().isEmpty() ? fallback.trimmed() : detail.trimmed();

    if (statusCode == 401) {
        return clean.isEmpty() ? QStringLiteral("Copilot token exchange failed: GitHub OAuth sign-in expired")
                               : QStringLiteral("Copilot token exchange failed: GitHub OAuth sign-in expired (%1)").arg(clean);
    }

    if (statusCode == 403) {
        return clean.isEmpty() ? QStringLiteral("Copilot token exchange failed: subscription or organization access unavailable")
                               : QStringLiteral("Copilot token exchange failed: subscription or organization access unavailable (%1)").arg(clean);
    }

    if (statusCode == 429) {
        return clean.isEmpty() ? QStringLiteral("Copilot token exchange failed: rate limit reached")
                               : QStringLiteral("Copilot token exchange failed: rate limit reached (%1)").arg(clean);
    }

    return clean.isEmpty() ? QStringLiteral("Copilot token exchange failed (%1)").arg(statusCode)
                           : QStringLiteral("Copilot token exchange failed (%1): %2").arg(statusCode).arg(clean);
}

} // namespace

CopilotAuthManager::CopilotAuthManager(KWalletSecretStore *secretStore, QNetworkAccessManager *networkManager, QObject *parent)
    : QObject(parent)
    , m_secretStore(secretStore)
    , m_networkManager(networkManager)
{
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
    }
}

bool CopilotAuthManager::hasGitHubOAuthToken() const
{
    if (!m_secretStore) {
        return false;
    }

    return m_secretStore->hasGitHubOAuthToken();
}

QString CopilotAuthManager::lastErrorString() const
{
    return m_lastError;
}

quint64 CopilotAuthManager::acquireSessionToken()
{
    const quint64 acquireId = m_nextAcquireId++;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 remainingMs = m_cached.expiresAtMs - nowMs;

    if (!m_cached.token.isEmpty() && remainingMs > kRefreshMarginMs) {
        const QString token = m_cached.token;
        const QUrl apiBaseUrl = m_cached.apiBaseUrl;
        const qint64 expiresAtMs = m_cached.expiresAtMs;

        QMetaObject::invokeMethod(this, [this, acquireId, token, apiBaseUrl, expiresAtMs] {
            Q_EMIT sessionTokenReady(acquireId, token, apiBaseUrl, expiresAtMs);
        }, Qt::QueuedConnection);

        return acquireId;
    }

    m_waiters.insert(acquireId, PendingAcquire{});
    startExchangeIfNeeded();
    return acquireId;
}

void CopilotAuthManager::cancelAcquire(quint64 acquireId)
{
    auto it = m_waiters.find(acquireId);
    if (it == m_waiters.end()) {
        return;
    }

    m_waiters.erase(it);

    if (m_waiters.isEmpty() && m_exchangeReply) {
        m_exchangeReply->abort();
    }
}

void CopilotAuthManager::invalidateSessionToken()
{
    m_cached = CachedSessionToken{};
}

void CopilotAuthManager::startExchangeIfNeeded()
{
    if (m_waiters.isEmpty()) {
        return;
    }

    if (m_exchangeReply) {
        return;
    }

    if (!m_secretStore) {
        completeAllWithError(QStringLiteral("Secret store unavailable"));
        return;
    }

    const QString oauth = m_secretStore->readGitHubOAuthToken().trimmed();
    if (oauth.isEmpty()) {
        completeAllWithError(QStringLiteral("GitHub OAuth token missing. Sign in under the Copilot provider settings."));
        return;
    }

    QNetworkRequest req(QUrl(QString::fromLatin1(kGitHubCopilotTokenUrl)));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", QByteArray("Bearer ") + oauth.toUtf8());

    const CopilotClientHeaders headers = defaultCopilotHeaders();
    req.setRawHeader("User-Agent", headers.userAgent);
    req.setRawHeader("Editor-Version", headers.editorVersion);
    req.setRawHeader("Editor-Plugin-Version", headers.editorPluginVersion);
    req.setRawHeader("Copilot-Integration-Id", headers.copilotIntegrationId);

    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_exchangeReply = m_networkManager->get(req);

    connect(m_exchangeReply, &QNetworkReply::finished, this, &CopilotAuthManager::onExchangeFinished);
}

void CopilotAuthManager::onExchangeFinished()
{
    QNetworkReply *reply = m_exchangeReply;
    m_exchangeReply = nullptr;

    if (!reply) {
        completeAllWithError(QStringLiteral("Network reply missing"));
        return;
    }

    const QByteArray body = reply->readAll();
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorString = reply->errorString();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    reply->deleteLater();

    if (error == QNetworkReply::OperationCanceledError) {
        completeAllWithError(QStringLiteral("Copilot token exchange cancelled"));
        return;
    }

    if (error != QNetworkReply::NoError) {
        completeAllWithError(classifyExchangeFailure(statusCode, extractErrorDetail(body), errorString));
        return;
    }

    QString parseError;
    const CachedSessionToken token = parseTokenResponse(body, &parseError);
    if (!parseError.isEmpty()) {
        completeAllWithError(parseError);
        return;
    }

    m_cached = token;
    completeAllWithToken(token);
}

void CopilotAuthManager::completeAllWithError(const QString &message)
{
    setError(message);

    const auto waiters = m_waiters;
    m_waiters.clear();

    for (auto it = waiters.begin(); it != waiters.end(); ++it) {
        Q_EMIT sessionTokenFailed(it.key(), message);
    }
}

void CopilotAuthManager::completeAllWithToken(const CachedSessionToken &token)
{
    setError({});

    const auto waiters = m_waiters;
    m_waiters.clear();

    for (auto it = waiters.begin(); it != waiters.end(); ++it) {
        Q_EMIT sessionTokenReady(it.key(), token.token, token.apiBaseUrl, token.expiresAtMs);
    }
}

CopilotAuthManager::CachedSessionToken CopilotAuthManager::parseTokenResponse(const QByteArray &bytes, QString *errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Copilot token response JSON parse failed");
        }
        return {};
    }

    const QJsonObject obj = doc.object();

    const QString token = obj.value(QStringLiteral("token")).toString();
    if (token.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Copilot token response missing token");
        }
        return {};
    }

    qint64 expiresAtMs = 0;
    const QJsonValue expiresVal = obj.value(QStringLiteral("expires_at"));

    if (expiresVal.isDouble()) {
        expiresAtMs = coerceUnixTimestampMs(static_cast<qint64>(expiresVal.toDouble()));
    } else if (expiresVal.isString()) {
        bool ok = false;
        const qint64 parsed = expiresVal.toString().trimmed().toLongLong(&ok);
        if (ok) {
            expiresAtMs = coerceUnixTimestampMs(parsed);
        }
    }

    if (expiresAtMs <= 0) {
        expiresAtMs = parseExpFromTokenStringMs(token);
    }

    if (expiresAtMs <= 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Copilot token response missing expires_at");
        }
        return {};
    }

    QUrl apiBaseUrl;
    const QJsonObject endpoints = obj.value(QStringLiteral("endpoints")).toObject();
    const QString api = endpoints.value(QStringLiteral("api")).toString();
    if (!api.isEmpty()) {
        apiBaseUrl = QUrl(api);
    }

    CachedSessionToken out;
    out.token = token;
    out.apiBaseUrl = apiBaseUrl;
    out.expiresAtMs = expiresAtMs;
    return out;
}

qint64 CopilotAuthManager::coerceUnixTimestampMs(qint64 value)
{
    if (value > 10'000'000'000LL) {
        return value;
    }

    return value * 1000LL;
}

qint64 CopilotAuthManager::parseExpFromTokenStringMs(const QString &token)
{
    static const QRegularExpression re(QStringLiteral("(?:^|;)\\s*exp=(\\d+)"));
    const QRegularExpressionMatch m = re.match(token);
    if (!m.hasMatch()) {
        return 0;
    }

    bool ok = false;
    const qint64 parsed = m.captured(1).toLongLong(&ok);
    if (!ok) {
        return 0;
    }

    return coerceUnixTimestampMs(parsed);
}

void CopilotAuthManager::setError(QString message)
{
    m_lastError = std::move(message);
}

} // namespace KateAiInlineCompletion
