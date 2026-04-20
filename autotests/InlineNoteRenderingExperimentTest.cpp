/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineNoteRenderingExperimentTest

    Empirically probes KTextEditor::InlineNoteProvider rendering by capturing
    real view output and checking colored probe bands in the resulting image.
*/

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/InlineNoteProvider>
#include <KTextEditor/View>

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QtMath>
#include <QVBoxLayout>
#include <QWidget>

namespace
{

enum class ProbeMode {
    OversizeSingleNote,
    MultilineSingleNote,
    PerLineNotes,
};

constexpr int kAnchorLine = 1;
constexpr QRgb kMagenta = qRgb(255, 0, 255);
constexpr QRgb kOrange = qRgb(255, 128, 0);
constexpr QRgb kCyan = qRgb(0, 255, 255);
constexpr QRgb kGreen = qRgb(0, 255, 0);
constexpr QRgb kBlue = qRgb(0, 0, 255);

class ProbeInlineNoteProvider final : public KTextEditor::InlineNoteProvider
{
    Q_OBJECT

public:
    explicit ProbeInlineNoteProvider(ProbeMode mode)
        : m_mode(mode)
    {
    }

    QList<int> inlineNotes(int line) const override
    {
        switch (m_mode) {
        case ProbeMode::OversizeSingleNote:
        case ProbeMode::MultilineSingleNote:
            return (line == kAnchorLine) ? QList<int>{0} : QList<int>{};
        case ProbeMode::PerLineNotes:
            return (line >= kAnchorLine && line <= kAnchorLine + 2) ? QList<int>{0} : QList<int>{};
        }

        return {};
    }

    QSize inlineNoteSize(const KTextEditor::InlineNote &note) const override
    {
        m_lastLineHeight = note.lineHeight();

        switch (m_mode) {
        case ProbeMode::OversizeSingleNote:
        case ProbeMode::MultilineSingleNote:
            return QSize(180, note.lineHeight() * 3);
        case ProbeMode::PerLineNotes:
            return QSize(140, note.lineHeight());
        }

        return QSize(0, note.lineHeight());
    }

    void paintInlineNote(const KTextEditor::InlineNote &note,
                         QPainter &painter,
                         Qt::LayoutDirection direction) const override
    {
        Q_UNUSED(direction);

        const int lh = note.lineHeight();
        m_lastLineHeight = lh;

        switch (m_mode) {
        case ProbeMode::OversizeSingleNote:
        case ProbeMode::MultilineSingleNote:
            painter.fillRect(QRect(0, 0, 180, lh), QColor::fromRgb(kMagenta));
            painter.fillRect(QRect(0, lh, 180, lh), QColor::fromRgb(kOrange));
            painter.fillRect(QRect(0, lh * 2, 180, lh), QColor::fromRgb(kCyan));
            return;
        case ProbeMode::PerLineNotes: {
            const int line = note.position().line();
            QRgb color = kMagenta;
            if (line == kAnchorLine + 1) {
                color = kGreen;
            } else if (line == kAnchorLine + 2) {
                color = kBlue;
            }
            painter.fillRect(QRect(0, 0, 140, lh), QColor::fromRgb(color));
            return;
        }
        }
    }

    [[nodiscard]] int lastLineHeight() const
    {
        return m_lastLineHeight;
    }

private:
    ProbeMode m_mode;
    mutable int m_lastLineHeight = -1;
};

QRect findColorBounds(const QImage &image, QRgb color)
{
    int minX = image.width();
    int minY = image.height();
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y) != color) {
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

struct RenderResult {
    QImage image;
    int lineHeight = -1;
    qreal devicePixelRatio = 1.0;
    QString imagePath;
};

RenderResult renderProbeImage(const QString &name, ProbeMode mode)
{
    RenderResult result;

    auto *editor = KTextEditor::Editor::instance();
    Q_ASSERT(editor);

    QWidget window;
    window.setWindowTitle(name);
    window.resize(900, 320);

    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);

    QScopedPointer<KTextEditor::Document> document(editor->createDocument(&window));
    Q_ASSERT(document);
    document->setText(QStringLiteral("alpha\n"
                                     "beta\n"
                                     "gamma\n"
                                     "delta\n"
                                     "epsilon\n"
                                     "zeta\n"));

    KTextEditor::View *view = document->createView(&window);
    Q_ASSERT(view);
    layout->addWidget(view);

    if (QWidget *editorWidget = view->editorWidget()) {
        QFont font(QStringLiteral("Monospace"));
        font.setStyleHint(QFont::Monospace);
        font.setPointSize(14);
        editorWidget->setFont(font);
    }

    ProbeInlineNoteProvider provider(mode);
    view->registerInlineNoteProvider(&provider);

    window.show();
    QTest::qWait(200);
    QCoreApplication::processEvents();
    view->update();
    if (QWidget *editorWidget = view->editorWidget()) {
        editorWidget->update();
    }
    QTest::qWait(200);
    QCoreApplication::processEvents();

    QWidget *editorWidget = view->editorWidget();
    Q_ASSERT(editorWidget);

    result.image = editorWidget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
    result.lineHeight = provider.lastLineHeight();
    result.devicePixelRatio = result.image.devicePixelRatio();

    const QString dirPath = QDir::tempPath() + QStringLiteral("/kate-ai-inline-note-rendering");
    QDir().mkpath(dirPath);
    result.imagePath = dirPath + QStringLiteral("/") + name + QStringLiteral(".png");
    result.image.save(result.imagePath);

    view->unregisterInlineNoteProvider(&provider);
    return result;
}

} // namespace

class InlineNoteRenderingExperimentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void oversizeSingleNoteClipsToSingleLine();
    void multilineSingleNoteStaysWithinAnchorLine();
    void perLineNotesPaintOnMultipleRealLines();
};

void InlineNoteRenderingExperimentTest::oversizeSingleNoteClipsToSingleLine()
{
    const RenderResult result = renderProbeImage(QStringLiteral("oversize_single_note"), ProbeMode::OversizeSingleNote);
    const QRect magentaBounds = findColorBounds(result.image, kMagenta);

    QVERIFY2(magentaBounds.isValid(), qPrintable(QStringLiteral("expected magenta probe band in %1").arg(result.imagePath)));
    QVERIFY(result.lineHeight > 0);
    const int expectedMaxHeight = qCeil(result.lineHeight * result.devicePixelRatio);
    QVERIFY2(magentaBounds.height() <= expectedMaxHeight,
             qPrintable(QStringLiteral("expected clip to one logical line in %1, got height=%2 lineHeight=%3 dpr=%4")
                            .arg(result.imagePath)
                            .arg(magentaBounds.height())
                            .arg(result.lineHeight)
                            .arg(result.devicePixelRatio)));
}

void InlineNoteRenderingExperimentTest::multilineSingleNoteStaysWithinAnchorLine()
{
    const RenderResult result = renderProbeImage(QStringLiteral("multiline_single_note"), ProbeMode::MultilineSingleNote);
    const QRect magentaBounds = findColorBounds(result.image, kMagenta);
    const QRect orangeBounds = findColorBounds(result.image, kOrange);
    const QRect cyanBounds = findColorBounds(result.image, kCyan);

    QVERIFY2(magentaBounds.isValid(), qPrintable(QStringLiteral("expected first probe band in %1").arg(result.imagePath)));
    QVERIFY(result.lineHeight > 0);
    QVERIFY2(!orangeBounds.isValid(), qPrintable(QStringLiteral("second probe band should be clipped in %1").arg(result.imagePath)));
    QVERIFY2(!cyanBounds.isValid(), qPrintable(QStringLiteral("third probe band should be clipped in %1").arg(result.imagePath)));
}

void InlineNoteRenderingExperimentTest::perLineNotesPaintOnMultipleRealLines()
{
    const RenderResult result = renderProbeImage(QStringLiteral("per_line_notes"), ProbeMode::PerLineNotes);
    const QRect magentaBounds = findColorBounds(result.image, kMagenta);
    const QRect greenBounds = findColorBounds(result.image, kGreen);
    const QRect blueBounds = findColorBounds(result.image, kBlue);

    QVERIFY2(magentaBounds.isValid(), qPrintable(QStringLiteral("expected first per-line band in %1").arg(result.imagePath)));
    QVERIFY2(greenBounds.isValid(), qPrintable(QStringLiteral("expected second per-line band in %1").arg(result.imagePath)));
    QVERIFY2(blueBounds.isValid(), qPrintable(QStringLiteral("expected third per-line band in %1").arg(result.imagePath)));
    QVERIFY2(magentaBounds.top() < greenBounds.top() && greenBounds.top() < blueBounds.top(),
             qPrintable(QStringLiteral("expected three real lines in ascending order in %1").arg(result.imagePath)));
}

QTEST_MAIN(InlineNoteRenderingExperimentTest)
#include "InlineNoteRenderingExperimentTest.moc"
