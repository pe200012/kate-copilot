/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: OpenAICompatibleProvider

    Implements OpenAI-style chat/completions streaming over SSE.
    Compatible endpoints include OpenAI and Ollama OpenAI-compat API.
*/

#pragma once

#include "network/AbstractAIProvider.h"
#include "network/SSEParser.h"

#include <QHash>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;

namespace KateAiInlineCompletion
{

class OpenAICompatibleProvider final : public AbstractAIProvider
{
    Q_OBJECT

public:
    explicit OpenAICompatibleProvider(QNetworkAccessManager *manager, QObject *parent = nullptr);

    quint64 start(const CompletionRequest &request) override;
    void cancel(quint64 requestId) override;

private:
    struct RequestContext {
        QPointer<QNetworkReply> reply;
        SSEParser parser;
        bool cancelled = false;
        bool finishedNotified = false;
    };

    void handleSseData(quint64 requestId, const QString &data);

    QNetworkAccessManager *m_manager = nullptr;
    quint64 m_nextRequestId = 1;
    QHash<quint64, RequestContext> m_requests;
};

} // namespace KateAiInlineCompletion
