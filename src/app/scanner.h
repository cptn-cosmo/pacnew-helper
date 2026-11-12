// SPDX-License-Identifier: GPL-3.0-or-later
//
// Finds .pacnew/.pacsave files and gathers everything needed to review them:
// file contents, the owning package and - if the pacman cache still has it -
// the default from the previous package version, which makes a three-way
// merge possible.

#pragma once

#include "merge.h"

#include <QDateTime>
#include <QList>
#include <QString>

struct Entry {
    enum class Status {
        Identical, // new default equals the current file
        Automatic, // all differences can be decided automatically
        NeedsReview, // conflicts, or no base to tell who changed what
        KeepOnly, // account databases: only keeping is allowed
        Managed, // symlink or generated file, maintained by another program
        Protected, // not readable without administrator rights
        Leftover, // .pacsave from a removed package
        Orphaned, // .pacnew whose live file no longer exists
    };

    QString path; // live file as seen by the system, e.g. /etc/pacman.conf
    QString kind; // "pacnew" or "pacsave"
    QString package;
    QString installedVersion;
    QString baseVersion; // package version the merge base came from
    QDateTime modified; // when the .pacnew/.pacsave was written

    bool liveExists = false;
    bool liveIsSymlink = false;
    // The live file says it is written by a program (NetworkManager,
    // rate-mirrors, reflector, ...); its header line is kept here.
    QString generatedBy;
    bool readable = true;
    bool hasBase = false;
    bool liveTrailingNewline = true;
    bool newTrailingNewline = true;
    QStringList live;
    QStringList theirs; // the .pacnew/.pacsave content
    QStringList base;

    // analysis
    bool identical = false;
    int upstream = 0;
    int local = 0;
    int conflicts = 0;
    int unknown = 0;

    Status status() const;
    // Nothing to bring over: keeping the current file is the whole job.
    bool canSimplyKeep() const;
    QList<Merge::Region> regions() const;
    void analyze();
};

namespace Scanner
{
// root is "/" normally; a different folder enables demo mode (no pacman
// queries, merge bases are read from <root>/base/<path> if present).
QList<Entry> scan(const QString &root);

// Reads the contents of one entry again (used after unlocking a protected file).
void setContents(Entry &entry, const QByteArray &live, const QByteArray &sidecar, bool liveExists);
} // namespace Scanner
