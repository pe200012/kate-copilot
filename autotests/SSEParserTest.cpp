/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SSEParserTest
*/

#include "network/SSEParser.h"

#include <QtTest>

using KateAiInlineCompletion::SSEMessage;
using KateAiInlineCompletion::SSEParser;

class SSEParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesSingleMessageLf();
    void parsesSingleMessageCrlf();
    void parsesChunkedInput();
    void joinsMultiLineData();
};

void SSEParserTest::parsesSingleMessageLf()
{
    SSEParser p;
    const QVector<SSEMessage> out = p.feed(QByteArray("data: {\"k\":1}\n\n"));

    QCOMPARE(out.size(), 1);
    QCOMPARE(out[0].event, QString());
    QCOMPARE(out[0].data, QStringLiteral("{\"k\":1}"));
}

void SSEParserTest::parsesSingleMessageCrlf()
{
    SSEParser p;
    const QVector<SSEMessage> out = p.feed(QByteArray("event: message\r\ndata: hello\r\n\r\n"));

    QCOMPARE(out.size(), 1);
    QCOMPARE(out[0].event, QStringLiteral("message"));
    QCOMPARE(out[0].data, QStringLiteral("hello"));
}

void SSEParserTest::parsesChunkedInput()
{
    SSEParser p;

    QCOMPARE(p.feed(QByteArray("data: part1\n")).size(), 0);
    QCOMPARE(p.feed(QByteArray("\n")).size(), 1);

    const QVector<SSEMessage> out = p.feed(QByteArray());
    QCOMPARE(out.size(), 0);
}

void SSEParserTest::joinsMultiLineData()
{
    SSEParser p;
    const QVector<SSEMessage> out = p.feed(QByteArray("data: a\ndata: b\n\n"));

    QCOMPARE(out.size(), 1);
    QCOMPARE(out[0].data, QStringLiteral("a\nb"));
}

QTEST_MAIN(SSEParserTest)

#include "SSEParserTest.moc"
