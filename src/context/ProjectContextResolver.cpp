/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ProjectContextResolver
*/

#include "context/ProjectContextResolver.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] QString absolutePathPreservingSegments(const QString &path)
{
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    if (normalized.isEmpty() || !QDir::isAbsolutePath(normalized)) {
        return {};
    }
    return normalized;
}

[[nodiscard]] QString parentPath(const QString &path)
{
    const QString parent = QFileInfo(path).absoluteDir().absolutePath();
    return parent == path ? path : QDir::cleanPath(parent);
}

[[nodiscard]] QString canonicalPathResolvingExistingAncestors(const QString &path)
{
    const QString localPath = absolutePathPreservingSegments(path);
    if (localPath.isEmpty()) {
        return {};
    }

    QString resolved = QDir::rootPath();
    const QStringList parts = localPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QStringLiteral(".")) {
            continue;
        }

        if (part == QStringLiteral("..")) {
            resolved = parentPath(resolved);
            continue;
        }

        const QString candidate = QDir(resolved).filePath(part);
        const QFileInfo info(candidate);
        if (info.exists()) {
            const QString canonical = info.canonicalFilePath();
            if (canonical.isEmpty()) {
                return {};
            }
            resolved = canonical;
        } else {
            resolved = candidate;
        }
    }

    return QDir::cleanPath(resolved);
}

[[nodiscard]] QString directoryForPath(const QString &path)
{
    const QFileInfo info(path);
    if (info.exists() && info.isDir()) {
        return info.absoluteFilePath();
    }
    return info.absoluteDir().absolutePath();
}

[[nodiscard]] bool hasCabalFile(const QDir &dir)
{
    return !dir.entryList(QStringList{QStringLiteral("*.cabal")}, QDir::Files, QDir::Name).isEmpty();
}

[[nodiscard]] bool hasAnyProjectMarker(const QDir &dir)
{
    static const QStringList markers = {
        QStringLiteral("CMakeLists.txt"),
        QStringLiteral("package.json"),
        QStringLiteral("package.yaml"),
        QStringLiteral("pyproject.toml"),
        QStringLiteral("Cargo.toml"),
        QStringLiteral("cabal.project"),
        QStringLiteral("stack.yaml"),
        QStringLiteral("hie.yaml"),
    };

    for (const QString &marker : markers) {
        if (dir.exists(marker)) {
            return true;
        }
    }

    return hasCabalFile(dir);
}
} // namespace

QString ProjectContextResolver::localPathFromUri(const QString &uriOrPath)
{
    const QString trimmed = uriOrPath.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl url(trimmed);
    if (url.isValid() && !url.scheme().isEmpty()) {
        return url.isLocalFile() ? absolutePathPreservingSegments(url.toLocalFile()) : QString();
    }

    return absolutePathPreservingSegments(trimmed);
}

QString ProjectContextResolver::canonicalPath(const QString &path)
{
    const QString localPath = localPathFromUri(path);
    if (localPath.isEmpty()) {
        return {};
    }

    return canonicalPathResolvingExistingAncestors(localPath);
}

QString ProjectContextResolver::findProjectRoot(const QString &path)
{
    const QString localPath = localPathFromUri(path);
    if (localPath.isEmpty()) {
        return {};
    }

    QDir dir(directoryForPath(localPath));
    QString markerRoot;

    while (true) {
        if (dir.exists(QStringLiteral(".git"))) {
            return dir.absolutePath();
        }

        if (markerRoot.isEmpty() && hasAnyProjectMarker(dir)) {
            markerRoot = dir.absolutePath();
        }

        if (!dir.cdUp()) {
            break;
        }
    }

    return markerRoot;
}

QString ProjectContextResolver::relativeDisplayPath(const QString &uriOrPath, const QString &projectRoot)
{
    const QString localPath = localPathFromUri(uriOrPath);
    const QString cleanPath = canonicalPath(uriOrPath);
    const QString cleanRoot = canonicalPath(projectRoot);
    if (!cleanPath.isEmpty() && !cleanRoot.isEmpty() && isWithinRoot(cleanPath, cleanRoot)) {
        const QString relative = QDir(cleanRoot).relativeFilePath(cleanPath);
        if (!relative.startsWith(QStringLiteral(".."))) {
            return relative;
        }
    }

    if (!localPath.isEmpty()) {
        return QFileInfo(localPath).fileName();
    }

    return uriOrPath.trimmed();
}

bool ProjectContextResolver::isWithinRoot(const QString &path, const QString &projectRoot)
{
    const QString cleanPath = canonicalPath(path);
    const QString cleanRoot = canonicalPath(projectRoot);
    if (cleanPath.isEmpty() || cleanRoot.isEmpty()) {
        return false;
    }

    return cleanPath == cleanRoot || cleanPath.startsWith(cleanRoot + QLatin1Char('/'));
}

} // namespace KateAiInlineCompletion
