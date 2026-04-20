/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: OpenAICompatibleProvider
*/

#include "network/OpenAICompatibleProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace KateAiInlineCompletion
{

OpenAICompatibleProvider::OpenAICompatibleProvider(QNetworkAccessManager *manager, QObject *parent)
    : AbstractAIProvider(parent)
    , m_manager(manager)
{
    if (!m_manager) {
        m_manager = new QNetworkAccessManager(this);
    }
}

quint64 OpenAICompatibleProvider::start(const CompletionRequest &request)
{
    const quint64 requestId = m_nextRequestId++;

    QNetworkRequest netRequest(request.endpoint);
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    netRequest.setRawHeader("Accept", "text/event-stream");

    if (!request.apiKey.isEmpty()) {
        netRequest.setRawHeader("Authorization", QByteArray("Bearer ") + request.apiKey.toUtf8());
    }

    netRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject payload;
    payload[QStringLiteral("model")] = request.model;
    payload[QStringLiteral("stream")] = true;
    payload[QStringLiteral("temperature")] = request.temperature;
    payload[QStringLiteral("max_tokens")] = request.maxTokens;

    if (!request.stopSequences.isEmpty()) {
        payload[QStringLiteral("stop")] = QJsonArray::fromStringList(request.stopSequences);
    }

    QJsonArray messages;
    if (!request.systemPrompt.trimmed().isEmpty()) {
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                    {QStringLiteral("content"), request.systemPrompt}});
    }

    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), request.userPrompt}});

    payload[QStringLiteral("messages")] = messages;

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_manager->post(netRequest, body);

    RequestContext ctx;
    ctx.reply = reply;
    m_requests.insert(requestId, ctx);

    connect(reply, &QNetworkReply::readyRead, this, [this, requestId] {
        auto it = m_requests.find(requestId);
        if (it == m_requests.end() || !it->reply) {
            return;
        }

        const QByteArray bytes = it->reply->readAll();
        const QVector<SSEMessage> messages = it->parser.feed(bytes);

        for (const SSEMessage &m : messages) {
            handleSseData(requestId, m.data);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, requestId] {
        auto it = m_requests.find(requestId);
        if (it == m_requests.end()) {
            return;
        }

        const bool cancelled = it->cancelled;
        const bool finishedNotified = it->finishedNotified;
        const auto reply = it->reply;

        if (reply) {
            reply->deleteLater();
        }

        const QNetworkReply::NetworkError error = reply ? reply->error() : QNetworkReply::UnknownNetworkError;
        const QString errorString = reply ? reply->errorString() : QStringLiteral("Network reply missing");

        m_requests.remove(requestId);

        if (finishedNotified) {
            return;
        }

        if (cancelled || error == QNetworkReply::OperationCanceledError) {
            Q_EMIT requestFinished(requestId);
            return;
        }

        if (error != QNetworkReply::NoError) {
            Q_EMIT requestFailed(requestId, errorString);
            return;
        }

        Q_EMIT requestFinished(requestId);
    });

    return requestId;
}

void OpenAICompatibleProvider::cancel(quint64 requestId)
{
    auto it = m_requests.find(requestId);
    if (it == m_requests.end()) {
        return;
    }

    it->cancelled = true;

    if (it->reply) {
        it->reply->abort();
    }
}

void OpenAICompatibleProvider::handleSseData(quint64 requestId, const QString &data)
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
        const QString message = err.value(QStringLiteral("message")).toString(QStringLiteral("AI provider error"));

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
    const QJsonObject deltaObj = choice0.value(QStringLiteral("delta")).toObject();
    const QString content = deltaObj.value(QStringLiteral("content")).toString();

    if (!content.isEmpty()) {
        Q_EMIT deltaReceived(requestId, content);
    }

    const QString finishReason = choice0.value(QStringLiteral("finish_reason")).toString();
    if (!finishReason.isEmpty()) {
        it->finishedNotified = true;
        Q_EMIT requestFinished(requestId);
    }
}

} // namespace KateAiInlineCompletion
