/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiInlineCompletionPlugin
*/

#include "plugin/KateAiInlineCompletionPlugin.h"

#include "plugin/KateAiInlineCompletionPluginView.h"
#include "settings/KateAiConfigPage.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

K_PLUGIN_FACTORY_WITH_JSON(KateAiInlineCompletionPluginFactory,
                           "kateaiinlinecompletion.json",
                           registerPlugin<KateAiInlineCompletionPlugin>();)

KateAiInlineCompletionPlugin::KateAiInlineCompletionPlugin(QObject *parent, const QList<QVariant> &args)
    : KTextEditor::Plugin(parent)
{
    Q_UNUSED(args);
    loadSettings();
}

QObject *KateAiInlineCompletionPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    return new KateAiInlineCompletionPluginView(this, mainWindow);
}

int KateAiInlineCompletionPlugin::configPages() const
{
    return 1;
}

KTextEditor::ConfigPage *KateAiInlineCompletionPlugin::configPage(int number, QWidget *parent)
{
    if (number != 0) {
        return nullptr;
    }

    return new KateAiConfigPage(parent, this);
}

KateAiInlineCompletion::CompletionSettings KateAiInlineCompletionPlugin::settings() const
{
    return m_settings;
}

void KateAiInlineCompletionPlugin::setSettings(const KateAiInlineCompletion::CompletionSettings &settings)
{
    m_settings = settings.validated();
    saveSettings();
    Q_EMIT settingsChanged();
}

void KateAiInlineCompletionPlugin::loadSettings()
{
    const auto config = KSharedConfig::openConfig();
    const KConfigGroup group(config, QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kConfigGroupName));
    m_settings = KateAiInlineCompletion::CompletionSettings::load(group);
}

void KateAiInlineCompletionPlugin::saveSettings() const
{
    const auto config = KSharedConfig::openConfig();
    KConfigGroup group(config, QString::fromLatin1(KateAiInlineCompletion::CompletionSettings::kConfigGroupName));
    m_settings.save(group);
}

#include "KateAiInlineCompletionPlugin.moc"
