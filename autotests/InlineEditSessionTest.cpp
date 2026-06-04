/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditSessionTest
*/

#include "inlineedit/InlineEditSession.h"
#include "plugin/KateAiInlineCompletionPlugin.h"
#include "settings/CompletionSettings.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QPushButton>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

using KateAiInlineCompletion::CompletionSettings;
using KateAiInlineCompletion::InlineEditSession;

namespace
{
class FakeSseServer final : public QObject
{
    Q_OBJECT

public:
    explicit FakeSseServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                m_sockets.append(socket);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    m_requestBuffer[socket] += socket->readAll();
                    const int headerEnd = m_requestBuffer[socket].indexOf("\r\n\r\n");
                    if (headerEnd < 0) {
                        return;
                    }

                    int contentLength = 0;
                    const QList<QByteArray> headerLines = m_requestBuffer[socket].left(headerEnd).split('\n');
                    for (QByteArray line : headerLines) {
                        line = line.trimmed();
                        if (line.toLower().startsWith("content-length:")) {
                            contentLength = line.mid(QByteArray("content-length:").size()).trimmed().toInt();
                        }
                    }
                    if (m_requestBuffer[socket].size() < headerEnd + 4 + contentLength) {
                        return;
                    }

                    m_lastRequestBody = m_requestBuffer[socket].mid(headerEnd + 4, contentLength);
                    ++m_requestCount;

                    socket->write("HTTP/1.1 200 OK\r\n");
                    socket->write("Content-Type: text/event-stream\r\n");
                    socket->write("Cache-Control: no-cache\r\n");
                    socket->write("Connection: close\r\n\r\n");
                    socket->flush();

                    int delayMs = m_initialFrameDelayMs;
                    for (const QByteArray &frame : std::as_const(m_frames)) {
                        QTimer::singleShot(delayMs, socket, [socket, frame] {
                            if (!socket->isOpen()) {
                                return;
                            }
                            socket->write(frame);
                            socket->flush();
                        });
                        delayMs += 20;
                    }
                    QTimer::singleShot(delayMs + 20, socket, [socket] {
                        if (socket->isOpen()) {
                            socket->disconnectFromHost();
                        }
                    });
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }

    QUrl endpoint() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/v1/chat/completions").arg(m_server.serverPort()));
    }

    void setInlineEditJson(const QString &jsonText)
    {
        QJsonObject delta;
        delta[QStringLiteral("content")] = jsonText;
        QJsonObject choice;
        choice[QStringLiteral("index")] = 0;
        choice[QStringLiteral("delta")] = delta;
        choice[QStringLiteral("finish_reason")] = QStringLiteral("stop");
        QJsonObject frame;
        frame[QStringLiteral("choices")] = QJsonArray{choice};
        m_frames = {QByteArray("data: ") + QJsonDocument(frame).toJson(QJsonDocument::Compact) + QByteArray("\n\n"), QByteArray("data: [DONE]\n\n")};
    }

    void setInitialFrameDelayMs(int delayMs)
    {
        m_initialFrameDelayMs = qMax(0, delayMs);
    }

    int requestCount() const
    {
        return m_requestCount;
    }

    QByteArray lastRequestBody() const
    {
        return m_lastRequestBody;
    }

private:
    QTcpServer m_server;
    QList<QPointer<QTcpSocket>> m_sockets;
    QHash<QTcpSocket *, QByteArray> m_requestBuffer;
    QByteArray m_lastRequestBody;
    QList<QByteArray> m_frames;
    int m_requestCount = 0;
    int m_initialFrameDelayMs = 0;
};

struct Harness {
    QWidget window;
    QVBoxLayout *layout = nullptr;
    QPushButton *otherFocusWidget = nullptr;
    QScopedPointer<KTextEditor::Document> doc;
    KTextEditor::View *view = nullptr;
    KateAiInlineCompletionPlugin plugin;
    QNetworkAccessManager manager;
    InlineEditSession *session = nullptr;

    explicit Harness(const QUrl &endpoint)
        : plugin(nullptr, {})
    {
        QStandardPaths::setTestModeEnabled(true);
        window.resize(900, 320);
        layout = new QVBoxLayout(&window);
        layout->setContentsMargins(0, 0, 0, 0);

        auto *editor = KTextEditor::Editor::instance();
        Q_ASSERT(editor);
        doc.reset(editor->createDocument(&window));
        Q_ASSERT(doc);
        doc->setText(QStringLiteral("int main() {\n    return oldValue;\n}\n"));

        view = doc->createView(&window);
        Q_ASSERT(view);
        layout->addWidget(view);
        otherFocusWidget = new QPushButton(QStringLiteral("other"), &window);
        layout->addWidget(otherFocusWidget);

        CompletionSettings settings = CompletionSettings::defaults();
        settings.enabled = false;
        settings.enableInlineEdits = true;
        settings.provider = QString::fromLatin1(CompletionSettings::kProviderOpenAICompatible);
        settings.endpoint = endpoint;
        settings.model = QStringLiteral("test-model");
        settings.inlineEditUseContext = false;
        plugin.setSettings(settings);

        session = new InlineEditSession(view, &plugin, nullptr, &manager, nullptr, nullptr, nullptr, view);
        window.show();
        QTest::qWait(120);
        qApp->processEvents();
    }
};
} // namespace

class InlineEditSessionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void triggerCreatesRequestForSelectedRange();
    void validResponseCreatesPreview();
    void acceptAppliesReplacementTransactionally();
    void responseOutsideRequestedRangeDoesNotCreatePreview();
    void selectionChangeCancelsActiveRequest();
    void dismissClearsPreview();
    void cursorMoveClearsPreview();
    void documentChangeClearsPreview();
};

void InlineEditSessionTest::triggerCreatesRequestForSelectedRange()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setInlineEditJson(QStringLiteral(R"({"edits":[{"startLine":2,"startColumn":5,"endLine":2,"endColumn":21,"newText":"return newValue;"}]})"));

    Harness harness(server.endpoint());
    harness.view->setSelection(KTextEditor::Range(1, 4, 1, 20));
    harness.session->triggerInlineEdit();

    QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 2000);
    const QJsonDocument body = QJsonDocument::fromJson(server.lastRequestBody());
    QVERIFY(body.isObject());
    const QJsonArray messages = body.object().value(QStringLiteral("messages")).toArray();
    QVERIFY(messages.size() >= 2);
    const QString userPrompt = messages.at(messages.size() - 1).toObject().value(QStringLiteral("content")).toString();
    QVERIFY(userPrompt.contains(QStringLiteral("startLine: 2")));
    QVERIFY(userPrompt.contains(QStringLiteral("startColumn: 5")));
    QVERIFY(userPrompt.contains(QStringLiteral("return oldValue;")));
}

void InlineEditSessionTest::validResponseCreatesPreview()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setInlineEditJson(QStringLiteral(R"({"edits":[{"startLine":2,"startColumn":5,"endLine":2,"endColumn":21,"newText":"return newValue;"}]})"));

    Harness harness(server.endpoint());
    QSignalSpy spy(harness.session, &InlineEditSession::previewStateChanged);
    harness.view->setSelection(KTextEditor::Range(1, 4, 1, 20));
    harness.session->triggerInlineEdit();

    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasPreview(), 2000);
    QVERIFY(spy.count() > 0);
    QCOMPARE(harness.session->currentSuggestion().edits.constFirst().newText, QStringLiteral("return newValue;"));
    QCOMPARE(harness.doc->text(), QStringLiteral("int main() {\n    return oldValue;\n}\n"));
}

void InlineEditSessionTest::acceptAppliesReplacementTransactionally()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setInlineEditJson(QStringLiteral(R"({"edits":[{"startLine":2,"startColumn":5,"endLine":2,"endColumn":21,"newText":"return newValue;"}]})"));

    Harness harness(server.endpoint());
    harness.view->setSelection(KTextEditor::Range(1, 4, 1, 20));
    harness.session->triggerInlineEdit();
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasPreview(), 2000);

    harness.session->acceptInlineEdit();

    QCOMPARE(harness.doc->text(), QStringLiteral("int main() {\n    return newValue;\n}\n"));
    QVERIFY(!harness.session->hasPreview());
}

void InlineEditSessionTest::responseOutsideRequestedRangeDoesNotCreatePreview()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setInlineEditJson(QStringLiteral("{\"edits\":[{\"startLine\":1,\"startColumn\":1,\"endLine\":1,\"endColumn\":11,\"newText\":\"int other()\"}]}"));

    Harness harness(server.endpoint());
    harness.view->setSelection(KTextEditor::Range(1, 4, 1, 20));
    harness.session->triggerInlineEdit();

    QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!harness.session->hasActiveRequest(), 2000);
    QVERIFY(!harness.session->hasPreview());
    QCOMPARE(harness.doc->text(), QStringLiteral("int main() {\n    return oldValue;\n}\n"));
}

void InlineEditSessionTest::selectionChangeCancelsActiveRequest()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setInlineEditJson(QStringLiteral(R"({"edits":[{"startLine":2,"startColumn":5,"endLine":2,"endColumn":21,"newText":"return newValue;"}]})"));
    server.setInitialFrameDelayMs(300);

    Harness harness(server.endpoint());
    harness.view->setSelection(KTextEditor::Range(1, 4, 1, 20));
    harness.session->triggerInlineEdit();
    QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 2000);

    harness.view->setSelection(KTextEditor::Range(0, 0, 0, 10));

    QTRY_VERIFY_WITH_TIMEOUT(!harness.session->hasActiveRequest(), 2000);
    QTest::qWait(400);
    QVERIFY(!harness.session->hasPreview());
    QCOMPARE(harness.doc->text(), QStringLiteral("int main() {\n    return oldValue;\n}\n"));
}

void InlineEditSessionTest::dismissClearsPreview()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setInlineEditJson(QStringLiteral(R"({"edits":[{"startLine":2,"startColumn":5,"endLine":2,"endColumn":21,"newText":"return newValue;"}]})"));

    Harness harness(server.endpoint());
    harness.view->setSelection(KTextEditor::Range(1, 4, 1, 20));
    harness.session->triggerInlineEdit();
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasPreview(), 2000);

    harness.session->dismissInlineEdit();

    QVERIFY(!harness.session->hasPreview());
    QCOMPARE(harness.doc->text(), QStringLiteral("int main() {\n    return oldValue;\n}\n"));
}

void InlineEditSessionTest::cursorMoveClearsPreview()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setInlineEditJson(QStringLiteral(R"({"edits":[{"startLine":2,"startColumn":5,"endLine":2,"endColumn":21,"newText":"return newValue;"}]})"));

    Harness harness(server.endpoint());
    harness.view->setSelection(KTextEditor::Range(1, 4, 1, 20));
    harness.session->triggerInlineEdit();
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasPreview(), 2000);

    harness.view->setCursorPosition(KTextEditor::Cursor(2, 0));
    QTRY_VERIFY_WITH_TIMEOUT(!harness.session->hasPreview(), 2000);
}

void InlineEditSessionTest::documentChangeClearsPreview()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setInlineEditJson(QStringLiteral(R"({"edits":[{"startLine":2,"startColumn":5,"endLine":2,"endColumn":21,"newText":"return newValue;"}]})"));

    Harness harness(server.endpoint());
    harness.view->setSelection(KTextEditor::Range(1, 4, 1, 20));
    harness.session->triggerInlineEdit();
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasPreview(), 2000);

    harness.doc->insertText(KTextEditor::Cursor(0, 0), QStringLiteral("// edit\n"));
    QTRY_VERIFY_WITH_TIMEOUT(!harness.session->hasPreview(), 2000);
}

QTEST_MAIN(InlineEditSessionTest)

#include "InlineEditSessionTest.moc"
