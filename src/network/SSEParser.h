/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SSEParser

    Provides framing for Server-Sent Events (SSE).
    The parser accepts incremental byte chunks and yields complete messages.

    Supported line endings:
    - \n
    - \r\n

    Message delimiter:
    - empty line (\n\n or \r\n\r\n)
*/

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace KateAiInlineCompletion
{

struct SSEMessage {
    QString event;
    QString data;
};

class SSEParser
{
public:
    [[nodiscard]] QVector<SSEMessage> feed(const QByteArray &chunk);
    void reset();

private:
    [[nodiscard]] static int findBoundary(const QByteArray &buffer, int *separatorLen);

    QByteArray m_buffer;
};

} // namespace KateAiInlineCompletion
