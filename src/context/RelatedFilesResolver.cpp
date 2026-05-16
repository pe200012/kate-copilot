/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: RelatedFilesResolver
*/

#include "context/RelatedFilesResolver.h"

#include "context/ContextFileFilter.h"
#include "context/ProjectContextResolver.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QtGlobal>

#include <algorithm>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] QString lowerSuffix(const QFileInfo &info)
{
    return info.suffix().toLower();
}

[[nodiscard]] QString baseNameForCompanions(const QFileInfo &info)
{
    QString fileName = info.fileName();
    const int firstDot = fileName.indexOf(QLatin1Char('.'));
    if (firstDot > 0) {
        return fileName.left(firstDot);
    }
    return info.completeBaseName();
}

[[nodiscard]] QHash<QString, QString> normalizedOpenDocuments(const QHash<QString, QString> &docs)
{
    QHash<QString, QString> out;
    for (auto it = docs.constBegin(); it != docs.constEnd(); ++it) {
        const QString path = ProjectContextResolver::localPathFromUri(it.key());
        if (!path.isEmpty()) {
            out.insert(path, it.value());
        }
    }
    return out;
}

struct CandidateAccumulator {
    QFileInfo current;
    ContextFileFilterOptions filterOptions;
    QHash<QString, QString> openDocs;
    QString projectRoot;
    QSet<QString> seen;
    QVector<RelatedFileCandidate> candidates;
    bool preferOpenTabs = true;

    void add(const QString &path, int score)
    {
        const QString clean = ProjectContextResolver::localPathFromUri(path);
        if (clean.isEmpty() || clean == current.absoluteFilePath() || seen.contains(clean)) {
            return;
        }

        if (!ProjectContextResolver::isWithinRoot(clean, projectRoot)) {
            return;
        }

        const bool open = openDocs.contains(clean);
        if (open) {
            if (!ContextFileFilter::isAllowedPath(clean, filterOptions)) {
                return;
            }
        } else if (!ContextFileFilter::isAllowedFile(clean, filterOptions)) {
            return;
        }

        seen.insert(clean);
        candidates.push_back(RelatedFileCandidate{clean, score + ((preferOpenTabs && open) ? 100 : 0), open});
    }
};

void addSameBasenameFiles(CandidateAccumulator *acc, const QStringList &extensions, int score)
{
    if (!acc) {
        return;
    }

    const QDir dir = acc->current.absoluteDir();
    const QString base = baseNameForCompanions(acc->current);
    for (const QString &ext : extensions) {
        acc->add(dir.filePath(base + QLatin1Char('.') + ext), score);
    }
}

void addNearbyCMake(CandidateAccumulator *acc, const QString &projectRoot)
{
    if (!acc) {
        return;
    }

    QDir dir = acc->current.absoluteDir();
    const QString root = projectRoot.isEmpty() ? QString() : QFileInfo(projectRoot).absoluteFilePath();
    while (true) {
        acc->add(dir.filePath(QStringLiteral("CMakeLists.txt")), 82);
        if (!root.isEmpty() && dir.absolutePath() == root) {
            break;
        }
        if (!dir.cdUp()) {
            break;
        }
    }
}

void addQtCompanions(CandidateAccumulator *acc)
{
    if (!acc) {
        return;
    }

    const QDir dir = acc->current.absoluteDir();
    const QString base = baseNameForCompanions(acc->current);
    acc->add(dir.filePath(base + QStringLiteral(".ui")), 80);
    acc->add(dir.filePath(base + QStringLiteral(".qrc")), 79);
    acc->add(dir.filePath(base + QStringLiteral(".json")), 78);

    const QStringList jsonFiles = dir.entryList(QStringList{QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &file : jsonFiles) {
        acc->add(dir.filePath(file), 65);
    }
}

void addCppCandidates(CandidateAccumulator *acc, const QString &projectRoot)
{
    const QString suffix = lowerSuffix(acc->current);
    static const QStringList sourceExts = {QStringLiteral("cpp"), QStringLiteral("cc"), QStringLiteral("cxx"), QStringLiteral("c")};
    static const QStringList headerExts = {QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("hh"), QStringLiteral("hxx")};

    if (sourceExts.contains(suffix)) {
        addSameBasenameFiles(acc, headerExts, 95);
    } else if (headerExts.contains(suffix)) {
        addSameBasenameFiles(acc, sourceExts, 95);
    }

    addNearbyCMake(acc, projectRoot);
    addQtCompanions(acc);
}

void addPythonModule(CandidateAccumulator *acc, const QString &module)
{
    QString clean = module.trimmed();
    if (clean.isEmpty()) {
        return;
    }

    QDir baseDir = acc->current.absoluteDir();
    while (clean.startsWith(QLatin1Char('.'))) {
        clean.remove(0, 1);
    }
    clean.replace(QLatin1Char('.'), QLatin1Char('/'));
    if (clean.isEmpty()) {
        return;
    }

    acc->add(baseDir.filePath(clean + QStringLiteral(".py")), 92);
    acc->add(baseDir.filePath(clean + QStringLiteral("/__init__.py")), 91);
}

void addPythonCandidates(CandidateAccumulator *acc, const QString &text)
{
    static const QRegularExpression importRe(QStringLiteral("^\\s*import\\s+(.+)$"));
    static const QRegularExpression fromRe(QStringLiteral("^\\s*from\\s+([.A-Za-z_][A-Za-z0-9_.]*)\\s+import\\s+"));

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QRegularExpressionMatch importMatch = importRe.match(line);
        if (importMatch.hasMatch()) {
            const QStringList modules = importMatch.captured(1).split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (QString module : modules) {
                module = module.trimmed().section(QLatin1Char(' '), 0, 0);
                addPythonModule(acc, module);
            }
        }

        const QRegularExpressionMatch fromMatch = fromRe.match(line);
        if (fromMatch.hasMatch()) {
            addPythonModule(acc, fromMatch.captured(1));
        }
    }
}

void addJsResolvedBase(CandidateAccumulator *acc, const QString &specifier, int score)
{
    if (!specifier.startsWith(QStringLiteral("./")) && !specifier.startsWith(QStringLiteral("../"))) {
        return;
    }

    const QDir dir = acc->current.absoluteDir();
    const QString base = dir.filePath(specifier);
    static const QStringList exts = {
        QStringLiteral("ts"), QStringLiteral("tsx"), QStringLiteral("js"), QStringLiteral("jsx"), QStringLiteral("mjs"), QStringLiteral("cjs"),
    };

    acc->add(base, score);
    for (const QString &ext : exts) {
        acc->add(base + QLatin1Char('.') + ext, score);
        acc->add(base + QStringLiteral("/index.") + ext, score - 1);
    }
}

void addJsCandidates(CandidateAccumulator *acc, const QString &text)
{
    static const QRegularExpression importRe(QStringLiteral("\\bfrom\\s+[\"'](\\.{1,2}/[^\"']+)[\"']"));
    static const QRegularExpression sideEffectImportRe(QStringLiteral("^\\s*import\\s+[\"'](\\.{1,2}/[^\"']+)[\"']"));
    static const QRegularExpression requireRe(QStringLiteral("\\brequire\\s*\\(\\s*[\"'](\\.{1,2}/[^\"']+)[\"']\\s*\\)"));

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        for (const QRegularExpression &re : {importRe, sideEffectImportRe, requireRe}) {
            const QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch()) {
                addJsResolvedBase(acc, match.captured(1), 92);
            }
        }
    }

    const QDir dir = acc->current.absoluteDir();
    const QString base = baseNameForCompanions(acc->current);
    acc->add(dir.filePath(base + QStringLiteral(".css")), 75);
    acc->add(dir.filePath(base + QStringLiteral(".scss")), 75);

    const QStringList siblingFiles = dir.entryList(QDir::Files, QDir::Name);
    for (const QString &file : siblingFiles) {
        if (file.startsWith(base + QStringLiteral(".test.")) || file.startsWith(base + QStringLiteral(".spec."))) {
            acc->add(dir.filePath(file), 74);
        }
    }
}

void addRustCandidates(CandidateAccumulator *acc, const QString &text, const QString &projectRoot)
{
    if (!projectRoot.isEmpty()) {
        acc->add(QDir(projectRoot).filePath(QStringLiteral("Cargo.toml")), 92);
    }
    acc->add(acc->current.absoluteDir().filePath(QStringLiteral("mod.rs")), 82);

    static const QRegularExpression modRe(QStringLiteral("^\\s*mod\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*;"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = modRe.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        const QString module = match.captured(1);
        acc->add(acc->current.absoluteDir().filePath(module + QStringLiteral(".rs")), 91);
        acc->add(acc->current.absoluteDir().filePath(module + QStringLiteral("/mod.rs")), 90);
    }
}

struct HaskellSourceRoot {
    QString path;
    int scoreOffset = 0;
};

[[nodiscard]] QVector<HaskellSourceRoot> uniqueExistingDirectories(const QVector<HaskellSourceRoot> &paths)
{
    QVector<HaskellSourceRoot> out;
    QSet<QString> seen;
    for (const HaskellSourceRoot &root : paths) {
        const QString clean = ProjectContextResolver::localPathFromUri(root.path);
        if (clean.isEmpty() || seen.contains(clean) || !QFileInfo(clean).isDir()) {
            continue;
        }
        seen.insert(clean);
        out.push_back(HaskellSourceRoot{clean, root.scoreOffset});
    }
    return out;
}

[[nodiscard]] QString haskellRootNameForRelativePath(const QString &relativePath)
{
    static const QStringList rootNames = {
        QStringLiteral("src"), QStringLiteral("app"), QStringLiteral("test"), QStringLiteral("tests"), QStringLiteral("lib"), QStringLiteral("library"),
    };

    for (const QString &rootName : rootNames) {
        if (relativePath == rootName || relativePath.startsWith(rootName + QLatin1Char('/'))) {
            return rootName;
        }
    }

    return {};
}

[[nodiscard]] int haskellRootScoreOffset(const QString &rootName, const QString &activeRootName)
{
    if (rootName == activeRootName) {
        return 8;
    }

    if (activeRootName == QStringLiteral("test") || activeRootName == QStringLiteral("tests")) {
        if (rootName == QStringLiteral("test") || rootName == QStringLiteral("tests")) {
            return 6;
        }
        return 0;
    }

    if (activeRootName == QStringLiteral("src") || activeRootName == QStringLiteral("lib") || activeRootName == QStringLiteral("library")) {
        if (rootName == QStringLiteral("src") || rootName == QStringLiteral("lib") || rootName == QStringLiteral("library")) {
            return 6;
        }
        return -4;
    }

    return 0;
}

[[nodiscard]] QVector<HaskellSourceRoot> haskellSourceRoots(const CandidateAccumulator &acc, const QString &projectRoot)
{
    QVector<HaskellSourceRoot> roots;

    if (!projectRoot.isEmpty()) {
        const QDir root(projectRoot);
        const QString activeRoot = haskellRootNameForRelativePath(root.relativeFilePath(acc.current.absoluteFilePath()));
        for (const QString &rootName : {QStringLiteral("src"), QStringLiteral("app"), QStringLiteral("test"), QStringLiteral("tests"), QStringLiteral("lib"), QStringLiteral("library")}) {
            roots.push_back(HaskellSourceRoot{root.filePath(rootName), haskellRootScoreOffset(rootName, activeRoot)});
        }
        roots.push_back(HaskellSourceRoot{root.absolutePath(), -8});
    }

    roots.push_back(HaskellSourceRoot{acc.current.absolutePath(), -10});

    return uniqueExistingDirectories(roots);
}

[[nodiscard]] QString stripHaskellExtension(QString path)
{
    if (path.endsWith(QStringLiteral(".lhs-boot"), Qt::CaseInsensitive)) {
        path.chop(9);
    } else if (path.endsWith(QStringLiteral(".hs-boot"), Qt::CaseInsensitive)) {
        path.chop(8);
    } else if (path.endsWith(QStringLiteral(".lhs"), Qt::CaseInsensitive)) {
        path.chop(4);
    } else if (path.endsWith(QStringLiteral(".hs"), Qt::CaseInsensitive)) {
        path.chop(3);
    }
    return path;
}

[[nodiscard]] QString stripHaskellTestSuffix(QString path)
{
    if (path.endsWith(QStringLiteral("Spec"))) {
        path.chop(4);
    } else if (path.endsWith(QStringLiteral("Test"))) {
        path.chop(4);
    }
    return path;
}

void addHaskellModule(CandidateAccumulator *acc, const QString &module, const QVector<HaskellSourceRoot> &sourceRoots, int score, bool sourceImport)
{
    QString clean = module.trimmed();
    if (clean.isEmpty() || clean.startsWith(QStringLiteral("Paths_"))) {
        return;
    }

    clean.replace(QLatin1Char('.'), QLatin1Char('/'));
    for (const HaskellSourceRoot &root : sourceRoots) {
        const QDir dir(root.path);
        const int rootScore = score + root.scoreOffset;
        if (sourceImport) {
            acc->add(dir.filePath(clean + QStringLiteral(".hs-boot")), rootScore + 3);
            acc->add(dir.filePath(clean + QStringLiteral(".lhs-boot")), rootScore + 2);
        }
        acc->add(dir.filePath(clean + QStringLiteral(".hs")), rootScore);
        acc->add(dir.filePath(clean + QStringLiteral(".lhs")), rootScore - 1);
    }
}

void addHaskellImports(CandidateAccumulator *acc, const QString &text, const QVector<HaskellSourceRoot> &sourceRoots)
{
    static const QRegularExpression importRe(QStringLiteral(
        "^\\s*>?\\s*import\\s+((?:safe\\s+|unsafe\\s+|qualified\\s+|\"[^\"]+\"\\s+|\\{-#\\s*SOURCE\\s*#-\\}\\s+)*)"
        "([A-Z][A-Za-z0-9_']*(?:\\.[A-Z][A-Za-z0-9_']*)*)"));

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = importRe.match(line);
        if (match.hasMatch()) {
            const bool sourceImport = match.captured(1).contains(QStringLiteral("SOURCE"), Qt::CaseInsensitive);
            addHaskellModule(acc, match.captured(2), sourceRoots, 94, sourceImport);
        }
    }
}

void addHaskellMetadata(CandidateAccumulator *acc, const QString &projectRoot)
{
    if (projectRoot.isEmpty()) {
        return;
    }

    const QDir root(projectRoot);
    const QStringList cabalFiles = root.entryList(QStringList{QStringLiteral("*.cabal")}, QDir::Files, QDir::Name);
    for (const QString &file : cabalFiles) {
        acc->add(root.filePath(file), 88);
    }

    acc->add(root.filePath(QStringLiteral("package.yaml")), 87);
    acc->add(root.filePath(QStringLiteral("cabal.project")), 86);
    acc->add(root.filePath(QStringLiteral("stack.yaml")), 86);
    acc->add(root.filePath(QStringLiteral("hie.yaml")), 84);
}

void addHaskellTestCompanions(CandidateAccumulator *acc, const QString &projectRoot)
{
    if (projectRoot.isEmpty()) {
        return;
    }

    const QDir root(projectRoot);
    const QString rel = root.relativeFilePath(acc->current.absoluteFilePath());
    const QString stem = stripHaskellExtension(rel);
    static const QStringList extensions = {QStringLiteral(".hs"), QStringLiteral(".lhs")};

    if (stem.startsWith(QStringLiteral("src/"))) {
        const QString modulePath = stem.mid(QStringLiteral("src/").size());
        for (const QString &testRoot : {QStringLiteral("test"), QStringLiteral("tests")}) {
            for (const QString &suffix : {QStringLiteral("Spec"), QStringLiteral("Test")}) {
                for (const QString &ext : extensions) {
                    acc->add(root.filePath(testRoot + QLatin1Char('/') + modulePath + suffix + ext), 83);
                }
            }
        }
        return;
    }

    for (const QString &testRoot : {QStringLiteral("test/"), QStringLiteral("tests/")}) {
        if (!stem.startsWith(testRoot)) {
            continue;
        }

        const QString modulePath = stripHaskellTestSuffix(stem.mid(testRoot.size()));
        for (const QString &sourceRoot : {QStringLiteral("src"), QStringLiteral("lib"), QStringLiteral("library")}) {
            for (const QString &ext : extensions) {
                acc->add(root.filePath(sourceRoot + QLatin1Char('/') + modulePath + ext), 93);
            }
        }
    }
}

void addHaskellCandidates(CandidateAccumulator *acc, const QString &text, const QString &projectRoot)
{
    const QVector<HaskellSourceRoot> sourceRoots = haskellSourceRoots(*acc, projectRoot);
    addHaskellImports(acc, text, sourceRoots);
    addHaskellMetadata(acc, projectRoot);
    addHaskellTestCompanions(acc, projectRoot);
}

void addGenericCompanions(CandidateAccumulator *acc)
{
    static const QStringList exts = {
        QStringLiteral("h"),   QStringLiteral("hpp"), QStringLiteral("hh"),  QStringLiteral("cpp"), QStringLiteral("cc"),
        QStringLiteral("cxx"), QStringLiteral("c"),   QStringLiteral("py"),  QStringLiteral("ts"),  QStringLiteral("tsx"),
        QStringLiteral("js"),  QStringLiteral("jsx"), QStringLiteral("css"), QStringLiteral("scss"), QStringLiteral("json"),
        QStringLiteral("ui"),  QStringLiteral("qrc"), QStringLiteral("rs"),  QStringLiteral("toml"), QStringLiteral("hs"),
        QStringLiteral("lhs"),
    };
    addSameBasenameFiles(acc, exts, 50);
}

[[nodiscard]] bool candidateLess(const RelatedFileCandidate &a, const RelatedFileCandidate &b)
{
    if (a.score != b.score) {
        return a.score > b.score;
    }
    if (a.fromOpenDocument != b.fromOpenDocument) {
        return a.fromOpenDocument;
    }
    const qint64 aSize = QFileInfo(a.path).exists() ? QFileInfo(a.path).size() : 0;
    const qint64 bSize = QFileInfo(b.path).exists() ? QFileInfo(b.path).size() : 0;
    if (aSize != bSize) {
        return aSize < bSize;
    }
    return a.path < b.path;
}
} // namespace

QString RelatedFilesResolver::findProjectRoot(const QString &path)
{
    return ProjectContextResolver::findProjectRoot(path);
}

QVector<RelatedFileCandidate> RelatedFilesResolver::resolve(const RelatedFilesResolveRequest &request)
{
    const QString currentPath = ProjectContextResolver::localPathFromUri(request.currentFilePath);
    if (currentPath.isEmpty()) {
        return {};
    }

    ContextFileFilterOptions filterOptions;
    filterOptions.maxFileChars = request.maxCharsPerFile;
    filterOptions.excludePatterns = request.excludePatterns;

    CandidateAccumulator acc;
    acc.current = QFileInfo(currentPath);
    acc.filterOptions = filterOptions;
    acc.openDocs = normalizedOpenDocuments(request.openDocuments);
    acc.preferOpenTabs = request.preferOpenTabs;

    QString projectRoot = request.projectRoot.trimmed().isEmpty() ? findProjectRoot(currentPath) : ProjectContextResolver::localPathFromUri(request.projectRoot);
    if (projectRoot.isEmpty()) {
        projectRoot = acc.current.absolutePath();
    }
    acc.projectRoot = projectRoot;
    const QString suffix = lowerSuffix(acc.current);
    const QString language = request.languageId.toLower();

    const bool cppLikeLanguage = language.contains(QStringLiteral("c++")) || language == QStringLiteral("c") || language.contains(QStringLiteral("cpp"))
        || language.contains(QStringLiteral("qt")) || language.contains(QStringLiteral("kde"));
    if (cppLikeLanguage
        || QStringList{QStringLiteral("cpp"), QStringLiteral("cc"), QStringLiteral("cxx"), QStringLiteral("c"), QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("hh"), QStringLiteral("hxx")}.contains(suffix)) {
        addCppCandidates(&acc, projectRoot);
    }

    if (language.contains(QStringLiteral("python")) || suffix == QStringLiteral("py")) {
        addPythonCandidates(&acc, request.currentText);
    }

    if (language.contains(QStringLiteral("typescript")) || language.contains(QStringLiteral("javascript")) || QStringList{QStringLiteral("ts"), QStringLiteral("tsx"), QStringLiteral("js"), QStringLiteral("jsx"), QStringLiteral("mjs"), QStringLiteral("cjs")}.contains(suffix)) {
        addJsCandidates(&acc, request.currentText);
    }

    if (language.contains(QStringLiteral("rust")) || suffix == QStringLiteral("rs")) {
        addRustCandidates(&acc, request.currentText, projectRoot);
    }

    if (language.contains(QStringLiteral("haskell")) || suffix == QStringLiteral("hs") || suffix == QStringLiteral("lhs")
        || suffix == QStringLiteral("hs-boot") || suffix == QStringLiteral("lhs-boot")) {
        addHaskellCandidates(&acc, request.currentText, projectRoot);
    }

    addGenericCompanions(&acc);

    std::stable_sort(acc.candidates.begin(), acc.candidates.end(), candidateLess);
    return acc.candidates;
}

} // namespace KateAiInlineCompletion
