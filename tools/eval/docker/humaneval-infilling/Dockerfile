# SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
# SPDX-License-Identifier: LGPL-2.0-or-later

# Container for running HumanEval-Infilling functional correctness evaluation.
#
# The evaluation executes untrusted model-generated Python code. Running it in a
# container provides filesystem isolation and enables additional runtime limits
# via `docker run` flags.

FROM python:3.11-slim

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1

RUN python -m pip install --no-cache-dir -U pip setuptools wheel \
    && python -m pip install --no-cache-dir numpy tqdm fire

WORKDIR /workspace
