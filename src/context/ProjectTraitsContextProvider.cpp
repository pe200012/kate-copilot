/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ProjectTraitsContextProvider
*/

#include "context/ProjectTraitsContextProvider.h"

#include "context/ProjectContextResolver.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTextStream>

namespace KateAiInlineCompletion
{

namespace
{
constexpr qsizetype kMaxMarkerReadBytes = 64 * 1024;

[[nodiscard]] QString localPathFromUri(const QString &uri)
{
    return ProjectContextResolver::localPathFromUri(uri);
}

[[nodiscard]] QString findProjectRoot(const QString &localPath)
{
    return ProjectContextResolver::findProjectRoot(localPath);
}

[[nodiscard]] QString readSmallTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    return QString::fromUtf8(file.read(kMaxMarkerReadBytes));
}

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

void addTrait(QVector<ContextItem> *items,
              const QString &providerId,
              const QString &id,
              const QString &uri,
              const QString &name,
              const QString &value,
              int importance)
{
    if (!items || value.trimmed().isEmpty()) {
        return;
    }

    items->push_back(trait(providerId, id, uri, name, value.trimmed(), importance));
}

void addFrameworkTrait(QVector<ContextItem> *items,
                       QSet<QString> *seen,
                       const QString &providerId,
                       const QString &uri,
                       const QString &framework,
                       int importance)
{
    if (!items || !seen) {
        return;
    }

    const QString normalized = framework.trimmed();
    if (normalized.isEmpty() || seen->contains(normalized)) {
        return;
    }

    seen->insert(normalized);
    addTrait(items,
             providerId,
             QStringLiteral("framework:%1").arg(normalized.toLower().replace(QLatin1Char(' '), QLatin1Char('_'))),
             uri,
             QStringLiteral("framework"),
             normalized,
             importance);
}
} // namespace

QString ProjectTraitsContextProvider::id() const
{
    return QStringLiteral("project-traits");
}

int ProjectTraitsContextProvider::matchScore(const ContextResolveRequest &request) const
{
    return localPathFromUri(request.uri).isEmpty() ? 0 : 70;
}

QVector<ContextItem> ProjectTraitsContextProvider::resolve(const ContextResolveRequest &request)
{
    QVector<ContextItem> items;

    const QString localPath = localPathFromUri(request.uri);
    const QString root = findProjectRoot(localPath);
    if (root.isEmpty()) {
        return items;
    }

    const QString providerId = id();
    const QDir rootDir(root);

    if (rootDir.exists(QStringLiteral(".git"))) {
        addTrait(&items, providerId, QStringLiteral("repository_root"), request.uri, QStringLiteral("repository_root"), root, 90);
    }

    if (rootDir.exists(QStringLiteral("CMakeLists.txt"))) {
        addTrait(&items, providerId, QStringLiteral("build_system:cmake"), request.uri, QStringLiteral("build_system"), QStringLiteral("CMake"), 82);
    }
    if (rootDir.exists(QStringLiteral("package.json"))) {
        addTrait(&items, providerId, QStringLiteral("build_system:package_json"), request.uri, QStringLiteral("build_system"), QStringLiteral("npm/package.json"), 78);
    }
    if (rootDir.exists(QStringLiteral("pyproject.toml"))) {
        addTrait(&items, providerId, QStringLiteral("build_system:pyproject"), request.uri, QStringLiteral("build_system"), QStringLiteral("Python/pyproject.toml"), 76);
    }
    if (rootDir.exists(QStringLiteral("Cargo.toml"))) {
        addTrait(&items, providerId, QStringLiteral("build_system:cargo"), request.uri, QStringLiteral("build_system"), QStringLiteral("Cargo/Rust"), 76);
    }

    QSet<QString> frameworks;

    const QString cmake = readSmallTextFile(rootDir.filePath(QStringLiteral("CMakeLists.txt")));
    if (!cmake.isEmpty()) {
        if (cmake.contains(QStringLiteral("KF6"), Qt::CaseInsensitive) || cmake.contains(QStringLiteral("KTextEditor"), Qt::CaseInsensitive)) {
            addFrameworkTrait(&items, &frameworks, providerId, request.uri, QStringLiteral("KDE Frameworks 6"), 74);
        }
        if (cmake.contains(QStringLiteral("Qt6"), Qt::CaseInsensitive) || cmake.contains(QStringLiteral("find_package(Qt6"), Qt::CaseInsensitive)) {
            addFrameworkTrait(&items, &frameworks, providerId, request.uri, QStringLiteral("Qt 6"), 73);
        }
    }

    const QString packageJson = readSmallTextFile(rootDir.filePath(QStringLiteral("package.json")));
    if (!packageJson.isEmpty()) {
        if (packageJson.contains(QStringLiteral("react"), Qt::CaseInsensitive)) {
            addFrameworkTrait(&items, &frameworks, providerId, request.uri, QStringLiteral("React"), 70);
        }
        if (packageJson.contains(QStringLiteral("vue"), Qt::CaseInsensitive)) {
            addFrameworkTrait(&items, &frameworks, providerId, request.uri, QStringLiteral("Vue"), 70);
        }
        if (packageJson.contains(QStringLiteral("next"), Qt::CaseInsensitive)) {
            addFrameworkTrait(&items, &frameworks, providerId, request.uri, QStringLiteral("Next.js"), 70);
        }
    }

    const QString pyproject = readSmallTextFile(rootDir.filePath(QStringLiteral("pyproject.toml")));
    if (!pyproject.isEmpty()) {
        if (pyproject.contains(QStringLiteral("django"), Qt::CaseInsensitive)) {
            addFrameworkTrait(&items, &frameworks, providerId, request.uri, QStringLiteral("Django"), 70);
        }
        if (pyproject.contains(QStringLiteral("fastapi"), Qt::CaseInsensitive)) {
            addFrameworkTrait(&items, &frameworks, providerId, request.uri, QStringLiteral("FastAPI"), 70);
        }
    }

    return items;
}

} // namespace KateAiInlineCompletion
