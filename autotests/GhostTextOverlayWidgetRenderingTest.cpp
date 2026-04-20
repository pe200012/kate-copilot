/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextOverlayWidgetRenderingTest

    Verifies GhostTextOverlayWidget renders multiline ghost text using a
    transparent overlay attached to view->editorWidget().
*/

#include "render/GhostTextOverlayWidget.h"

#include "session/GhostTextState.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QDir>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QScopedPointer>
#include <QScrollBar>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

using KateAiInlineCompletion::GhostTextOverlayWidget;
using KateAiInlineCompletion::GhostTextState;

namespace
{

GhostTextState makeState(int line, int column, const QString &text)
{
    GhostTextState s;
    s.anchor.line = line;
    s.anchor.column = column;
    s.anchor.generation = 1;
    s.anchorTracked = true;
    s.visibleText = text;
    s.streaming = true;
    s.suppressed = false;
    return s;
}

QImage grabEditorWidget(QWidget *editorWidget)
{
    return editorWidget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

QImage renderOverlayWidget(QWidget *widget)
{
    const qreal dpr = widget->devicePixelRatioF();
    const QSize pixelSize(qRound(widget->width() * dpr), qRound(widget->height() * dpr));
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    widget->render(&painter);
    return image;
}

QRect findChangedBounds(const QImage &before, const QImage &after, const QRect &region)
{
    int minX = region.right() + 1;
    int minY = region.bottom() + 1;
    int maxX = -1;
    int maxY = -1;

    const QRect bounded = before.rect().intersected(after.rect()).intersected(region);
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            if (before.pixel(x, y) == after.pixel(x, y)) {
                continue;
            }

            minX = qMin(minX, x);
            minY = qMin(minY, y);
            maxX = qMax(maxX, x);
            maxY = qMax(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY) {
        return {};
    }

    return QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}

QRect findNonTransparentBounds(const QImage &image)
{
    int minX = image.width();
    int minY = image.height();
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) == 0) {
                continue;
            }

            minX = qMin(minX, x);
            minY = qMin(minY, y);
            maxX = qMax(maxX, x);
            maxY = qMax(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY) {
        return {};
    }

    return QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}

void saveDebugImage(const QString &name, const QImage &image)
{
    const QString dirPath = QDir::tempPath() + QStringLiteral("/kate-ai-inline-note-rendering");
    QDir().mkpath(dirPath);
    image.save(dirPath + QStringLiteral("/") + name + QStringLiteral(".png"));
}

int lineHeightPx(KTextEditor::View *view, int line)
{
    const QPoint p0 = view->cursorToCoordinate(KTextEditor::Cursor(line, 0));
    const QPoint p1 = view->cursorToCoordinate(KTextEditor::Cursor(line + 1, 0));
    const int diff = p1.y() - p0.y();
    return qMax(1, diff);
}

QPoint cursorInEditorWidget(KTextEditor::View *view, QWidget *editorWidget, const KTextEditor::Cursor &cursor)
{
    const QPoint inView = view->cursorToCoordinate(cursor);
    return editorWidget->mapFrom(view, inView);
}

QString longLine()
{
    return QStringLiteral("return ghost_value + another_ghost_value + yet_another_ghost_value + trailing_suffix_marker");
}

} // namespace

class GhostTextOverlayWidgetRenderingTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void multilineOverlayRendersAtMidLineCursor();
    void overlayUsesViewConfigFont();
    void multilineOverlayRendersAtEofCursor();
    void overlayDisappearsWhenAnchorScrollsAwayAndReturnsAfterScrollBack();
};

void GhostTextOverlayWidgetRenderingTest::multilineOverlayRendersAtMidLineCursor()
{
    auto *editor = KTextEditor::Editor::instance();
    QVERIFY(editor);

    QWidget window;
    window.resize(900, 320);

    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    QScopedPointer<KTextEditor::Document> doc(editor->createDocument(&window));
    QVERIFY(doc);

    doc->setText(QStringLiteral("prefixSUFFIX\n\n\n\n\n"));

    KTextEditor::View *view = doc->createView(&window);
    QVERIFY(view);
    layout->addWidget(view);

    QWidget *editorWidget = view->editorWidget();
    QVERIFY(editorWidget);

    QFont font(QStringLiteral("Monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(14);
    editorWidget->setFont(font);

    window.show();
    QTest::qWait(200);
    QCoreApplication::processEvents();

    const int anchorLine = 0;
    const int anchorColumn = 6;
    view->setCursorPosition(KTextEditor::Cursor(anchorLine, anchorColumn));
    QTest::qWait(100);
    QCoreApplication::processEvents();

    const QImage baseline = grabEditorWidget(editorWidget);

    auto *overlay = new GhostTextOverlayWidget(view, editorWidget);
    overlay->setState(makeState(anchorLine, anchorColumn,
                                QStringLiteral("if n <= 1:\n    return n\nreturn fib(n - 1) + fib(n - 2)")));
    overlay->show();
    overlay->raise();

    QTest::qWait(150);
    QCoreApplication::processEvents();

    const QImage after = grabEditorWidget(editorWidget);

    saveDebugImage(QStringLiteral("overlay_baseline"), baseline);
    saveDebugImage(QStringLiteral("overlay_after"), after);

    const QRect changed = findChangedBounds(baseline, after, after.rect());
    QVERIFY2(changed.isValid(), "expected overlay to change rendered pixels");

    const int lh = lineHeightPx(view, anchorLine);
    const QPoint anchorInEditor = cursorInEditorWidget(view, editorWidget, KTextEditor::Cursor(anchorLine, anchorColumn));

    QVERIFY2(changed.bottom() >= anchorInEditor.y() + lh,
             "expected multiline overlay to paint pixels into following line area");

    const QRect anchorBand(qMax(0, anchorInEditor.x() - 2),
                           qMax(0, anchorInEditor.y() - 2),
                           qMin(after.width() - qMax(0, anchorInEditor.x() - 2), 200),
                           qMin(after.height() - qMax(0, anchorInEditor.y() - 2), lh + 4));
    const QRect anchorBandChanged = findChangedBounds(baseline, after, anchorBand);
    QVERIFY2(anchorBandChanged.isValid(), "expected overlay to paint near the cursor on the anchor line");
}

void GhostTextOverlayWidgetRenderingTest::overlayUsesViewConfigFont()
{
    auto *editor = KTextEditor::Editor::instance();
    QVERIFY(editor);

    QWidget window;
    window.resize(900, 320);

    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    QScopedPointer<KTextEditor::Document> doc(editor->createDocument(&window));
    QVERIFY(doc);
    doc->setText(QStringLiteral("prefixSUFFIX\n\n\n"));

    KTextEditor::View *view = doc->createView(&window);
    QVERIFY(view);
    layout->addWidget(view);

    QWidget *editorWidget = view->editorWidget();
    QVERIFY(editorWidget);

    window.show();
    QTest::qWait(200);
    QCoreApplication::processEvents();

    const QFont bigFont(QStringLiteral("Monospace"), 22);
    view->setConfigValue(QStringLiteral("font"), bigFont);
    QTest::qWait(120);
    QCoreApplication::processEvents();

    QFont smallFont(QStringLiteral("Monospace"));
    smallFont.setStyleHint(QFont::Monospace);
    smallFont.setPointSize(10);
    editorWidget->setFont(smallFont);
    QTest::qWait(120);
    QCoreApplication::processEvents();

    const int anchorLine = 0;
    const int anchorColumn = 6;
    view->setCursorPosition(KTextEditor::Cursor(anchorLine, anchorColumn));
    QTest::qWait(80);
    QCoreApplication::processEvents();

    const QImage baseline = grabEditorWidget(editorWidget);

    auto *overlay = new GhostTextOverlayWidget(view, editorWidget);
    overlay->setState(makeState(anchorLine, anchorColumn, QStringLiteral("ghost")));
    overlay->show();
    overlay->raise();

    QTest::qWait(150);
    QCoreApplication::processEvents();

    const QImage after = grabEditorWidget(editorWidget);

    saveDebugImage(QStringLiteral("overlay_font_baseline"), baseline);
    saveDebugImage(QStringLiteral("overlay_font_after"), after);

    const QRect changed = findChangedBounds(baseline, after, after.rect());
    QVERIFY2(changed.isValid(), "expected overlay to change rendered pixels");

    const int lh = lineHeightPx(view, anchorLine);
    const int expectedMin = (lh * 7) / 10;
    QVERIFY2(changed.height() >= expectedMin,
             "expected overlay text height to track view config font");
}

void GhostTextOverlayWidgetRenderingTest::multilineOverlayRendersAtEofCursor()
{
    auto *editor = KTextEditor::Editor::instance();
    QVERIFY(editor);

    QWidget window;
    window.resize(900, 320);

    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    QScopedPointer<KTextEditor::Document> doc(editor->createDocument(&window));
    QVERIFY(doc);
    doc->setText(QStringLiteral("def fib(n):\n    return n\n"));

    KTextEditor::View *view = doc->createView(&window);
    QVERIFY(view);
    layout->addWidget(view);

    QWidget *editorWidget = view->editorWidget();
    QVERIFY(editorWidget);

    window.show();
    QTest::qWait(180);
    QCoreApplication::processEvents();

    const int anchorLine = doc->lines() - 1;
    const int anchorColumn = doc->line(anchorLine).size();
    view->setCursorPosition(KTextEditor::Cursor(anchorLine, anchorColumn));
    QTest::qWait(100);
    QCoreApplication::processEvents();

    auto *overlay = new GhostTextOverlayWidget(view, editorWidget);
    overlay->setState(makeState(anchorLine, anchorColumn,
                                QStringLiteral("if n <= 1:\n    return n\n%1")
                                    .arg(longLine())));
    overlay->show();
    overlay->raise();

    QTest::qWait(120);
    QCoreApplication::processEvents();

    const QImage overlayImage = renderOverlayWidget(overlay);
    saveDebugImage(QStringLiteral("overlay_eof_only"), overlayImage);
    saveDebugImage(QStringLiteral("overlay_eof_editor"), grabEditorWidget(editorWidget));

    const QRect painted = findNonTransparentBounds(overlayImage);
    QVERIFY2(painted.isValid(), "expected overlay to render visible pixels at EOF");

    const int lh = lineHeightPx(view, anchorLine);
    const QPoint anchorInEditor = cursorInEditorWidget(view, editorWidget, KTextEditor::Cursor(anchorLine, anchorColumn));
    QVERIFY(painted.left() >= qMax(0, anchorInEditor.x() - 4));
    QVERIFY(painted.bottom() >= anchorInEditor.y() + lh);
}

void GhostTextOverlayWidgetRenderingTest::overlayDisappearsWhenAnchorScrollsAwayAndReturnsAfterScrollBack()
{
    auto *editor = KTextEditor::Editor::instance();
    QVERIFY(editor);

    QWidget window;
    window.resize(900, 220);

    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    QStringList lines;
    for (int i = 0; i < 80; ++i) {
        lines << QStringLiteral("line_%1 = %1").arg(i);
    }

    QScopedPointer<KTextEditor::Document> doc(editor->createDocument(&window));
    QVERIFY(doc);
    doc->setText(lines.join(QLatin1Char('\n')) + QLatin1Char('\n'));

    KTextEditor::View *view = doc->createView(&window);
    QVERIFY(view);
    layout->addWidget(view);

    QWidget *editorWidget = view->editorWidget();
    QVERIFY(editorWidget);

    window.show();
    QTest::qWait(220);
    QCoreApplication::processEvents();

    const int anchorLine = 8;
    view->setCursorPosition(KTextEditor::Cursor(anchorLine, 4));
    QTest::qWait(120);
    QCoreApplication::processEvents();

    auto *overlay = new GhostTextOverlayWidget(view, editorWidget);
    overlay->setState(makeState(anchorLine, 4, QStringLiteral("ghost_value\n    child_value\nfinal_value")));
    overlay->show();
    overlay->raise();

    QTest::qWait(120);
    QCoreApplication::processEvents();

    const QImage visibleImage = renderOverlayWidget(overlay);
    saveDebugImage(QStringLiteral("overlay_scroll_visible"), visibleImage);
    const QRect visibleBounds = findNonTransparentBounds(visibleImage);
    QVERIFY(visibleBounds.isValid());

    QScrollBar *scrollBar = view->verticalScrollBar();
    QVERIFY(scrollBar);
    const int visibleScrollValue = scrollBar->value();

    view->setCursorPosition(KTextEditor::Cursor(doc->lines() - 1, 0));
    QTest::qWait(80);
    QCoreApplication::processEvents();

    scrollBar->setValue(scrollBar->maximum());
    overlay->update();
    QTest::qWait(150);
    QCoreApplication::processEvents();

    const QImage awayImage = renderOverlayWidget(overlay);
    saveDebugImage(QStringLiteral("overlay_scroll_away"), awayImage);
    const QRect awayBounds = findNonTransparentBounds(awayImage);
    QVERIFY2(awayBounds.isValid(), "expected overlay pixels to remain renderable after scrolling");
    QVERIFY2(awayBounds.bottom() < visibleBounds.top(),
             "expected the overlay paint band to move upward when the anchor scrolls above the viewport");

    scrollBar->setValue(visibleScrollValue);
    overlay->update();
    QTest::qWait(150);
    QCoreApplication::processEvents();

    const QImage returnedImage = renderOverlayWidget(overlay);
    saveDebugImage(QStringLiteral("overlay_scroll_returned"), returnedImage);
    const QRect returnedBounds = findNonTransparentBounds(returnedImage);
    QVERIFY(returnedBounds.isValid());
    QVERIFY(qAbs(returnedBounds.top() - visibleBounds.top()) <= lineHeightPx(view, anchorLine));
}

QTEST_MAIN(GhostTextOverlayWidgetRenderingTest)

#include "GhostTextOverlayWidgetRenderingTest.moc"
