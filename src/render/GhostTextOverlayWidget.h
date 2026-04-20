/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: GhostTextOverlayWidget

    Renders GhostTextState as ghost text using a transparent overlay widget
    attached to KTextEditor::View::editorWidget().

    Rendering policy:
    - All ghost lines are painted by this overlay.
    - The first line starts at the anchor cursor coordinate.
    - Following lines start at the view's text area left edge.
    - The document buffer stays unchanged until acceptance.
*/

#pragma once

#include "session/GhostTextState.h"

#include <KTextEditor/Cursor>

#include <QPointer>
#include <QWidget>

namespace KTextEditor
{
class View;
}

namespace KateAiInlineCompletion
{

class GhostTextOverlayWidget final : public QWidget
{
    Q_OBJECT

public:
    GhostTextOverlayWidget(KTextEditor::View *view, QWidget *editorWidget);

    void setState(const GhostTextState &state);
    [[nodiscard]] GhostTextState state() const;

    [[nodiscard]] bool isActive() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateGeometryFromParent();

    [[nodiscard]] QFont effectiveTextFont() const;
    [[nodiscard]] QPoint cursorToEditorWidget(const KTextEditor::Cursor &cursor) const;
    [[nodiscard]] QRect textAreaRectInEditorWidget() const;
    [[nodiscard]] int lineHeightPx() const;

    QPointer<KTextEditor::View> m_view;
    QPointer<QWidget> m_editorWidget;

    GhostTextState m_state;
};

} // namespace KateAiInlineCompletion
