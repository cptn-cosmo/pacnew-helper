// SPDX-License-Identifier: GPL-3.0-or-later

#include "merge.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

using namespace Qt::StringLiterals;

#include <algorithm>
#include <vector>

namespace Merge
{

namespace
{
// Beyond this many cells the LCS table would get too large; the unmatched
// middle part is then treated as one changed block. Config files never get
// near this (a 2000 x 2000 line diff is 4M cells, 16 MB).
constexpr qsizetype MaxCells = 4'000'000;

QList<int> toIds(const QStringList &lines, QHash<QString, int> &ids)
{
    QList<int> out;
    out.reserve(lines.size());
    for (const QString &line : lines) {
        auto it = ids.constFind(line);
        if (it == ids.constEnd())
            it = ids.insert(line, int(ids.size()));
        out.append(*it);
    }
    return out;
}

bool rangesEqual(const QStringList &x, qsizetype x1, qsizetype x2, const QStringList &y, qsizetype y1, qsizetype y2)
{
    if (x2 - x1 != y2 - y1)
        return false;
    for (qsizetype i = 0; i < x2 - x1; ++i) {
        if (x.at(x1 + i) != y.at(y1 + i))
            return false;
    }
    return true;
}

QStringList slice(const QStringList &l, qsizetype from, qsizetype to)
{
    return l.mid(from, to - from);
}

// The setting a config line is about: "ParallelDownloads = 5" and
// "#ParallelDownloads = 10" both give "ParallelDownloads". Lines without an
// obvious key are their own key.
QString lineKey(const QString &line)
{
    QString t = line.trimmed();
    while (t.startsWith(u'#') || t.startsWith(u';'))
        t = t.mid(1).trimmed();
    static const QRegularExpression separator(u"[=:\\s]"_s);
    const qsizetype i = t.indexOf(separator);
    const QString key = (i < 0 ? t : t.left(i)).trimmed();
    return key.isEmpty() ? line : key;
}

// Splits a coarse difference into smaller pieces by aligning lines that set
// the same option. Classic three-way merge treats adjacent edits as one
// conflict, so an option the package adds right next to one the user changed
// would otherwise be lost with "keep mine". With a base, each piece is
// classified again by whether its lines come from the previous default.
QList<Region> splitByKey(const Region &r, bool haveBase)
{
    QStringList mineKeys;
    QStringList theirsKeys;
    for (const QString &l : r.mine)
        mineKeys << lineKey(l);
    for (const QString &l : r.theirs)
        theirsKeys << lineKey(l);
    const QSet<QString> baseSet(r.base.cbegin(), r.base.cend());

    auto allInBase = [&](const QStringList &lines) {
        return std::all_of(lines.cbegin(), lines.cend(), [&](const QString &l) {
            return baseSet.contains(l);
        });
    };
    auto classify = [&](const QStringList &m, const QStringList &t) {
        if (!haveBase)
            return Kind::Unknown;
        const bool mineFromBase = allInBase(m);
        const bool theirsFromBase = allInBase(t);
        if (m.isEmpty()) // only in the new version: added upstream, or removed by the user
            return theirsFromBase ? Kind::Local : Kind::Upstream;
        if (t.isEmpty()) // only in the user's file: added by the user, or removed upstream
            return mineFromBase ? Kind::Upstream : Kind::Local;
        if (mineFromBase && !theirsFromBase)
            return Kind::Upstream;
        if (theirsFromBase && !mineFromBase)
            return Kind::Local;
        return Kind::Conflict;
    };

    QList<Region> out;
    auto push = [&](Kind kind, const QStringList &m, const QStringList &t, qsizetype mi, qsizetype ti) {
        if (m.isEmpty() && t.isEmpty())
            return;
        if (!out.isEmpty()) {
            Region &last = out.last();
            if (last.kind == kind && last.mineLine + last.mine.size() == r.mineLine + mi && last.theirsLine + last.theirs.size() == r.theirsLine + ti) {
                last.mine += m;
                last.theirs += t;
                if (kind == Kind::Unchanged)
                    last.base += m;
                return;
            }
        }
        Region s;
        s.kind = kind;
        s.mine = m;
        s.theirs = t;
        s.base = kind == Kind::Unchanged ? m : QStringList();
        s.mineLine = r.mineLine + mi;
        s.theirsLine = r.theirsLine + ti;
        out.append(s);
    };

    qsizetype im = 0;
    qsizetype it = 0;
    for (const Block &b : matchingBlocks(mineKeys, theirsKeys)) {
        if (b.a > im || b.b > it) {
            const QStringList m = slice(r.mine, im, b.a);
            const QStringList t = slice(r.theirs, it, b.b);
            push(classify(m, t), m, t, im, it);
        }
        for (qsizetype k = 0; k < b.size; ++k) {
            const QString &ml = r.mine.at(b.a + k);
            const QString &tl = r.theirs.at(b.b + k);
            push(ml == tl ? Kind::Unchanged : classify({ml}, {tl}), {ml}, {tl}, b.a + k, b.b + k);
        }
        im = b.a + b.size;
        it = b.b + b.size;
    }
    // Nothing to split: keep the region as it was (including its base text).
    if (out.size() == 1 && out.first().kind != Kind::Unchanged && out.first().mine == r.mine && out.first().theirs == r.theirs)
        out.first().base = r.base;
    return out;
}
} // namespace

QList<Block> matchingBlocks(const QStringList &a, const QStringList &b)
{
    QHash<QString, int> idMap;
    const QList<int> x = toIds(a, idMap);
    const QList<int> y = toIds(b, idMap);
    const qsizetype n = x.size();
    const qsizetype m = y.size();

    // Common prefix and suffix are matched directly; only the middle needs LCS.
    qsizetype prefix = 0;
    while (prefix < n && prefix < m && x[prefix] == y[prefix])
        ++prefix;
    qsizetype suffix = 0;
    while (suffix < n - prefix && suffix < m - prefix && x[n - 1 - suffix] == y[m - 1 - suffix])
        ++suffix;

    // Matched line pairs, in order.
    std::vector<std::pair<qsizetype, qsizetype>> pairs;
    for (qsizetype i = 0; i < prefix; ++i)
        pairs.emplace_back(i, i);

    const qsizetype mn = n - prefix - suffix;
    const qsizetype mm = m - prefix - suffix;
    if (mn > 0 && mm > 0 && (mn + 1) * (mm + 1) <= MaxCells) {
        // table[i][j] = LCS length of x[prefix+i..] and y[prefix+j..]
        const qsizetype w = mm + 1;
        std::vector<quint32> table((mn + 1) * w, 0);
        for (qsizetype i = mn - 1; i >= 0; --i) {
            for (qsizetype j = mm - 1; j >= 0; --j) {
                if (x[prefix + i] == y[prefix + j])
                    table[i * w + j] = table[(i + 1) * w + j + 1] + 1;
                else
                    table[i * w + j] = std::max(table[(i + 1) * w + j], table[i * w + j + 1]);
            }
        }
        qsizetype i = 0;
        qsizetype j = 0;
        while (i < mn && j < mm) {
            if (x[prefix + i] == y[prefix + j]) {
                pairs.emplace_back(prefix + i, prefix + j);
                ++i;
                ++j;
            } else if (table[(i + 1) * w + j] >= table[i * w + j + 1]) {
                ++i;
            } else {
                ++j;
            }
        }
    }

    for (qsizetype k = 0; k < suffix; ++k)
        pairs.emplace_back(n - suffix + k, m - suffix + k);

    // Collapse consecutive pairs into blocks.
    QList<Block> blocks;
    for (const auto &[ai, bi] : pairs) {
        if (!blocks.isEmpty()) {
            Block &last = blocks.last();
            if (last.a + last.size == ai && last.b + last.size == bi) {
                ++last.size;
                continue;
            }
        }
        blocks.append({ai, bi, 1});
    }
    blocks.append({n, m, 0});
    return blocks;
}

QList<Region> threeWay(const QStringList &base, const QStringList &mine, const QStringList &theirs)
{
    // Classic merge3 (as in bzr / python-merge3): find regions where base,
    // mine and theirs all agree ("sync regions"); everything between two sync
    // regions was changed by one side, both sides, or neither.
    struct Sync {
        qsizetype baseStart, baseEnd, mineStart, mineEnd, theirsStart, theirsEnd;
    };
    const QList<Block> am = matchingBlocks(base, mine);
    const QList<Block> bm = matchingBlocks(base, theirs);

    QList<Sync> syncs;
    qsizetype ia = 0;
    qsizetype ib = 0;
    while (ia < am.size() && ib < bm.size()) {
        const Block &a = am[ia];
        const Block &b = bm[ib];
        const qsizetype lo = std::max(a.a, b.a);
        const qsizetype hi = std::min(a.a + a.size, b.a + b.size);
        if (lo < hi) {
            const qsizetype mineStart = a.b + (lo - a.a);
            const qsizetype theirsStart = b.b + (lo - b.a);
            syncs.append({lo, hi, mineStart, mineStart + (hi - lo), theirsStart, theirsStart + (hi - lo)});
        }
        if (a.a + a.size < b.a + b.size)
            ++ia;
        else
            ++ib;
    }
    syncs.append({base.size(), base.size(), mine.size(), mine.size(), theirs.size(), theirs.size()});

    QList<Region> regions;
    auto appendUnchanged = [&](const QStringList &lines, qsizetype mineLine, qsizetype theirsLine) {
        if (lines.isEmpty())
            return;
        if (!regions.isEmpty() && regions.last().kind == Kind::Unchanged) {
            regions.last().mine += lines;
            regions.last().theirs += lines;
            regions.last().base += lines;
            return;
        }
        regions.append({Kind::Unchanged, lines, lines, lines, mineLine, theirsLine});
    };

    qsizetype iz = 0; // base
    ia = 0; // mine
    ib = 0; // theirs
    for (const Sync &s : syncs) {
        if (s.mineStart > ia || s.theirsStart > ib) {
            const bool mineIsBase = rangesEqual(mine, ia, s.mineStart, base, iz, s.baseStart);
            const bool theirsIsBase = rangesEqual(theirs, ib, s.theirsStart, base, iz, s.baseStart);
            const bool same = rangesEqual(mine, ia, s.mineStart, theirs, ib, s.theirsStart);
            Region r;
            r.base = slice(base, iz, s.baseStart);
            r.mine = slice(mine, ia, s.mineStart);
            r.theirs = slice(theirs, ib, s.theirsStart);
            r.mineLine = ia;
            r.theirsLine = ib;
            if (same) {
                // Both sides made the identical change: nothing to decide.
                appendUnchanged(r.mine, ia, ib);
            } else if (mineIsBase && !theirsIsBase) {
                r.kind = Kind::Upstream;
                regions.append(r);
            } else if (theirsIsBase && !mineIsBase) {
                r.kind = Kind::Local;
                regions.append(r);
            } else {
                // A conflict often hides independent edits; split it up.
                r.kind = Kind::Conflict;
                for (const Region &piece : splitByKey(r, true)) {
                    if (piece.kind == Kind::Unchanged)
                        appendUnchanged(piece.mine, piece.mineLine, piece.theirsLine);
                    else
                        regions.append(piece);
                }
            }
            ia = s.mineStart;
            ib = s.theirsStart;
        }
        iz = s.baseStart;
        if (s.baseEnd > s.baseStart) {
            appendUnchanged(slice(base, s.baseStart, s.baseEnd), s.mineStart, s.theirsStart);
            iz = s.baseEnd;
            ia = s.mineEnd;
            ib = s.theirsEnd;
        }
    }
    return regions;
}

QList<Region> twoWay(const QStringList &mine, const QStringList &theirs)
{
    QList<Region> regions;
    qsizetype ia = 0;
    qsizetype ib = 0;
    for (const Block &blk : matchingBlocks(mine, theirs)) {
        auto appendUnchanged = [&](const QStringList &lines, qsizetype mineLine, qsizetype theirsLine) {
            if (!regions.isEmpty() && regions.last().kind == Kind::Unchanged) {
                regions.last().mine += lines;
                regions.last().theirs += lines;
                return;
            }
            regions.append({Kind::Unchanged, {}, lines, lines, mineLine, theirsLine});
        };
        if (blk.a > ia || blk.b > ib) {
            Region r;
            r.kind = Kind::Unknown;
            r.mine = slice(mine, ia, blk.a);
            r.theirs = slice(theirs, ib, blk.b);
            r.mineLine = ia;
            r.theirsLine = ib;
            for (const Region &piece : splitByKey(r, false)) {
                if (piece.kind == Kind::Unchanged)
                    appendUnchanged(piece.mine, piece.mineLine, piece.theirsLine);
                else
                    regions.append(piece);
            }
        }
        if (blk.size > 0)
            appendUnchanged(slice(mine, blk.a, blk.a + blk.size), blk.a, blk.b);
        ia = blk.a + blk.size;
        ib = blk.b + blk.size;
    }
    return regions;
}

QStringList splitLines(const QString &text, bool *trailingNewline)
{
    if (trailingNewline)
        *trailingNewline = text.endsWith(u'\n');
    if (text.isEmpty())
        return {};
    QString t = text;
    t.replace(u"\r\n"_s, u"\n"_s);
    if (t.endsWith(u'\n'))
        t.chop(1);
    return t.split(u'\n');
}

QString joinLines(const QStringList &lines, bool trailingNewline)
{
    QString out = lines.join(u'\n');
    if (trailingNewline && !lines.isEmpty())
        out += u'\n';
    return out;
}

bool isCommentLine(const QString &line)
{
    const QString t = line.trimmed();
    return t.isEmpty() || t.startsWith(u'#') || t.startsWith(u';') || t.startsWith(u"//"_s);
}

bool isCommentOnly(const Region &region)
{
    // Every line that appears on only one side must be a comment/blank.
    QHash<QString, int> balance;
    for (const QString &l : region.mine)
        ++balance[l];
    for (const QString &l : region.theirs)
        --balance[l];
    for (auto it = balance.cbegin(); it != balance.cend(); ++it) {
        if (it.value() != 0 && !isCommentLine(it.key()))
            return false;
    }
    return true;
}

} // namespace Merge
