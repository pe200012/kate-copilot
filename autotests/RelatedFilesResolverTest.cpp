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
}

class RelatedFilesResolverTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cppSourceFindsHeaderCMakeAndQtCompanions();
    void pythonImportsResolveLocalModules();
    void javascriptImportsResolveRelativeFilesAndSiblings();
    void rustModulesResolveCargoAndModuleFiles();
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
