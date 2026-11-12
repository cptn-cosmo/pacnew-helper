// SPDX-License-Identifier: GPL-3.0-or-later
//
// The object QML talks to ("App"): scanning, the file under review and all
// actions. Privileged actions go through the KAuth helper; in demo mode
// (--root) they run directly against the demo folder.

#pragma once

#include "changemodel.h"
#include "filelistmodel.h"
#include "historymodel.h"

#include <QFutureWatcher>
#include <QObject>
#include <QVariantMap>

#include <functional>

class Controller : public QObject
{
    Q_OBJECT

    Q_PROPERTY(FileListModel *files READ files CONSTANT)
    Q_PROPERTY(ChangeModel *changes READ changes CONSTANT)
    Q_PROPERTY(HistoryModel *history READ history CONSTANT)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool demoMode READ demoMode CONSTANT)
    Q_PROPERTY(QString root READ root CONSTANT)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)

public:
    explicit Controller(const QString &root, QObject *parent = nullptr);

    FileListModel *files();
    ChangeModel *changes();
    HistoryModel *history();
    bool scanning() const;
    bool busy() const;
    bool demoMode() const;
    QString root() const;
    QVariantMap current() const;

    Q_INVOKABLE void rescan();
    Q_INVOKABLE void open(const QString &path, const QString &kind);
    Q_INVOKABLE void close();

    // Actions on the current file
    Q_INVOKABLE void applyChoices();
    Q_INVOKABLE void keepMine();
    Q_INVOKABLE void useNew();
    Q_INVOKABLE void deleteLeftover();
    Q_INVOKABLE void restoreLeftover();
    Q_INVOKABLE void unlock();

    // Keeps every file that needs no changes and removes its extra copy.
    Q_INVOKABLE void keepAllUnchanged();
    Q_INVOKABLE void reloadHistory();
    Q_INVOKABLE void restoreBackup(const QString &backupId);

    // Nothing is written without approval: the UI first shows preview() for an
    // action, and only runs execute() with the same arguments once the user
    // approved it. Actions: "apply" (the current choices), "replace", "keep",
    // "delete", "restore-leftover" (current file), "keep-all" (bulk) and
    // "undo" (arg = backup id).
    //
    // The preview is a map with
    //   title, actionText (button), bullets (list of sentences), and
    //   items: list of {path, heading, note, added, removed, rows}, where rows
    //   is the change to the file on disk: {type: context|removed|added|gap,
    //   text, oldLine, newLine, count}.
    Q_INVOKABLE QVariantMap preview(const QString &action, const QString &arg = {});
    Q_INVOKABLE void execute(const QString &action, const QString &arg = {});

Q_SIGNALS:
    void scanningChanged();
    void busyChanged();
    void currentChanged();
    void message(const QString &text, bool error);
    // The current file was dealt with; afterApply is an optional follow-up hint.
    void finished(const QString &summary, const QString &afterApply);

private:
    using Callback = std::function<void(const QVariantMap &data)>;

    void runAction(const QString &action, const QVariantMap &args, Callback onSuccess);
    void applyOperation(const QString &operation, const QByteArray &content, const QString &summary);
    void setBusy(bool busy);
    Entry *currentEntry();
    Ops::Context demoContext() const;

    QString m_root;
    bool m_demo = false;
    FileListModel m_files;
    ChangeModel m_changes;
    HistoryModel m_history;
    QFutureWatcher<QList<Entry>> m_scan;
    bool m_busy = false;
    QString m_currentPath;
    QString m_currentKind;
};
