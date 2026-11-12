// SPDX-License-Identifier: GPL-3.0-or-later

#include "operations.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

using namespace Qt::StringLiterals;

class OperationsTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_tmp;
    Ops::Context m_ctx;

    QString p(const QString &path) const
    {
        return m_tmp.path() + u"/root"_s + path;
    }
    void write(const QString &path, const QByteArray &data, mode_t mode = 0644)
    {
        QDir().mkpath(QFileInfo(p(path)).absolutePath());
        QFile f(p(path));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(data);
        f.close();
        ::chmod(QFile::encodeName(p(path)).constData(), mode);
    }
    QByteArray contents(const QString &path) const
    {
        QFile f(p(path));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<missing>");
    }
    bool exists(const QString &path) const
    {
        return QFileInfo::exists(p(path));
    }
    mode_t mode(const QString &path) const
    {
        struct stat st;
        ::stat(QFile::encodeName(p(path)).constData(), &st);
        return st.st_mode & 07777;
    }

private Q_SLOTS:
    void init()
    {
        QDir(m_tmp.path()).removeRecursively();
        QDir().mkpath(m_tmp.path() + u"/root/etc"_s);
        m_ctx.root = m_tmp.path() + u"/root"_s;
        m_ctx.backupDir = m_tmp.path() + u"/backups"_s;
        m_ctx.validateSudoers = false;
        write(u"/etc/foo.conf"_s, "old\n", 0640);
        write(u"/etc/foo.conf.pacnew"_s, "new\n", 0644);
    }

    void mergeWritesContentKeepsModeAndRemovesPacnew()
    {
        const auto r = Ops::apply(m_ctx, u"/etc/foo.conf"_s, u"pacnew"_s, Ops::Operation::Merge, "merged\n");
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(contents(u"/etc/foo.conf"_s), QByteArray("merged\n"));
        QCOMPARE(mode(u"/etc/foo.conf"_s), mode_t(0640));
        QVERIFY(!exists(u"/etc/foo.conf.pacnew"_s));
        QVERIFY(!r.backupId.isEmpty());
        const auto backups = Ops::listBackups(m_ctx);
        QCOMPARE(backups.size(), 1);
        QCOMPARE(backups[0].path, u"/etc/foo.conf"_s);
        QCOMPARE(backups[0].operation, u"merge"_s);
    }

    void replaceUsesPacnew()
    {
        QVERIFY(Ops::apply(m_ctx, u"/etc/foo.conf"_s, u"pacnew"_s, Ops::Operation::Replace).ok);
        QCOMPARE(contents(u"/etc/foo.conf"_s), QByteArray("new\n"));
        QCOMPARE(mode(u"/etc/foo.conf"_s), mode_t(0644));
        QVERIFY(!exists(u"/etc/foo.conf.pacnew"_s));
    }

    void keepRemovesOnlyPacnew()
    {
        QVERIFY(Ops::apply(m_ctx, u"/etc/foo.conf"_s, u"pacnew"_s, Ops::Operation::Keep).ok);
        QCOMPARE(contents(u"/etc/foo.conf"_s), QByteArray("old\n"));
        QVERIFY(!exists(u"/etc/foo.conf.pacnew"_s));
    }

    void restoreUndoesAChange()
    {
        const auto r = Ops::apply(m_ctx, u"/etc/foo.conf"_s, u"pacnew"_s, Ops::Operation::Merge, "merged\n");
        QVERIFY(r.ok);
        const auto undo = Ops::restore(m_ctx, r.backupId);
        QVERIFY2(undo.ok, qPrintable(undo.error));
        QCOMPARE(contents(u"/etc/foo.conf"_s), QByteArray("old\n"));
        QCOMPARE(contents(u"/etc/foo.conf.pacnew"_s), QByteArray("new\n"));
        QCOMPARE(mode(u"/etc/foo.conf"_s), mode_t(0640));
        QCOMPARE(Ops::listBackups(m_ctx).size(), 2); // the restore is undoable too
    }

    void rejectsPathsOutsideEtc_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("var") << u"/var/foo.conf"_s;
        QTest::newRow("dotdot") << u"/etc/../etc/foo.conf"_s;
        QTest::newRow("dot") << u"/etc/./foo.conf"_s;
        QTest::newRow("relative") << u"etc/foo.conf"_s;
        QTest::newRow("dir") << u"/etc/"_s;
    }
    void rejectsPathsOutsideEtc()
    {
        QFETCH(QString, path);
        const auto r = Ops::apply(m_ctx, path, u"pacnew"_s, Ops::Operation::Replace);
        QVERIFY(!r.ok);
        QCOMPARE(contents(u"/etc/foo.conf"_s), QByteArray("old\n"));
    }

    void rejectsSymlinkedDirectoryEscape()
    {
        QDir().mkpath(m_tmp.path() + u"/outside"_s);
        QFile::link(m_tmp.path() + u"/outside"_s, p(u"/etc/escape"_s));
        write(u"/etc/escape/x.conf"_s, "x\n");
        write(u"/etc/escape/x.conf.pacnew"_s, "y\n");
        const auto r = Ops::apply(m_ctx, u"/etc/escape/x.conf"_s, u"pacnew"_s, Ops::Operation::Replace);
        QVERIFY(!r.ok);
        QCOMPARE(contents(u"/etc/escape/x.conf"_s), QByteArray("x\n"));
    }

    void requiresSidecar()
    {
        write(u"/etc/lonely.conf"_s, "x\n");
        QVERIFY(!Ops::apply(m_ctx, u"/etc/lonely.conf"_s, u"pacnew"_s, Ops::Operation::Keep).ok);
        QVERIFY(!Ops::apply(m_ctx, u"/etc/foo.conf"_s, u"evil"_s, Ops::Operation::Keep).ok);
    }

    void neverWritesSymlinkedLiveFile()
    {
        write(u"/etc/target"_s, "managed\n");
        QFile::link(p(u"/etc/target"_s), p(u"/etc/resolv.conf"_s));
        write(u"/etc/resolv.conf.pacnew"_s, "new\n");
        QVERIFY(!Ops::apply(m_ctx, u"/etc/resolv.conf"_s, u"pacnew"_s, Ops::Operation::Replace).ok);
        QVERIFY(!Ops::apply(m_ctx, u"/etc/resolv.conf"_s, u"pacnew"_s, Ops::Operation::Merge, "x").ok);
        QCOMPARE(contents(u"/etc/target"_s), QByteArray("managed\n"));
        // Keeping (i.e. removing the .pacnew) is fine.
        QVERIFY(Ops::apply(m_ctx, u"/etc/resolv.conf"_s, u"pacnew"_s, Ops::Operation::Keep).ok);
        QVERIFY(!exists(u"/etc/resolv.conf.pacnew"_s));
    }

    void accountFilesCanOnlyBeKept()
    {
        write(u"/etc/passwd"_s, "root:x:0:0::/root:/bin/bash\ncosmo:x:1000:1000::/home/cosmo:/bin/fish\n");
        write(u"/etc/passwd.pacnew"_s, "root:x:0:0::/root:/bin/bash\n");
        QVERIFY(!Ops::apply(m_ctx, u"/etc/passwd"_s, u"pacnew"_s, Ops::Operation::Replace).ok);
        QVERIFY(!Ops::apply(m_ctx, u"/etc/passwd"_s, u"pacnew"_s, Ops::Operation::Merge, "x").ok);
        QVERIFY(contents(u"/etc/passwd"_s).contains("cosmo"));
        QVERIFY(Ops::apply(m_ctx, u"/etc/passwd"_s, u"pacnew"_s, Ops::Operation::Keep).ok);
    }

    void pacsaveOperations()
    {
        write(u"/etc/gone.conf.pacsave"_s, "saved\n");
        QVERIFY(!Ops::apply(m_ctx, u"/etc/gone.conf"_s, u"pacsave"_s, Ops::Operation::Merge, "x").ok);
        QVERIFY(Ops::apply(m_ctx, u"/etc/gone.conf"_s, u"pacsave"_s, Ops::Operation::RestoreLeftover).ok);
        QCOMPARE(contents(u"/etc/gone.conf"_s), QByteArray("saved\n"));
        write(u"/etc/gone.conf.pacsave"_s, "again\n");
        QVERIFY(!Ops::apply(m_ctx, u"/etc/gone.conf"_s, u"pacsave"_s, Ops::Operation::RestoreLeftover).ok); // live exists now
        QVERIFY(Ops::apply(m_ctx, u"/etc/gone.conf"_s, u"pacsave"_s, Ops::Operation::Delete).ok);
        QVERIFY(!exists(u"/etc/gone.conf.pacsave"_s));
    }

    void readReturnsBothFiles()
    {
        const auto r = Ops::read(m_ctx, u"/etc/foo.conf"_s, u"pacnew"_s);
        QVERIFY(r.ok);
        QCOMPARE(r.live, QByteArray("old\n"));
        QCOMPARE(r.sidecar, QByteArray("new\n"));
    }

    void rejectsBadBackupIds()
    {
        QVERIFY(!Ops::restore(m_ctx, u"../../etc"_s).ok);
        QVERIFY(!Ops::restore(m_ctx, u"20260101-000000-000"_s).ok);
    }
};

QTEST_GUILESS_MAIN(OperationsTest)
#include "operationstest.moc"
