/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiConfigPage
*/

#include "settings/KateAiConfigPage.h"

#include "plugin/KateAiInlineCompletionPlugin.h"
#include "settings/KWalletSecretStore.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
[[nodiscard]] int indexOfData(QComboBox *combo, const QString &value)
{
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toString() == value) {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] bool isCopilotProvider(const QString &provider)
{
    return provider == QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderGitHubCopilotCodex);
}

[[nodiscard]] QString providerDefaultEndpoint(const QString &provider)
{
    if (provider == QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderOllama)) {
        return QStringLiteral("http://localhost:11434/v1/chat/completions");
    }

    if (provider == QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderGitHubCopilotCodex)) {
        return QStringLiteral("https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions");
    }

    return QStringLiteral("https://api.openai.com/v1/chat/completions");
}

[[nodiscard]] QString providerDefaultModel(const QString &provider)
{
    if (provider == QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderOllama)) {
        return QStringLiteral("llama3.2");
    }

    if (provider == QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderGitHubCopilotCodex)) {
        return QStringLiteral("cushman-ml");
    }

    return QStringLiteral("gpt-4o-mini");
}

[[nodiscard]] QString providerHintText(const QString &provider)
{
    if (provider == QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderOllama)) {
        return i18n("Recommended preset: endpoint /v1/chat/completions, model qwen3-coder-q4:latest, template FIM v3.");
    }

    if (provider == QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderGitHubCopilotCodex)) {
        return i18n("GitHub Copilot uses the fixed Codex completions endpoint. Sign in below and keep the Copilot NWO value aligned with your session.");
    }

    return i18n("Use a streaming chat-completions endpoint. The recommended Ollama preset is /v1/chat/completions with qwen3-coder-q4:latest and FIM v3.");
}

[[nodiscard]] QString shortcutHintText()
{
    return i18n("Shortcuts: Tab accepts the full suggestion. Ctrl+Alt+Shift+Right accepts the next word. Ctrl+Alt+Shift+L accepts the next line. Esc clears the suggestion.");
}

} // namespace

KateAiConfigPage::KateAiConfigPage(QWidget *parent, KateAiInlineCompletionPlugin *plugin)
    : KTextEditor::ConfigPage(parent)
    , m_plugin(plugin)
    , m_secretStore(new KateAiInlineCompletion::KWalletSecretStore(parent ? parent->winId() : 0, this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    auto *root = new QVBoxLayout(this);

    auto *generalBox = new QGroupBox(i18n("General"), this);
    root->addWidget(generalBox);

    auto *generalForm = new QFormLayout(generalBox);

    m_enabled = new QCheckBox(i18n("Enable AI inline completion"), generalBox);
    generalForm->addRow(m_enabled);

    m_debounceMs = new QSpinBox(generalBox);
    m_debounceMs->setRange(KateAiInlineCompletion::CompletionSettings::kDebounceMinMs,
                           KateAiInlineCompletion::CompletionSettings::kDebounceMaxMs);
    m_debounceMs->setSuffix(i18n(" ms"));
    generalForm->addRow(i18n("Debounce"), m_debounceMs);

    m_maxPrefixChars = new QSpinBox(generalBox);
    m_maxPrefixChars->setRange(KateAiInlineCompletion::CompletionSettings::kPrefixMinChars,
                               KateAiInlineCompletion::CompletionSettings::kPrefixMaxChars);
    generalForm->addRow(i18n("Max prefix characters"), m_maxPrefixChars);

    m_maxSuffixChars = new QSpinBox(generalBox);
    m_maxSuffixChars->setRange(KateAiInlineCompletion::CompletionSettings::kSuffixMinChars,
                               KateAiInlineCompletion::CompletionSettings::kSuffixMaxChars);
    generalForm->addRow(i18n("Max suffix characters"), m_maxSuffixChars);

    m_suppressCompletionPopup = new QCheckBox(i18n("Suppress AI suggestion while completion popup is active"), generalBox);
    generalForm->addRow(m_suppressCompletionPopup);

    m_shortcutHint = new QLabel(shortcutHintText(), generalBox);
    m_shortcutHint->setObjectName(QStringLiteral("shortcutHintLabel"));
    m_shortcutHint->setWordWrap(true);
    generalForm->addRow(m_shortcutHint);

    m_promptBox = new QGroupBox(i18n("Prompt"), this);
    root->addWidget(m_promptBox);

    auto *promptForm = new QFormLayout(m_promptBox);

    m_promptTemplate = new QComboBox(m_promptBox);
    m_promptTemplate->addItem(i18n("FIM v3"), QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kPromptTemplateFimV3));
    m_promptTemplate->addItem(i18n("FIM v2"), QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kPromptTemplateFimV2));
    m_promptTemplate->addItem(i18n("FIM v1"), QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kPromptTemplateFimV1));
    promptForm->addRow(i18n("Template"), m_promptTemplate);

    auto *providerBox = new QGroupBox(i18n("Provider"), this);
    root->addWidget(providerBox);

    auto *providerForm = new QFormLayout(providerBox);

    m_provider = new QComboBox(providerBox);
    m_provider->setObjectName(QStringLiteral("providerCombo"));
    m_provider->addItem(i18n("OpenAI-compatible"), QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderOpenAICompatible));
    m_provider->addItem(i18n("Ollama"), QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderOllama));
    m_provider->addItem(i18n("GitHub Copilot (OAuth)"), QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kProviderGitHubCopilotCodex));
    providerForm->addRow(i18n("Backend"), m_provider);

    m_providerHint = new QLabel(providerBox);
    m_providerHint->setObjectName(QStringLiteral("providerHintLabel"));
    m_providerHint->setWordWrap(true);
    providerForm->addRow(m_providerHint);

    m_endpoint = new QLineEdit(providerBox);
    m_endpoint->setPlaceholderText(QStringLiteral("https://api.openai.com/v1/chat/completions"));
    providerForm->addRow(i18n("Endpoint"), m_endpoint);

    m_model = new QLineEdit(providerBox);
    m_model->setPlaceholderText(QStringLiteral("gpt-4o-mini"));
    providerForm->addRow(i18n("Model"), m_model);

    m_copilotClientId = new QLineEdit(providerBox);
    m_copilotClientId->setPlaceholderText(QStringLiteral("Iv1.b507a08c87ecfe98"));
    providerForm->addRow(i18n("Copilot OAuth Client ID"), m_copilotClientId);

    m_copilotNwo = new QLineEdit(providerBox);
    m_copilotNwo->setPlaceholderText(QStringLiteral("github/copilot.vim"));
    providerForm->addRow(i18n("Copilot NWO"), m_copilotNwo);

    m_apiKeyBox = new QGroupBox(i18n("Credentials"), this);
    root->addWidget(m_apiKeyBox);

    auto *secretLayout = new QVBoxLayout(m_apiKeyBox);

    m_apiKeyStatus = new QLabel(m_apiKeyBox);
    secretLayout->addWidget(m_apiKeyStatus);

    auto *apiKeyRow = new QHBoxLayout;
    secretLayout->addLayout(apiKeyRow);

    m_apiKey = new QLineEdit(m_apiKeyBox);
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_apiKey->setPlaceholderText(i18n("API key (stored in KWallet)"));
    apiKeyRow->addWidget(m_apiKey);

    m_saveApiKey = new QPushButton(i18n("Save"), m_apiKeyBox);
    apiKeyRow->addWidget(m_saveApiKey);

    m_clearApiKey = new QPushButton(i18n("Clear"), m_apiKeyBox);
    apiKeyRow->addWidget(m_clearApiKey);

    m_copilotBox = new QGroupBox(i18n("GitHub Copilot OAuth"), this);
    root->addWidget(m_copilotBox);

    auto *copilotLayout = new QVBoxLayout(m_copilotBox);

    m_copilotStatus = new QLabel(m_copilotBox);
    copilotLayout->addWidget(m_copilotStatus);

    auto *copilotButtons = new QHBoxLayout;
    copilotLayout->addLayout(copilotButtons);

    m_copilotSignIn = new QPushButton(i18n("Sign in"), m_copilotBox);
    copilotButtons->addWidget(m_copilotSignIn);

    m_copilotSignOut = new QPushButton(i18n("Sign out"), m_copilotBox);
    copilotButtons->addWidget(m_copilotSignOut);

    auto *verifyRow = new QHBoxLayout;
    copilotLayout->addLayout(verifyRow);

    m_copilotVerificationUri = new QLineEdit(m_copilotBox);
    m_copilotVerificationUri->setReadOnly(true);
    m_copilotVerificationUri->setPlaceholderText(i18n("Verification URL"));
    verifyRow->addWidget(m_copilotVerificationUri);

    m_copilotOpenVerificationUri = new QPushButton(i18n("Open"), m_copilotBox);
    verifyRow->addWidget(m_copilotOpenVerificationUri);

    m_copilotCopyVerificationUri = new QPushButton(i18n("Copy"), m_copilotBox);
    verifyRow->addWidget(m_copilotCopyVerificationUri);

    auto *codeRow = new QHBoxLayout;
    copilotLayout->addLayout(codeRow);

    m_copilotUserCode = new QLineEdit(m_copilotBox);
    m_copilotUserCode->setReadOnly(true);
    m_copilotUserCode->setPlaceholderText(i18n("User code"));
    codeRow->addWidget(m_copilotUserCode);

    m_copilotCopyUserCode = new QPushButton(i18n("Copy"), m_copilotBox);
    codeRow->addWidget(m_copilotCopyUserCode);

    root->addStretch(1);

    m_copilotPollTimer.setSingleShot(false);
    connect(&m_copilotPollTimer, &QTimer::timeout, this, &KateAiConfigPage::slotCopilotPollTimeout);

    reset();

    connect(m_enabled, &QCheckBox::toggled, this, &KateAiConfigPage::slotUiChanged);
    connect(m_debounceMs, qOverload<int>(&QSpinBox::valueChanged), this, &KateAiConfigPage::slotUiChanged);
    connect(m_maxPrefixChars, qOverload<int>(&QSpinBox::valueChanged), this, &KateAiConfigPage::slotUiChanged);
    connect(m_maxSuffixChars, qOverload<int>(&QSpinBox::valueChanged), this, &KateAiConfigPage::slotUiChanged);
    connect(m_suppressCompletionPopup, &QCheckBox::toggled, this, &KateAiConfigPage::slotUiChanged);

    connect(m_provider, qOverload<int>(&QComboBox::currentIndexChanged), this, &KateAiConfigPage::slotProviderChanged);
    connect(m_endpoint, &QLineEdit::textChanged, this, &KateAiConfigPage::slotUiChanged);
    connect(m_model, &QLineEdit::textChanged, this, &KateAiConfigPage::slotUiChanged);

    connect(m_copilotClientId, &QLineEdit::textChanged, this, &KateAiConfigPage::slotUiChanged);
    connect(m_copilotNwo, &QLineEdit::textChanged, this, &KateAiConfigPage::slotUiChanged);

    connect(m_promptTemplate, qOverload<int>(&QComboBox::currentIndexChanged), this, &KateAiConfigPage::slotUiChanged);

    connect(m_apiKey, &QLineEdit::textChanged, this, &KateAiConfigPage::slotUiChanged);
    connect(m_apiKey, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_saveApiKey->setEnabled(m_walletAvailable && !text.isEmpty());
    });
    connect(m_saveApiKey, &QPushButton::clicked, this, &KateAiConfigPage::slotSaveApiKey);
    connect(m_clearApiKey, &QPushButton::clicked, this, &KateAiConfigPage::slotClearApiKey);

    connect(m_copilotSignIn, &QPushButton::clicked, this, &KateAiConfigPage::slotCopilotSignIn);
    connect(m_copilotSignOut, &QPushButton::clicked, this, &KateAiConfigPage::slotCopilotSignOut);
    connect(m_copilotOpenVerificationUri, &QPushButton::clicked, this, &KateAiConfigPage::slotCopilotOpenVerificationUri);
    connect(m_copilotCopyUserCode, &QPushButton::clicked, this, &KateAiConfigPage::slotCopilotCopyUserCode);
    connect(m_copilotCopyVerificationUri, &QPushButton::clicked, this, &KateAiConfigPage::slotCopilotCopyVerificationUri);
}

KateAiConfigPage::~KateAiConfigPage() = default;

QString KateAiConfigPage::name() const
{
    return i18n("AI Completion");
}

QString KateAiConfigPage::fullName() const
{
    return i18n("Configure AI Inline Completion");
}

QIcon KateAiConfigPage::icon() const
{
    return QIcon::fromTheme(QStringLiteral("tools-wizard"));
}

void KateAiConfigPage::apply()
{
    const KateAiInlineCompletion::CompletionSettings newSettings = readUi().validated();

    if (m_plugin) {
        m_plugin->setSettings(newSettings);
    }

    bool secretsOk = true;

    if (m_clearApiKeyOnApply) {
        secretsOk = m_secretStore->removeApiKey();
        m_clearApiKeyOnApply = false;
    }

    const QString key = m_apiKey->text();
    if (!key.isEmpty()) {
        secretsOk = m_secretStore->writeApiKey(key);
        m_apiKey->clear();
    }

    refreshApiKeyStatus();
    refreshCopilotStatus();

    if (secretsOk) {
        setChanged(false);
    }
}

void KateAiConfigPage::defaults()
{
    loadUi(KateAiInlineCompletion::CompletionSettings::defaults());
    setChanged(true);
}

void KateAiConfigPage::reset()
{
    if (m_plugin) {
        loadUi(m_plugin->settings());
    } else {
        loadUi(KateAiInlineCompletion::CompletionSettings::defaults());
    }

    m_apiKey->clear();
    m_clearApiKeyOnApply = false;

    stopCopilotDeviceFlow({});

    refreshApiKeyStatus();
    refreshCopilotStatus();

    updateCredentialsUi();
    setChanged(false);
}

void KateAiConfigPage::slotUiChanged()
{
    setChanged(true);
}

void KateAiConfigPage::slotProviderChanged()
{
    const QString provider = m_provider->currentData().toString();

    const QString prevEndpoint = m_endpoint->text().trimmed();
    const QString oldDefaultEndpoint = providerDefaultEndpoint(m_prevProviderInUi);

    if (isCopilotProvider(provider)) {
        m_endpoint->setText(providerDefaultEndpoint(provider));
    } else if (!prevEndpoint.isEmpty() && prevEndpoint == oldDefaultEndpoint) {
        m_endpoint->setText(providerDefaultEndpoint(provider));
    }

    const QString prevModel = m_model->text().trimmed();
    const QString oldDefaultModel = providerDefaultModel(m_prevProviderInUi);
    if (!prevModel.isEmpty() && prevModel == oldDefaultModel) {
        m_model->setText(providerDefaultModel(provider));
    }

    m_prevProviderInUi = provider;

    updateCredentialsUi();
    slotUiChanged();
}

void KateAiConfigPage::slotSaveApiKey()
{
    const QString key = m_apiKey->text();
    if (key.isEmpty()) {
        return;
    }

    const bool ok = m_secretStore->writeApiKey(key);
    if (ok) {
        m_apiKey->clear();
    }

    refreshApiKeyStatus();
}

void KateAiConfigPage::slotClearApiKey()
{
    const bool ok = m_secretStore->removeApiKey();
    if (ok) {
        m_apiKey->clear();
        m_clearApiKeyOnApply = false;
    } else {
        m_clearApiKeyOnApply = true;
    }

    refreshApiKeyStatus();
}

void KateAiConfigPage::slotCopilotSignIn()
{
    startCopilotDeviceFlow();
}

void KateAiConfigPage::slotCopilotSignOut()
{
    stopCopilotDeviceFlow({});

    const bool ok = m_secretStore->removeGitHubOAuthToken();
    if (ok) {
        refreshCopilotStatus();
        updateCredentialsUi();
        setChanged(true);
    } else {
        refreshCopilotStatus();
    }
}

void KateAiConfigPage::slotCopilotOpenVerificationUri()
{
    const QUrl url(m_copilotVerificationUri->text().trimmed());
    if (url.isValid()) {
        QDesktopServices::openUrl(url);
    }
}

void KateAiConfigPage::slotCopilotCopyUserCode()
{
    if (!QGuiApplication::clipboard()) {
        return;
    }

    QGuiApplication::clipboard()->setText(m_copilotUserCode->text().trimmed());
}

void KateAiConfigPage::slotCopilotCopyVerificationUri()
{
    if (!QGuiApplication::clipboard()) {
        return;
    }

    QGuiApplication::clipboard()->setText(m_copilotVerificationUri->text().trimmed());
}

void KateAiConfigPage::slotCopilotPollTimeout()
{
    pollCopilotDeviceFlow();
}

void KateAiConfigPage::loadUi(const KateAiInlineCompletion::CompletionSettings &settings)
{
    const KateAiInlineCompletion::CompletionSettings v = settings.validated();

    m_enabled->setChecked(v.enabled);
    m_debounceMs->setValue(v.debounceMs);
    m_maxPrefixChars->setValue(v.maxPrefixChars);
    m_maxSuffixChars->setValue(v.maxSuffixChars);
    m_suppressCompletionPopup->setChecked(v.suppressWhenCompletionPopupVisible);

    const int providerIndex = indexOfData(m_provider, v.provider);
    m_provider->setCurrentIndex(providerIndex >= 0 ? providerIndex : 0);
    m_prevProviderInUi = m_provider->currentData().toString();

    m_endpoint->setText(v.endpoint.toString());
    m_model->setText(v.model);

    m_copilotClientId->setText(v.copilotClientId);
    m_copilotNwo->setText(v.copilotNwo);

    const int templateIndex = indexOfData(m_promptTemplate, v.promptTemplate);
    m_promptTemplate->setCurrentIndex(templateIndex >= 0 ? templateIndex : 0);

    updateCredentialsUi();
}

KateAiInlineCompletion::CompletionSettings KateAiConfigPage::readUi() const
{
    KateAiInlineCompletion::CompletionSettings s = KateAiInlineCompletion::CompletionSettings::defaults();

    s.enabled = m_enabled->isChecked();
    s.debounceMs = m_debounceMs->value();
    s.maxPrefixChars = m_maxPrefixChars->value();
    s.maxSuffixChars = m_maxSuffixChars->value();
    s.suppressWhenCompletionPopupVisible = m_suppressCompletionPopup->isChecked();

    s.provider = m_provider->currentData().toString();
    s.endpoint = QUrl(m_endpoint->text().trimmed());
    s.model = m_model->text().trimmed();
    s.promptTemplate = m_promptTemplate->currentData().toString();

    s.copilotClientId = m_copilotClientId->text().trimmed();
    s.copilotNwo = m_copilotNwo->text().trimmed();

    return s;
}

void KateAiConfigPage::setChanged(bool isChanged)
{
    m_changed = isChanged;
    if (m_changed) {
        Q_EMIT changed();
    }
}

void KateAiConfigPage::updateCredentialsUi()
{
    const QString provider = m_provider->currentData().toString();
    const bool copilot = isCopilotProvider(provider);

    m_promptBox->setEnabled(!copilot);

    m_endpoint->setEnabled(!copilot);

    m_apiKeyBox->setVisible(!copilot);
    m_copilotBox->setVisible(copilot);

    m_copilotClientId->setEnabled(copilot);
    m_copilotNwo->setEnabled(copilot);

    refreshProviderHint();
    refreshApiKeyStatus();
    refreshCopilotStatus();
}

void KateAiConfigPage::refreshProviderHint()
{
    if (!m_providerHint) {
        return;
    }

    m_providerHint->setText(providerHintText(m_provider->currentData().toString()));
}

void KateAiConfigPage::refreshApiKeyStatus()
{
    m_walletAvailable = m_secretStore->isAvailable();
    if (!m_walletAvailable) {
        m_apiKeyStatus->setText(i18n("KWallet is unavailable: %1", m_secretStore->lastErrorString()));
        m_saveApiKey->setEnabled(false);
        m_clearApiKey->setEnabled(false);
        return;
    }

    const bool hasKey = m_secretStore->hasApiKey();
    m_apiKeyStatus->setText(hasKey ? i18n("API key is stored in KWallet") : i18n("KWallet currently has no API key"));

    m_saveApiKey->setEnabled(!m_apiKey->text().isEmpty());
    m_clearApiKey->setEnabled(hasKey);
}

void KateAiConfigPage::refreshCopilotStatus()
{
    const bool walletOk = m_secretStore->isAvailable();
    if (!walletOk) {
        m_copilotStatus->setText(i18n("KWallet is unavailable: %1", m_secretStore->lastErrorString()));
        m_copilotSignIn->setEnabled(false);
        m_copilotSignOut->setEnabled(false);
        return;
    }

    const bool hasToken = m_secretStore->hasGitHubOAuthToken();
    const bool flowActive = m_copilotPollTimer.isActive();

    if (hasToken) {
        m_copilotStatus->setText(i18n("GitHub Copilot is signed in"));
    } else if (flowActive) {
        m_copilotStatus->setText(i18n("GitHub Copilot authorization is waiting in your browser"));
    } else {
        m_copilotStatus->setText(i18n("GitHub Copilot is signed out"));
    }

    m_copilotSignIn->setEnabled(!flowActive);
    m_copilotSignOut->setEnabled(hasToken);

    const bool hasUri = !m_copilotVerificationUri->text().trimmed().isEmpty();
    const bool hasCode = !m_copilotUserCode->text().trimmed().isEmpty();

    m_copilotOpenVerificationUri->setEnabled(hasUri);
    m_copilotCopyVerificationUri->setEnabled(hasUri);
    m_copilotCopyUserCode->setEnabled(hasCode);
}

void KateAiConfigPage::startCopilotDeviceFlow()
{
    if (!m_networkManager) {
        return;
    }

    stopCopilotDeviceFlow({});

    const KateAiInlineCompletion::CompletionSettings s = readUi().validated();

    QNetworkRequest req(QUrl(QStringLiteral("https://github.com/login/device/code")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "GitHubCopilotChat/0.26.7");

    QJsonObject payload;
    payload[QStringLiteral("client_id")] = s.copilotClientId;
    payload[QStringLiteral("scope")] = QStringLiteral("read:user");

    QNetworkReply *reply = m_networkManager->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            stopCopilotDeviceFlow(i18n("Device flow request failed: %1", reply->errorString()));
            return;
        }

        const QByteArray body = reply->readAll();

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            stopCopilotDeviceFlow(i18n("Device flow response parse failed"));
            return;
        }

        const QJsonObject obj = doc.object();
        const QString deviceCode = obj.value(QStringLiteral("device_code")).toString();
        const QString userCode = obj.value(QStringLiteral("user_code")).toString();
        const QString verificationUri = obj.value(QStringLiteral("verification_uri")).toString();
        const int expiresIn = obj.value(QStringLiteral("expires_in")).toInt(0);
        const int interval = obj.value(QStringLiteral("interval")).toInt(5);

        if (deviceCode.isEmpty() || userCode.isEmpty() || verificationUri.isEmpty() || expiresIn <= 0) {
            stopCopilotDeviceFlow(i18n("Device flow response missing fields"));
            return;
        }

        m_copilotDeviceCode = deviceCode;
        m_copilotPollIntervalSec = qMax(1, interval);
        m_copilotDeviceExpiresAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);

        m_copilotVerificationUri->setText(verificationUri);
        m_copilotUserCode->setText(userCode);

        refreshCopilotStatus();

        m_copilotPollTimer.start(m_copilotPollIntervalSec * 1000);

        QDesktopServices::openUrl(QUrl(verificationUri));

        pollCopilotDeviceFlow();
    });
}

void KateAiConfigPage::pollCopilotDeviceFlow()
{
    if (!m_networkManager) {
        return;
    }

    if (m_copilotDeviceCode.isEmpty()) {
        return;
    }

    if (QDateTime::currentDateTimeUtc() >= m_copilotDeviceExpiresAt) {
        stopCopilotDeviceFlow(i18n("Device flow expired"));
        return;
    }

    const KateAiInlineCompletion::CompletionSettings s = readUi().validated();

    QNetworkRequest req(QUrl(QStringLiteral("https://github.com/login/oauth/access_token")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "GitHubCopilotChat/0.26.7");

    QJsonObject payload;
    payload[QStringLiteral("client_id")] = s.copilotClientId;
    payload[QStringLiteral("device_code")] = m_copilotDeviceCode;
    payload[QStringLiteral("grant_type")] = QStringLiteral("urn:ietf:params:oauth:grant-type:device_code");

    QNetworkReply *reply = m_networkManager->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            stopCopilotDeviceFlow(i18n("Device flow polling failed: %1", reply->errorString()));
            return;
        }

        const QByteArray body = reply->readAll();

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            stopCopilotDeviceFlow(i18n("Device flow polling response parse failed"));
            return;
        }

        const QJsonObject obj = doc.object();

        const QString accessToken = obj.value(QStringLiteral("access_token")).toString();
        if (!accessToken.isEmpty()) {
            const bool ok = m_secretStore->writeGitHubOAuthToken(accessToken);
            if (ok) {
                stopCopilotDeviceFlow(i18n("Signed in"));
                refreshCopilotStatus();
                updateCredentialsUi();
                setChanged(true);
                return;
            }

            stopCopilotDeviceFlow(i18n("Failed to store token in KWallet"));
            return;
        }

        const QString err = obj.value(QStringLiteral("error")).toString();
        if (err == QStringLiteral("authorization_pending")) {
            refreshCopilotStatus();
            return;
        }

        if (err == QStringLiteral("slow_down")) {
            m_copilotPollIntervalSec += 5;
            m_copilotPollTimer.start(m_copilotPollIntervalSec * 1000);
            refreshCopilotStatus();
            return;
        }

        if (!err.isEmpty()) {
            stopCopilotDeviceFlow(i18n("Device flow error: %1", err));
            return;
        }

        refreshCopilotStatus();
    });
}

void KateAiConfigPage::stopCopilotDeviceFlow(QString infoMessage)
{
    if (m_copilotPollTimer.isActive()) {
        m_copilotPollTimer.stop();
    }

    m_copilotDeviceCode.clear();
    m_copilotPollIntervalSec = 5;
    m_copilotDeviceExpiresAt = {};

    if (!infoMessage.trimmed().isEmpty()) {
        m_copilotStatus->setText(infoMessage);
    }

    refreshCopilotStatus();
}
