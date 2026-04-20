/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KWalletSecretStore

    Provides encrypted storage for API credentials via KWallet.
*/

#pragma once

#include <KWallet>

#include <QObject>
#include <QPointer>
#include <QString>

class QWidget;

namespace KateAiInlineCompletion
{

class KWalletSecretStore final : public QObject
{
    Q_OBJECT

public:
    static constexpr const char *kFolderName = "kate-ai-inline-completion";
    static constexpr const char *kApiKeyEntry = "apiKey";
    static constexpr const char *kGitHubOAuthTokenEntry = "githubOAuthToken";

    explicit KWalletSecretStore(WId windowId, QObject *parent = nullptr);

    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] QString lastErrorString() const;

    [[nodiscard]] bool hasApiKey() const;
    [[nodiscard]] QString readApiKey() const;

    bool writeApiKey(const QString &apiKey);
    bool removeApiKey();

    [[nodiscard]] bool hasGitHubOAuthToken() const;
    [[nodiscard]] QString readGitHubOAuthToken() const;

    bool writeGitHubOAuthToken(const QString &oauthToken);
    bool removeGitHubOAuthToken();

private:
    [[nodiscard]] bool ensureOpen() const;
    void setError(QString message) const;

    WId m_windowId = 0;
    mutable QPointer<KWallet::Wallet> m_wallet;
    mutable QString m_lastError;
};

} // namespace KateAiInlineCompletion
