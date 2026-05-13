# SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
# SPDX-License-Identifier: LGPL-2.0-or-later

add_library(kateaiinlinecompletion_settings STATIC)

target_sources(kateaiinlinecompletion_settings
    PRIVATE
        settings/CompletionSettings.cpp
)

target_include_directories(kateaiinlinecompletion_settings
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kateaiinlinecompletion_settings
    PUBLIC
        Qt6::Core
        KF6::ConfigCore
)

add_library(kateaiinlinecompletion_secrets STATIC)

target_sources(kateaiinlinecompletion_secrets
    PRIVATE
        settings/KWalletSecretStore.cpp
)

target_include_directories(kateaiinlinecompletion_secrets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kateaiinlinecompletion_secrets
    PUBLIC
        Qt6::Core
        KF6::Wallet
)

add_library(kateaiinlinecompletion_sse STATIC)

target_sources(kateaiinlinecompletion_sse
    PRIVATE
        network/SSEParser.cpp
)

target_include_directories(kateaiinlinecompletion_sse
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kateaiinlinecompletion_sse
    PUBLIC
        Qt6::Core
)

add_library(kateaiinlinecompletion_context STATIC)

target_sources(kateaiinlinecompletion_context
    PRIVATE
        context/ContextProviderRegistry.cpp
        context/CurrentFileContextProvider.cpp
        context/ProjectTraitsContextProvider.cpp
        context/OpenTabsContextProvider.cpp
)

target_include_directories(kateaiinlinecompletion_context
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kateaiinlinecompletion_context
    PUBLIC
        Qt6::Core
        KF6::TextEditor
)

add_library(kateaiinlinecompletion_prompt STATIC)

target_sources(kateaiinlinecompletion_prompt
    PRIVATE
        prompt/PromptTemplate.cpp
        prompt/CopilotCodexPromptBuilder.cpp
        prompt/PromptAssembler.cpp
)

target_include_directories(kateaiinlinecompletion_prompt
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kateaiinlinecompletion_prompt
    PUBLIC
        kateaiinlinecompletion_settings
        kateaiinlinecompletion_context
        Qt6::Core
        KF6::TextEditor
)

add_library(kateaiinlinecompletion_auth STATIC)

target_sources(kateaiinlinecompletion_auth
    PRIVATE
        auth/CopilotAuthManager.cpp
)

target_include_directories(kateaiinlinecompletion_auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kateaiinlinecompletion_auth
    PUBLIC
        kateaiinlinecompletion_secrets
        Qt6::Core
        Qt6::Network
)

set_target_properties(
    kateaiinlinecompletion_settings
    kateaiinlinecompletion_secrets
    kateaiinlinecompletion_sse
    kateaiinlinecompletion_prompt
    kateaiinlinecompletion_context
    kateaiinlinecompletion_auth
    PROPERTIES POSITION_INDEPENDENT_CODE ON
)

kcoreaddons_add_plugin(kateaiinlinecompletion
    SOURCES
        plugin/KateAiInlineCompletionPlugin.cpp
        plugin/KateAiInlineCompletionPlugin.h
        plugin/KateAiInlineCompletionPluginView.cpp
        plugin/KateAiInlineCompletionPluginView.h

        settings/KateAiConfigPage.cpp
        settings/KateAiConfigPage.h

        session/EditorSession.cpp
        session/EditorSession.h
        session/GhostTextState.h
        session/SuggestionAnchorTracker.cpp
        session/SuggestionAnchorTracker.h
        session/SuggestionPostProcessor.cpp
        session/SuggestionPostProcessor.h

        render/GhostTextInlineNoteProvider.cpp
        render/GhostTextInlineNoteProvider.h
        render/GhostTextOverlayWidget.cpp
        render/GhostTextOverlayWidget.h
        render/GhostTextPushDownOverlay.cpp
        render/GhostTextPushDownOverlay.h

        network/AbstractAIProvider.h
        network/OpenAICompatibleProvider.cpp
        network/OpenAICompatibleProvider.h
        network/CopilotCodexProvider.cpp
        network/CopilotCodexProvider.h

        context/ContextItem.h
        context/ContextProvider.h
        context/ContextProviderRegistry.h
        context/CurrentFileContextProvider.h
        context/ProjectTraitsContextProvider.h
        context/OpenTabsContextProvider.h

        prompt/PromptAssembler.h

        plugin/kateaiinlinecompletion.json
    INSTALL_NAMESPACE "kf6/ktexteditor"
)

target_compile_definitions(kateaiinlinecompletion
    PRIVATE
        TRANSLATION_DOMAIN="kateaiinlinecompletion"
)

target_link_libraries(kateaiinlinecompletion
    PRIVATE
        kateaiinlinecompletion_settings
        kateaiinlinecompletion_secrets
        kateaiinlinecompletion_sse
        kateaiinlinecompletion_context
        kateaiinlinecompletion_prompt
        kateaiinlinecompletion_auth
        Qt6::Widgets
        Qt6::Network
        KF6::CoreAddons
        KF6::I18n
        KF6::TextEditor
        KF6::XmlGui
        KF6::ConfigCore
)
