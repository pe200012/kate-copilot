/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiConfigPage

    Provides a configuration UI inside Kate's plugin settings dialog.
*/

#pragma once

#include "settings/CompletionSettings.h"

#include <KTextEditor/ConfigPage>

#include <QDateTime>
#include <QScopedPointer>
#include <QTimer>

class KateAiInlineCompletionPlugin;

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class QNetworkAccessManager;

namespace KateAiInlineCompletion
{
class KWalletSecretStore;
}

class KateAiConfigPage final : public KTextEditor::ConfigPage
{
    Q_OBJECT

public:
    explicit KateAiConfigPage(QWidget *parent, KateAiInlineCompletionPlugin *plugin);
    ~KateAiConfigPage() override;

    QString name() const override;
    QString fullName() const override;
    QIcon icon() const override;

public Q_SLOTS:
    void apply() override;
    void defaults() override;
    void reset() override;

private Q_SLOTS:
    void slotUiChanged();

    void slotProviderChanged();

    void slotSaveApiKey();
    void slotClearApiKey();

    void slotCopilotSignIn();
    void slotCopilotSignOut();
    void slotCopilotVerifySession();
    void slotCopilotOpenVerificationUri();
    void slotCopilotCopyUserCode();
    void slotCopilotCopyVerificationUri();
    void slotCopilotPollTimeout();

private:
    void loadUi(const KateAiInlineCompletion::CompletionSettings &settings);
    [[nodiscard]] KateAiInlineCompletion::CompletionSettings readUi() const;

    void setChanged(bool isChanged);

    void updateCredentialsUi();
    void refreshProviderHint();
    void refreshApiKeyStatus();
    void refreshCopilotStatus();

    void startCopilotDeviceFlow();
    void pollCopilotDeviceFlow();
    void stopCopilotDeviceFlow(QString infoMessage);

    KateAiInlineCompletionPlugin *m_plugin = nullptr;
    QScopedPointer<KateAiInlineCompletion::KWalletSecretStore> m_secretStore;

    QScopedPointer<QNetworkAccessManager> m_networkManager;

    bool m_changed = false;
    bool m_clearApiKeyOnApply = false;
    bool m_walletAvailable = false;

    // General
    QCheckBox *m_enabled = nullptr;
    QSpinBox *m_debounceMs = nullptr;
    QSpinBox *m_maxPrefixChars = nullptr;
    QSpinBox *m_maxSuffixChars = nullptr;
    QCheckBox *m_suppressCompletionPopup = nullptr;
    QLabel *m_shortcutHint = nullptr;

    // Prompt
    QGroupBox *m_promptBox = nullptr;
    QComboBox *m_promptTemplate = nullptr;

    // Context
    QGroupBox *m_contextBox = nullptr;
    QCheckBox *m_enableContextualPrompt = nullptr;
    QSpinBox *m_maxContextItems = nullptr;
    QSpinBox *m_maxContextChars = nullptr;
    QCheckBox *m_enableOpenTabsContext = nullptr;
    QCheckBox *m_enableRecentEditsContext = nullptr;
    QCheckBox *m_enableDiagnosticsContext = nullptr;
    QCheckBox *m_enableRelatedFilesContext = nullptr;
    QSpinBox *m_relatedFilesMaxFiles = nullptr;
    QSpinBox *m_relatedFilesMaxChars = nullptr;
    QSpinBox *m_relatedFilesMaxCharsPerFile = nullptr;
    QLineEdit *m_contextExcludePatterns = nullptr;

    // Provider
    QComboBox *m_provider = nullptr;
    QLabel *m_providerHint = nullptr;
    QLineEdit *m_endpoint = nullptr;
    QLineEdit *m_model = nullptr;

    QLineEdit *m_copilotClientId = nullptr;
    QLineEdit *m_copilotNwo = nullptr;

    // Credentials: API key
    QGroupBox *m_apiKeyBox = nullptr;
    QLabel *m_apiKeyStatus = nullptr;
    QLineEdit *m_apiKey = nullptr;
    QPushButton *m_saveApiKey = nullptr;
    QPushButton *m_clearApiKey = nullptr;

    // Credentials: Copilot OAuth
    QGroupBox *m_copilotBox = nullptr;
    QLabel *m_copilotStatus = nullptr;
    QPushButton *m_copilotSignIn = nullptr;
    QPushButton *m_copilotSignOut = nullptr;
    QPushButton *m_copilotVerifySession = nullptr;

    QLineEdit *m_copilotVerificationUri = nullptr;
    QPushButton *m_copilotOpenVerificationUri = nullptr;
    QPushButton *m_copilotCopyVerificationUri = nullptr;

    QLineEdit *m_copilotUserCode = nullptr;
    QPushButton *m_copilotCopyUserCode = nullptr;

    QString m_prevProviderInUi;

    QTimer m_copilotPollTimer;
    QString m_copilotDeviceCode;
    int m_copilotPollIntervalSec = 5;
    QDateTime m_copilotDeviceExpiresAt;
};
