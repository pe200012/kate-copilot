/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: AbstractAIProvider

    Defines the streaming completion provider interface.
*/

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QUrl>

namespace KateAiInlineCompletion
{

struct CompletionRequest {
    QUrl endpoint;
    QString apiKey;
    QString model;

    // Chat-style prompts
    QString systemPrompt;
    QString userPrompt;

    // Completion-style prompts (prompt + suffix)
    QString prompt;
    QString suffix;

    // Provider-specific controls
    QString nwo;
    QJsonObject extra;

    QStringList stopSequences;

    int maxTokens = 256;
    double temperature = 0.2;
    double topP = 1.0;
    int n = 1;
};

class AbstractAIProvider : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    ~AbstractAIProvider() override = default;

    virtual quint64 start(const CompletionRequest &request) = 0;
    virtual void cancel(quint64 requestId) = 0;

Q_SIGNALS:
    void deltaReceived(quint64 requestId, QString delta);
    void requestFinished(quint64 requestId);
    void requestFailed(quint64 requestId, QString message);
};

} // namespace KateAiInlineCompletion
