/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: KateAiInlineCompletionPlugin

    Entry point for the Kate/KTextEditor plugin.
    Holds global settings shared across all MainWindow instances.
*/

#pragma once

#include "settings/CompletionSettings.h"

#include <KTextEditor/Plugin>

#include <QObject>

class QWidget;

namespace KTextEditor
{
class ConfigPage;
class MainWindow;
}

class KateAiInlineCompletionPlugin final : public KTextEditor::Plugin
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit KateAiInlineCompletionPlugin(QObject *parent = nullptr, const QList<QVariant> &args = {});

    QObject *createView(KTextEditor::MainWindow *mainWindow) override;

    int configPages() const override;
    KTextEditor::ConfigPage *configPage(int number, QWidget *parent) override;

    [[nodiscard]] KateAiInlineCompletion::CompletionSettings settings() const;
    void setSettings(const KateAiInlineCompletion::CompletionSettings &settings);

Q_SIGNALS:
    void settingsChanged();

private:
    void loadSettings();
    void saveSettings() const;

    KateAiInlineCompletion::CompletionSettings m_settings;
};
