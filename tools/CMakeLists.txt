# SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
# SPDX-License-Identifier: LGPL-2.0-or-later

add_executable(kateaiinlinecompletion_ollama_smoke_test)

target_sources(kateaiinlinecompletion_ollama_smoke_test
    PRIVATE
        ollama_smoke_test.cpp
        ${CMAKE_SOURCE_DIR}/src/network/OpenAICompatibleProvider.cpp
        ${CMAKE_SOURCE_DIR}/src/network/AbstractAIProvider.h
)

target_include_directories(kateaiinlinecompletion_ollama_smoke_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(kateaiinlinecompletion_ollama_smoke_test
    PRIVATE
        kateaiinlinecompletion_sse
        Qt6::Core
        Qt6::Network
)
