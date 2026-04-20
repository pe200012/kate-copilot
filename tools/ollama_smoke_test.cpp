/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: OllamaSmokeTest

    A small CLI tool to validate OpenAI-compatible streaming against an Ollama server.

    Example:
      ./kateaiinlinecompletion_ollama_smoke_test \
        --endpoint http://192.168.62.31:11434/v1/chat/completions \
        --model codestral:latest \
        --prompt "Write a single-line Python program that prints hello."
*/

#include "network/OpenAICompatibleProvider.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QTextStream>
#include <QTimer>

using KateAiInlineCompletion::CompletionRequest;
using KateAiInlineCompletion::OpenAICompatibleProvider;

static QTextStream qout(stdout);
static QTextStream qerr(stderr);

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kateaiinlinecompletion_ollama_smoke_test"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Ollama OpenAI-compatible streaming smoke test"));
    parser.addHelpOption();

    const QCommandLineOption endpointOpt(
        QStringList{QStringLiteral("e"), QStringLiteral("endpoint")},
        QStringLiteral("OpenAI-compatible chat completions endpoint"),
        QStringLiteral("url"),
        QStringLiteral("http://192.168.62.31:11434/v1/chat/completions"));

    const QCommandLineOption modelOpt(QStringList{QStringLiteral("m"), QStringLiteral("model")},
                                     QStringLiteral("Model id"),
                                     QStringLiteral("model"),
                                     QStringLiteral("codestral:latest"));

    const QCommandLineOption promptOpt(QStringList{QStringLiteral("p"), QStringLiteral("prompt")},
                                      QStringLiteral("User prompt"),
                                      QStringLiteral("text"),
                                      QStringLiteral("Write a single-line Python program that prints hello."));

    const QCommandLineOption systemOpt(QStringList{QStringLiteral("s"), QStringLiteral("system")},
                                      QStringLiteral("System prompt"),
                                      QStringLiteral("text"),
                                      QString());

    const QCommandLineOption apiKeyOpt(QStringList{QStringLiteral("k"), QStringLiteral("api-key")},
                                      QStringLiteral("API key (Ollama accepts any value)"),
                                      QStringLiteral("key"),
                                      QStringLiteral("ollama"));

    const QCommandLineOption maxTokensOpt(QStringList{QStringLiteral("t"), QStringLiteral("max-tokens")},
                                         QStringLiteral("max_tokens"),
                                         QStringLiteral("n"),
                                         QStringLiteral("80"));

    const QCommandLineOption temperatureOpt(QStringList{QStringLiteral("T"), QStringLiteral("temperature")},
                                           QStringLiteral("temperature"),
                                           QStringLiteral("value"),
                                           QStringLiteral("0.2"));

    const QCommandLineOption timeoutOpt(QStringList{QStringLiteral("timeout-ms")},
                                       QStringLiteral("Timeout in milliseconds"),
                                       QStringLiteral("ms"),
                                       QStringLiteral("90000"));

    parser.addOption(endpointOpt);
    parser.addOption(modelOpt);
    parser.addOption(promptOpt);
    parser.addOption(systemOpt);
    parser.addOption(apiKeyOpt);
    parser.addOption(maxTokensOpt);
    parser.addOption(temperatureOpt);
    parser.addOption(timeoutOpt);

    parser.process(app);

    const QUrl endpoint(parser.value(endpointOpt));
    if (!endpoint.isValid() || endpoint.isRelative()) {
        qerr << "Invalid endpoint URL: " << parser.value(endpointOpt) << "\n";
        return 2;
    }

    bool okTokens = false;
    const int maxTokens = parser.value(maxTokensOpt).toInt(&okTokens);
    if (!okTokens || maxTokens <= 0) {
        qerr << "Invalid max_tokens: " << parser.value(maxTokensOpt) << "\n";
        return 2;
    }

    bool okTemp = false;
    const double temperature = parser.value(temperatureOpt).toDouble(&okTemp);
    if (!okTemp || temperature < 0.0 || temperature > 2.0) {
        qerr << "Invalid temperature: " << parser.value(temperatureOpt) << "\n";
        return 2;
    }

    bool okTimeout = false;
    const int timeoutMs = parser.value(timeoutOpt).toInt(&okTimeout);
    if (!okTimeout || timeoutMs < 1000) {
        qerr << "Invalid timeout-ms: " << parser.value(timeoutOpt) << "\n";
        return 2;
    }

    CompletionRequest req;
    req.endpoint = endpoint;
    req.apiKey = parser.value(apiKeyOpt);
    req.model = parser.value(modelOpt);
    req.systemPrompt = parser.value(systemOpt);
    req.userPrompt = parser.value(promptOpt);
    req.maxTokens = maxTokens;
    req.temperature = temperature;

    QNetworkAccessManager manager;
    OpenAICompatibleProvider provider(&manager);

    const quint64 requestId = provider.start(req);

    QString out;
    bool finished = false;
    bool failed = false;
    QString error;

    QEventLoop loop;

    QObject::connect(&provider, &OpenAICompatibleProvider::deltaReceived, &app, [&](quint64 id, QString delta) {
        if (id != requestId) {
            return;
        }

        out += delta;
        qout << delta;
        qout.flush();
    });

    QObject::connect(&provider, &OpenAICompatibleProvider::requestFinished, &app, [&](quint64 id) {
        if (id != requestId) {
            return;
        }
        finished = true;
        loop.quit();
    });

    QObject::connect(&provider, &OpenAICompatibleProvider::requestFailed, &app, [&](quint64 id, QString message) {
        if (id != requestId) {
            return;
        }
        failed = true;
        error = std::move(message);
        loop.quit();
    });

    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(timeoutMs);
    QObject::connect(&timeout, &QTimer::timeout, &app, [&] {
        failed = true;
        error = QStringLiteral("timeout");
        provider.cancel(requestId);
        loop.quit();
    });

    loop.exec();
    qout << "\n";

    if (failed) {
        qerr << "request failed: " << error << "\n";
        return 1;
    }

    if (!finished) {
        qerr << "request ended without a finish signal\n";
        return 1;
    }

    qout << "---\n";
    qout << "endpoint: " << endpoint.toString() << "\n";
    qout << "model: " << req.model << "\n";
    qout << "chars: " << out.size() << "\n";

    return 0;
}
