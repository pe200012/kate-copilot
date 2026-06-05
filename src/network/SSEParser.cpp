/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SSEParser
*/

#include "network/SSEParser.h"

#include <cstring>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] bool lineStartsWith(const QByteArray &block, qsizetype start, qsizetype lineLen, const char *prefix, qsizetype prefixLen)
{
    return lineLen >= prefixLen && std::memcmp(block.constData() + start, prefix, static_cast<size_t>(prefixLen)) == 0;
}

void appendDataField(QString &data, bool &hasData, const QByteArray &block, qsizetype payloadStart, qsizetype payloadEnd)
{
    if (payloadStart < payloadEnd && block[payloadStart] == ' ') {
        ++payloadStart;
    }

    if (hasData) {
        data.append(QLatin1Char('\n'));
    }

    data.append(QString::fromUtf8(block.constData() + payloadStart, payloadEnd - payloadStart));
    hasData = true;
}
} // namespace

qsizetype SSEParser::findBoundary(const QByteArray &buffer, qsizetype *separatorLen)
{
    const qsizetype crlf = buffer.indexOf("\r\n\r\n");
    const qsizetype lf = buffer.indexOf("\n\n");

    if (crlf >= 0 && (lf < 0 || crlf < lf)) {
        *separatorLen = 4;
        return crlf;
    }

    if (lf >= 0) {
        *separatorLen = 2;
        return lf;
    }

    *separatorLen = 0;
    return -1;
}

QVector<SSEMessage> SSEParser::feed(const QByteArray &chunk)
{
    if (!chunk.isEmpty()) {
        m_buffer += chunk;
    }

    QVector<SSEMessage> out;

    while (true) {
        qsizetype sepLen = 0;
        const qsizetype boundary = findBoundary(m_buffer, &sepLen);
        if (boundary < 0) {
            break;
        }

        const QByteArray block = m_buffer.left(boundary);
        m_buffer.remove(0, boundary + sepLen);

        if (block.trimmed().isEmpty()) {
            continue;
        }

        QString eventName;
        QString data;
        bool hasData = false;

        qsizetype lineStart = 0;
        while (lineStart <= block.size()) {
            qsizetype lineStop = block.indexOf('\n', lineStart);
            if (lineStop < 0) {
                lineStop = block.size();
            }

            qsizetype lineEnd = lineStop;
            if (lineEnd > lineStart && block[lineEnd - 1] == '\r') {
                --lineEnd;
            }

            const qsizetype lineLen = lineEnd - lineStart;
            if (lineLen > 0 && block[lineStart] != ':') {
                if (lineStartsWith(block, lineStart, lineLen, "event:", 6)) {
                    eventName = QString::fromUtf8(block.constData() + lineStart + 6, lineEnd - lineStart - 6).trimmed();
                } else if (lineStartsWith(block, lineStart, lineLen, "data:", 5)) {
                    appendDataField(data, hasData, block, lineStart + 5, lineEnd);
                } else if (lineLen == 4 && lineStartsWith(block, lineStart, lineLen, "data", 4)) {
                    appendDataField(data, hasData, block, lineEnd, lineEnd);
                }
            }

            if (lineStop == block.size()) {
                break;
            }
            lineStart = lineStop + 1;
        }

        if (!hasData) {
            continue;
        }

        SSEMessage msg;
        msg.event = eventName;
        msg.data = data;
        out.push_back(msg);
    }

    return out;
}

void SSEParser::reset()
{
    m_buffer.clear();
}

} // namespace KateAiInlineCompletion
