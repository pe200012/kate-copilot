"""Module: ollama_openai_client

OpenAI-compatible Chat Completions client.

The Kate plugin uses a streaming SSE provider. Offline evaluation focuses on
prompt quality, so this client uses `stream=false` to simplify correctness and
reproducibility.

Endpoint example:
- http://192.168.62.31:11434/v1/chat/completions

Ollama accepts any bearer token, so `api_key` is treated as optional.
"""

from __future__ import annotations

from dataclasses import dataclass

import requests


@dataclass(frozen=True)
class ChatCompletionParams:
    endpoint: str
    model: str
    system_prompt: str
    user_prompt: str
    api_key: str
    max_tokens: int
    temperature: float
    stop: list[str]
    timeout_s: float


class OpenAICompatibleError(RuntimeError):
    pass


def chat_completion(params: ChatCompletionParams) -> str:
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json",
    }
    if params.api_key.strip():
        headers["Authorization"] = f"Bearer {params.api_key.strip()}"

    payload: dict = {
        "model": params.model,
        "stream": False,
        "temperature": params.temperature,
        "max_tokens": params.max_tokens,
        "messages": [],
    }

    if params.system_prompt.strip():
        payload["messages"].append({"role": "system", "content": params.system_prompt})
    payload["messages"].append({"role": "user", "content": params.user_prompt})

    if params.stop:
        payload["stop"] = params.stop

    r = requests.post(params.endpoint, json=payload, headers=headers, timeout=params.timeout_s)

    if r.status_code >= 400:
        raise OpenAICompatibleError(f"HTTP {r.status_code}: {r.text}")

    try:
        data = r.json()
    except ValueError as e:
        raise OpenAICompatibleError(f"Invalid JSON: {e}: {r.text}") from e

    if isinstance(data, dict) and "error" in data:
        err = data.get("error")
        if isinstance(err, dict) and "message" in err:
            raise OpenAICompatibleError(str(err.get("message")))
        raise OpenAICompatibleError(str(err))

    try:
        choice0 = data["choices"][0]
    except Exception as e:
        raise OpenAICompatibleError(f"Missing choices: {data}") from e

    if isinstance(choice0, dict):
        msg = choice0.get("message")
        if isinstance(msg, dict) and isinstance(msg.get("content"), str):
            return msg["content"]
        if isinstance(choice0.get("text"), str):
            return choice0["text"]

    raise OpenAICompatibleError(f"Unsupported response shape: {data}")


@dataclass(frozen=True)
class TextCompletionParams:
    endpoint: str
    model: str
    prompt: str
    suffix: str
    api_key: str
    max_tokens: int
    temperature: float
    stop: list[str]
    timeout_s: float


def text_completion(params: TextCompletionParams) -> str:
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json",
    }
    if params.api_key.strip():
        headers["Authorization"] = f"Bearer {params.api_key.strip()}"

    payload: dict = {
        "model": params.model,
        "stream": False,
        "prompt": params.prompt,
        "temperature": params.temperature,
        "max_tokens": params.max_tokens,
    }

    if params.suffix:
        payload["suffix"] = params.suffix

    if params.stop:
        payload["stop"] = params.stop

    r = requests.post(params.endpoint, json=payload, headers=headers, timeout=params.timeout_s)

    if r.status_code >= 400:
        raise OpenAICompatibleError(f"HTTP {r.status_code}: {r.text}")

    try:
        data = r.json()
    except ValueError as e:
        raise OpenAICompatibleError(f"Invalid JSON: {e}: {r.text}") from e

    if isinstance(data, dict) and "error" in data:
        err = data.get("error")
        if isinstance(err, dict) and "message" in err:
            raise OpenAICompatibleError(str(err.get("message")))
        raise OpenAICompatibleError(str(err))

    try:
        choice0 = data["choices"][0]
    except Exception as e:
        raise OpenAICompatibleError(f"Missing choices: {data}") from e

    if isinstance(choice0, dict) and isinstance(choice0.get("text"), str):
        return choice0["text"]

    raise OpenAICompatibleError(f"Unsupported response shape: {data}")
