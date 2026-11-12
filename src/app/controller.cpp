// SPDX-License-Identifier: GPL-3.0-or-later

#include "controller.h"
#include "fileinfo.h"

#include <KAuth/Action>
#include <KAuth/ActionReply>
#include <KAuth/ExecuteJob>
#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QTimer>
#include <QtConcurrent>

using namespace Qt::StringLiterals;

namespace
{
const QString HelperId = u"gay.geeks.pacnewhelper"_s;

// How the file on disk changes, as rows for the summary: changed lines plus a
// little unchanged context around them, long unchanged stretches folded.
struct DiffRows {
    QVariantList rows;
    int added = 0;
    int removed = 0;
};

DiffRows diffRows(const QStringList &before, const QStringList &after, int context = 2)
{
    struct Line {
        QString type; // context | removed | added
        QString text;
        int oldLine = 0;
        int newLine = 0;
    };
    QList<Line> all;
    qsizetype ia = 0;
    qsizetype ib = 0;
    DiffRows out;
    for (const Merge::Block &b : Merge::matchingBlocks(before, after)) {
        for (; ia < b.a; ++ia) {
            all.append({u"removed"_s, before.at(ia), int(ia + 1), 0});
            ++out.removed;
        }
        for (; ib < b.b; ++ib) {
            all.append({u"added"_s, after.at(ib), 0, int(ib + 1)});
            ++out.added;
        }
        for (qsizetype k = 0; k < b.size; ++k)
            all.append({u"context"_s, before.at(b.a + k), int(b.a + k + 1), int(b.b + k + 1)});
        ia = b.a + b.size;
        ib = b.b + b.size;
    }

    // Keep context lines within `context` of a change, fold the rest.
    QList<bool> keep(all.size(), false);
    for (qsizetype i = 0; i < all.size(); ++i) {
        if (all[i].type == u"context")
            continue;
        for (qsizetype j = std::max<qsizetype>(0, i - context); j <= std::min(all.size() - 1, i + context); ++j)
            keep[j] = true;
    }
    int folded = 0;
    auto flushGap = [&] {
        if (folded > 0)
            out.rows.append(QVariantMap{{u"type"_s, u"gap"_s}, {u"count"_s, folded}});
        folded = 0;
    };
    for (qsizetype i = 0; i < all.size(); ++i) {
        if (!keep[i]) {
            ++folded;
            continue;
        }
        flushGap();
        const Line &l = all[i];
        out.rows.append(QVariantMap{{u"type"_s, l.type}, {u"text"_s, l.text}, {u"oldLine"_s, l.oldLine}, {u"newLine"_s, l.newLine}});
    }
    // A trailing fold isn't worth a row when nothing changed after it.
    if (out.added + out.removed > 0)
        flushGap();
    return out;
}

QVariantMap item(const QString &path, const QString &heading, const QString &note, const DiffRows &diff = {})
{
    return {{u"path"_s, path},
            {u"heading"_s, heading},
            {u"note"_s, note},
            {u"rows"_s, diff.rows},
            {u"added"_s, diff.added},
            {u"removed"_s, diff.removed}};
}

QString backupSentence()
{
    return i18n("Before anything is changed, a backup of the files involved is saved. You can undo this at any time from History.");
}
} // namespace

Controller::Controller(const QString &root, QObject *parent)
    : QObject(parent)
    , m_root(QDir(root).absolutePath())
    , m_demo(QDir(root).canonicalPath() != u"/")
{
    connect(&m_scan, &QFutureWatcher<QList<Entry>>::finished, this, [this] {
        m_files.setEntries(m_scan.result());
        Q_EMIT scanningChanged();
        // Keep the file under review open if it is still there.
        if (!m_currentPath.isEmpty())
            open(m_currentPath, m_currentKind);
    });
}

FileListModel *Controller::files()
{
    return &m_files;
}

ChangeModel *Controller::changes()
{
    return &m_changes;
}

HistoryModel *Controller::history()
{
    return &m_history;
}

bool Controller::scanning() const
{
    return m_scan.isRunning();
}

bool Controller::busy() const
{
    return m_busy;
}

bool Controller::demoMode() const
{
    return m_demo;
}

QString Controller::root() const
{
    return m_root;
}

Ops::Context Controller::demoContext() const
{
    Ops::Context ctx;
    ctx.root = m_root;
    ctx.backupDir = m_root + u"/backups"_s;
    ctx.validateSudoers = false;
    return ctx;
}

void Controller::rescan()
{
    if (m_scan.isRunning())
        return;
    const QString root = m_root;
    m_scan.setFuture(QtConcurrent::run([root] {
        return Scanner::scan(root);
    }));
    Q_EMIT scanningChanged();
}

Entry *Controller::currentEntry()
{
    return m_files.entry(m_files.indexOf(m_currentPath, m_currentKind));
}

void Controller::open(const QString &path, const QString &kind)
{
    m_currentPath = path;
    m_currentKind = kind;
    if (Entry *e = currentEntry())
        m_changes.load(*e);
    else
        m_changes.clear();
    Q_EMIT currentChanged();
}

void Controller::close()
{
    m_currentPath.clear();
    m_currentKind.clear();
    m_changes.clear();
    Q_EMIT currentChanged();
}

QVariantMap Controller::current() const
{
    const int row = m_files.indexOf(m_currentPath, m_currentKind);
    if (row < 0)
        return {};
    const Entry &e = m_files.entries().at(row);
    const FileInfo::Info info = FileInfo::lookup(e.path);
    return {
        {u"path"_s, e.path},
        {u"kind"_s, e.kind},
        {u"fileName"_s, QFileInfo(e.path).fileName()},
        {u"title"_s, info.title},
        {u"description"_s, info.description},
        {u"advice"_s, info.advice},
        {u"afterApply"_s, info.afterApply},
        {u"highRisk"_s, info.risk == FileInfo::Risk::High},
        {u"keepOnly"_s, info.policy == FileInfo::Policy::KeepOnly},
        {u"package"_s, e.package},
        {u"installedVersion"_s, e.installedVersion},
        {u"baseVersion"_s, e.baseVersion},
        {u"hasBase"_s, e.hasBase},
        {u"status"_s, FileListModel::statusId(e.status())},
        {u"liveExists"_s, e.liveExists},
        {u"managed"_s, e.liveIsSymlink || !e.generatedBy.isEmpty()},
        {u"symlink"_s, e.liveIsSymlink},
        {u"generatedBy"_s, e.generatedBy},
        {u"readable"_s, e.readable},
        {u"identical"_s, e.identical},
        {u"modified"_s, e.modified},
        {u"leftoverText"_s, Merge::joinLines(e.theirs, true)},
    };
}

void Controller::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT busyChanged();
}

void Controller::runAction(const QString &action, const QVariantMap &args, Callback onSuccess)
{
    setBusy(true);

    if (m_demo) {
        // Demo mode: same code path as the helper, but on the demo folder and
        // without administrator rights.
        QTimer::singleShot(150, this, [this, action, args, onSuccess] {
            const Ops::Context ctx = demoContext();
            QVariantMap data;
            QString error;
            if (action == u"apply") {
                const auto op = Ops::parseOperation(args.value(u"operation"_s).toString());
                const Ops::Result r = op ? Ops::apply(ctx, args.value(u"path"_s).toString(), args.value(u"kind"_s).toString(), *op,
                                                      args.value(u"content"_s).toByteArray())
                                         : Ops::Result{false, u"Unknown operation"_s, {}};
                error = r.ok ? QString() : r.error;
                data.insert(u"backupId"_s, r.backupId);
            } else if (action == u"read") {
                const Ops::ReadResult r = Ops::read(ctx, args.value(u"path"_s).toString(), args.value(u"kind"_s).toString());
                error = r.ok ? QString() : r.error;
                data = {{u"live"_s, r.live}, {u"sidecar"_s, r.sidecar}, {u"liveExists"_s, r.liveExists}};
            } else if (action == u"restore") {
                const Ops::Result r = Ops::restore(ctx, args.value(u"backupId"_s).toString());
                error = r.ok ? QString() : r.error;
            }
            setBusy(false);
            if (error.isEmpty())
                onSuccess(data);
            else
                Q_EMIT message(error, true);
        });
        return;
    }

    KAuth::Action kauthAction(HelperId + u'.' + action);
    kauthAction.setHelperId(HelperId);
    kauthAction.setArguments(args);
    KAuth::ExecuteJob *job = kauthAction.execute();
    connect(job, &KJob::result, this, [this, job, onSuccess] {
        setBusy(false);
        if (job->error() == 0) {
            onSuccess(job->data());
            return;
        }
        const int code = job->error();
        if (code == KAuth::ActionReply::UserCancelledError || code == KAuth::ActionReply::AuthorizationDeniedError) {
            Q_EMIT message(i18n("Nothing was changed because the administrator password was not entered."), true);
            return;
        }
        if (code == KAuth::ActionReply::NoSuchActionError || code == KAuth::ActionReply::NoResponderError
            || code == KAuth::ActionReply::BackendError) {
            Q_EMIT message(i18n("The helper that changes system files is not available. Please install Pacnew Helper as a package. (%1)",
                                job->errorString()),
                           true);
            return;
        }
        Q_EMIT message(job->errorText().isEmpty() ? job->errorString() : job->errorText(), true);
    });
    job->start();
}

void Controller::applyOperation(const QString &operation, const QByteArray &content, const QString &summary)
{
    Entry *e = currentEntry();
    if (!e)
        return;
    const QString path = e->path;
    const QString kind = e->kind;
    const QString afterApply = (operation == u"keep" || operation == u"delete") ? QString() : FileInfo::lookup(path).afterApply;
    QVariantMap args{{u"path"_s, path}, {u"kind"_s, kind}, {u"operation"_s, operation}};
    if (!content.isNull())
        args.insert(u"content"_s, content);
    runAction(u"apply"_s, args, [this, path, kind, summary, afterApply](const QVariantMap &) {
        m_files.removeEntry(m_files.indexOf(path, kind));
        if (path == m_currentPath && kind == m_currentKind)
            close();
        Q_EMIT finished(summary, afterApply);
    });
}

void Controller::applyChoices()
{
    Entry *e = currentEntry();
    if (!e)
        return;
    const QString name = QFileInfo(e->path).fileName();
    // Avoid needless rewrites: if the choices reproduce one of the two files,
    // use the matching simpler operation.
    if (m_changes.resultIsMine())
        applyOperation(u"keep"_s, {}, i18n("%1 is up to date. Your version was kept and the extra copy removed.", name));
    else if (m_changes.resultIsNew())
        applyOperation(u"replace"_s, {}, i18n("%1 was replaced with the new version.", name));
    else
        applyOperation(u"merge"_s, m_changes.resultText().toUtf8(), i18n("%1 was updated.", name));
}

void Controller::keepMine()
{
    if (Entry *e = currentEntry())
        applyOperation(u"keep"_s, {}, i18n("%1: your version was kept and the extra copy removed.", QFileInfo(e->path).fileName()));
}

void Controller::useNew()
{
    if (Entry *e = currentEntry())
        applyOperation(u"replace"_s, {}, i18n("%1 was replaced with the new version.", QFileInfo(e->path).fileName()));
}

void Controller::deleteLeftover()
{
    if (Entry *e = currentEntry())
        applyOperation(u"delete"_s, {}, i18n("The saved settings %1 were removed.", QFileInfo(e->path + u'.' + e->kind).fileName()));
}

void Controller::restoreLeftover()
{
    if (Entry *e = currentEntry())
        applyOperation(u"restore-leftover"_s, {}, i18n("%1 was restored from the saved settings.", QFileInfo(e->path).fileName()));
}

void Controller::unlock()
{
    Entry *e = currentEntry();
    if (!e)
        return;
    const QString path = e->path;
    const QString kind = e->kind;
    runAction(u"read"_s, {{u"path"_s, path}, {u"kind"_s, kind}}, [this, path, kind](const QVariantMap &data) {
        const int row = m_files.indexOf(path, kind);
        Entry *entry = m_files.entry(row);
        if (!entry)
            return;
        Scanner::setContents(*entry, data.value(u"live"_s).toByteArray(), data.value(u"sidecar"_s).toByteArray(),
                             data.value(u"liveExists"_s).toBool());
        m_files.updateEntry(row);
        open(path, kind);
    });
}

void Controller::keepAllUnchanged()
{
    // One after another; polkit remembers the authorization for the session,
    // so the password is asked for at most once.
    QList<Entry> todo;
    for (const Entry &e : m_files.entries()) {
        if (e.canSimplyKeep())
            todo.append(e);
    }
    if (todo.isEmpty())
        return;
    // The step function refers to itself only weakly; each pending action holds
    // the strong reference, so it is freed once the last one has finished.
    auto next = std::make_shared<std::function<void(int)>>();
    std::weak_ptr<std::function<void(int)>> weakNext = next;
    *next = [this, todo, weakNext](int i) {
        if (i >= todo.size()) {
            Q_EMIT message(i18np("Done: 1 file kept as it is, its extra copy removed.", "Done: %1 files kept as they are, their extra copies removed.", todo.size()), false);
            return;
        }
        const Entry &e = todo.at(i);
        auto self = weakNext.lock();
        runAction(u"apply"_s, {{u"path"_s, e.path}, {u"kind"_s, e.kind}, {u"operation"_s, u"keep"_s}}, [this, e, i, self](const QVariantMap &) {
            m_files.removeEntry(m_files.indexOf(e.path, e.kind));
            (*self)(i + 1);
        });
    };
    (*next)(0);
}

void Controller::reloadHistory()
{
    Ops::Context ctx = m_demo ? demoContext() : Ops::Context{};
    m_history.reload(ctx);
}

void Controller::restoreBackup(const QString &backupId)
{
    runAction(u"restore"_s, {{u"backupId"_s, backupId}}, [this](const QVariantMap &) {
        Q_EMIT message(i18n("The earlier version was restored."), false);
        reloadHistory();
        rescan();
    });
}

QVariantMap Controller::preview(const QString &action, const QString &arg)
{
    QVariantList bullets;
    QVariantList items;
    QString title;
    QString actionText = i18nc("@action:button", "Approve and Save");

    if (action == u"keep-all") {
        int n = 0;
        for (const Entry &e : m_files.entries()) {
            if (!e.canSimplyKeep())
                continue;
            ++n;
            QString why;
            switch (e.status()) {
            case Entry::Status::Identical:
                why = i18n("The new version is exactly the same as yours.");
                break;
            case Entry::Status::Managed:
                why = e.generatedBy.isEmpty() ? i18n("This file is maintained by another program.")
                                              : i18n("This file is written by a program (“%1”).", e.generatedBy);
                break;
            default:
                why = i18n("The update brings nothing new; only your own changes differ.");
                break;
            }
            items.append(item(e.path,
                              i18n("Stays exactly as it is"),
                              why + u' ' + i18n("The extra copy %1 is removed.", QFileInfo(e.path).fileName() + u'.' + e.kind)));
        }
        title = i18np("Keep 1 file and tidy up", "Keep %1 files and tidy up", n);
        bullets << i18np("1 file stays exactly as it is.", "%1 files stay exactly as they are.", n)
                << i18np("Its extra copy (.pacnew) is removed.", "Their extra copies (.pacnew) are removed.", n) << backupSentence();
        return {{u"title"_s, title}, {u"actionText"_s, actionText}, {u"bullets"_s, bullets}, {u"items"_s, items}};
    }

    if (action == u"undo") {
        const Ops::Context ctx = m_demo ? demoContext() : Ops::Context{};
        for (const Ops::Backup &b : Ops::listBackups(ctx)) {
            if (b.id != arg)
                continue;
            title = i18n("Undo a change to %1", QFileInfo(b.path).fileName());
            bullets << i18n("%1 is put back exactly as it was on %2, before this change.", b.path, QLocale().toString(b.time.toLocalTime(), QLocale::ShortFormat))
                    << i18n("If the change removed the extra copy (%1), it comes back too, so you can review it again.", QFileInfo(b.path).fileName() + u'.' + b.kind)
                    << i18n("The current version is backed up first, so this undo can be undone as well.");
            items.append(item(b.path, i18n("Restored from the backup"), i18n("The backup itself is protected, so its contents can't be shown before restoring.")));
            return {{u"title"_s, title}, {u"actionText"_s, i18nc("@action:button", "Approve and Undo")}, {u"bullets"_s, bullets}, {u"items"_s, items}};
        }
        return {};
    }

    Entry *e = currentEntry();
    if (!e)
        return {};
    const QString name = QFileInfo(e->path).fileName();
    const QString sidecarName = name + u'.' + e->kind;
    const FileInfo::Info info = FileInfo::lookup(e->path);

    QString effective = action;
    if (action == u"apply")
        effective = m_changes.resultIsMine() ? u"keep"_s : m_changes.resultIsNew() ? u"replace"_s : u"merge"_s;

    if (effective == u"merge") {
        const QStringList after = Merge::splitLines(m_changes.resultText());
        const DiffRows diff = diffRows(e->live, after);
        const int taking = m_changes.takingNewCount();
        const int keeping = m_changes.changeCount() - taking;
        title = i18n("Update %1", name);
        bullets << i18np("1 change from the new version is applied.", "%1 changes from the new version are applied.", taking);
        if (keeping > 0)
            bullets << i18np("1 difference keeps your current version.", "%1 differences keep your current version.", keeping);
        bullets << i18n("Everything else in the file stays as it is.") << i18n("The extra copy %1 is removed afterwards.", sidecarName);
        items.append(item(e->path, i18n("Changes to your file"), {}, diff));
    } else if (effective == u"replace") {
        const DiffRows diff = diffRows(e->live, e->theirs);
        title = e->liveExists ? i18n("Replace %1 with the new version", name) : i18n("Put the new version of %1 in place", name);
        if (e->liveExists) {
            bullets << i18n("Your current %1 is replaced completely by the new version.", name);
            if (e->local + e->conflicts + e->unknown > 0)
                bullets << i18n("Changes made on this computer that only exist in your version are lost.");
        } else {
            bullets << i18n("The new version is saved as %1.", e->path);
        }
        bullets << i18n("The extra copy %1 is removed afterwards.", sidecarName);
        items.append(item(e->path, e->liveExists ? i18n("Changes to your file") : i18n("New file"), {}, diff));
    } else if (effective == u"keep") {
        title = i18n("Keep your version of %1", name);
        bullets << i18n("%1 stays exactly as it is.", e->path)
                << i18n("The new version %1 is removed, so improvements it contains are not taken over.", sidecarName);
        if (e->identical)
            bullets.last() = i18n("The extra copy %1 is removed. It is identical, so nothing is lost.", sidecarName);
        items.append(item(e->path, i18n("No changes to your file"), i18n("Only %1 is removed.", sidecarName)));
    } else if (effective == u"delete") {
        title = i18n("Remove saved settings %1", sidecarName);
        bullets << i18n("The saved settings %1 are deleted.", e->path + u'.' + e->kind);
        if (e->liveExists)
            bullets << i18n("%1 itself is not touched.", e->path);
        items.append(item(e->path + u'.' + e->kind, i18n("Deleted"), {}, diffRows(e->theirs, {})));
    } else if (effective == u"restore-leftover") {
        title = i18n("Put %1 back in place", name);
        bullets << i18n("The saved settings are moved back to %1.", e->path);
        items.append(item(e->path, i18n("New file"), {}, diffRows({}, e->theirs)));
    } else {
        return {};
    }

    bullets << backupSentence();
    if (effective != u"keep" && effective != u"delete" && !info.afterApply.isEmpty())
        bullets << info.afterApply;
    return {{u"title"_s, title}, {u"actionText"_s, actionText}, {u"bullets"_s, bullets}, {u"items"_s, items}, {u"highRisk"_s, info.risk == FileInfo::Risk::High}};
}

void Controller::execute(const QString &action, const QString &arg)
{
    if (action == u"apply")
        applyChoices();
    else if (action == u"replace")
        useNew();
    else if (action == u"keep")
        keepMine();
    else if (action == u"delete")
        deleteLeftover();
    else if (action == u"restore-leftover")
        restoreLeftover();
    else if (action == u"keep-all")
        keepAllUnchanged();
    else if (action == u"undo")
        restoreBackup(arg);
}
