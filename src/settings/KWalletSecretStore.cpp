/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KWalletSecretStore
*/

#include "settings/KWalletSecretStore.h"

#include <QString>
#include <utility>

namespace KateAiInlineCompletion
{

KWalletSecretStore::KWalletSecretStore(WId windowId, QObject *parent)
    : QObject(parent)
    , m_windowId(windowId)
{
}

bool KWalletSecretStore::isAvailable() const
{
    return ensureOpen();
}

QString KWalletSecretStore::lastErrorString() const
{
    return m_lastError;
}

bool KWalletSecretStore::hasApiKey() const
{
    if (!ensureOpen()) {
        return false;
    }

    return m_wallet->hasEntry(QString::fromLatin1(kApiKeyEntry));
}

QString KWalletSecretStore::readApiKey() const
{
    if (!ensureOpen()) {
        return {};
    }

    QString value;
    const int rc = m_wallet->readPassword(QString::fromLatin1(kApiKeyEntry), value);
    if (rc != 0) {
        setError(QStringLiteral("KWallet readPassword failed with code %1").arg(rc));
        return {};
    }

    setError({});
    return value;
}

bool KWalletSecretStore::writeApiKey(const QString &apiKey)
{
    if (apiKey.isEmpty()) {
        setError(QStringLiteral("API key is empty"));
        return false;
    }

    if (!ensureOpen()) {
        return false;
    }

    const int rc = m_wallet->writePassword(QString::fromLatin1(kApiKeyEntry), apiKey);
    if (rc != 0) {
        setError(QStringLiteral("KWallet writePassword failed with code %1").arg(rc));
        return false;
    }

    m_wallet->sync();
    setError({});
    return true;
}

bool KWalletSecretStore::removeApiKey()
{
    if (!ensureOpen()) {
        return false;
    }

    const int rc = m_wallet->removeEntry(QString::fromLatin1(kApiKeyEntry));
    if (rc != 0) {
        setError(QStringLiteral("KWallet removeEntry failed with code %1").arg(rc));
        return false;
    }

    m_wallet->sync();
    setError({});
    return true;
}

bool KWalletSecretStore::hasGitHubOAuthToken() const
{
    if (!ensureOpen()) {
        return false;
    }

    return m_wallet->hasEntry(QString::fromLatin1(kGitHubOAuthTokenEntry));
}

QString KWalletSecretStore::readGitHubOAuthToken() const
{
    if (!ensureOpen()) {
        return {};
    }

    QString value;
    const int rc = m_wallet->readPassword(QString::fromLatin1(kGitHubOAuthTokenEntry), value);
    if (rc != 0) {
        setError(QStringLiteral("KWallet readPassword failed with code %1").arg(rc));
        return {};
    }

    setError({});
    return value;
}

bool KWalletSecretStore::writeGitHubOAuthToken(const QString &oauthToken)
{
    if (oauthToken.isEmpty()) {
        setError(QStringLiteral("GitHub OAuth token is empty"));
        return false;
    }

    if (!ensureOpen()) {
        return false;
    }

    const int rc = m_wallet->writePassword(QString::fromLatin1(kGitHubOAuthTokenEntry), oauthToken);
    if (rc != 0) {
        setError(QStringLiteral("KWallet writePassword failed with code %1").arg(rc));
        return false;
    }

    m_wallet->sync();
    setError({});
    return true;
}

bool KWalletSecretStore::removeGitHubOAuthToken()
{
    if (!ensureOpen()) {
        return false;
    }

    const int rc = m_wallet->removeEntry(QString::fromLatin1(kGitHubOAuthTokenEntry));
    if (rc != 0) {
        setError(QStringLiteral("KWallet removeEntry failed with code %1").arg(rc));
        return false;
    }

    m_wallet->sync();
    setError({});
    return true;
}

bool KWalletSecretStore::ensureOpen() const
{
    if (m_wallet && m_wallet->isOpen()) {
        return true;
    }

    if (!KWallet::Wallet::isEnabled()) {
        setError(QStringLiteral("KWallet is disabled"));
        return false;
    }

    KWallet::Wallet *wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), m_windowId, KWallet::Wallet::Synchronous);
    if (!wallet) {
        setError(QStringLiteral("KWallet openWallet returned null"));
        return false;
    }

    const QString folder = QString::fromLatin1(kFolderName);
    const bool folderReady = wallet->hasFolder(folder) || wallet->createFolder(folder);
    if (!folderReady) {
        setError(QStringLiteral("KWallet folder setup failed"));
        wallet->deleteLater();
        return false;
    }

    if (!wallet->setFolder(folder)) {
        setError(QStringLiteral("KWallet setFolder failed"));
        wallet->deleteLater();
        return false;
    }

    m_wallet = wallet;
    setError({});
    return true;
}

void KWalletSecretStore::setError(QString message) const
{
    m_lastError = std::move(message);
}

} // namespace KateAiInlineCompletion
