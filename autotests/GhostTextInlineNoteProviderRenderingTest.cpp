/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextInlineNoteProviderRenderingTest
*/

#include "render/GhostTextInlineNoteProvider.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/InlineNoteProvider>
#include <KTextEditor/View>

#include <QColor>
#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QScopedPointer>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

using KateAiInlineCompletion::GhostTextInlineNoteProvider;
using KateAiInlineCompletion::GhostTextState;

namespace
{

GhostTextState makeState(const QString &visibleText)
{
    GhostTextState state;
    state.anchor.line = 1;
    state.anchor.column = 0;
    state.anchor.generation = 1;
    state.visibleText = visibleText;
    return state;
}

QRect findChangedPixelBounds(const QImage &before, const QImage &after)
{
    int minX = before.width();
    int minY = before.height();
    int maxX = -1;
    int maxY = -1;

    const QRect bounded = before.rect().intersected(after.rect());
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

QImage grabEditorWidget(QWidget *editorWidget)
{
    return editorWidget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

void saveDebugImage(const QString &name, const QImage &image)
{
    const QString dirPath = QDir::tempPath() + QStringLiteral("/kate-ai-inline-note-rendering");
    QDir().mkpath(dirPath);
    image.save(dirPath + QStringLiteral("/") + name + QStringLiteral(".png"));
}

enum class UpdateSignalMode {
    ChangedLine,
    Reset,
};

class StatefulSingleNoteNewlineStringProvider final : public KTextEditor::InlineNoteProvider
{
    Q_OBJECT

public:
    explicit StatefulSingleNoteNewlineStringProvider(UpdateSignalMode mode)
        : m_mode(mode)
    {
    }

    void setText(const QString &text)
    {
        m_text = text;
        if (m_mode == UpdateSignalMode::ChangedLine) {
            Q_EMIT inlineNotesChanged(1);
            return;
        }

        Q_EMIT inlineNotesReset();
    }

    QList<int> inlineNotes(int line) const override
    {
        return (line == 1 && !m_text.isEmpty()) ? QList<int>{0} : QList<int>{};
    }

    QSize inlineNoteSize(const KTextEditor::InlineNote &note) const override
    {
        const QFontMetrics fm(note.font());
        int width = 0;
        const QStringList lines = m_text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (const QString &line : lines) {
            width = qMax(width, fm.horizontalAdvance(line));
        }
        return QSize(width, note.lineHeight());
    }

    void paintInlineNote(const KTextEditor::InlineNote &note,
                         QPainter &painter,
                         Qt::LayoutDirection direction) const override
    {
        Q_UNUSED(direction);

        painter.setFont(note.font());
        painter.setPen(QColor(255, 0, 255));
        painter.drawText(QRect(0, 0, qMax(220, inlineNoteSize(note).width()), note.lineHeight()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         m_text);
    }

private:
    UpdateSignalMode m_mode;
    QString m_text = QStringLiteral("first\n    second\nthird");
};

} // namespace

class GhostTextInlineNoteProviderRenderingTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void realProviderMultilineUpdateRendersPixels();
    void singleNoteNewlineStringRendersPixels();
    void singleNoteNewlineDynamicUpdateViaLineChangedRendersPixels();
    void singleNoteNewlineDynamicUpdateViaResetRendersPixels();
};

static KTextEditor::View *prepareView(QWidget &window, QScopedPointer<KTextEditor::Document> &document)
{
    auto *editor = KTextEditor::Editor::instance();
    Q_ASSERT(editor);

    window.resize(900, 320);

    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    document.reset(editor->createDocument(&window));
    Q_ASSERT(document);
    document->setText(QStringLiteral("top\n\n\n\nbottom\n"));

    KTextEditor::View *view = document->createView(&window);
    Q_ASSERT(view);
    layout->addWidget(view);

    QWidget *editorWidget = view->editorWidget();
    Q_ASSERT(editorWidget);

    QFont font(QStringLiteral("Monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(14);
    editorWidget->setFont(font);

    window.show();
    QTest::qWait(200);
    QCoreApplication::processEvents();
    return view;
}

void GhostTextInlineNoteProviderRenderingTest::realProviderMultilineUpdateRendersPixels()
{
    QWidget window;
    QScopedPointer<KTextEditor::Document> document;
    KTextEditor::View *view = prepareView(window, document);
    QVERIFY(view);

    QWidget *editorWidget = view->editorWidget();
    QVERIFY(editorWidget);

    GhostTextInlineNoteProvider provider;
    view->registerInlineNoteProvider(&provider);

    provider.setState(makeState(QStringLiteral("first")));
    QTest::qWait(100);
    QCoreApplication::processEvents();
    const QImage singleLineImage = grabEditorWidget(editorWidget);

    provider.setState(makeState(QStringLiteral("first\n    second\nthird")));
    QTest::qWait(100);
    QCoreApplication::processEvents();
    const QImage multilineImage = grabEditorWidget(editorWidget);

    saveDebugImage(QStringLiteral("ghost_inline_single_line"), singleLineImage);
    saveDebugImage(QStringLiteral("ghost_inline_multiline"), multilineImage);

    const QRect changedBounds = findChangedPixelBounds(singleLineImage, multilineImage);
    QVERIFY2(changedBounds.isValid(), "expected multiline update to change rendered pixels");

    view->unregisterInlineNoteProvider(&provider);
}

void GhostTextInlineNoteProviderRenderingTest::singleNoteNewlineStringRendersPixels()
{
    QWidget window;
    QScopedPointer<KTextEditor::Document> document;
    KTextEditor::View *view = prepareView(window, document);
    QVERIFY(view);

    QWidget *editorWidget = view->editorWidget();
    QVERIFY(editorWidget);

    const QImage baselineImage = grabEditorWidget(editorWidget);

    StatefulSingleNoteNewlineStringProvider provider(UpdateSignalMode::ChangedLine);
    view->registerInlineNoteProvider(&provider);

    QTest::qWait(100);
    QCoreApplication::processEvents();
    const QImage newlineStringImage = grabEditorWidget(editorWidget);
    saveDebugImage(QStringLiteral("single_note_newline_string"), newlineStringImage);

    const QRect changedBounds = findChangedPixelBounds(baselineImage, newlineStringImage);
    QVERIFY2(changedBounds.isValid(), "expected newline string inline note to draw pixels");

    view->unregisterInlineNoteProvider(&provider);
}

static void runSingleNoteDynamicUpdateScenario(UpdateSignalMode mode, const QString &imageStem)
{
    QWidget window;
    QScopedPointer<KTextEditor::Document> document;
    KTextEditor::View *view = prepareView(window, document);
    QVERIFY(view);

    QWidget *editorWidget = view->editorWidget();
    QVERIFY(editorWidget);

    StatefulSingleNoteNewlineStringProvider provider(mode);
    view->registerInlineNoteProvider(&provider);

    provider.setText(QStringLiteral("first"));
    QTest::qWait(100);
    QCoreApplication::processEvents();
    const QImage singleLineImage = grabEditorWidget(editorWidget);

    provider.setText(QStringLiteral("first\n    second\nthird"));
    QTest::qWait(100);
    QCoreApplication::processEvents();
    const QImage multilineImage = grabEditorWidget(editorWidget);

    saveDebugImage(imageStem + QStringLiteral("_single"), singleLineImage);
    saveDebugImage(imageStem + QStringLiteral("_multiline"), multilineImage);

    const QRect changedBounds = findChangedPixelBounds(singleLineImage, multilineImage);
    QVERIFY2(changedBounds.isValid(), "expected multiline update to change rendered pixels");

    view->unregisterInlineNoteProvider(&provider);
}

void GhostTextInlineNoteProviderRenderingTest::singleNoteNewlineDynamicUpdateViaLineChangedRendersPixels()
{
    runSingleNoteDynamicUpdateScenario(UpdateSignalMode::ChangedLine,
                                       QStringLiteral("single_note_newline_dynamic_changed_line"));
}

void GhostTextInlineNoteProviderRenderingTest::singleNoteNewlineDynamicUpdateViaResetRendersPixels()
{
    runSingleNoteDynamicUpdateScenario(UpdateSignalMode::Reset,
                                       QStringLiteral("single_note_newline_dynamic_reset"));
}

QTEST_MAIN(GhostTextInlineNoteProviderRenderingTest)

#include "GhostTextInlineNoteProviderRenderingTest.moc"
