/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RecentEditsTracker

    Tracks compact line-based edit summaries for open KTextEditor documents.
*/

#pragma once

#include "context/RecentEdit.h"

#include <KTextEditor/Document>

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QVector>

namespace KateAiInlineCompletion
{

struct RecentEditsTrackerOptions {
    int maxFiles = 20;
    int maxEdits = 8;
    int diffContextLines = 3;
    int maxCharsPerEdit = 2000;
    int debounceMs = 500;
    int maxLinesPerEdit = 10;
    int maxDocumentChars = 200000;
};

class RecentEditsTracker final : public QObject
{
    Q_OBJECT

public:
    explicit RecentEditsTracker(QObject *parent = nullptr);
    ~RecentEditsTracker() override;

    void setOptions(const RecentEditsTrackerOptions &options);
    [[nodiscard]] RecentEditsTrackerOptions options() const;

    void trackDocument(KTextEditor::Document *document, const QString &uriOverride = QString());
    void untrackDocument(KTextEditor::Document *document);

    void addRecentEdit(const RecentEdit &edit);
    [[nodiscard]] QVector<RecentEdit> recentEdits() const;

    void flushPendingEdits();
    void clear();

private:
    struct DocumentState {
        QPointer<KTextEditor::Document> document;
        QString uriOverride;
        QStringList snapshot;
        QTimer *timer = nullptr;
        bool pending = false;
        QMetaObject::Connection textChangedConnection;
        QMetaObject::Connection destroyedConnection;
    };

    void onDocumentTextChanged(KTextEditor::Document *document);
    void processPendingEdit(KTextEditor::Document *document);
    void pruneHistory();

    [[nodiscard]] QStringList captureLines(KTextEditor::Document *document) const;
    [[nodiscard]] RecentEdit buildEdit(const QString &uri, const QStringList &before, const QStringList &after) const;
    [[nodiscard]] QString documentUri(const DocumentState &state) const;
    [[nodiscard]] QString boundedText(const QString &text) const;
    [[nodiscard]] bool shouldMerge(const RecentEdit &existing, const RecentEdit &incoming) const;

    RecentEditsTrackerOptions m_options;
    QHash<KTextEditor::Document *, DocumentState *> m_documents;
    QVector<RecentEdit> m_edits;
};

} // namespace KateAiInlineCompletion
