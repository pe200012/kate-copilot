/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineNoteRenderingABTest
*/

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
#include <QTextLayout>
#include <QTextOption>
#include <QVBoxLayout>
#include <QWidget>

namespace
{

struct RenderScenario {
    QString documentText;
    int anchorLine = 0;
    int anchorColumn = 0;
    QString suggestionText;
};

struct RenderArtifacts {
    QImage fullImage;
    QImage cropImage;
    QImage returnCropImage;
    QRect changedBounds;
    QRect blockBounds;
    QVector<QRect> lineBounds;
    int firstTopOffsetPx = 0;
    int secondIndentStartPx = 0;
    int blockHeightPx = 0;
    int visibleLineCount = 0;
};

QString buildPythonEofDocumentText()
{
    QStringList lines;
    for (int i = 1; i <= 32; ++i) {
        lines << QStringLiteral("# helper line %1").arg(i, 2, 10, QLatin1Char('0'));
    }
    lines << QString();
    return lines.join(QLatin1Char('\n'));
}

QStringList splitSuggestionLines(const QString &text)
{
    return text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
}

int maxLineWidth(const QStringList &lines, const QFontMetrics &fm)
{
    int width = 0;
    for (const QString &line : lines) {
        width = qMax(width, fm.horizontalAdvance(line));
    }
    return qMax(1, width);
}

class BaseSingleNoteProvider : public KTextEditor::InlineNoteProvider
{
    Q_OBJECT

public:
    explicit BaseSingleNoteProvider(const RenderScenario &scenario)
        : m_scenario(scenario)
    {
    }

    QList<int> inlineNotes(int line) const override
    {
        return line == m_scenario.anchorLine ? QList<int>{m_scenario.anchorColumn} : QList<int>{};
    }

    QSize inlineNoteSize(const KTextEditor::InlineNote &note) const override
    {
        const QStringList lines = splitSuggestionLines(m_scenario.suggestionText);
        const QFontMetrics fm(note.font());
        return QSize(maxLineWidth(lines, fm), note.lineHeight() * qMax(1, lines.size()));
    }

protected:
    static QColor ghostColor()
    {
        return QColor(255, 0, 255);
    }

    RenderScenario m_scenario;
};

class AlignTopSingleNoteProvider final : public BaseSingleNoteProvider
{
    Q_OBJECT

public:
    using BaseSingleNoteProvider::BaseSingleNoteProvider;

    void paintInlineNote(const KTextEditor::InlineNote &note,
                         QPainter &painter,
                         Qt::LayoutDirection direction) const override
    {
        painter.setFont(note.font());
        painter.setPen(ghostColor());

        const QSize size = inlineNoteSize(note);
        const QRect rect(0, 0, size.width(), size.height());
        const Qt::Alignment hAlign = (direction == Qt::RightToLeft) ? Qt::AlignRight : Qt::AlignLeft;
        painter.drawText(rect, hAlign | Qt::AlignTop, m_scenario.suggestionText);
    }
};

class QTextLayoutSingleNoteProvider final : public BaseSingleNoteProvider
{
    Q_OBJECT

public:
    using BaseSingleNoteProvider::BaseSingleNoteProvider;

    void paintInlineNote(const KTextEditor::InlineNote &note,
                         QPainter &painter,
                         Qt::LayoutDirection direction) const override
    {
        Q_UNUSED(direction);

        painter.setFont(note.font());
        painter.setPen(ghostColor());

        const QSize size = inlineNoteSize(note);
        const QStringList lines = m_scenario.suggestionText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

        QTextOption option;
        option.setWrapMode(QTextOption::NoWrap);

        qreal y = 0.0;
        for (const QString &logicalLine : lines) {
            QTextLayout layout(logicalLine, note.font());
            layout.setTextOption(option);
            layout.beginLayout();
            QTextLine textLine = layout.createLine();
            if (!textLine.isValid()) {
                layout.endLayout();
                y += note.lineHeight();
                continue;
            }

            textLine.setLineWidth(size.width());
            textLine.setPosition(QPointF(0.0, 0.0));
            layout.endLayout();
            textLine.draw(&painter, QPointF(0.0, y));
            y += note.lineHeight();
        }
    }
};

QImage grabEditorWidget(QWidget *editorWidget)
{
    return editorWidget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

QString outputDirPath()
{
    const QString dirPath = QDir::tempPath() + QStringLiteral("/kate-ai-inline-note-rendering");
    QDir().mkpath(dirPath);
    return dirPath;
}

void saveDebugImage(const QString &name, const QImage &image)
{
    image.save(outputDirPath() + QStringLiteral("/") + name + QStringLiteral(".png"));
}

QRect findChangedBoundsInRegion(const QImage &before, const QImage &after, const QRect &region)
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

KTextEditor::View *prepareView(const RenderScenario &scenario,
                               QWidget &window,
                               QScopedPointer<KTextEditor::Document> &document)
{
    auto *editor = KTextEditor::Editor::instance();
    Q_ASSERT(editor);

    window.resize(900, 420);

    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    document.reset(editor->createDocument(&window));
    Q_ASSERT(document);
    document->setText(scenario.documentText);

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

    view->setCursorPosition(KTextEditor::Cursor(scenario.anchorLine, scenario.anchorColumn));
    QTest::qWait(100);
    QCoreApplication::processEvents();

    return view;
}

int lineHeightPx(KTextEditor::View *view, int anchorLine)
{
    const int prevLine = qMax(0, anchorLine - 1);
    const QPoint prev = view->cursorToCoordinate(KTextEditor::Cursor(prevLine, 0));
    const QPoint anchor = view->cursorToCoordinate(KTextEditor::Cursor(anchorLine, 0));
    const int diff = anchor.y() - prev.y();
    return qMax(1, diff > 0 ? diff : QFontMetrics(view->editorWidget()->font()).height());
}

QRect cropRectForAnchor(KTextEditor::View *view, const RenderScenario &scenario, int lineHeight, qreal dpr)
{
    const QWidget *editorWidget = view->editorWidget();
    const QPoint anchorInView = view->cursorToCoordinate(KTextEditor::Cursor(scenario.anchorLine, scenario.anchorColumn));
    const QPoint anchorGlobal = view->mapToGlobal(anchorInView);
    const QPoint anchorInEditor = editorWidget->mapFromGlobal(anchorGlobal);
    const int top = qMax(0, qRound((anchorInEditor.y() - 6) * dpr));
    const int height = qMin(qRound(editorWidget->height() * dpr) - top, qRound((lineHeight * 5 + 20) * dpr));
    const int width = qMin(qRound(editorWidget->width() * dpr), qRound(560 * dpr));
    return QRect(0, top, width, height);
}

RenderArtifacts renderScenario(const QString &prefix,
                               const RenderScenario &scenario,
                               KTextEditor::InlineNoteProvider &provider)
{
    RenderArtifacts result;

    QWidget window;
    QScopedPointer<KTextEditor::Document> document;
    KTextEditor::View *view = prepareView(scenario, window, document);
    Q_ASSERT(view);

    QWidget *editorWidget = view->editorWidget();
    Q_ASSERT(editorWidget);

    const QImage baselineImage = grabEditorWidget(editorWidget);

    view->registerInlineNoteProvider(&provider);
    QTest::qWait(120);
    QCoreApplication::processEvents();

    result.fullImage = grabEditorWidget(editorWidget);

    const qreal dpr = result.fullImage.devicePixelRatio();
    const int lineHeight = lineHeightPx(view, scenario.anchorLine);
    const QRect cropRect = cropRectForAnchor(view, scenario, lineHeight, dpr);
    result.cropImage = result.fullImage.copy(cropRect);

    view->setCursorPosition(KTextEditor::Cursor(0, 0));
    QTest::qWait(100);
    QCoreApplication::processEvents();
    view->setCursorPosition(KTextEditor::Cursor(scenario.anchorLine, scenario.anchorColumn));
    QTest::qWait(120);
    QCoreApplication::processEvents();

    const QImage returnImage = grabEditorWidget(editorWidget);
    result.returnCropImage = returnImage.copy(cropRect);

    saveDebugImage(prefix + QStringLiteral("_full"), result.fullImage);
    saveDebugImage(prefix + QStringLiteral("_crop"), result.cropImage);
    saveDebugImage(prefix + QStringLiteral("_return_crop"), result.returnCropImage);

    result.changedBounds = findChangedBoundsInRegion(baselineImage, result.fullImage, cropRect);
    result.blockBounds = result.changedBounds.isValid() ? result.changedBounds.translated(-cropRect.topLeft()) : QRect();
    result.blockHeightPx = result.blockBounds.height();

    const QPoint anchorInView = view->cursorToCoordinate(KTextEditor::Cursor(scenario.anchorLine, scenario.anchorColumn));
    const QPoint anchorGlobal = view->mapToGlobal(anchorInView);
    const QPoint anchorInEditor = editorWidget->mapFromGlobal(anchorGlobal);
    qInfo() << prefix << "anchorInView=" << anchorInView << "anchorInEditor=" << anchorInEditor << "cropRect=" << cropRect << "dpr=" << dpr;
    const int anchorYInCrop = qRound(anchorInEditor.y() * dpr) - cropRect.top();
    for (int i = 0; i < 3; ++i) {
        const int scaledLineHeight = qRound(lineHeight * dpr);
        const int bandTop = qMax(0, anchorYInCrop + i * scaledLineHeight - qRound(2 * dpr));
        const int bandBottom = qMin(result.cropImage.height() - 1, bandTop + scaledLineHeight + qRound(4 * dpr));
        const QRect bandRect(0, bandTop, result.cropImage.width(), qMax(1, bandBottom - bandTop + 1));
        const QRect lineBounds = findChangedBoundsInRegion(baselineImage.copy(cropRect), result.cropImage, QRect(0, bandRect.top(), bandRect.width(), bandRect.height()));
        result.lineBounds.push_back(lineBounds);
        if (lineBounds.isValid()) {
            ++result.visibleLineCount;
        }
    }

    if (!result.lineBounds.isEmpty() && result.lineBounds.at(0).isValid()) {
        result.firstTopOffsetPx = result.lineBounds.at(0).top() - anchorYInCrop;
    }

    if (result.lineBounds.size() >= 2 && result.lineBounds.at(0).isValid() && result.lineBounds.at(1).isValid()) {
        result.secondIndentStartPx = result.lineBounds.at(1).left() - result.lineBounds.at(0).left();
    }

    qInfo().noquote() << prefix
                      << "firstTopOffsetPx=" << result.firstTopOffsetPx
                      << "secondIndentStartPx=" << result.secondIndentStartPx
                      << "blockHeightPx=" << result.blockHeightPx
                      << "visibleLineCount=" << result.visibleLineCount
                      << "changedBounds=" << result.changedBounds;

    view->unregisterInlineNoteProvider(&provider);
    return result;
}

} // namespace

class InlineNoteRenderingABTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compareAlignTopAndQTextLayoutAtPythonEof();
};

void InlineNoteRenderingABTest::compareAlignTopAndQTextLayoutAtPythonEof()
{
    RenderScenario scenario;
    scenario.documentText = buildPythonEofDocumentText();
    scenario.anchorLine = 32;
    scenario.anchorColumn = 0;
    scenario.suggestionText = QStringLiteral("if n <= 1:\n    return n\nreturn fib(n - 1) + fib(n - 2)");

    AlignTopSingleNoteProvider alignTopProvider(scenario);
    QTextLayoutSingleNoteProvider textLayoutProvider(scenario);

    const RenderArtifacts alignTop = renderScenario(QStringLiteral("ab_align_top"), scenario, alignTopProvider);
    const RenderArtifacts textLayout = renderScenario(QStringLiteral("ab_qtextlayout"), scenario, textLayoutProvider);

    QVERIFY2(!alignTop.fullImage.isNull(), "expected AlignTop full screenshot");
    QVERIFY2(!alignTop.cropImage.isNull(), "expected AlignTop crop screenshot");
    QVERIFY2(!alignTop.returnCropImage.isNull(), "expected AlignTop return crop screenshot");
    QVERIFY2(alignTop.changedBounds.isValid(), "expected AlignTop to render changed pixels near EOF anchor");
    QVERIFY2(alignTop.visibleLineCount >= 1, "expected AlignTop to render at least one visible ghost line");

    QVERIFY2(!textLayout.fullImage.isNull(), "expected QTextLayout full screenshot");
    QVERIFY2(!textLayout.cropImage.isNull(), "expected QTextLayout crop screenshot");
    QVERIFY2(!textLayout.returnCropImage.isNull(), "expected QTextLayout return crop screenshot");
    QVERIFY2(textLayout.changedBounds.isValid(), "expected QTextLayout to render changed pixels near EOF anchor");
    QVERIFY2(textLayout.visibleLineCount >= 1, "expected QTextLayout to render at least one visible ghost line");
}

QTEST_MAIN(InlineNoteRenderingABTest)

#include "InlineNoteRenderingABTest.moc"
