/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: EditorSessionIntegrationTest
*/

#include "context/DiagnosticStore.h"
#include "context/RecentEditsTracker.h"
#include "plugin/KateAiInlineCompletionPlugin.h"
#include "render/GhostTextOverlayWidget.h"
#include "session/CompletionCache.h"
#include "session/EditorSession.h"
#include "settings/CompletionSettings.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QApplication>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQueue>
#include <QPushButton>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

using KateAiInlineCompletion::CompletionCache;
using KateAiInlineCompletion::CompletionSettings;
using KateAiInlineCompletion::DiagnosticStore;
using KateAiInlineCompletion::EditorSession;
using KateAiInlineCompletion::GhostTextOverlayWidget;
using KateAiInlineCompletion::RecentEditsTracker;

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

                    const QByteArray header = m_requestBuffer[socket].left(headerEnd);
                    const QList<QByteArray> headerLines = header.split('\n');
                    int contentLength = 0;
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

                    const QList<QByteArray> frames = m_frameQueue.isEmpty() ? m_frames : m_frameQueue.dequeue();
                    const int frameDelayMs = m_frameDelayQueue.isEmpty() ? m_frameDelayMs : m_frameDelayQueue.dequeue();

                    int delayMs = 0;
                    for (const QByteArray &frame : frames) {
                        QTimer::singleShot(delayMs, socket, [socket, frame] {
                            if (!socket->isOpen()) {
                                return;
                            }
                            socket->write(frame);
                            socket->flush();
                        });
                        delayMs += frameDelayMs;
                    }

                    QTimer::singleShot(delayMs + frameDelayMs, socket, [socket] {
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
        setCompletionFrames({text});
    }

    void setCompletionFrames(const QStringList &texts, int frameDelayMs = 25)
    {
        m_frames = completionFrames(texts);
        m_frameDelayMs = frameDelayMs;
    }

    void enqueueCompletion(const QString &text, int frameDelayMs = 25)
    {
        enqueueCompletionFrames({text}, frameDelayMs);
    }

    void enqueueCompletionFrames(const QStringList &texts, int frameDelayMs = 25)
    {
        m_frameQueue.enqueue(completionFrames(texts));
        m_frameDelayQueue.enqueue(frameDelayMs);
    }

    void enqueueCompletionChoices(const QVector<QString> &choices, int frameDelayMs = 25)
    {
        m_frameQueue.enqueue(completionChoiceFrames(choices));
        m_frameDelayQueue.enqueue(frameDelayMs);
    }

    static QList<QByteArray> completionFrames(const QStringList &texts)
    {
        QList<QByteArray> frames;
        for (const QString &text : texts) {
            QJsonObject delta;
            delta[QStringLiteral("content")] = text;

            QJsonObject choice;
            choice[QStringLiteral("delta")] = delta;
            choice[QStringLiteral("finish_reason")] = QJsonValue();

            QJsonArray choices;
            choices.append(choice);

            QJsonObject obj;
            obj[QStringLiteral("choices")] = choices;

            const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
            frames.push_back(QByteArray("data: ") + payload + QByteArray("\n\n"));
        }
        frames.push_back(QByteArray("data: [DONE]\n\n"));
        return frames;
    }

    void setCompletionChoices(const QVector<QString> &choices)
    {
        m_frames = completionChoiceFrames(choices);
        m_frameDelayMs = 25;
    }

    static QList<QByteArray> completionChoiceFrames(const QVector<QString> &choices)
    {
        QList<QByteArray> frames;
        for (int i = 0; i < choices.size(); ++i) {
            QJsonObject delta;
            delta[QStringLiteral("content")] = choices.at(i);

            QJsonObject choice;
            choice[QStringLiteral("index")] = i;
            choice[QStringLiteral("delta")] = delta;
            choice[QStringLiteral("finish_reason")] = QStringLiteral("stop");

            QJsonArray choicesArray;
            choicesArray.append(choice);

            QJsonObject obj;
            obj[QStringLiteral("choices")] = choicesArray;

            const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
            frames.push_back(QByteArray("data: ") + payload + QByteArray("\n\n"));
        }
        frames.push_back(QByteArray("data: [DONE]\n\n"));
        return frames;
    }


    QUrl endpoint() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/v1/chat/completions").arg(m_server.serverPort()));
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
    QQueue<QList<QByteArray>> m_frameQueue;
    QQueue<int> m_frameDelayQueue;
    int m_frameDelayMs = 25;
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
    CompletionCache completionCache;
    RecentEditsTracker recentEditsTracker;
    DiagnosticStore diagnosticStore;
    EditorSession *session = nullptr;
    GhostTextOverlayWidget *overlay = nullptr;

    explicit SessionHarness(const QUrl &endpoint, bool initiallyEnabled = true)
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
        settings.enabled = initiallyEnabled;
        settings.debounceMs = CompletionSettings::kDebounceMinMs;
        settings.provider = QString::fromLatin1(CompletionSettings::kProviderOpenAICompatible);
        settings.endpoint = endpoint;
        settings.model = QStringLiteral("test-model");
        settings.suppressWhenCompletionPopupVisible = false;
        plugin.setSettings(settings);

        recentEditsTracker.trackDocument(doc.data(), QStringLiteral("/tmp/editor-session.cpp"));
        session = new EditorSession(view, &plugin, nullptr, &manager, nullptr, &recentEditsTracker, &diagnosticStore, &completionCache, view);
        overlay = view->editorWidget()->findChild<GhostTextOverlayWidget *>();
        Q_ASSERT(overlay);

        window.show();
        QTest::qWait(150);
        qApp->processEvents();
    }
};

void waitForSuggestion(FakeSseServer &server, KTextEditor::View *view, EditorSession *session)
{
    view->setCursorPosition(KTextEditor::Cursor(0, 6));
    view->editorWidget()->setFocus();

    QTRY_VERIFY_WITH_TIMEOUT(server.requestCount() > 0, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(session->hasVisibleSuggestion(), 2000);
}

} // namespace

class EditorSessionIntegrationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void singleLineSuggestionUsesInlineNoteInsteadOfOverlay();
    void multilineSuggestionUsesOverlay();
    void tracksAnchorThroughDocumentEdits();
    void ctrlRightAcceptsNextWord();
    void ctrlAltRightAcceptsNextLine();
    void promptContextSlotsExcludeCurrentFileMetadataTraits();
    void requestUsesCompletionStrategySettings();
    void afterAcceptRequestUsesAfterAcceptStrategy();
    void manualTriggerRequestsCandidatesAfterSingleCandidateCache();
    void manualTriggerRequestsMultipleCandidatesAndCycles();
    void acceptingAfterCyclingInsertsSelectedCandidate();
    void partialAcceptAfterCyclingKeepsSelectedCandidate();
    void speculativeRequestStoresNextSuggestionInCacheOnly();
    void typingPrefixOfVisibleSuggestionKeepsRemainingSuggestionWithoutRequest();
    void typingDuringStreamingKeepsRequestAndUsesLaterDeltas();
    void typingNonmatchingTextClearsSuggestionAndSchedulesRequest();
    void cachedSuggestionCanBeShownWithoutProviderRequest();
    void tabAcceptsStreamedSuggestion();
    void tabAcceptsSuggestionWithRestOfLineOverlap();
    void escapeClearsStreamedSuggestion();
    void focusOutClearsStreamedSuggestion();
};

void EditorSessionIntegrationTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void EditorSessionIntegrationTest::singleLineSuggestionUsesInlineNoteInsteadOfOverlay()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);

    QVERIFY(harness.session->hasVisibleSuggestion());
    QVERIFY2(!harness.overlay->isActive(), "single-line suggestions should be rendered by InlineNoteProvider");
}

void EditorSessionIntegrationTest::multilineSuggestionUsesOverlay()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("first\nsecond"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);

    QTRY_VERIFY_WITH_TIMEOUT(harness.overlay->isActive(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("first\nsecond"), 2000);
}

void EditorSessionIntegrationTest::tracksAnchorThroughDocumentEdits()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()\nmore"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);

    QTRY_VERIFY_WITH_TIMEOUT(harness.overlay->isActive(), 2000);
    QCOMPARE(harness.overlay->state().anchor.column, 6);

    QVERIFY(harness.doc->insertText(KTextEditor::Cursor(0, 0), QStringLiteral("ZZ")));
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().anchor.column, 8, 2000);
}

void EditorSessionIntegrationTest::ctrlRightAcceptsNextWord()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("first second third"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);

    harness.view->editorWidget()->setFocus();
    QTRY_VERIFY(harness.view->editorWidget()->hasFocus());

    QTest::keyClick(harness.view->editorWidget(), Qt::Key_Right, Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);

    QTRY_VERIFY_WITH_TIMEOUT(harness.doc->text().contains(QStringLiteral("prefixfirst SUFFIX")), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasVisibleSuggestion(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!harness.overlay->isActive(), 2000);

    harness.session->acceptFullSuggestion();
    QTRY_VERIFY_WITH_TIMEOUT(harness.doc->text().contains(QStringLiteral("prefixfirst second thirdSUFFIX")), 2000);
}

void EditorSessionIntegrationTest::ctrlAltRightAcceptsNextLine()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("hello\nworld"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);

    harness.view->editorWidget()->setFocus();
    QTRY_VERIFY(harness.view->editorWidget()->hasFocus());

    harness.session->acceptNextLine();

    QTRY_VERIFY_WITH_TIMEOUT(harness.doc->text().contains(QStringLiteral("prefixhello\nSUFFIX")), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasVisibleSuggestion(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!harness.overlay->isActive(), 2000);

    harness.session->acceptFullSuggestion();
    QTRY_VERIFY_WITH_TIMEOUT(harness.doc->text().contains(QStringLiteral("prefixhello\nworldSUFFIX")), 2000);
}

void EditorSessionIntegrationTest::promptContextSlotsExcludeCurrentFileMetadataTraits()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);

    const QByteArray body = server.lastRequestBody();
    QVERIFY(!body.isEmpty());
    QVERIFY(!body.contains("file_path:"));
    QVERIFY(!body.contains("language:"));
    QVERIFY(!body.contains("cursor_line:"));
    QVERIFY(!body.contains("cursor_column:"));
}

void EditorSessionIntegrationTest::requestUsesCompletionStrategySettings()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    CompletionSettings settings = harness.plugin.settings();
    settings.singleLineMaxTokens = 37;
    settings.completionTemperature = 0.4;
    settings.singleLineStopAtNewline = true;
    harness.plugin.setSettings(settings);

    waitForSuggestion(server, harness.view, harness.session);

    const QJsonDocument document = QJsonDocument::fromJson(server.lastRequestBody());
    QVERIFY(document.isObject());
    const QJsonObject payload = document.object();
    QCOMPARE(payload.value(QStringLiteral("max_tokens")).toInt(), 37);
    QCOMPARE(payload.value(QStringLiteral("temperature")).toDouble(), 0.4);
    const QJsonArray stopSequences = payload.value(QStringLiteral("stop")).toArray();
    QVERIFY(stopSequences.size() <= 4);
    QVERIFY(stopSequences.contains(QStringLiteral("\n")));
}

void EditorSessionIntegrationTest::afterAcceptRequestUsesAfterAcceptStrategy()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    CompletionSettings settings = harness.plugin.settings();
    settings.singleLineMaxTokens = 37;
    settings.afterAcceptMaxTokens = 55;
    harness.plugin.setSettings(settings);

    waitForSuggestion(server, harness.view, harness.session);
    const int firstRequestCount = server.requestCount();

    harness.session->acceptFullSuggestion();
    harness.session->triggerSuggestion();
    QTRY_VERIFY_WITH_TIMEOUT(server.requestCount() > firstRequestCount, 2000);

    const QJsonDocument document = QJsonDocument::fromJson(server.lastRequestBody());
    QVERIFY(document.isObject());
    const QJsonObject payload = document.object();
    QCOMPARE(payload.value(QStringLiteral("max_tokens")).toInt(), 55);
}

void EditorSessionIntegrationTest::manualTriggerRequestsCandidatesAfterSingleCandidateCache()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.enqueueCompletion(QStringLiteral("single\nline"));
    server.enqueueCompletionChoices({QStringLiteral("first\nline"), QStringLiteral("second\nline"), QStringLiteral("third\nline")});
    server.setCompletion(QStringLiteral("fallback"));

    SessionHarness harness(server.endpoint(), false);
    CompletionSettings settings = harness.plugin.settings();
    settings.enabled = true;
    settings.enableCandidateCycling = false;
    harness.plugin.setSettings(settings);

    harness.view->setCursorPosition(KTextEditor::Cursor(0, 6));
    harness.session->triggerSuggestion();
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("single\nline"), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(harness.completionCache.size() >= 1, 2000);
    const int firstRequestCount = server.requestCount();

    settings.enableCandidateCycling = true;
    settings.manualCandidateCount = 3;
    harness.plugin.setSettings(settings);
    harness.session->triggerSuggestion();

    QTRY_VERIFY_WITH_TIMEOUT(server.requestCount() > firstRequestCount, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(harness.session->candidateCount(), 3, 2000);
    const QJsonDocument document = QJsonDocument::fromJson(server.lastRequestBody());
    QVERIFY(document.isObject());
    QCOMPARE(document.object().value(QStringLiteral("n")).toInt(), 3);
}

void EditorSessionIntegrationTest::manualTriggerRequestsMultipleCandidatesAndCycles()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletionChoices({QStringLiteral("first\nline"), QStringLiteral("second\nline"), QStringLiteral("third\nline")});

    SessionHarness harness(server.endpoint());
    QSignalSpy candidateSpy(harness.session, &EditorSession::candidateStateChanged);
    harness.view->setCursorPosition(KTextEditor::Cursor(0, 6));
    harness.view->editorWidget()->setFocus();

    harness.session->triggerSuggestion();

    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasVisibleSuggestion(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(harness.session->candidateCount(), 3, 2000);
    QVERIFY(candidateSpy.count() > 0);
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("first\nline"), 2000);

    const QJsonDocument document = QJsonDocument::fromJson(server.lastRequestBody());
    QVERIFY(document.isObject());
    QCOMPARE(document.object().value(QStringLiteral("n")).toInt(), 3);

    harness.session->nextCandidate();
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("second\nline"), 2000);

    harness.session->nextCandidate();
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("third\nline"), 2000);

    harness.session->previousCandidate();
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("second\nline"), 2000);
}

void EditorSessionIntegrationTest::acceptingAfterCyclingInsertsSelectedCandidate()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletionChoices({QStringLiteral("first\nline"), QStringLiteral("second\nline")});

    SessionHarness harness(server.endpoint());
    harness.view->setCursorPosition(KTextEditor::Cursor(0, 6));
    harness.view->editorWidget()->setFocus();

    harness.session->triggerSuggestion();
    QTRY_COMPARE_WITH_TIMEOUT(harness.session->candidateCount(), 2, 2000);

    harness.session->nextCandidate();
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("second\nline"), 2000);
    harness.session->acceptFullSuggestion();

    QTRY_VERIFY_WITH_TIMEOUT(harness.doc->text().contains(QStringLiteral("prefixsecond\nlineSUFFIX")), 2000);
}

void EditorSessionIntegrationTest::partialAcceptAfterCyclingKeepsSelectedCandidate()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletionChoices({QStringLiteral("shared first\nline"), QStringLiteral("shared second\nline")});

    SessionHarness harness(server.endpoint(), false);
    CompletionSettings settings = harness.plugin.settings();
    settings.enabled = true;
    settings.enableCandidateCycling = true;
    harness.plugin.setSettings(settings);

    harness.view->setCursorPosition(KTextEditor::Cursor(0, 6));
    harness.session->triggerSuggestion();
    QTRY_VERIFY_WITH_TIMEOUT(server.requestCount() > 0, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(harness.session->candidateCount(), 2, 2000);

    harness.session->nextCandidate();
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("shared second\nline"), 2000);

    harness.session->acceptNextWord();
    QTRY_VERIFY_WITH_TIMEOUT(harness.doc->text().contains(QStringLiteral("prefixshared SUFFIX")), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("second\nline"), 2000);
}

void EditorSessionIntegrationTest::speculativeRequestStoresNextSuggestionInCacheOnly()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.enqueueCompletion(QStringLiteral("first\nline"));
    server.enqueueCompletion(QStringLiteral("second\nline"));
    server.setCompletion(QStringLiteral("network-fallback"));

    SessionHarness harness(server.endpoint(), false);
    CompletionSettings settings = harness.plugin.settings();
    settings.enabled = true;
    settings.enableSpeculativeRequests = true;
    settings.speculativeRequestDelayMs = 0;
    settings.speculativeRequestMaxTokens = 32;
    settings.enableCompletionCache = true;
    settings.afterAcceptMaxTokens = 32;
    settings.enableCandidateCycling = false;
    harness.plugin.setSettings(settings);

    harness.view->setCursorPosition(KTextEditor::Cursor(0, 6));
    harness.session->triggerSuggestion();
    QTRY_VERIFY_WITH_TIMEOUT(server.requestCount() > 0, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasVisibleSuggestion(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("first\nline"), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(server.requestCount() >= 2, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(harness.completionCache.size() >= 2, 2000);
    QCOMPARE(harness.overlay->state().visibleText, QStringLiteral("first\nline"));

    const int requestCountAfterSpeculation = server.requestCount();
    harness.session->acceptFullSuggestion();
    harness.session->triggerSuggestion();

    QTest::qWait(100);
    QCOMPARE(server.requestCount(), requestCountAfterSpeculation);
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("second\nline"), 2000);
}

void EditorSessionIntegrationTest::typingPrefixOfVisibleSuggestionKeepsRemainingSuggestionWithoutRequest()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost\nline"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);
    QTRY_VERIFY_WITH_TIMEOUT(harness.overlay->isActive(), 2000);
    const int initialRequests = server.requestCount();

    harness.view->editorWidget()->setFocus();
    QTest::keyClicks(harness.view->editorWidget(), QStringLiteral("ghost"));

    QTRY_VERIFY_WITH_TIMEOUT(harness.doc->text().contains(QStringLiteral("prefixghostSUFFIX")), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasVisibleSuggestion(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("\nline"), 2000);
    QTest::qWait(CompletionSettings::kDebounceMinMs * 3);
    QCOMPARE(server.requestCount(), initialRequests);
}

void EditorSessionIntegrationTest::typingDuringStreamingKeepsRequestAndUsesLaterDeltas()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletionFrames({QStringLiteral("ghost"), QStringLiteral("\nTail")}, 250);

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);
    const int initialRequests = server.requestCount();

    harness.view->editorWidget()->setFocus();
    QTest::keyClicks(harness.view->editorWidget(), QStringLiteral("ghost"));

    QTRY_VERIFY_WITH_TIMEOUT(harness.doc->text().contains(QStringLiteral("prefixghostSUFFIX")), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasVisibleSuggestion(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(harness.overlay->state().visibleText, QStringLiteral("\nTail"), 3000);
    QTest::qWait(100);
    QCOMPARE(harness.completionCache.size(), 0);
    QCOMPARE(server.requestCount(), initialRequests);
}

void EditorSessionIntegrationTest::typingNonmatchingTextClearsSuggestionAndSchedulesRequest()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost\nline"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);
    const int initialRequests = server.requestCount();

    harness.view->editorWidget()->setFocus();
    QTest::keyClick(harness.view->editorWidget(), Qt::Key_X);

    QTRY_VERIFY_WITH_TIMEOUT(!harness.session->hasVisibleSuggestion(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(server.requestCount() > initialRequests, 2000);
}

void EditorSessionIntegrationTest::cachedSuggestionCanBeShownWithoutProviderRequest()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("cachedGhost"));

    SessionHarness harness(server.endpoint());
    CompletionSettings settings = harness.plugin.settings();
    settings.enableCandidateCycling = false;
    harness.plugin.setSettings(settings);
    waitForSuggestion(server, harness.view, harness.session);
    QTRY_COMPARE_WITH_TIMEOUT(server.requestCount(), 1, 2000);
    QTest::qWait(100);

    harness.session->dismissSuggestion();
    QVERIFY(!harness.session->hasVisibleSuggestion());

    harness.session->triggerSuggestion();
    QTRY_VERIFY_WITH_TIMEOUT(harness.session->hasVisibleSuggestion(), 2000);
    QTest::qWait(CompletionSettings::kDebounceMinMs * 3);
    QCOMPARE(server.requestCount(), 1);
}

void EditorSessionIntegrationTest::tabAcceptsStreamedSuggestion()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);

    harness.view->editorWidget()->setFocus();
    QTRY_VERIFY(harness.view->editorWidget()->hasFocus());

    QTest::keyClick(harness.view->editorWidget(), Qt::Key_Tab);

    QTRY_VERIFY_WITH_TIMEOUT(!harness.overlay->isActive(), 2000);
    QVERIFY(harness.doc->text().contains(QStringLiteral("prefixghost()SUFFIX")));
}

void EditorSessionIntegrationTest::tabAcceptsSuggestionWithRestOfLineOverlap()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()SUFFIX"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);

    harness.view->editorWidget()->setFocus();
    QTRY_VERIFY(harness.view->editorWidget()->hasFocus());

    QTest::keyClick(harness.view->editorWidget(), Qt::Key_Tab);

    QTRY_VERIFY_WITH_TIMEOUT(harness.doc->text().contains(QStringLiteral("prefixghost()SUFFIX\n")), 2000);
    QVERIFY(!harness.doc->text().contains(QStringLiteral("prefixghost()SUFFIXSUFFIX")));
}

void EditorSessionIntegrationTest::escapeClearsStreamedSuggestion()
{
    FakeSseServer server;
    QVERIFY(server.listen());
    server.setCompletion(QStringLiteral("ghost()"));

    SessionHarness harness(server.endpoint());
    waitForSuggestion(server, harness.view, harness.session);

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
    waitForSuggestion(server, harness.view, harness.session);

    harness.otherFocusWidget->setFocus();
    QTRY_VERIFY_WITH_TIMEOUT(harness.otherFocusWidget->hasFocus(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!harness.overlay->isActive(), 2000);
}

QTEST_MAIN(EditorSessionIntegrationTest)

#include "EditorSessionIntegrationTest.moc"
