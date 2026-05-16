# SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
# SPDX-License-Identifier: LGPL-2.0-or-later

include(ECMAddTests)

ecm_add_test(CompletionSettingsTest.cpp
    TEST_NAME kateaiinlinecompletion_completion_settings_test
    LINK_LIBRARIES
        kateaiinlinecompletion_settings
        Qt6::Test
)

ecm_add_test(SSEParserTest.cpp
    TEST_NAME kateaiinlinecompletion_sse_parser_test
    LINK_LIBRARIES
        kateaiinlinecompletion_sse
        Qt6::Test
)

ecm_add_test(PromptTemplateTest.cpp
    TEST_NAME kateaiinlinecompletion_prompt_template_test
    LINK_LIBRARIES
        kateaiinlinecompletion_prompt
        Qt6::Test
)

ecm_add_test(ContextProviderRegistryTest.cpp
    TEST_NAME kateaiinlinecompletion_context_provider_registry_test
    LINK_LIBRARIES
        kateaiinlinecompletion_context
        Qt6::Test
)

ecm_add_test(PromptAssemblerTest.cpp
    TEST_NAME kateaiinlinecompletion_prompt_assembler_test
    LINK_LIBRARIES
        kateaiinlinecompletion_prompt
        kateaiinlinecompletion_context
        Qt6::Test
)

ecm_add_test(SuggestionPostProcessorTest.cpp
    ${CMAKE_SOURCE_DIR}/src/session/SuggestionPostProcessor.cpp
    TEST_NAME kateaiinlinecompletion_suggestion_post_processor_test
    LINK_LIBRARIES
        kateaiinlinecompletion_prompt
        Qt6::Test
        KF6::TextEditor
)
target_include_directories(kateaiinlinecompletion_suggestion_post_processor_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

ecm_add_test(RecentEditsTrackerTest.cpp
    TEST_NAME kateaiinlinecompletion_recent_edits_tracker_test
    LINK_LIBRARIES
        kateaiinlinecompletion_context
        Qt6::Test
        KF6::TextEditor
)

ecm_add_test(RecentEditsContextProviderTest.cpp
    TEST_NAME kateaiinlinecompletion_recent_edits_context_provider_test
    LINK_LIBRARIES
        kateaiinlinecompletion_context
        Qt6::Test
        KF6::TextEditor
)

ecm_add_test(GhostTextInlineNoteProviderTest.cpp
    ${CMAKE_SOURCE_DIR}/src/render/GhostTextInlineNoteProvider.cpp
    TEST_NAME kateaiinlinecompletion_ghost_text_inline_note_provider_test
    LINK_LIBRARIES
        Qt6::Test
        KF6::TextEditor
)
target_include_directories(kateaiinlinecompletion_ghost_text_inline_note_provider_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

ecm_add_test(InlineNoteRenderingExperimentTest.cpp
    TEST_NAME kateaiinlinecompletion_inline_note_rendering_experiment_test
    LINK_LIBRARIES
        Qt6::Test
        Qt6::Widgets
        KF6::TextEditor
)

ecm_add_test(GhostTextInlineNoteProviderRenderingTest.cpp
    ${CMAKE_SOURCE_DIR}/src/render/GhostTextInlineNoteProvider.cpp
    TEST_NAME kateaiinlinecompletion_ghost_text_inline_note_provider_rendering_test
    LINK_LIBRARIES
        Qt6::Test
        Qt6::Widgets
        KF6::TextEditor
)
target_include_directories(kateaiinlinecompletion_ghost_text_inline_note_provider_rendering_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

ecm_add_test(InlineNoteRenderingABTest.cpp
    TEST_NAME kateaiinlinecompletion_inline_note_rendering_ab_test
    LINK_LIBRARIES
        Qt6::Test
        Qt6::Widgets
        KF6::TextEditor
)

ecm_add_test(GhostTextOverlayWidgetRenderingTest.cpp
    ${CMAKE_SOURCE_DIR}/src/render/GhostTextOverlayWidget.cpp
    TEST_NAME kateaiinlinecompletion_ghost_text_overlay_widget_rendering_test
    LINK_LIBRARIES
        Qt6::Test
        Qt6::Widgets
        KF6::TextEditor
)
target_include_directories(kateaiinlinecompletion_ghost_text_overlay_widget_rendering_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

ecm_add_test(SuggestionAnchorTrackerTest.cpp
    ${CMAKE_SOURCE_DIR}/src/session/SuggestionAnchorTracker.cpp
    TEST_NAME kateaiinlinecompletion_suggestion_anchor_tracker_test
    LINK_LIBRARIES
        Qt6::Test
        Qt6::Widgets
        KF6::TextEditor
)
target_include_directories(kateaiinlinecompletion_suggestion_anchor_tracker_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

ecm_add_test(EditorSessionIntegrationTest.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/KateAiInlineCompletionPlugin.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/KateAiInlineCompletionPluginView.cpp
    ${CMAKE_SOURCE_DIR}/src/session/EditorSession.cpp
    ${CMAKE_SOURCE_DIR}/src/session/SuggestionAnchorTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/session/SuggestionPostProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/context/ContextProviderRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/context/CurrentFileContextProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/context/ProjectTraitsContextProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/context/OpenTabsContextProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/context/RecentEditsTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/context/RecentEditsContextProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt/PromptAssembler.cpp
    ${CMAKE_SOURCE_DIR}/src/render/GhostTextInlineNoteProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/render/GhostTextOverlayWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/network/AbstractAIProvider.h
    ${CMAKE_SOURCE_DIR}/src/network/OpenAICompatibleProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/network/CopilotCodexProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/network/SSEParser.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt/PromptTemplate.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt/CopilotCodexPromptBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/KateAiConfigPage.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/CompletionSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/KWalletSecretStore.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/CopilotAuthManager.cpp
    TEST_NAME kateaiinlinecompletion_editor_session_integration_test
    LINK_LIBRARIES
        Qt6::Test
        Qt6::Widgets
        Qt6::Network
        KF6::TextEditor
        KF6::Wallet
        KF6::I18n
        KF6::ConfigCore
        KF6::CoreAddons
        KF6::XmlGui
)
target_include_directories(kateaiinlinecompletion_editor_session_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

ecm_add_test(KateAiConfigPageTest.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/KateAiInlineCompletionPlugin.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/KateAiInlineCompletionPluginView.cpp
    ${CMAKE_SOURCE_DIR}/src/session/EditorSession.cpp
    ${CMAKE_SOURCE_DIR}/src/session/SuggestionAnchorTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/session/SuggestionPostProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/context/ContextProviderRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/context/CurrentFileContextProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/context/ProjectTraitsContextProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/context/OpenTabsContextProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/context/RecentEditsTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/context/RecentEditsContextProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt/PromptAssembler.cpp
    ${CMAKE_SOURCE_DIR}/src/render/GhostTextInlineNoteProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/render/GhostTextOverlayWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/network/AbstractAIProvider.h
    ${CMAKE_SOURCE_DIR}/src/network/OpenAICompatibleProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/network/CopilotCodexProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/network/SSEParser.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt/PromptTemplate.cpp
    ${CMAKE_SOURCE_DIR}/src/prompt/CopilotCodexPromptBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/auth/CopilotAuthManager.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/KateAiConfigPage.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/CompletionSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/KWalletSecretStore.cpp
    TEST_NAME kateaiinlinecompletion_config_page_test
    LINK_LIBRARIES
        Qt6::Test
        Qt6::Widgets
        Qt6::Network
        KF6::TextEditor
        KF6::Wallet
        KF6::I18n
        KF6::ConfigCore
        KF6::CoreAddons
        KF6::XmlGui
)
target_include_directories(kateaiinlinecompletion_config_page_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)
