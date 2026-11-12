// SPDX-License-Identifier: GPL-3.0-or-later

#include "controller.h"
#include "fileinfo.h"

#include <KAuth/Action>
#include <KAuth/ActionReply>
#include <KAuth/ExecuteJob>
#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QtConcurrent>

using namespace Qt::StringLiterals;

namespace
{
const QString HelperId = u"gay.geeks.pacnewhelper"_s;
}

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
