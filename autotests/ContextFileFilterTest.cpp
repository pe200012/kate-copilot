/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ContextFileFilterTest
*/

#include "context/ContextFileFilter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using KateAiInlineCompletion::ContextFileFilter;
using KateAiInlineCompletion::ContextFileFilterOptions;

namespace
{
void writeFile(const QString &path, const QByteArray &bytes)
{
    QFileInfo info(path);
    QVERIFY(QDir().mkpath(info.absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
}
}

class ContextFileFilterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsExcludedDirectoriesAndGeneratedFiles();
    void rejectsPrivateBinaryAndLargeFiles();
    void allowsSourceNamesContainingBroadSecurityWords();
    void rejectsCommonSecretFilesAndDirectories();
    void appliesUserExcludePatterns();
    void readsBoundedUtf8Text();
};

void ContextFileFilterTest::rejectsExcludedDirectoriesAndGeneratedFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("node_modules/pkg")));
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("src")));

    ContextFileFilterOptions options;
    options.maxFileChars = 4000;

    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("node_modules/pkg/index.ts")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("src/moc_widget.cpp")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("src/ui_widget.h")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("src/app.min.js")), options));
    QVERIFY(ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("src/widget.cpp")), options));
}

void ContextFileFilterTest::rejectsPrivateBinaryAndLargeFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString secret = dir.filePath(QStringLiteral(".env.local"));
    const QString binary = dir.filePath(QStringLiteral("image.png"));
    const QString large = dir.filePath(QStringLiteral("large.cpp"));

    writeFile(secret, QByteArray("token=abc"));
    writeFile(binary, QByteArray("\x89PNG\0\1", 6));
    writeFile(large, QByteArray(32, 'x'));

    ContextFileFilterOptions options;
    options.maxFileChars = 16;

    QVERIFY(!ContextFileFilter::isAllowedFile(secret, options));
    QVERIFY(!ContextFileFilter::isAllowedFile(binary, options));
    QVERIFY(!ContextFileFilter::isAllowedFile(large, options));
}

void ContextFileFilterTest::allowsSourceNamesContainingBroadSecurityWords()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ContextFileFilterOptions options;
    options.maxFileChars = 4000;

    QVERIFY(ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("src/tokenizer.cpp")), options));
    QVERIFY(ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("src/private_api.cpp")), options));
    QVERIFY(ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("tests/password_validator_test.cpp")), options));
}

void ContextFileFilterTest::rejectsCommonSecretFilesAndDirectories()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ContextFileFilterOptions options;
    options.maxFileChars = 4000;

    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral(".env.production")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral(".envrc")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("secrets/api.txt")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("credentials/prod.json")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral(".ssh/id_ed25519")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("certs/prod.pem")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("certs/prod.key")), options));
}

void ContextFileFilterTest::appliesUserExcludePatterns()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ContextFileFilterOptions options;
    options.excludePatterns = {QStringLiteral("*.generated.h"), QStringLiteral("third_party/*")};

    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("foo.generated.h")), options));
    QVERIFY(!ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("third_party/lib.h")), options));
    QVERIFY(ContextFileFilter::isAllowedPath(dir.filePath(QStringLiteral("src/lib.h")), options));
}

void ContextFileFilterTest::readsBoundedUtf8Text()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath(QStringLiteral("foo.cpp"));
    writeFile(filePath, QByteArray("class Foo {};\n"));

    ContextFileFilterOptions options;
    options.maxFileChars = 100;

    bool ok = false;
    const QString text = ContextFileFilter::readTextFile(filePath, options, &ok);

    QVERIFY(ok);
    QCOMPARE(text, QStringLiteral("class Foo {};\n"));
}

QTEST_MAIN(ContextFileFilterTest)

#include "ContextFileFilterTest.moc"
