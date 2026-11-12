// SPDX-License-Identifier: GPL-3.0-or-later
//
// The file operations that need administrator rights. They run inside the
// KAuth helper (as root) and are kept free of any KAuth/D-Bus code so they can
// be unit-tested against a temporary directory.
//
// Every argument coming from the unprivileged app is treated as untrusted:
//   - paths must be absolute, below /etc, without "." or ".." components and
//     must not leave /etc through symlinked directories;
//   - the matching .pacnew/.pacsave file must exist next to the live file;
//   - symlinked live files (managed by other programs) are never written;
//   - account databases (passwd, shadow, ...) can only be kept;
//   - sudoers files are checked with visudo before they are installed.
// Before anything changes, the affected files are backed up so the operation
// can be undone from the app.

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace Ops
{

enum class Operation {
    Merge, // write the given content to the live file, remove the sidecar
    Replace, // use the sidecar (new default) as the live file
    Keep, // keep the live file, remove the sidecar
    Delete, // .pacsave only: remove the leftover
    RestoreLeftover, // .pacsave only: move it back into place (live missing)
};

struct Context {
    QString root = QStringLiteral("/"); // filesystem root (tests use a temp dir)
    QString backupDir = QStringLiteral("/var/lib/pacnew-helper/backups");
    bool validateSudoers = true; // run visudo -cf before installing sudoers files
};

struct Result {
    bool ok = false;
    QString error;
    QString backupId;
};

struct ReadResult {
    bool ok = false;
    QString error;
    QByteArray live;
    QByteArray sidecar;
    bool liveExists = false;
};

struct Backup {
    QString id;
    QDateTime time;
    QString path;
    QString kind;
    QString operation;
};

constexpr qint64 MaxFileSize = 16 * 1024 * 1024;

std::optional<Operation> parseOperation(const QString &name);
QString operationName(Operation op);

// Validates path/kind and returns the absolute on-disk path of the live file,
// or an empty string with *error set.
QString resolve(const Context &ctx, const QString &path, const QString &kind, QString *error);

Result apply(const Context &ctx, const QString &path, const QString &kind, Operation op, const QByteArray &content = {});
ReadResult read(const Context &ctx, const QString &path, const QString &kind);
Result restore(const Context &ctx, const QString &backupId);
QList<Backup> listBackups(const Context &ctx);

} // namespace Ops
