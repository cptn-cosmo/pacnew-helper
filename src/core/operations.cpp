// SPDX-License-Identifier: GPL-3.0-or-later

#include "operations.h"
#include "fileinfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace Ops
{

namespace
{
QString errnoText()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}

QString onDisk(const Context &ctx, const QString &path)
{
    QString root = ctx.root;
    while (root.endsWith(u'/'))
        root.chop(1);
    return root + path;
}

bool lstatRegular(const QString &file, struct stat *st)
{
    if (::lstat(QFile::encodeName(file).constData(), st) != 0)
        return false;
    return S_ISREG(st->st_mode);
}

bool exists(const QString &file)
{
    struct stat st;
    return ::lstat(QFile::encodeName(file).constData(), &st) == 0;
}

bool isSymlink(const QString &file)
{
    struct stat st;
    return ::lstat(QFile::encodeName(file).constData(), &st) == 0 && S_ISLNK(st.st_mode);
}

bool readAll(const QString &file, QByteArray *out, QString *error)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = f.errorString();
        return false;
    }
    if (f.size() > MaxFileSize) {
        *error = u"File is too large"_s;
        return false;
    }
    *out = f.readAll();
    return true;
}

bool writeAtomically(const QString &target, const QByteArray &data, mode_t mode, uid_t uid, gid_t gid, bool sudoers, QString *error)
{
    const QFileInfo fi(target);
    QByteArray tmpl = QFile::encodeName(fi.absolutePath() + u"/.pacnew-helper-XXXXXX"_s);
    const int fd = ::mkstemp(tmpl.data());
    if (fd < 0) {
        *error = u"Could not create a temporary file: %1"_s.arg(errnoText());
        return false;
    }
    const QString tmpPath = QFile::decodeName(tmpl);
    auto fail = [&](const QString &message) {
        *error = message;
        ::close(fd);
        ::unlink(tmpl.constData());
        return false;
    };

    qint64 written = 0;
    while (written < data.size()) {
        const ssize_t n = ::write(fd, data.constData() + written, size_t(data.size() - written));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return fail(u"Could not write: %1"_s.arg(errnoText()));
        }
        written += n;
    }
    if (::fchmod(fd, mode & 07777) != 0)
        return fail(u"Could not set permissions: %1"_s.arg(errnoText()));
    // Only root may change ownership; as a normal user (tests) the file is ours anyway.
    if ((::geteuid() == 0 || uid == ::geteuid()) && ::fchown(fd, uid, gid) != 0)
        return fail(u"Could not set owner: %1"_s.arg(errnoText()));
    if (::fsync(fd) != 0)
        return fail(u"Could not save: %1"_s.arg(errnoText()));

    if (sudoers) {
        QProcess visudo;
        visudo.start(u"visudo"_s, {u"-cf"_s, tmpPath});
        if (!visudo.waitForFinished(30000) || visudo.exitStatus() != QProcess::NormalExit || visudo.exitCode() != 0) {
            const QString out = QString::fromLocal8Bit(visudo.readAllStandardError() + visudo.readAllStandardOutput()).trimmed();
            return fail(u"The result is not a valid sudoers file, nothing was changed.\n%1"_s.arg(out));
        }
    }

    ::close(fd);
    if (::rename(tmpl.constData(), QFile::encodeName(target).constData()) != 0) {
        const QString message = u"Could not replace the file: %1"_s.arg(errnoText());
        ::unlink(tmpl.constData());
        *error = message;
        return false;
    }
    const int dirFd = ::open(QFile::encodeName(fi.absolutePath()).constData(), O_RDONLY | O_DIRECTORY);
    if (dirFd >= 0) {
        ::fsync(dirFd);
        ::close(dirFd);
    }
    return true;
}

bool copyWithMeta(const QString &from, const QString &to, QString *error)
{
    struct stat st;
    if (!lstatRegular(from, &st)) {
        *error = u"Not a regular file: %1"_s.arg(from);
        return false;
    }
    QByteArray data;
    if (!readAll(from, &data, error))
        return false;
    QFile out(to);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
        *error = out.errorString();
        return false;
    }
    out.close();
    ::chmod(QFile::encodeName(to).constData(), 0600); // backups may contain secrets
    return true;
}

QJsonObject statJson(const QString &file)
{
    struct stat st;
    if (::lstat(QFile::encodeName(file).constData(), &st) != 0 || !S_ISREG(st.st_mode))
        return {{u"exists"_s, false}};
    return {{u"exists"_s, true}, {u"mode"_s, int(st.st_mode & 07777)}, {u"uid"_s, int(st.st_uid)}, {u"gid"_s, int(st.st_gid)}};
}

// Saves the current state of the live file and its sidecar into a new,
// uniquely named backup folder.
bool makeBackup(const Context &ctx, const QString &path, const QString &kind, const QString &operation, const QString &live, QString *id, QString *error)
{
    const QString sidecar = live + u'.' + kind;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QDir dir(ctx.backupDir);
    if (!dir.mkpath(u"."_s)) {
        *error = u"Could not create the backup folder %1"_s.arg(ctx.backupDir);
        return false;
    }
    ::chmod(QFile::encodeName(ctx.backupDir).constData(), 0755);
    // Two changes within the same millisecond (e.g. undo right after a change)
    // must never share a folder, or the second would overwrite the first.
    const QString stamp = now.toString(u"yyyyMMdd-HHmmss-zzz"_s);
    *id = stamp;
    for (int n = 1; !dir.mkdir(*id); ++n) {
        if (n > 100) {
            *error = u"Could not create the backup folder %1"_s.arg(ctx.backupDir);
            return false;
        }
        *id = stamp + u'-' + QString::number(n);
    }
    const QString bdir = dir.filePath(*id);

    QJsonObject manifest{
        {u"id"_s, *id},
        {u"time"_s, now.toString(Qt::ISODateWithMs)},
        {u"path"_s, path},
        {u"kind"_s, kind},
        {u"operation"_s, operation},
        {u"live"_s, statJson(live)},
        {u"sidecar"_s, statJson(sidecar)},
    };
    if (exists(live) && !isSymlink(live) && !copyWithMeta(live, bdir + u"/live"_s, error))
        return false;
    if (exists(sidecar) && !copyWithMeta(sidecar, bdir + u"/sidecar"_s, error))
        return false;

    QFile mf(bdir + u"/manifest.json"_s);
    if (!mf.open(QIODevice::WriteOnly) || mf.write(QJsonDocument(manifest).toJson()) < 0) {
        *error = mf.errorString();
        return false;
    }
    mf.close();
    ::chmod(QFile::encodeName(mf.fileName()).constData(), 0644);
    return true;
}

bool removeFile(const QString &file, QString *error)
{
    if (::unlink(QFile::encodeName(file).constData()) != 0) {
        *error = u"Could not remove %1: %2"_s.arg(file, errnoText());
        return false;
    }
    return true;
}
} // namespace

std::optional<Operation> parseOperation(const QString &name)
{
    if (name == u"merge")
        return Operation::Merge;
    if (name == u"replace")
        return Operation::Replace;
    if (name == u"keep")
        return Operation::Keep;
    if (name == u"delete")
        return Operation::Delete;
    if (name == u"restore-leftover")
        return Operation::RestoreLeftover;
    return std::nullopt;
}

QString operationName(Operation op)
{
    switch (op) {
    case Operation::Merge:
        return u"merge"_s;
    case Operation::Replace:
        return u"replace"_s;
    case Operation::Keep:
        return u"keep"_s;
    case Operation::Delete:
        return u"delete"_s;
    case Operation::RestoreLeftover:
        return u"restore-leftover"_s;
    }
    return {};
}

QString resolve(const Context &ctx, const QString &path, const QString &kind, QString *error)
{
    if (kind != u"pacnew" && kind != u"pacsave") {
        *error = u"Unknown file type"_s;
        return {};
    }
    if (!path.startsWith(u"/etc/"_s) || path.contains(u'\0') || path.endsWith(u'/')) {
        *error = u"Only files below /etc can be changed"_s;
        return {};
    }
    const QStringList parts = path.split(u'/', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == u"." || part == u"..") {
            *error = u"Invalid path"_s;
            return {};
        }
    }

    const QString live = onDisk(ctx, path);
    // The containing folder must really be inside /etc (no symlinked directories).
    const QString etc = QFileInfo(onDisk(ctx, u"/etc"_s)).canonicalFilePath();
    const QString parent = QFileInfo(QFileInfo(live).absolutePath()).canonicalFilePath();
    if (etc.isEmpty() || parent.isEmpty() || !(parent == etc || parent.startsWith(etc + u'/'))) {
        *error = u"Only files below /etc can be changed"_s;
        return {};
    }

    struct stat st;
    if (!lstatRegular(live + u'.' + kind, &st)) {
        *error = u"There is no .%1 file for %2"_s.arg(kind, path);
        return {};
    }
    return live;
}

Result apply(const Context &ctx, const QString &path, const QString &kind, Operation op, const QByteArray &content)
{
    Result r;
    const QString live = resolve(ctx, path, kind, &r.error);
    if (live.isEmpty())
        return r;
    const QString sidecar = live + u'.' + kind;
    const FileInfo::Info info = FileInfo::lookup(path);

    const bool pacnewOp = op == Operation::Merge || op == Operation::Replace || op == Operation::Keep;
    if (pacnewOp != (kind == u"pacnew")) {
        r.error = u"This action is not available for .%1 files"_s.arg(kind);
        return r;
    }
    const bool writesLive = op == Operation::Merge || op == Operation::Replace || op == Operation::RestoreLeftover;
    if (writesLive && info.policy == FileInfo::Policy::KeepOnly) {
        r.error = u"%1 must not be replaced; only keeping your version is allowed"_s.arg(path);
        return r;
    }
    if (writesLive && isSymlink(live)) {
        r.error = u"%1 is managed by another program and is left unchanged"_s.arg(path);
        return r;
    }
    if (op == Operation::Merge && !exists(live)) {
        r.error = u"%1 does not exist"_s.arg(path);
        return r;
    }
    if (op == Operation::RestoreLeftover && exists(live)) {
        r.error = u"%1 already exists"_s.arg(path);
        return r;
    }
    if (content.size() > MaxFileSize) {
        r.error = u"File is too large"_s;
        return r;
    }

    if (!makeBackup(ctx, path, kind, operationName(op), live, &r.backupId, &r.error))
        return r;

    struct stat sideSt;
    lstatRegular(sidecar, &sideSt);
    switch (op) {
    case Operation::Merge: {
        struct stat liveSt;
        if (!lstatRegular(live, &liveSt)) {
            r.error = u"%1 is not a regular file"_s.arg(path);
            return r;
        }
        if (!writeAtomically(live, content, liveSt.st_mode, liveSt.st_uid, liveSt.st_gid, ctx.validateSudoers && FileInfo::isSudoers(path), &r.error))
            return r;
        if (!removeFile(sidecar, &r.error))
            return r;
        break;
    }
    case Operation::Replace:
    case Operation::RestoreLeftover: {
        QByteArray data;
        if (!readAll(sidecar, &data, &r.error))
            return r;
        if (!writeAtomically(live, data, sideSt.st_mode, sideSt.st_uid, sideSt.st_gid, ctx.validateSudoers && FileInfo::isSudoers(path), &r.error))
            return r;
        if (!removeFile(sidecar, &r.error))
            return r;
        break;
    }
    case Operation::Keep:
    case Operation::Delete:
        if (!removeFile(sidecar, &r.error))
            return r;
        break;
    }
    r.ok = true;
    return r;
}

ReadResult read(const Context &ctx, const QString &path, const QString &kind)
{
    ReadResult r;
    const QString live = resolve(ctx, path, kind, &r.error);
    if (live.isEmpty())
        return r;
    struct stat st;
    r.liveExists = lstatRegular(live, &st);
    if (r.liveExists && !readAll(live, &r.live, &r.error))
        return r;
    if (!readAll(live + u'.' + kind, &r.sidecar, &r.error))
        return r;
    r.ok = true;
    return r;
}

Result restore(const Context &ctx, const QString &backupId)
{
    Result r;
    static const QRegularExpression idPattern(u"^\\d{8}-\\d{6}-\\d{3}(-\\d{1,3})?$"_s);
    if (!idPattern.match(backupId).hasMatch()) {
        r.error = u"Invalid backup"_s;
        return r;
    }
    const QString bdir = QDir(ctx.backupDir).filePath(backupId);
    QFile mf(bdir + u"/manifest.json"_s);
    if (!mf.open(QIODevice::ReadOnly)) {
        r.error = u"Backup not found"_s;
        return r;
    }
    const QJsonObject m = QJsonDocument::fromJson(mf.readAll()).object();
    const QString path = m.value(u"path"_s).toString();
    const QString kind = m.value(u"kind"_s).toString();
    const QJsonObject liveMeta = m.value(u"live"_s).toObject();
    const QJsonObject sideMeta = m.value(u"sidecar"_s).toObject();

    // Same rules as for any other change (the sidecar may be gone by now, so
    // validate the folder via a temporary check of the parent directory).
    if (kind != u"pacnew" && kind != u"pacsave") {
        r.error = u"Invalid backup"_s;
        return r;
    }
    const QString live = onDisk(ctx, path);
    const QString sidecar = live + u'.' + kind;
    {
        const QString etc = QFileInfo(onDisk(ctx, u"/etc"_s)).canonicalFilePath();
        const QString parent = QFileInfo(QFileInfo(live).absolutePath()).canonicalFilePath();
        const bool insideEtc = !etc.isEmpty() && !parent.isEmpty() && (parent == etc || parent.startsWith(etc + u'/'));
        const bool cleanPath = path.startsWith(u"/etc/"_s) && !path.split(u'/').contains(u".."_s) && !path.split(u'/').contains(u"."_s);
        if (!insideEtc || !cleanPath) {
            r.error = u"Invalid backup"_s;
            return r;
        }
    }
    if (isSymlink(live)) {
        r.error = u"%1 is managed by another program and is left unchanged"_s.arg(path);
        return r;
    }

    // Back up the current state first, so the restore itself can be undone.
    if (!makeBackup(ctx, path, kind, u"restore"_s, live, &r.backupId, &r.error))
        return r;

    auto put = [&](const QJsonObject &meta, const QString &stored, const QString &target) -> bool {
        if (!meta.value(u"exists"_s).toBool()) {
            if (exists(target))
                return removeFile(target, &r.error);
            return true;
        }
        QByteArray data;
        if (!readAll(bdir + u'/' + stored, &data, &r.error))
            return false;
        return writeAtomically(target, data, mode_t(meta.value(u"mode"_s).toInt(0644)), uid_t(meta.value(u"uid"_s).toInt()),
                               gid_t(meta.value(u"gid"_s).toInt()), ctx.validateSudoers && FileInfo::isSudoers(path), &r.error);
    };
    // Account databases are never rewritten; for them only the sidecar comes back.
    if (FileInfo::lookup(path).policy != FileInfo::Policy::KeepOnly && !put(liveMeta, u"live"_s, live))
        return r;
    if (!put(sideMeta, u"sidecar"_s, sidecar))
        return r;
    r.ok = true;
    return r;
}

QList<Backup> listBackups(const Context &ctx)
{
    QList<Backup> out;
    const QDir dir(ctx.backupDir);
    const QStringList ids = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QString &id : ids) {
        QFile mf(dir.filePath(id) + u"/manifest.json"_s);
        if (!mf.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject m = QJsonDocument::fromJson(mf.readAll()).object();
        Backup b;
        b.id = id;
        b.time = QDateTime::fromString(m.value(u"time"_s).toString(), Qt::ISODateWithMs);
        b.path = m.value(u"path"_s).toString();
        b.kind = m.value(u"kind"_s).toString();
        b.operation = m.value(u"operation"_s).toString();
        out.append(b);
    }
    return out;
}

} // namespace Ops
