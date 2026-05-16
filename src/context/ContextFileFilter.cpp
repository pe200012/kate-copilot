/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ContextFileFilter
*/

#include "context/ContextFileFilter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtGlobal>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] QString normalizedPath(QString path)
{
    path = QDir::fromNativeSeparators(path.trimmed());
    return QDir::cleanPath(path);
}

[[nodiscard]] QStringList pathParts(const QString &path)
{
    return normalizedPath(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
}

[[nodiscard]] bool hasExcludedDirectory(const QString &path)
{
    static const QStringList exact = {
        QStringLiteral(".git"),
        QStringLiteral("build"),
        QStringLiteral("node_modules"),
        QStringLiteral("dist"),
        QStringLiteral("target"),
        QStringLiteral(".venv"),
        QStringLiteral("venv"),
        QStringLiteral("site-packages"),
        QStringLiteral("__pycache__"),
    };

    const QStringList parts = pathParts(path);
    for (int i = 0; i + 1 < parts.size(); ++i) {
        const QString part = parts.at(i);
        if (exact.contains(part) || QDir::match(QStringLiteral("cmake-build-*"), part)) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool isGeneratedFileName(const QString &fileName)
{
    const QString lower = fileName.toLower();
    return lower.endsWith(QStringLiteral(".min.js")) || QDir::match(QStringLiteral("moc_*.cpp"), fileName)
        || QDir::match(QStringLiteral("ui_*.h"), fileName) || QDir::match(QStringLiteral("qrc_*.cpp"), fileName);
}

[[nodiscard]] bool isPrivateLookingPath(const QString &path)
{
    const QString lower = normalizedPath(path).toLower();
    const QString file = QFileInfo(lower).fileName();
    return file == QStringLiteral(".env") || file.startsWith(QStringLiteral(".env.")) || lower.contains(QStringLiteral("secret"))
        || lower.contains(QStringLiteral("token")) || lower.contains(QStringLiteral("credential")) || lower.contains(QStringLiteral("password"))
        || lower.contains(QStringLiteral("private"));
}

[[nodiscard]] bool matchesUserPattern(const QString &path, const QStringList &patterns)
{
    const QString cleanPath = normalizedPath(path);
    const QString fileName = QFileInfo(cleanPath).fileName();
    const QStringList parts = pathParts(cleanPath);

    for (const QString &rawPattern : patterns) {
        const QString pattern = QDir::fromNativeSeparators(rawPattern.trimmed());
        if (pattern.isEmpty()) {
            continue;
        }

        if (QDir::match(pattern, fileName) || QDir::match(pattern, cleanPath)) {
            return true;
        }

        for (int i = 0; i < parts.size(); ++i) {
            const QString suffixPath = parts.mid(i).join(QLatin1Char('/'));
            if (QDir::match(pattern, suffixPath)) {
                return true;
            }
        }
    }

    return false;
}

[[nodiscard]] bool hasBinaryExtension(const QString &path)
{
    static const QStringList binaryExtensions = {
        QStringLiteral("a"),    QStringLiteral("bin"),  QStringLiteral("class"), QStringLiteral("dll"), QStringLiteral("dylib"),
        QStringLiteral("exe"),  QStringLiteral("gif"),  QStringLiteral("gz"),    QStringLiteral("ico"), QStringLiteral("jar"),
        QStringLiteral("jpeg"), QStringLiteral("jpg"),  QStringLiteral("o"),     QStringLiteral("obj"), QStringLiteral("pdf"),
        QStringLiteral("png"),  QStringLiteral("pyc"),  QStringLiteral("so"),    QStringLiteral("tar"), QStringLiteral("webp"),
        QStringLiteral("zip"),
    };

    return binaryExtensions.contains(QFileInfo(path).suffix().toLower());
}

[[nodiscard]] bool dataLooksBinary(const QByteArray &data)
{
    return data.contains('\0');
}
} // namespace

bool ContextFileFilter::isAllowedPath(const QString &path, const ContextFileFilterOptions &options)
{
    const QString cleanPath = normalizedPath(path);
    if (cleanPath.isEmpty()) {
        return false;
    }

    const QFileInfo info(cleanPath);
    const QString fileName = info.fileName();
    if (fileName.isEmpty()) {
        return false;
    }

    if (hasExcludedDirectory(cleanPath) || isGeneratedFileName(fileName) || isPrivateLookingPath(cleanPath) || hasBinaryExtension(cleanPath)) {
        return false;
    }

    if (matchesUserPattern(cleanPath, options.excludePatterns)) {
        return false;
    }

    return true;
}

bool ContextFileFilter::isAllowedFile(const QString &path, const ContextFileFilterOptions &options)
{
    if (!isAllowedPath(path, options)) {
        return false;
    }

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || !info.isReadable()) {
        return false;
    }

    if (options.maxFileChars >= 0 && info.size() > options.maxFileChars) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray sample = file.read(qMin<qint64>(info.size(), 4096));
    return !dataLooksBinary(sample);
}

QString ContextFileFilter::readTextFile(const QString &path, const ContextFileFilterOptions &options, bool *ok)
{
    if (ok) {
        *ok = false;
    }

    if (!isAllowedFile(path, options)) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QByteArray bytes = file.read(options.maxFileChars + 1);
    if (options.maxFileChars >= 0 && bytes.size() > options.maxFileChars) {
        return {};
    }

    if (dataLooksBinary(bytes)) {
        return {};
    }

    if (ok) {
        *ok = true;
    }
    return QString::fromUtf8(bytes);
}

} // namespace KateAiInlineCompletion
