/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextPushDownOverlay
*/

#include "render/GhostTextPushDownOverlay.h"

#include <KTextEditor/View>

#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QWheelEvent>

namespace KateAiInlineCompletion
{

static bool hasRenderableMultilineText(const GhostTextState &s)
{
    if (s.suppressed || s.visibleText.isEmpty()) {
        return false;
    }

    if (s.anchor.line < 0 || s.anchor.column < 0) {
        return false;
    }

    return s.visibleText.contains(QLatin1Char('\n'));
}

GhostTextPushDownOverlay::GhostTextPushDownOverlay(KTextEditor::View *view, QWidget *editorWidget)
    : QWidget(editorWidget)
    , m_view(view)
    , m_editorWidget(editorWidget)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    m_snapshotTimer.setSingleShot(true);
    m_snapshotTimer.setInterval(30);
    connect(&m_snapshotTimer, &QTimer::timeout, this, &GhostTextPushDownOverlay::takeSnapshot);

    if (m_editorWidget) {
        m_editorWidget->installEventFilter(this);
        updateGeometryFromParent();
    }

    setVisible(false);
}

void GhostTextPushDownOverlay::setState(const GhostTextState &state)
{
    const bool prevActive = isActive();

    m_state = state;

    const bool nextActive = isActive();

    if (prevActive != nextActive) {
        setVisible(nextActive);
    }

    if (nextActive) {
        scheduleSnapshot();
        update();
    }
}

GhostTextState GhostTextPushDownOverlay::state() const
{
    return m_state;
}

bool GhostTextPushDownOverlay::isActive() const
{
    return hasRenderableMultilineText(m_state) && m_view && m_editorWidget;
}

void GhostTextPushDownOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (!isActive()) {
        return;
    }

    const QSize snapshotLogicalSize = m_snapshot.isNull() ? QSize() : m_snapshot.deviceIndependentSize().toSize();
    if (snapshotLogicalSize != size()) {
        takeSnapshot();
    }

    if (m_snapshot.isNull()) {
        return;
    }

    const int ySplit = anchorYSplitPx();
    const int lh = lineHeightPx();
    const QStringList lines = ghostLines();

    const int extraLines = qMax(0, lines.size() - 1);
    const int shiftPx = extraLines * lh;

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QPalette pal = m_editorWidget ? m_editorWidget->palette() : palette();
    const QColor bg = pal.color(QPalette::Base);
    const QColor fg = pal.color(QPalette::Text);

    painter.fillRect(rect(), bg);

    const int w = width();
    const int h = height();

    const int ySplitClamped = qBound(0, ySplit, h);

    if (ySplitClamped > 0) {
        painter.save();
        painter.setClipRect(QRect(0, 0, w, ySplitClamped));
        painter.drawPixmap(0, 0, m_snapshot);
        painter.restore();
    }

    if (shiftPx > 0 && ySplitClamped < h) {
        painter.fillRect(QRect(0, ySplitClamped, w, shiftPx), bg);

        const int targetY = ySplitClamped + shiftPx;
        const int remainingTarget = h - targetY;
        if (remainingTarget > 0) {
            painter.save();
            painter.setClipRect(QRect(0, targetY, w, remainingTarget));
            painter.drawPixmap(0, shiftPx, m_snapshot);
            painter.restore();
        }

        QColor ghost = QColor::fromRgbF((fg.redF() + bg.redF()) * 0.5,
                                        (fg.greenF() + bg.greenF()) * 0.5,
                                        (fg.blueF() + bg.blueF()) * 0.5);
        ghost.setAlphaF(0.75);

        painter.setPen(ghost);
        painter.setFont(m_editorWidget->font());

        const int x0 = anchorX0Px();

        const int maxLinesToDraw = qMin(extraLines, shiftPx / lh);
        for (int i = 0; i < maxLinesToDraw; ++i) {
            const int lineIndex = i + 1;
            const int yLineTop = ySplitClamped + i * lh;
            const QRect lineRect(x0, yLineTop, w - x0, lh);

            painter.drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter, lines.value(lineIndex));
        }
    }
}

void GhostTextPushDownOverlay::mousePressEvent(QMouseEvent *event)
{
    forwardMouseEvent(event);
}

void GhostTextPushDownOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    forwardMouseEvent(event);
}

void GhostTextPushDownOverlay::mouseMoveEvent(QMouseEvent *event)
{
    forwardMouseEvent(event);
}

void GhostTextPushDownOverlay::wheelEvent(QWheelEvent *event)
{
    forwardWheelEvent(event);
}

bool GhostTextPushDownOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_editorWidget || watched != m_editorWidget || !event) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
        updateGeometryFromParent();
        if (isActive()) {
            scheduleSnapshot();
            update();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void GhostTextPushDownOverlay::updateGeometryFromParent()
{
    if (!m_editorWidget) {
        return;
    }

    setGeometry(m_editorWidget->rect());
    raise();
}

void GhostTextPushDownOverlay::scheduleSnapshot()
{
    if (!m_snapshotTimer.isActive()) {
        m_snapshotTimer.start();
    }
}

void GhostTextPushDownOverlay::takeSnapshot()
{
    if (!m_editorWidget) {
        return;
    }

    updateGeometryFromParent();

    const qreal dpr = m_editorWidget->devicePixelRatioF();
    const QSize pixelSize = (QSizeF(size()) * dpr).toSize();
    if (pixelSize.isEmpty()) {
        return;
    }

    QPixmap pix(pixelSize);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);

    const QWidget::RenderFlags flags = QWidget::RenderFlag::DrawWindowBackground;
    m_editorWidget->render(&pix, QPoint(), QRegion(), flags);

    m_snapshot = pix;
    update();
}

int GhostTextPushDownOverlay::lineHeightPx() const
{
    if (!m_view || !m_editorWidget) {
        return fontMetrics().height();
    }

    const int line = qMax(0, m_state.anchor.line);

    const QPoint inView0 = m_view->cursorToCoordinate(KTextEditor::Cursor(line, 0));
    const QPoint inView1 = m_view->cursorToCoordinate(KTextEditor::Cursor(line + 1, 0));

    const QPoint global0 = m_view->mapToGlobal(inView0);
    const QPoint global1 = m_view->mapToGlobal(inView1);

    const QPoint p0 = m_editorWidget->mapFromGlobal(global0);
    const QPoint p1 = m_editorWidget->mapFromGlobal(global1);

    const int diff = p1.y() - p0.y();
    if (diff > 0) {
        return diff;
    }

    return m_editorWidget->fontMetrics().height();
}

int GhostTextPushDownOverlay::anchorYSplitPx() const
{
    if (!m_view || !m_editorWidget) {
        return 0;
    }

    const int lh = lineHeightPx();

    const int line = qMax(0, m_state.anchor.line);
    const QPoint inView = m_view->cursorToCoordinate(KTextEditor::Cursor(line, 0));
    const QPoint global = m_view->mapToGlobal(inView);
    const QPoint inEditor = m_editorWidget->mapFromGlobal(global);

    return inEditor.y() + lh;
}

int GhostTextPushDownOverlay::anchorX0Px() const
{
    if (!m_view || !m_editorWidget) {
        return 0;
    }

    const int line = qMax(0, m_state.anchor.line);
    const QPoint inView = m_view->cursorToCoordinate(KTextEditor::Cursor(line, 0));
    const QPoint global = m_view->mapToGlobal(inView);
    const QPoint inEditor = m_editorWidget->mapFromGlobal(global);

    return inEditor.x();
}

QStringList GhostTextPushDownOverlay::ghostLines() const
{
    return m_state.visibleText.split(QLatin1Char('\n'));
}

void GhostTextPushDownOverlay::forwardMouseEvent(QMouseEvent *event)
{
    if (!event || !m_editorWidget) {
        return;
    }

    Q_EMIT interactionOccurred();

    QMouseEvent forwarded(event->type(),
                           event->position(),
                           event->globalPosition(),
                           event->button(),
                           event->buttons(),
                           event->modifiers());

    QCoreApplication::sendEvent(m_editorWidget, &forwarded);
    event->accept();
}

void GhostTextPushDownOverlay::forwardWheelEvent(QWheelEvent *event)
{
    if (!event || !m_editorWidget) {
        return;
    }

    Q_EMIT interactionOccurred();

    QWheelEvent forwarded(event->position(),
                           event->globalPosition(),
                           event->pixelDelta(),
                           event->angleDelta(),
                           event->buttons(),
                           event->modifiers(),
                           event->phase(),
                           event->inverted(),
                           event->source());

    QCoreApplication::sendEvent(m_editorWidget, &forwarded);
    event->accept();
}

} // namespace KateAiInlineCompletion
