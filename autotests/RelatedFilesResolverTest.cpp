/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RelatedFilesResolverTest
*/

#include "context/RelatedFilesResolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using KateAiInlineCompletion::RelatedFileCandidate;
using KateAiInlineCompletion::RelatedFilesResolveRequest;
using KateAiInlineCompletion::RelatedFilesResolver;

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

QStringList paths(const QVector<RelatedFileCandidate> &candidates)
{
    QStringList out;
    for (const RelatedFileCandidate &candidate : candidates) {
        out.push_back(QFileInfo(candidate.path).fileName());
    }
    return out;
}

QStringList relativePaths(const QVector<RelatedFileCandidate> &candidates, const QString &root)
{
    QStringList out;
    const QDir rootDir(root);
    for (const RelatedFileCandidate &candidate : candidates) {
        out.push_back(rootDir.relativeFilePath(candidate.path));
    }
    return out;
}
}

class RelatedFilesResolverTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cppSourceFindsHeaderCMakeAndQtCompanions();
    void pythonImportsResolveLocalModules();
    void pythonRelativeImportsRespectDotDepth();
    void javascriptImportsResolveRelativeFilesAndSiblings();
    void qtJsonHeuristicIncludesMetadataAndSkipsUnrelatedJson();
    void rustModulesResolveCargoAndModuleFiles();
    void haskellImportsResolveLocalModulesAndPackageMetadata();
    void haskellSourceAndSpecFilesResolveEachOther();
    void haskellLiterateImportsAndSourceImportsResolveBootFiles();
    void haskellPrefersActiveSourceRootForDuplicateModules();
    void rejectsImportsOutsideProjectRootAndRemoteUris();
    void preferOpenDocumentsMarksOpenCandidates();
};

void RelatedFilesResolverTest::cppSourceFindsHeaderCMakeAndQtCompanions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString srcDir = dir.filePath(QStringLiteral("src"));
    writeText(srcDir + QStringLiteral("/Foo.cpp"), QStringLiteral("#include \"Foo.h\"\n"));
    writeText(srcDir + QStringLiteral("/Foo.h"), QStringLiteral("class Foo {};\n"));
    writeText(srcDir + QStringLiteral("/CMakeLists.txt"), QStringLiteral("add_library(foo Foo.cpp)\n"));
    writeText(srcDir + QStringLiteral("/Foo.ui"), QStringLiteral("<ui/>\n"));
    writeText(srcDir + QStringLiteral("/Foo.json"), QStringLiteral("{}\n"));
    writeText(srcDir + QStringLiteral("/moc_Foo.cpp"), QStringLiteral("generated\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = srcDir + QStringLiteral("/Foo.cpp");
    request.currentText = QStringLiteral("#include \"Foo.h\"\n");
    request.languageId = QStringLiteral("C++");
    request.projectRoot = dir.path();

    const QStringList names = paths(RelatedFilesResolver::resolve(request));

    QVERIFY(names.indexOf(QStringLiteral("Foo.h")) >= 0);
    QVERIFY(names.indexOf(QStringLiteral("CMakeLists.txt")) >= 0);
    QVERIFY(names.indexOf(QStringLiteral("Foo.ui")) >= 0);
    QVERIFY(names.indexOf(QStringLiteral("Foo.json")) >= 0);
    QVERIFY(!names.contains(QStringLiteral("moc_Foo.cpp")));
    QVERIFY(names.indexOf(QStringLiteral("Foo.h")) < names.indexOf(QStringLiteral("CMakeLists.txt")));
}

void RelatedFilesResolverTest::pythonImportsResolveLocalModules()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString pkgDir = dir.filePath(QStringLiteral("pkg"));
    writeText(pkgDir + QStringLiteral("/main.py"), QStringLiteral("import helper\nfrom .tools import run\n"));
    writeText(pkgDir + QStringLiteral("/helper.py"), QStringLiteral("def help(): pass\n"));
    writeText(pkgDir + QStringLiteral("/tools/__init__.py"), QStringLiteral("def run(): pass\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = pkgDir + QStringLiteral("/main.py");
    request.currentText = QStringLiteral("import helper\nfrom .tools import run\n");
    request.languageId = QStringLiteral("Python");
    request.projectRoot = dir.path();

    const QStringList names = paths(RelatedFilesResolver::resolve(request));

    QVERIFY(names.contains(QStringLiteral("helper.py")));
    QVERIFY(names.contains(QStringLiteral("__init__.py")));
}

void RelatedFilesResolverTest::pythonRelativeImportsRespectDotDepth()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString root = dir.filePath(QStringLiteral("repo"));
    writeText(root + QStringLiteral("/pkg/app/main.py"), QStringLiteral("from ..core.foo import Bar\nfrom ...shared.util import baz\n"));
    writeText(root + QStringLiteral("/pkg/core/foo.py"), QStringLiteral("class Bar: pass\n"));
    writeText(root + QStringLiteral("/shared/util.py"), QStringLiteral("def baz(): pass\n"));
    writeText(root + QStringLiteral("/pkg/app/core/foo.py"), QStringLiteral("class Wrong: pass\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = root + QStringLiteral("/pkg/app/main.py");
    request.currentText = QStringLiteral("from ..core.foo import Bar\nfrom ...shared.util import baz\n");
    request.languageId = QStringLiteral("Python");
    request.projectRoot = root;

    const QStringList rel = relativePaths(RelatedFilesResolver::resolve(request), root);

    QVERIFY(rel.contains(QStringLiteral("pkg/core/foo.py")));
    QVERIFY(rel.contains(QStringLiteral("shared/util.py")));
    QVERIFY(!rel.contains(QStringLiteral("pkg/app/core/foo.py")));
}

void RelatedFilesResolverTest::javascriptImportsResolveRelativeFilesAndSiblings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString srcDir = dir.filePath(QStringLiteral("src"));
    writeText(srcDir + QStringLiteral("/Button.tsx"), QStringLiteral("import theme from './theme';\nconst x = require('./util');\n"));
    writeText(srcDir + QStringLiteral("/theme.ts"), QStringLiteral("export default {};\n"));
    writeText(srcDir + QStringLiteral("/util.js"), QStringLiteral("module.exports = {};\n"));
    writeText(srcDir + QStringLiteral("/Button.css"), QStringLiteral(".button {}\n"));
    writeText(srcDir + QStringLiteral("/Button.test.tsx"), QStringLiteral("test('x', () => {});\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = srcDir + QStringLiteral("/Button.tsx");
    request.currentText = QStringLiteral("import theme from './theme';\nconst x = require('./util');\n");
    request.languageId = QStringLiteral("TypeScript");
    request.projectRoot = dir.path();

    const QStringList names = paths(RelatedFilesResolver::resolve(request));

    QVERIFY(names.contains(QStringLiteral("theme.ts")));
    QVERIFY(names.contains(QStringLiteral("util.js")));
    QVERIFY(names.contains(QStringLiteral("Button.css")));
    QVERIFY(names.contains(QStringLiteral("Button.test.tsx")));
}

void RelatedFilesResolverTest::qtJsonHeuristicIncludesMetadataAndSkipsUnrelatedJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString srcDir = dir.filePath(QStringLiteral("src"));
    writeText(srcDir + QStringLiteral("/Foo.cpp"), QStringLiteral("#include \"Foo.h\"\n"));
    writeText(srcDir + QStringLiteral("/Foo.h"), QStringLiteral("class Foo {};\n"));
    writeText(srcDir + QStringLiteral("/Foo.json"), QStringLiteral("{}\n"));
    writeText(srcDir + QStringLiteral("/metadata.json"), QStringLiteral("{\"KPlugin\": {\"Name\": \"Foo\"}}\n"));
    writeText(srcDir + QStringLiteral("/unrelated.json"), QStringLiteral("{\"theme\": \"dark\"}\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = srcDir + QStringLiteral("/Foo.cpp");
    request.currentText = QStringLiteral("#include \"Foo.h\"\n");
    request.languageId = QStringLiteral("C++");
    request.projectRoot = dir.path();

    const QStringList names = paths(RelatedFilesResolver::resolve(request));

    QVERIFY(names.contains(QStringLiteral("Foo.json")));
    QVERIFY(names.contains(QStringLiteral("metadata.json")));
    QVERIFY(!names.contains(QStringLiteral("unrelated.json")));
}

void RelatedFilesResolverTest::rustModulesResolveCargoAndModuleFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString srcDir = dir.filePath(QStringLiteral("src"));
    writeText(dir.filePath(QStringLiteral("Cargo.toml")), QStringLiteral("[package]\nname='demo'\n"));
    writeText(srcDir + QStringLiteral("/main.rs"), QStringLiteral("mod foo;\n"));
    writeText(srcDir + QStringLiteral("/foo.rs"), QStringLiteral("pub fn foo() {}\n"));
    writeText(srcDir + QStringLiteral("/mod.rs"), QStringLiteral("pub mod foo;\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = srcDir + QStringLiteral("/main.rs");
    request.currentText = QStringLiteral("mod foo;\n");
    request.languageId = QStringLiteral("Rust");
    request.projectRoot = dir.path();

    const QStringList names = paths(RelatedFilesResolver::resolve(request));

    QVERIFY(names.contains(QStringLiteral("Cargo.toml")));
    QVERIFY(names.contains(QStringLiteral("foo.rs")));
    QVERIFY(names.contains(QStringLiteral("mod.rs")));
}

void RelatedFilesResolverTest::haskellImportsResolveLocalModulesAndPackageMetadata()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString srcDir = dir.filePath(QStringLiteral("src"));
    writeText(dir.filePath(QStringLiteral("demo.cabal")), QStringLiteral("cabal-version: 3.0\nname: demo\n"));
    writeText(dir.filePath(QStringLiteral("package.yaml")), QStringLiteral("name: demo\n"));
    writeText(dir.filePath(QStringLiteral("stack.yaml")), QStringLiteral("resolver: lts-22.0\n"));
    writeText(dir.filePath(QStringLiteral("cabal.project")), QStringLiteral("packages: .\n"));
    writeText(dir.filePath(QStringLiteral("hie.yaml")), QStringLiteral("cradle:\n  cabal:\n"));
    writeText(srcDir + QStringLiteral("/Foo/Bar.hs"),
              QStringLiteral("module Foo.Bar where\nimport qualified Foo.Baz as Baz\nimport \"text\" Data.Text (Text)\nimport {-# SOURCE #-} Foo.Source\nimport Paths_demo\n"));
    writeText(srcDir + QStringLiteral("/Foo/Baz.hs"), QStringLiteral("module Foo.Baz where\n"));
    writeText(srcDir + QStringLiteral("/Foo/Source.lhs"), QStringLiteral("> module Foo.Source where\n"));
    writeText(srcDir + QStringLiteral("/Paths_demo.hs"), QStringLiteral("module Paths_demo where\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = srcDir + QStringLiteral("/Foo/Bar.hs");
    request.currentText = QStringLiteral("module Foo.Bar where\nimport qualified Foo.Baz as Baz\nimport \"text\" Data.Text (Text)\nimport {-# SOURCE #-} Foo.Source\nimport Paths_demo\n");
    request.languageId = QStringLiteral("Haskell");
    request.projectRoot = dir.path();

    const QVector<RelatedFileCandidate> candidates = RelatedFilesResolver::resolve(request);
    const QStringList names = paths(candidates);
    const QStringList rels = relativePaths(candidates, dir.path());

    QVERIFY(rels.contains(QStringLiteral("src/Foo/Baz.hs")));
    QVERIFY(rels.contains(QStringLiteral("src/Foo/Source.lhs")));
    QVERIFY(names.contains(QStringLiteral("demo.cabal")));
    QVERIFY(names.contains(QStringLiteral("package.yaml")));
    QVERIFY(names.contains(QStringLiteral("stack.yaml")));
    QVERIFY(names.contains(QStringLiteral("cabal.project")));
    QVERIFY(names.contains(QStringLiteral("hie.yaml")));
    QVERIFY(!rels.contains(QStringLiteral("src/Paths_demo.hs")));
    QVERIFY(rels.indexOf(QStringLiteral("src/Foo/Baz.hs")) < rels.indexOf(QStringLiteral("demo.cabal")));
}

void RelatedFilesResolverTest::haskellSourceAndSpecFilesResolveEachOther()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString srcDir = dir.filePath(QStringLiteral("src"));
    const QString testDir = dir.filePath(QStringLiteral("test"));
    writeText(srcDir + QStringLiteral("/Foo/Bar.hs"), QStringLiteral("module Foo.Bar where\n"));
    writeText(testDir + QStringLiteral("/Foo/BarSpec.hs"), QStringLiteral("module Foo.BarSpec where\nimport Foo.Bar\n"));
    writeText(testDir + QStringLiteral("/Foo/BarTest.hs"), QStringLiteral("module Foo.BarTest where\nimport Foo.Bar\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = srcDir + QStringLiteral("/Foo/Bar.hs");
    request.currentText = QStringLiteral("module Foo.Bar where\n");
    request.languageId = QStringLiteral("Haskell");
    request.projectRoot = dir.path();

    QStringList rels = relativePaths(RelatedFilesResolver::resolve(request), dir.path());
    QVERIFY(rels.contains(QStringLiteral("test/Foo/BarSpec.hs")));
    QVERIFY(rels.contains(QStringLiteral("test/Foo/BarTest.hs")));

    request.currentFilePath = testDir + QStringLiteral("/Foo/BarSpec.hs");
    request.currentText = QStringLiteral("module Foo.BarSpec where\nimport Foo.Bar\n");

    rels = relativePaths(RelatedFilesResolver::resolve(request), dir.path());
    QVERIFY(rels.contains(QStringLiteral("src/Foo/Bar.hs")));
}

void RelatedFilesResolverTest::haskellLiterateImportsAndSourceImportsResolveBootFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString srcDir = dir.filePath(QStringLiteral("src"));
    writeText(srcDir + QStringLiteral("/Foo/Doc.lhs"), QStringLiteral("> module Foo.Doc where\n> import Foo.Baz\n> import {-# SOURCE #-} Foo.Boot\n"));
    writeText(srcDir + QStringLiteral("/Foo/Baz.hs"), QStringLiteral("module Foo.Baz where\n"));
    writeText(srcDir + QStringLiteral("/Foo/Boot.hs-boot"), QStringLiteral("module Foo.Boot where\n"));
    writeText(srcDir + QStringLiteral("/Foo/Boot.hs"), QStringLiteral("module Foo.Boot where\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = srcDir + QStringLiteral("/Foo/Doc.lhs");
    request.currentText = QStringLiteral("> module Foo.Doc where\n> import Foo.Baz\n> import {-# SOURCE #-} Foo.Boot\n");
    request.languageId = QStringLiteral("Haskell");
    request.projectRoot = dir.path();

    QStringList rels = relativePaths(RelatedFilesResolver::resolve(request), dir.path());

    QVERIFY(rels.contains(QStringLiteral("src/Foo/Baz.hs")));
    QVERIFY(rels.contains(QStringLiteral("src/Foo/Boot.hs-boot")));
    QVERIFY(rels.indexOf(QStringLiteral("src/Foo/Boot.hs-boot")) < rels.indexOf(QStringLiteral("src/Foo/Boot.hs")));

    request.currentFilePath = srcDir + QStringLiteral("/Foo/Boot.hs-boot");
    request.currentText = QStringLiteral("module Foo.Boot where\nimport Foo.Baz\n");

    QVERIFY(relativePaths(RelatedFilesResolver::resolve(request), dir.path()).contains(QStringLiteral("src/Foo/Baz.hs")));
}

void RelatedFilesResolverTest::haskellPrefersActiveSourceRootForDuplicateModules()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString srcDir = dir.filePath(QStringLiteral("src"));
    const QString testDir = dir.filePath(QStringLiteral("test"));
    writeText(srcDir + QStringLiteral("/Foo/Bar.hs"), QStringLiteral("module Foo.Bar where\nimport Foo.Baz\n"));
    writeText(srcDir + QStringLiteral("/Foo/Baz.hs"), QStringLiteral("module Foo.Baz where\n"));
    writeText(testDir + QStringLiteral("/Foo/Baz.hs"), QStringLiteral("module Foo.Baz where\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = srcDir + QStringLiteral("/Foo/Bar.hs");
    request.currentText = QStringLiteral("module Foo.Bar where\nimport Foo.Baz\n");
    request.languageId = QStringLiteral("Haskell");
    request.projectRoot = dir.path();

    QStringList rels = relativePaths(RelatedFilesResolver::resolve(request), dir.path());

    QVERIFY(rels.contains(QStringLiteral("src/Foo/Baz.hs")));
    QVERIFY(rels.contains(QStringLiteral("test/Foo/Baz.hs")));
    QVERIFY(rels.indexOf(QStringLiteral("src/Foo/Baz.hs")) < rels.indexOf(QStringLiteral("test/Foo/Baz.hs")));

    writeText(testDir + QStringLiteral("/Foo/BarSpec.hs"), QStringLiteral("module Foo.BarSpec where\nimport Foo.Baz\n"));
    request.currentFilePath = testDir + QStringLiteral("/Foo/BarSpec.hs");
    request.currentText = QStringLiteral("module Foo.BarSpec where\nimport Foo.Baz\n");

    rels = relativePaths(RelatedFilesResolver::resolve(request), dir.path());

    QVERIFY(rels.contains(QStringLiteral("src/Foo/Baz.hs")));
    QVERIFY(rels.contains(QStringLiteral("test/Foo/Baz.hs")));
    QVERIFY(rels.indexOf(QStringLiteral("test/Foo/Baz.hs")) < rels.indexOf(QStringLiteral("src/Foo/Baz.hs")));
}

void RelatedFilesResolverTest::rejectsImportsOutsideProjectRootAndRemoteUris()
{
    QTemporaryDir parent;
    QVERIFY(parent.isValid());

    const QString root = parent.filePath(QStringLiteral("project"));
    QVERIFY(QDir().mkpath(root + QStringLiteral("/.git")));
    QVERIFY(QDir().mkpath(root + QStringLiteral("/src")));

    const QString source = root + QStringLiteral("/src/App.ts");
    const QString outside = parent.filePath(QStringLiteral("outside.ts"));
    writeText(source, QStringLiteral("import outside from '../../outside';\n"));
    writeText(outside, QStringLiteral("export const secret = 1;\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = source;
    request.currentText = QStringLiteral("import outside from '../../outside';\n");
    request.languageId = QStringLiteral("TypeScript");
    request.projectRoot = root;

    const QStringList names = paths(RelatedFilesResolver::resolve(request));
    QVERIFY(!names.contains(QStringLiteral("outside.ts")));

    request.currentFilePath = QStringLiteral("https://example.invalid/App.ts");
    QVERIFY(RelatedFilesResolver::resolve(request).isEmpty());
}

void RelatedFilesResolverTest::preferOpenDocumentsMarksOpenCandidates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString source = dir.filePath(QStringLiteral("Foo.cpp"));
    const QString header = dir.filePath(QStringLiteral("Foo.h"));
    writeText(source, QStringLiteral("#include \"Foo.h\"\n"));

    RelatedFilesResolveRequest request;
    request.currentFilePath = source;
    request.currentText = QStringLiteral("#include \"Foo.h\"\n");
    request.languageId = QStringLiteral("C++");
    request.projectRoot = dir.path();
    request.preferOpenTabs = true;
    request.openDocuments.insert(QFileInfo(header).absoluteFilePath(), QStringLiteral("class FooFromOpenDoc {};\n"));

    const QVector<RelatedFileCandidate> candidates = RelatedFilesResolver::resolve(request);

    QVERIFY(!candidates.isEmpty());
    QCOMPARE(QFileInfo(candidates.constFirst().path).fileName(), QStringLiteral("Foo.h"));
    QVERIFY(candidates.constFirst().fromOpenDocument);
}

QTEST_MAIN(RelatedFilesResolverTest)

#include "RelatedFilesResolverTest.moc"
