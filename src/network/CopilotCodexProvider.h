/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CopilotCodexProvider

    Streaming completion provider for GitHub Copilot inline suggestions.

    Endpoint (default):
    - https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions

    Protocol:
    - request: JSON with prompt + suffix, stream=true
    - response: SSE with `data: {json}` chunks and `data: [DONE]`
      where json contains `choices[0].text` deltas.
*/

#pragma once

#include "auth/CopilotAuthManager.h"
#include "network/AbstractAIProvider.h"
#include "network/SSEParser.h"

#include <QHash>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

namespace KateAiInlineCompletion
{

class CopilotCodexProvider final : public AbstractAIProvider
{
    Q_OBJECT

public:
    explicit CopilotCodexProvider(QNetworkAccessManager *manager,
                                  CopilotAuthManager *authManager,
                                  QObject *parent = nullptr);

    quint64 start(const CompletionRequest &request) override;
    void cancel(quint64 requestId) override;

private Q_SLOTS:
    void onAuthTokenReady(quint64 acquireId, const QString &token, const QUrl &apiBaseUrl, qint64 expiresAtMs);
    void onAuthTokenFailed(quint64 acquireId, const QString &message);

private:
    struct RequestContext {
        CompletionRequest request;
        quint64 authAcquireId = 0;
        QPointer<QNetworkReply> reply;
        SSEParser parser;
        bool cancelled = false;
        bool finishedNotified = false;
        bool retriedAfterAuth = false;
    };

    void beginNetworkRequest(quint64 requestId, const QString &sessionToken);
    void handleSseData(quint64 requestId, const QString &data);

    [[nodiscard]] static QByteArray buildAuthorizationHeaderValue(const QString &token);
    static void applyCopilotClientHeaders(QNetworkRequest *req);

    QNetworkAccessManager *m_manager = nullptr;
    CopilotAuthManager *m_authManager = nullptr;

    quint64 m_nextRequestId = 1;
    QHash<quint64, RequestContext> m_requests;
    QHash<quint64, quint64> m_acquireIdToRequestId;
};

} // namespace KateAiInlineCompletion
