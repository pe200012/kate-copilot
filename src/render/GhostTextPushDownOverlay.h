/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextPushDownOverlay

    Paints multi-line ghost text in a KTextEditor view using a pixel push-down
    effect.

    Rendering model:
    - The first line of ghost text stays in InlineNoteProvider (single-line).
    - Additional lines are rendered by this overlay.
    - The overlay captures a snapshot of the editor widget and shifts the
      content below the anchor line down by N line-heights.

    Interaction model:
    - Mouse and wheel interaction clears the suggestion before forwarding the
      event to the editor widget.
*/

#pragma once

#include "session/GhostTextState.h"

#include <QPointer>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

namespace KTextEditor
{
class View;
}

namespace KateAiInlineCompletion
{

class GhostTextPushDownOverlay final : public QWidget
{
    Q_OBJECT

public:
    GhostTextPushDownOverlay(KTextEditor::View *view, QWidget *editorWidget);

    void setState(const GhostTextState &state);
    [[nodiscard]] GhostTextState state() const;

Q_SIGNALS:
    void interactionOccurred();

protected:
    void paintEvent(QPaintEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateGeometryFromParent();

    void scheduleSnapshot();
    void takeSnapshot();

    void forwardMouseEvent(QMouseEvent *event);
    void forwardWheelEvent(QWheelEvent *event);

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] int lineHeightPx() const;
    [[nodiscard]] int anchorYSplitPx() const;
    [[nodiscard]] int anchorX0Px() const;
    [[nodiscard]] QStringList ghostLines() const;

    QPointer<KTextEditor::View> m_view;
    QPointer<QWidget> m_editorWidget;

    GhostTextState m_state;

    QPixmap m_snapshot;
    QTimer m_snapshotTimer;
};

} // namespace KateAiInlineCompletion
