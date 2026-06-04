/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditPreviewOverlay

    Minimal visual preview for one structured inline edit suggestion.
*/

#pragma once

#include "inlineedit/InlineEdit.h"

#include <QPointer>
#include <QWidget>

namespace KTextEditor
{
class View;
}

namespace KateAiInlineCompletion
{

class InlineEditPreviewOverlay final : public QWidget
{
    Q_OBJECT

public:
    InlineEditPreviewOverlay(KTextEditor::View *view, QWidget *editorWidget);
    ~InlineEditPreviewOverlay() override;

    void setSuggestion(const InlineEditSuggestion &suggestion);
    void clear();
    void refresh();

    [[nodiscard]] InlineEditSuggestion suggestion() const;
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
    InlineEditSuggestion m_suggestion;
};

} // namespace KateAiInlineCompletion
