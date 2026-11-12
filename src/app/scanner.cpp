// SPDX-License-Identifier: GPL-3.0-or-later

#include "scanner.h"
#include "fileinfo.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
QString run(const QString &program, const QStringList &args, int *exitCode = nullptr)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(60000)) {
        p.kill();
        if (exitCode)
            *exitCode = -1;
        return {};
    }
    if (exitCode)
        *exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

QByteArray runBytes(const QString &program, const QStringList &args, bool *ok)
{
    QProcess p;
    p.start(program, args);
    *ok = p.waitForFinished(60000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    return *ok ? p.readAllStandardOutput() : QByteArray();
}

// Returns <0, 0, >0 like pacman's vercmp.
int vercmp(const QString &a, const QString &b)
{
    return run(u"vercmp"_s, {a, b}).toInt();
}

QString decode(const QByteArray &data)
{
    return QString::fromUtf8(data);
}

struct CachedPackage {
    QString version;
    QString file;
};

QStringList cacheDirs()
{
    QStringList dirs = run(u"pacman-conf"_s, {u"CacheDir"_s}).split(u'\n', Qt::SkipEmptyParts);
    if (dirs.isEmpty())
        dirs << u"/var/cache/pacman/pkg/"_s;
    return dirs;
}

QList<CachedPackage> cachedVersions(const QString &package, const QStringList &dirs)
{
    // <name>-<pkgver>-<pkgrel>-<arch>.pkg.tar.<ext>
    const QRegularExpression re(u"^"_s + QRegularExpression::escape(package) + u"-([^-]+-[^-]+)-([^-]+)\\.pkg\\.tar\\.(zst|xz|gz|bz2)$"_s);
    QList<CachedPackage> out;
    for (const QString &dir : dirs) {
        const QStringList files = QDir(dir).entryList({package + u"-*.pkg.tar.*"_s}, QDir::Files);
        for (const QString &file : files) {
            const auto m = re.match(file);
            if (m.hasMatch())
                out.append({m.captured(1), QDir(dir).filePath(file)});
        }
    }
    return out;
}

qsizetype similarity(const QStringList &a, const QStringList &b)
{
    qsizetype same = 0;
    for (const auto &blk : Merge::matchingBlocks(a, b))
        same += blk.size;
    return same;
}

// The default the user's file was most likely created from: among cached
// versions older than the installed one, the one closest to the live file.
void findBase(Entry &e, const QStringList &dirs)
{
    if (e.package.isEmpty() || e.installedVersion.isEmpty() || !e.liveExists)
        return;
    QList<CachedPackage> older;
    for (const CachedPackage &c : cachedVersions(e.package, dirs)) {
        if (vercmp(c.version, e.installedVersion) < 0)
            older.append(c);
    }
    std::sort(older.begin(), older.end(), [](const CachedPackage &a, const CachedPackage &b) {
        return vercmp(a.version, b.version) > 0; // newest first
    });
    if (older.size() > 4)
        older.resize(4);

    const QString member = e.path.mid(1); // tar paths have no leading '/'
    qsizetype best = -1;
    for (const CachedPackage &c : std::as_const(older)) {
        bool ok = false;
        const QByteArray data = runBytes(u"bsdtar"_s, {u"-xOf"_s, c.file, member}, &ok);
        if (!ok)
            continue;
        const QStringList lines = Merge::splitLines(decode(data));
        const qsizetype score = similarity(lines, e.live);
        if (score > best) {
            best = score;
            e.base = lines;
            e.baseVersion = c.version;
            e.hasBase = true;
        }
    }
}

bool readFile(const QString &file, QByteArray *out)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    *out = f.readAll();
    return true;
}
} // namespace

Entry::Status Entry::status() const
{
    if (kind == u"pacsave")
        return Status::Leftover;
    if (!liveExists)
        return Status::Orphaned;
    if (liveIsSymlink || !generatedBy.isEmpty())
        return Status::Managed;
    if (!readable)
        return Status::Protected;
    if (identical)
        return Status::Identical;
    if (FileInfo::lookup(path).policy == FileInfo::Policy::KeepOnly)
        return Status::KeepOnly;
    if (conflicts > 0 || unknown > 0)
        return Status::NeedsReview;
    return Status::Automatic;
}

bool Entry::canSimplyKeep() const
{
    switch (status()) {
    case Status::Identical:
    case Status::Managed:
        return true;
    case Status::Automatic:
        return upstream == 0;
    default:
        return false;
    }
}

QList<Merge::Region> Entry::regions() const
{
    return hasBase ? Merge::threeWay(base, live, theirs) : Merge::twoWay(live, theirs);
}

void Entry::analyze()
{
    upstream = local = conflicts = unknown = 0;
    identical = readable && liveExists && live == theirs;
    if (!readable || !liveExists)
        return;
    for (const Merge::Region &r : regions()) {
        switch (r.kind) {
        case Merge::Kind::Upstream:
            ++upstream;
            break;
        case Merge::Kind::Local:
            ++local;
            break;
        case Merge::Kind::Conflict:
            ++conflicts;
            break;
        case Merge::Kind::Unknown:
            ++unknown;
            break;
        case Merge::Kind::Unchanged:
            break;
        }
    }
}

namespace Scanner
{

void setContents(Entry &e, const QByteArray &live, const QByteArray &sidecar, bool liveExists)
{
    e.readable = true;
    e.liveExists = liveExists;
    e.live = Merge::splitLines(decode(live), &e.liveTrailingNewline);
    e.theirs = Merge::splitLines(decode(sidecar), &e.newTrailingNewline);

    // Files written by tools announce it in a comment near the top. Only
    // trust markers that the new default doesn't contain itself.
    static const QRegularExpression marker(u"generated by|managed by|do not edit|rate-mirrors|reflector"_s,
                                           QRegularExpression::CaseInsensitiveOption);
    e.generatedBy.clear();
    for (const QString &line : e.live.mid(0, 15)) {
        if (Merge::isCommentLine(line) && marker.match(line).hasMatch() && !e.theirs.contains(line)) {
            e.generatedBy = line.trimmed();
            break;
        }
    }
    e.analyze();
}

QList<Entry> scan(const QString &root)
{
    const bool demo = QDir(root).canonicalPath() != u"/";
    QString prefix = root;
    while (prefix.endsWith(u'/'))
        prefix.chop(1);

    QList<Entry> entries;
    QDirIterator it(prefix + u"/etc"_s, {u"*.pacnew"_s, u"*.pacsave"_s}, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo fi = it.nextFileInfo();
        if (fi.isSymLink())
            continue;
        Entry e;
        e.kind = fi.suffix();
        const QString sidecarOnDisk = fi.absoluteFilePath();
        const QString liveOnDisk = sidecarOnDisk.chopped(e.kind.size() + 1);
        e.path = liveOnDisk.mid(prefix.size());
        e.modified = fi.lastModified();

        const QFileInfo live(liveOnDisk);
        e.liveIsSymlink = live.isSymLink();
        e.liveExists = live.exists() || e.liveIsSymlink;

        QByteArray liveData;
        QByteArray sideData;
        const bool liveOk = !e.liveExists || readFile(liveOnDisk, &liveData);
        const bool sideOk = readFile(sidecarOnDisk, &sideData);
        if (liveOk && sideOk)
            setContents(e, liveData, sideData, e.liveExists);
        else
            e.readable = false;
        entries.append(e);
    }

    if (!demo) {
        const QStringList dirs = cacheDirs();
        for (Entry &e : entries) {
            // .pacsave files belong to removed packages; .pacnew to installed ones.
            int code = 0;
            const QString owner = run(u"pacman"_s, {u"-Qqo"_s, e.liveExists ? e.path : e.path + u'.' + e.kind}, &code);
            if (code == 0 && !owner.isEmpty()) {
                e.package = owner.section(u'\n', 0, 0);
                e.installedVersion = run(u"pacman"_s, {u"-Q"_s, e.package}).section(u' ', 1, 1);
            }
            if (e.kind == u"pacnew" && e.readable && !e.identical) {
                findBase(e, dirs);
                e.analyze();
            }
        }
    } else {
        // Demo mode: an optional fixture for the previous default.
        for (Entry &e : entries) {
            QByteArray data;
            if (e.readable && !e.identical && readFile(prefix + u"/base"_s + e.path, &data)) {
                e.base = Merge::splitLines(decode(data));
                e.hasBase = true;
                e.baseVersion = u"demo"_s;
                e.analyze();
            }
        }
    }

    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.path < b.path;
    });
    return entries;
}

} // namespace Scanner
