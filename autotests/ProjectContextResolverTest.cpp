/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ProjectContextResolverTest
*/

#include "context/ProjectContextResolver.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

using KateAiInlineCompletion::ProjectContextResolver;

namespace
{
void writeText(const QString &path, const QString &text)
{
    QFileInfo info(path);
    QVERIFY(QDir().mkpath(info.absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(text.toUtf8());
}
}

class ProjectContextResolverTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localPathFromUriAcceptsLocalFilesAndRejectsRemoteOrRelativePaths();
    void findProjectRootUsesSharedMixedProjectMarkers();
    void relativeDisplayPathStaysInsideProjectRoot();
    void relativeDisplayPathUsesCanonicalRootAliases();
    void isWithinRootRejectsCanonicalSymlinkEscapes();
    void isWithinRootRejectsSymlinkDirectoryEscapesForMissingLeaves();
    void isWithinRootResolvesDotDotAfterSymlinkDirectories();
};

void ProjectContextResolverTest::localPathFromUriAcceptsLocalFilesAndRejectsRemoteOrRelativePaths()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath(QStringLiteral("src/Main.hs"));
    writeText(filePath, QStringLiteral("module Main where\n"));

    QCOMPARE(ProjectContextResolver::localPathFromUri(QUrl::fromLocalFile(filePath).toString()), QFileInfo(filePath).absoluteFilePath());
    QCOMPARE(ProjectContextResolver::localPathFromUri(filePath), QFileInfo(filePath).absoluteFilePath());
    QVERIFY(ProjectContextResolver::localPathFromUri(QStringLiteral("https://example.invalid/Main.hs")).isEmpty());
    QVERIFY(ProjectContextResolver::localPathFromUri(QStringLiteral("src/Main.hs")).isEmpty());
}

void ProjectContextResolverTest::findProjectRootUsesSharedMixedProjectMarkers()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString root = dir.filePath(QStringLiteral("repo"));
    const QString source = root + QStringLiteral("/src/Foo/Bar.hs");
    writeText(root + QStringLiteral("/kate-copilot.cabal"), QStringLiteral("cabal-version: 3.0\n"));
    writeText(source, QStringLiteral("module Foo.Bar where\n"));

    QCOMPARE(ProjectContextResolver::findProjectRoot(source), QFileInfo(root).absoluteFilePath());
}

void ProjectContextResolverTest::relativeDisplayPathStaysInsideProjectRoot()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString root = dir.filePath(QStringLiteral("repo"));
    const QString source = root + QStringLiteral("/src/Foo.cpp");
    writeText(source, QStringLiteral("int foo();\n"));

    QCOMPARE(ProjectContextResolver::relativeDisplayPath(source, root), QStringLiteral("src/Foo.cpp"));
}

void ProjectContextResolverTest::relativeDisplayPathUsesCanonicalRootAliases()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString realRoot = dir.filePath(QStringLiteral("real-repo"));
    const QString linkRoot = dir.filePath(QStringLiteral("linked-repo"));
    const QString source = realRoot + QStringLiteral("/src/Foo.cpp");
    writeText(source, QStringLiteral("int foo();\n"));
    QVERIFY(QFile::link(realRoot, linkRoot));

    QCOMPARE(ProjectContextResolver::relativeDisplayPath(source, linkRoot), QStringLiteral("src/Foo.cpp"));
}

void ProjectContextResolverTest::isWithinRootRejectsCanonicalSymlinkEscapes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString root = dir.filePath(QStringLiteral("repo"));
    const QString outside = dir.filePath(QStringLiteral("outside/Secret.hs"));
    const QString link = root + QStringLiteral("/src/LinkedSecret.hs");
    writeText(outside, QStringLiteral("module Secret where\n"));
    QVERIFY(QDir().mkpath(QFileInfo(link).absolutePath()));
    QVERIFY(QFile::link(outside, link));

    QVERIFY(!ProjectContextResolver::isWithinRoot(link, root));
}

void ProjectContextResolverTest::isWithinRootRejectsSymlinkDirectoryEscapesForMissingLeaves()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString root = dir.filePath(QStringLiteral("repo"));
    const QString outside = dir.filePath(QStringLiteral("outside"));
    const QString linkDir = root + QStringLiteral("/src/outside-link");
    const QString missingLeaf = linkDir + QStringLiteral("/NewFile.cpp");
    QVERIFY(QDir().mkpath(outside));
    QVERIFY(QDir().mkpath(QFileInfo(linkDir).absolutePath()));
    QVERIFY(QFile::link(outside, linkDir));

    QVERIFY(!ProjectContextResolver::isWithinRoot(missingLeaf, root));
}

void ProjectContextResolverTest::isWithinRootResolvesDotDotAfterSymlinkDirectories()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString root = dir.filePath(QStringLiteral("repo"));
    const QString outsideDir = dir.filePath(QStringLiteral("outside/dir"));
    const QString existingOutside = dir.filePath(QStringLiteral("outside/Existing.cpp"));
    const QString linkDir = root + QStringLiteral("/src/outside-link");
    QVERIFY(QDir().mkpath(outsideDir));
    writeText(existingOutside, QStringLiteral("int outside();\n"));
    QVERIFY(QDir().mkpath(QFileInfo(linkDir).absolutePath()));
    QVERIFY(QFile::link(outsideDir, linkDir));

    QVERIFY(!ProjectContextResolver::isWithinRoot(linkDir + QStringLiteral("/../Existing.cpp"), root));
    QVERIFY(!ProjectContextResolver::isWithinRoot(linkDir + QStringLiteral("/../Missing.cpp"), root));
}

QTEST_MAIN(ProjectContextResolverTest)

#include "ProjectContextResolverTest.moc"
