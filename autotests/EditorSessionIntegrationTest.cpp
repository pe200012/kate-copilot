/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: EditorSessionIntegrationTest
*/

#include "plugin/KateAiInlineCompletionPlugin.h"
#include "render/GhostTextOverlayWidget.h"
#include "session/EditorSession.h"
#include "settings/CompletionSettings.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QApplication>
#include <QComboBox>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QPushButton>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

using KateAiInlineCompletion::CompletionSettings;
using KateAiInlineCompletion::EditorSession;
using KateAiInlineCompletion::GhostTextOverlayWidget;

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
                    if (!m_requestBuffer[socket].contains("\r\n\r\n")) {
                        return;
                    }

                    ++m_requestCount;
                    socket->write("HTTP/1.1 200 OK\r\n");
                    socket->write("Content-Type: text/event-stream\r\n");
                    socket->write("Cache-Control: no-cache\r\n");
                    socket->write("Connection: close\r\n\r\n");
                    socket->flush();

                    int delayMs = 0;
                    for (const QByteArray &frame : m_frames) {
                        QTimer::singleShot(delayMs, socket, [socket, frame] {
                            if (!socket->isOpen()) {
                                return;
                            }
                            socket->write(frame);
                            socket->flush();
                        });
                        delayMs += 25;
                    }

                    QTimer::singleShot(delayMs + 25, socket, [socket] {
                        if (!socket->isOpen()) {
                            return;
                        }
                        socket->disconnectFromHost();
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

    void setCompletion(const QString &text)
    {
        const QByteArray json = QStringLiteral(
                                    "data: {\"choices\":[{\"delta\":{\"content\":\"%1\"},\"finish_reason\":null}]}\n\n")
                                    .arg(text)
                                    .toUtf8();
        m_frames = {json, QByteArray("data: [DONE]\n\n")};
    }

    QUrl endpoint() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/v1/chat/completions").arg(m_server.serverPort()));
    }

    int requestCount() const
    {
        return m_requestCount;
    }

private:
    QTcpServer m_server;
    QList<QPointer<QTcpSocket>> m_sockets;
    QHash<QTcpSocket *, QByteArray> m_requestBuffer;
    QList<QByteArray> m_frames;
    int m_requestCount = 0;
};

struct SessionHarness {
    QWidget window;
    QVBoxLayout *layout = nullptr;
    QPushButton *otherFocusWidget = nullptr;
    QScopedPointer<KTextEditor::Document> doc;
    KTextEditor::View *view = nullptr;
    KateAiInlineCompletionPlugin plugin;
    QNetworkAccessManager manager;
    EditorSession *session = nullptr;
    GhostTextOverlayWidget *overlay = nullptr;

    explicit SessionHarness(const QUrl &endpoint)
        : plugin(nullptr, {})
    {
        window.resize(900, 320);
        layout = new QVBoxLayout(&window);
        layout->setContentsMargins(0, 0, 0, 0);

        auto *editor = KTextEditor::Editor::instance();
        Q_ASSERT(editor);

        doc.reset(editor->createDocument(&window));
        Q_ASSERT(doc);
        doc->setText(QStringLiteral("prefixSUFFIX\n\n\n"));

        view = doc->createView(&window);
        Q_ASSERT(view);
        layout->addWidget(view);

        otherFocusWidget = new QPushButton(QStringLiteral("other"), &window);
        layout->addWidget(otherFocusWidget);

        CompletionSettings settings = CompletionSettings::defaults();
        settings.enabled = true;
        settings.debounceMs = CompletionSettings::kDebounceMinMs;
        settings.provider = QString::fromLatin1(CompletionSettings::kProviderOpenAICompatible);
        settings.endpoint = endpoint;
        settings.model = QStringLiteral("test-model");
        settings.suppressWhenCompletionPopupVisible = false;
        plugin.setSettings(settings);

        session = new EditorSession(view, &plugin, nullptr, &manager, nullptr, view);
        overlay = view->editorWidget()->findChild<GhostTextOverlayWidget *>();
        Q_ASSERT(overlay);

        window.show();
        QTest::qWait(150);
        qApp->processEvents();
    }
};

void waitForSuggestion(FakeSseServer &server, KTextEditor::View *view, GhostTextOverlayWidget *overlay)
{
    view->setCursorPosition(KTextEditor::Cursor(0, 6));
    view->editorWidget()->setFocus();

    QTRY_VERIFY_WITH_TIMEOUT(server.requestCount() > 0, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(overlay->isActive(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!overlay->state().visibleText.isEmpty(), 2000);
}

} // namespace

class EditorSessionIntegrationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void tracksAnchorThroughDocumentEdits();
    void tabAcceptsStreamedSuggestion();
    void escapeClearsStreamedSuggestion();
    void focusOutClearsStreamedSuggestion();
};

void EditorSessionIntegrationTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void EditorSessionIntegrationTest::tracksAnchorThroughDocumentEdits()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.overlay);

    QCOMPARE(harness.overlay->state().anchor.column, 6);

    QVERIFY(harness.doc->insertText(KTextEditor::Cursor(0, 0), QStringLiteral("ZZ")));
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().anchor.column, 8, 2000);
}

void EditorSessionIntegrationTest::tabAcceptsStreamedSuggestion()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.overlay);

    harness.view->editorWidget()->setFocus();
    QTRY_VERIFY(harness.view->editorWidget()->hasFocus());

    QTest::keyClick(harness.view->editorWidget(), Qt::Key_Tab);

    QTRY_VERIFY_WITH_TIMEOUT(!harness.overlay->isActive(), 2000);
    QVERIFY(harness.doc->text().contains(QStringLiteral("prefixghost()SUFFIX")));
}

void EditorSessionIntegrationTest::escapeClearsStreamedSuggestion()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.overlay);

    QTest::keyClick(harness.view->editorWidget(), Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!harness.overlay->isActive(), 2000);
    QVERIFY(!harness.doc->text().contains(QStringLiteral("ghost()")));
}

void EditorSessionIntegrationTest::focusOutClearsStreamedSuggestion()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.overlay);

    harness.otherFocusWidget->setFocus();
    QTRY_VERIFY_WITH_TIMEOUT(harness.otherFocusWidget->hasFocus(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!harness.overlay->isActive(), 2000);
}

QTEST_MAIN(EditorSessionIntegrationTest)

#include "EditorSessionIntegrationTest.moc"
