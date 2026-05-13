# SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
# SPDX-License-Identifier: LGPL-2.0-or-later

FROM ubuntu:26.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        appstream \
        build-essential \
        ca-certificates \
        cmake \
        dbus-x11 \
        extra-cmake-modules \
        libegl1 \
        libgl1 \
        libkf6config-dev \
        libkf6coreaddons-dev \
        libkf6i18n-dev \
        libkf6texteditor-dev \
        libkf6wallet-dev \
        libkf6xmlgui-dev \
        libxkbcommon-x11-0 \
        ninja-build \
        qt6-base-dev \
        qt6-base-dev-tools \
        qt6-tools-dev \
    && rm -rf /var/lib/apt/lists/*

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8
