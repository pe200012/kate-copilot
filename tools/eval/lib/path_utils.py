"""Module: path_utils

Filesystem-safe path helpers.

Some model ids contain characters such as ':' which are valid on Linux paths but
conflict with Docker volume syntax (<host>:<container>:<mode>). This module
normalizes such ids when they are used as directory names.
"""

from __future__ import annotations

import re


_ALLOWED = re.compile(r"[^A-Za-z0-9_.-]+")


def slugify_segment(text: str) -> str:
    text = (text or "").strip()
    if not text:
        return "_"

    text = text.replace("/", "_")
    text = text.replace(":", "_")
    text = _ALLOWED.sub("_", text)

    text = re.sub(r"_+", "_", text)
    return text.strip("_") or "_"
