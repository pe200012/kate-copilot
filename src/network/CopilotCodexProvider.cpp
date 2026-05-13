/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CopilotCodexProvider
*/

#include "network/CopilotCodexProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] QString classifyCopilotFailure(int statusCode, const QString &detail)
{
    const QString clean = detail.trimmed();

    if (statusCode == 401) {
        return clean.isEmpty() ? QStringLiteral("authentication expired") : QStringLiteral("authentication expired: %1").arg(clean);
    }

    if (statusCode == 403) {
        return clean.isEmpty() ? QStringLiteral("subscription or organization access unavailable")
                               : QStringLiteral("subscription or organization access unavailable: %1").arg(clean);
    }

    if (statusCode == 429) {
        return clean.isEmpty() ? QStringLiteral("rate limit reached") : QStringLiteral("rate limit reached: %1").arg(clean);
    }

    if (clean.contains(QStringLiteral("quota"), Qt::CaseInsensitive)
        || clean.contains(QStringLiteral("billing"), Qt::CaseInsensitive)
        || clean.contains(QStringLiteral("entitlement"), Qt::CaseInsensitive)) {
        return QStringLiteral("quota or entitlement issue: %1").arg(clean);
    }

    return clean;
}
}

CopilotCodexProvider::CopilotCodexProvider(QNetworkAccessManager *manager, CopilotAuthManager *authManager, QObject *parent)
    : AbstractAIProvider(parent)
    , m_manager(manager)
    , m_authManager(authManager)
{
    if (!m_manager) {
        m_manager = new QNetworkAccessManager(this);
    }

    if (m_authManager) {
        connect(m_authManager, &CopilotAuthManager::sessionTokenReady, this, &CopilotCodexProvider::onAuthTokenReady);
        connect(m_authManager, &CopilotAuthManager::sessionTokenFailed, this, &CopilotCodexProvider::onAuthTokenFailed);
    }
}

quint64 CopilotCodexProvider::start(const CompletionRequest &request)
{
    const quint64 requestId = m_nextRequestId++;

    RequestContext ctx;
    ctx.request = request;

    if (!m_authManager) {
        Q_EMIT requestFailed(requestId, QStringLiteral("Copilot auth manager missing"));
        return requestId;
    }

    ctx.authAcquireId = m_authManager->acquireSessionToken();
    m_acquireIdToRequestId.insert(ctx.authAcquireId, requestId);

    m_requests.insert(requestId, ctx);
    return requestId;
}

void CopilotCodexProvider::cancel(quint64 requestId)
{
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }

    it->cancelled = true;

    if (it->authAcquireId != 0 && m_authManager) {
        m_acquireIdToRequestId.remove(it->authAcquireId);
        m_authManager->cancelAcquire(it->authAcquireId);
        it->authAcquireId = 0;
    }

    if (it->reply) {
        it->reply->abort();
    }
}

void CopilotCodexProvider::onAuthTokenReady(quint64 acquireId, const QString &token, const QUrl &apiBaseUrl, qint64 expiresAtMs)
{
    Q_UNUSED(apiBaseUrl);
    Q_UNUSED(expiresAtMs);

    const auto mapIt = m_acquireIdToRequestId.find(acquireId);
    if (mapIt == m_acquireIdToRequestId.end()) {
        return;
    }

    const quint64 requestId = mapIt.value();
    m_acquireIdToRequestId.erase(mapIt);

    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }

    it->authAcquireId = 0;

    if (it->cancelled) {
        it->finishedNotified = true;
        Q_EMIT requestFinished(requestId);
        m_requests.remove(requestId);
        return;
    }

    beginNetworkRequest(requestId, token);
}

void CopilotCodexProvider::onAuthTokenFailed(quint64 acquireId, const QString &message)
{
    const auto mapIt = m_acquireIdToRequestId.find(acquireId);
    if (mapIt == m_acquireIdToRequestId.end()) {
        return;
    }

    const quint64 requestId = mapIt.value();
    m_acquireIdToRequestId.erase(mapIt);

    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }

    it->authAcquireId = 0;
    it->finishedNotified = true;

    if (it->cancelled) {
        Q_EMIT requestFinished(requestId);
        m_requests.remove(requestId);
        return;
    }

    Q_EMIT requestFailed(requestId, message);
    m_requests.remove(requestId);
}

void CopilotCodexProvider::beginNetworkRequest(quint64 requestId, const QString &sessionToken)
{
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }

    QNetworkRequest netRequest(it->request.endpoint);
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    netRequest.setRawHeader("Accept", "text/event-stream");
    netRequest.setRawHeader("Authorization", buildAuthorizationHeaderValue(sessionToken));

    applyCopilotClientHeaders(&netRequest);

    netRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject payload;
    payload[QStringLiteral("prompt")] = it->request.prompt;
    payload[QStringLiteral("suffix")] = it->request.suffix;
    payload[QStringLiteral("model")] = it->request.model;
    payload[QStringLiteral("stream")] = true;
    payload[QStringLiteral("temperature")] = it->request.temperature;
    payload[QStringLiteral("max_tokens")] = it->request.maxTokens;

    if (it->request.topP > 0.0) {
        payload[QStringLiteral("top_p")] = it->request.topP;
    }

    if (it->request.n > 0) {
        payload[QStringLiteral("n")] = it->request.n;
    }

    if (!it->request.stopSequences.isEmpty()) {
        payload[QStringLiteral("stop")] = QJsonArray::fromStringList(it->request.stopSequences);
    }

    if (!it->request.nwo.trimmed().isEmpty()) {
        payload[QStringLiteral("nwo")] = it->request.nwo;
    }

    if (!it->request.extra.isEmpty()) {
        payload[QStringLiteral("extra")] = it->request.extra;
    }

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_manager->post(netRequest, body);
    it->reply = reply;

    connect(reply, &QNetworkReply::readyRead, this, [this, requestId] {
        auto it2 = m_requests.find(requestId);
        if (it2 == m_requests.end() || !it2->reply) {
            return;
        }

        const QByteArray bytes = it2->reply->readAll();
        const QVector<SSEMessage> messages = it2->parser.feed(bytes);

        for (const SSEMessage &m : messages) {
            handleSseData(requestId, m.data);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, requestId] {
        auto it2 = m_requests.find(requestId);
        if (it2 == m_requests.end()) {
            return;
        }

        const bool cancelled = it2->cancelled;
        const bool finishedNotified = it2->finishedNotified;
        QNetworkReply *reply2 = it2->reply;

        const int statusCode = reply2 ? reply2->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;
        const QNetworkReply::NetworkError error = reply2 ? reply2->error() : QNetworkReply::UnknownNetworkError;
        const QString errorString = reply2 ? reply2->errorString() : QStringLiteral("Network reply missing");
        const QByteArray responseBody = reply2 ? reply2->readAll() : QByteArray{};

        if (reply2) {
            reply2->deleteLater();
        }

        it2->reply = nullptr;

        if (finishedNotified) {
            m_requests.remove(requestId);
            return;
        }

        if (cancelled || error == QNetworkReply::OperationCanceledError) {
            Q_EMIT requestFinished(requestId);
            m_requests.remove(requestId);
            return;
        }

        if ((statusCode == 401 || statusCode == 403) && !it2->retriedAfterAuth && m_authManager) {
            it2->retriedAfterAuth = true;
            it2->parser.reset();
            m_authManager->invalidateSessionToken();
            it2->authAcquireId = m_authManager->acquireSessionToken();
            m_acquireIdToRequestId.insert(it2->authAcquireId, requestId);
            return;
        }

        if (statusCode >= 400) {
            QString detail;

            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                const QJsonObject obj = doc.object();

                if (obj.contains(QStringLiteral("error"))) {
                    const QJsonValue errVal = obj.value(QStringLiteral("error"));
                    if (errVal.isObject()) {
                        detail = errVal.toObject().value(QStringLiteral("message")).toString();
                    } else if (errVal.isString()) {
                        detail = errVal.toString();
                    }
                }

                if (detail.trimmed().isEmpty()) {
                    detail = obj.value(QStringLiteral("message")).toString();
                }
            }

            if (detail.trimmed().isEmpty()) {
                detail = QString::fromUtf8(responseBody).trimmed();
            }

            if (detail.trimmed().isEmpty()) {
                detail = errorString;
            }

            const QString classified = classifyCopilotFailure(statusCode, detail);
            const QString finalDetail = classified.trimmed().isEmpty() ? errorString : classified;
            Q_EMIT requestFailed(requestId, QStringLiteral("Copilot HTTP %1: %2").arg(statusCode).arg(finalDetail));
            m_requests.remove(requestId);
            return;
        }

        if (error != QNetworkReply::NoError) {
            Q_EMIT requestFailed(requestId, errorString);
            m_requests.remove(requestId);
            return;
        }

        Q_EMIT requestFinished(requestId);
        m_requests.remove(requestId);
    });
}

void CopilotCodexProvider::handleSseData(quint64 requestId, const QString &data)
{
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }

    if (it->finishedNotified) {
        return;
    }

    const QString trimmed = data.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (trimmed == QStringLiteral("[DONE]")) {
        it->finishedNotified = true;
        Q_EMIT requestFinished(requestId);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        it->finishedNotified = true;
        Q_EMIT requestFailed(requestId, QStringLiteral("Invalid JSON in SSE data"));
        if (it->reply) {
            it->reply->abort();
        }
        return;
    }

    const QJsonObject obj = doc.object();

    if (obj.contains(QStringLiteral("error"))) {
        const QJsonObject err = obj.value(QStringLiteral("error")).toObject();
        const QString message = err.value(QStringLiteral("message")).toString(QStringLiteral("Copilot provider error"));

        it->finishedNotified = true;
        Q_EMIT requestFailed(requestId, message);
        if (it->reply) {
            it->reply->abort();
        }
        return;
    }

    const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        return;
    }

    const QJsonObject choice0 = choices.at(0).toObject();
    const QString text = choice0.value(QStringLiteral("text")).toString();
    if (!text.isEmpty()) {
        Q_EMIT deltaReceived(requestId, text);
    }

    const QString finishReason = choice0.value(QStringLiteral("finish_reason")).toString();
    if (!finishReason.isEmpty()) {
        it->finishedNotified = true;
        Q_EMIT requestFinished(requestId);
    }
}

QByteArray CopilotCodexProvider::buildAuthorizationHeaderValue(const QString &token)
{
    return QByteArray("Bearer ") + token.toUtf8();
}

void CopilotCodexProvider::applyCopilotClientHeaders(QNetworkRequest *req)
{
    if (!req) {
        return;
    }

    req->setRawHeader("User-Agent", "GitHubCopilotChat/0.26.7");
    req->setRawHeader("Editor-Version", "vscode/1.99.3");
    req->setRawHeader("Editor-Plugin-Version", "copilot-chat/0.26.7");
    req->setRawHeader("Copilot-Integration-Id", "vscode-chat");
}

} // namespace KateAiInlineCompletion
