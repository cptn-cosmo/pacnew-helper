// SPDX-License-Identifier: GPL-3.0-or-later
//
// Line-based diff and three-way merge for configuration files.
//
// Terminology used throughout the project:
//   mine   – the file currently in use (/etc/foo)
//   theirs – the new default shipped by the package (/etc/foo.pacnew)
//   base   – the default from the previous package version, i.e. what
//            "mine" was originally created from (taken from the pacman cache)

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace Merge
{

struct Block {
    qsizetype a = 0;
    qsizetype b = 0;
    qsizetype size = 0;
};

// Longest-common-subsequence matching blocks between two line lists, in
// order, terminated by a {a.size(), b.size(), 0} sentinel (like Python's
// difflib.SequenceMatcher.get_matching_blocks()).
QList<Block> matchingBlocks(const QStringList &a, const QStringList &b);

enum class Kind {
    Unchanged, // identical in mine and theirs
    Upstream, // only the package changed this part   -> take theirs
    Local, // only the user changed this part        -> keep mine
    Conflict, // both changed it, differently          -> ask
    Unknown, // differs, but no base to tell who changed it
};

struct Region {
    Kind kind = Kind::Unchanged;
    QStringList base; // empty for two-way regions
    QStringList mine;
    QStringList theirs;
    qsizetype mineLine = 0; // 0-based index of the first line in mine
    qsizetype theirsLine = 0; // 0-based index of the first line in theirs
};

QList<Region> threeWay(const QStringList &base, const QStringList &mine, const QStringList &theirs);
QList<Region> twoWay(const QStringList &mine, const QStringList &theirs);

// Splits text into lines without terminators. trailingNewline reports whether
// the text ended with '\n' (so it can be reproduced when joining).
QStringList splitLines(const QString &text, bool *trailingNewline = nullptr);
QString joinLines(const QStringList &lines, bool trailingNewline);

// True if the lines that differ between mine and theirs are all comments or
// blank lines, i.e. the change can't alter behaviour.
bool isCommentOnly(const Region &region);
bool isCommentLine(const QString &line);

} // namespace Merge
