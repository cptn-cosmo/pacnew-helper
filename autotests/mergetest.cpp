// SPDX-License-Identifier: GPL-3.0-or-later

#include "merge.h"

#include <QTest>

using namespace Qt::StringLiterals;
using Merge::Kind;

class MergeTest : public QObject
{
    Q_OBJECT

private:
    static QStringList L(const char *s)
    {
        return Merge::splitLines(QString::fromUtf8(s));
    }
    static QList<Kind> kinds(const QList<Merge::Region> &regions)
    {
        QList<Kind> out;
        for (const auto &r : regions)
            out << r.kind;
        return out;
    }

private Q_SLOTS:
    void matchingBlocks()
    {
        const auto blocks = Merge::matchingBlocks(L("a\nb\nc\nd\n"), L("a\nx\nc\nd\ny\n"));
        QCOMPARE(blocks.size(), 3);
        QCOMPARE(blocks[0].a, 0);
        QCOMPARE(blocks[0].size, 1);
        QCOMPARE(blocks[1].a, 2);
        QCOMPARE(blocks[1].b, 2);
        QCOMPARE(blocks[1].size, 2);
        QCOMPARE(blocks[2].size, 0); // sentinel
        QCOMPARE(blocks[2].a, 4);
        QCOMPARE(blocks[2].b, 5);
    }

    void localAndUpstream()
    {
        // The user changed b, the package changed d: both should be detected.
        const auto r = Merge::threeWay(L("a\nb\nc\nd\n"), L("a\nB\nc\nd\n"), L("a\nb\nc\nD\n"));
        QCOMPARE(kinds(r), (QList<Kind>{Kind::Unchanged, Kind::Local, Kind::Unchanged, Kind::Upstream}));
        QCOMPARE(r[1].mine, QStringList{u"B"_s});
        QCOMPARE(r[1].theirs, QStringList{u"b"_s});
        QCOMPARE(r[3].mine, QStringList{u"d"_s});
        QCOMPARE(r[3].theirs, QStringList{u"D"_s});
        QCOMPARE(r[3].mineLine, 3);
        QCOMPARE(r[3].theirsLine, 3);
    }

    void conflict()
    {
        const auto r = Merge::threeWay(L("a\nb\nc\n"), L("a\nmine\nc\n"), L("a\ntheirs\nc\n"));
        QCOMPARE(kinds(r), (QList<Kind>{Kind::Unchanged, Kind::Conflict, Kind::Unchanged}));
        QCOMPARE(r[1].base, QStringList{u"b"_s});
    }

    void identicalChangeIsNotAConflict()
    {
        const auto r = Merge::threeWay(L("a\nb\nc\n"), L("a\nX\nc\n"), L("a\nX\nc\n"));
        QCOMPARE(kinds(r), (QList<Kind>{Kind::Unchanged}));
        QCOMPARE(r[0].mine, L("a\nX\nc\n"));
    }

    void upstreamAppendsAndLocalAppends()
    {
        // Package adds an option at the top, user added one at the end.
        const auto r = Merge::threeWay(L("a\nb\n"), L("a\nb\nmine\n"), L("new\na\nb\n"));
        QCOMPARE(kinds(r), (QList<Kind>{Kind::Upstream, Kind::Unchanged, Kind::Local}));
        QVERIFY(r[0].mine.isEmpty());
        QCOMPARE(r[0].theirs, QStringList{u"new"_s});
        QCOMPARE(r[2].mine, QStringList{u"mine"_s});
        QVERIFY(r[2].theirs.isEmpty());
    }

    void additionNextToConflictIsNotLost()
    {
        // pacman 7: the package enabled ParallelDownloads and added
        // DownloadUser right below it; the user had set ParallelDownloads = 10.
        const QStringList base = L("CheckSpace\n#ParallelDownloads = 5\n\nSigLevel = Required\n");
        const QStringList mine = L("CheckSpace\nParallelDownloads = 10\n\nSigLevel = Required\n");
        const QStringList theirs = L("CheckSpace\nParallelDownloads = 5\nDownloadUser = alpm\n#DisableSandbox\n\nSigLevel = Required\n");
        const auto r = Merge::threeWay(base, mine, theirs);
        QCOMPARE(kinds(r), (QList<Kind>{Kind::Unchanged, Kind::Conflict, Kind::Upstream, Kind::Unchanged}));
        QCOMPARE(r[1].mine, QStringList{u"ParallelDownloads = 10"_s});
        QCOMPARE(r[1].theirs, QStringList{u"ParallelDownloads = 5"_s});
        QVERIFY(r[2].mine.isEmpty());
        QCOMPARE(r[2].theirs, (QStringList{u"DownloadUser = alpm"_s, u"#DisableSandbox"_s}));
        QCOMPARE(r[2].theirsLine, 2);
    }

    void unchangedFileHasNoChanges()
    {
        const auto r = Merge::threeWay(L("a\nb\n"), L("a\nb\n"), L("a\nb\n"));
        QCOMPARE(kinds(r), (QList<Kind>{Kind::Unchanged}));
    }

    void twoWay()
    {
        const auto r = Merge::twoWay(L("a\nb\nc\n"), L("a\nX\nc\nd\n"));
        QCOMPARE(kinds(r), (QList<Kind>{Kind::Unchanged, Kind::Unknown, Kind::Unchanged, Kind::Unknown}));
        QCOMPARE(r[1].mine, QStringList{u"b"_s});
        QCOMPARE(r[1].theirs, QStringList{u"X"_s});
        QVERIFY(r[3].mine.isEmpty());
        QCOMPARE(r[3].theirs, QStringList{u"d"_s});
    }

    void reconstruction()
    {
        // Taking "mine" everywhere must give back mine; "theirs" must give theirs.
        const QStringList base = L("1\n2\n3\n4\n5\n6\n7\n");
        const QStringList mine = L("1\n2b\n3\n4\n5\n6\n7\nmine\n");
        const QStringList theirs = L("0\n1\n2\n3\n4x\n5\n7\n");
        QStringList allMine;
        QStringList allTheirs;
        for (const auto &r : Merge::threeWay(base, mine, theirs)) {
            allMine += r.mine;
            allTheirs += r.theirs;
        }
        QCOMPARE(allMine, mine);
        QCOMPARE(allTheirs, theirs);
    }

    void lines()
    {
        bool nl = false;
        QCOMPARE(Merge::splitLines(u"a\nb\n"_s, &nl), (QStringList{u"a"_s, u"b"_s}));
        QVERIFY(nl);
        QCOMPARE(Merge::splitLines(u"a\r\nb"_s, &nl), (QStringList{u"a"_s, u"b"_s}));
        QVERIFY(!nl);
        QCOMPARE(Merge::joinLines({u"a"_s, u"b"_s}, true), u"a\nb\n"_s);
        QCOMPARE(Merge::splitLines(QString()), QStringList());
    }

    void commentOnly()
    {
        Merge::Region r;
        r.mine = {u"# old comment"_s, u"Key = 1"_s};
        r.theirs = {u"# new comment"_s, u""_s, u"Key = 1"_s};
        QVERIFY(Merge::isCommentOnly(r));
        r.theirs = {u"# new comment"_s, u"Key = 2"_s};
        QVERIFY(!Merge::isCommentOnly(r));
    }
};

QTEST_GUILESS_MAIN(MergeTest)
#include "mergetest.moc"
