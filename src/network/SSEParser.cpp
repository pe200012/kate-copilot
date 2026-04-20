/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SSEParser
*/

#include "network/SSEParser.h"

#include <QStringList>

using namespace KateAiInlineCompletion;

int SSEParser::findBoundary(const QByteArray &buffer, int *separatorLen)
{
    const int crlf = buffer.indexOf("\r\n\r\n");
    const int lf = buffer.indexOf("\n\n");

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
        int sepLen = 0;
        const int boundary = findBoundary(m_buffer, &sepLen);
        if (boundary < 0) {
            break;
        }

        const QByteArray block = m_buffer.left(boundary);
        m_buffer.remove(0, boundary + sepLen);

        if (block.trimmed().isEmpty()) {
            continue;
        }

        QString eventName;
        QStringList dataLines;

        const QList<QByteArray> lines = block.split('\n');
        for (QByteArray line : lines) {
            if (line.endsWith('\r')) {
                line.chop(1);
            }

            if (line.startsWith(':')) {
                continue;
            }

            if (line.startsWith("event:")) {
                eventName = QString::fromUtf8(line.mid(6)).trimmed();
                continue;
            }

            if (line.startsWith("data:")) {
                QByteArray payload = line.mid(5);
                if (!payload.isEmpty() && payload[0] == ' ') {
                    payload = payload.mid(1);
                }

                dataLines.push_back(QString::fromUtf8(payload));
                continue;
            }
        }

        if (dataLines.isEmpty()) {
            continue;
        }

        SSEMessage msg;
        msg.event = eventName;
        msg.data = dataLines.join(QStringLiteral("\n"));
        out.push_back(msg);
    }

    return out;
}

void SSEParser::reset()
{
    m_buffer.clear();
}
