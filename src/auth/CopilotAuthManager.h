/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CopilotAuthManager

    Manages GitHub Copilot authentication for editor completions.

    Responsibilities:
    - read GitHub OAuth token from KWalletSecretStore
    - exchange it for a short-lived Copilot session token via
      https://api.github.com/copilot_internal/v2/token
    - cache the Copilot session token in-memory with an expiry margin
    - coalesce concurrent exchange requests
*/

#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace KateAiInlineCompletion
{

class KWalletSecretStore;

class CopilotAuthManager final : public QObject
{
    Q_OBJECT

public:
    static constexpr qint64 kRefreshMarginMs = 5LL * 60LL * 1000LL;

    explicit CopilotAuthManager(KWalletSecretStore *secretStore,
                                QNetworkAccessManager *networkManager,
                                QObject *parent = nullptr);

    [[nodiscard]] bool hasGitHubOAuthToken() const;

    [[nodiscard]] QString lastErrorString() const;

    quint64 acquireSessionToken();
    void cancelAcquire(quint64 acquireId);

    void invalidateSessionToken();

Q_SIGNALS:
    void sessionTokenReady(quint64 acquireId, QString token, QUrl apiBaseUrl, qint64 expiresAtMs);
    void sessionTokenFailed(quint64 acquireId, QString message);

private:
    struct CachedSessionToken {
        QString token;
        QUrl apiBaseUrl;
        qint64 expiresAtMs = 0;
    };

    struct PendingAcquire {
        bool cancelled = false;
    };

    void startExchangeIfNeeded();
    void onExchangeFinished();

    void completeAllWithError(const QString &message);
    void completeAllWithToken(const CachedSessionToken &token);

    [[nodiscard]] static CachedSessionToken parseTokenResponse(const QByteArray &bytes, QString *errorOut);
    [[nodiscard]] static qint64 coerceUnixTimestampMs(qint64 value);
    [[nodiscard]] static qint64 parseExpFromTokenStringMs(const QString &token);

    void setError(QString message);

    QPointer<KWalletSecretStore> m_secretStore;
    QNetworkAccessManager *m_networkManager = nullptr;

    quint64 m_nextAcquireId = 1;
    QHash<quint64, PendingAcquire> m_waiters;

    QPointer<QNetworkReply> m_exchangeReply;
    CachedSessionToken m_cached;

    QString m_lastError;
};

} // namespace KateAiInlineCompletion
