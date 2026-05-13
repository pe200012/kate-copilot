/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CurrentFileContextProvider
*/

#include "context/CurrentFileContextProvider.h"

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] ContextItem trait(const QString &providerId,
                                const QString &id,
                                const QString &uri,
                                const QString &name,
                                const QString &value,
                                int importance)
{
    ContextItem item;
    item.kind = ContextItem::Kind::Trait;
    item.providerId = providerId;
    item.id = id;
    item.uri = uri;
    item.name = name;
    item.value = value;
    item.importance = importance;
    return item;
}
} // namespace

QString CurrentFileContextProvider::id() const
{
    return QStringLiteral("current-file");
}

int CurrentFileContextProvider::matchScore(const ContextResolveRequest &request) const
{
    Q_UNUSED(request);
    return 100;
}

QVector<ContextItem> CurrentFileContextProvider::resolve(const ContextResolveRequest &request)
{
    QVector<ContextItem> items;
    items.reserve(4);

    const QString providerId = id();

    if (!request.uri.trimmed().isEmpty()) {
        items.push_back(trait(providerId,
                              QStringLiteral("file_path"),
                              request.uri,
                              QStringLiteral("file_path"),
                              request.uri,
                              100));
    }

    if (!request.languageId.trimmed().isEmpty()) {
        items.push_back(trait(providerId,
                              QStringLiteral("language"),
                              request.uri,
                              QStringLiteral("language"),
                              request.languageId,
                              95));
    }

    if (request.position.isValid()) {
        items.push_back(trait(providerId,
                              QStringLiteral("cursor_line"),
                              request.uri,
                              QStringLiteral("cursor_line"),
                              QString::number(request.position.line() + 1),
                              90));
        items.push_back(trait(providerId,
                              QStringLiteral("cursor_column"),
                              request.uri,
                              QStringLiteral("cursor_column"),
                              QString::number(request.position.column() + 1),
                              90));
    }

    return items;
}

} // namespace KateAiInlineCompletion
