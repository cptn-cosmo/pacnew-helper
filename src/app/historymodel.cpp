// SPDX-License-Identifier: GPL-3.0-or-later

#include "historymodel.h"

#include <KFormat>
#include <KLocalizedString>

#include <QLocale>

using namespace Qt::StringLiterals;

HistoryModel::HistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int HistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_backups.size());
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const Ops::Backup &b = m_backups.at(index.row());
    switch (role) {
    case IdRole:
        return b.id;
    case PathRole:
        return b.path;
    case TimeRole:
        return KFormat().formatRelativeDateTime(b.time.toLocalTime(), QLocale::ShortFormat);
    case ActionRole:
        if (b.operation == u"merge")
            return i18nc("@info history entry", "Updated with your choices");
        if (b.operation == u"replace")
            return i18nc("@info history entry", "Replaced with the new version");
        if (b.operation == u"keep")
            return i18nc("@info history entry", "Kept your version, removed the new copy");
        if (b.operation == u"delete")
            return i18nc("@info history entry", "Removed saved settings");
        if (b.operation == u"restore-leftover")
            return i18nc("@info history entry", "Restored saved settings");
        if (b.operation == u"restore")
            return i18nc("@info history entry", "Undid an earlier change");
        return b.operation;
    case CanRestoreRole:
        return true;
    }
    return {};
}

QHash<int, QByteArray> HistoryModel::roleNames() const
{
    return {
        {IdRole, "backupId"},
        {PathRole, "path"},
        {TimeRole, "time"},
        {ActionRole, "actionText"},
        {CanRestoreRole, "canRestore"},
    };
}

void HistoryModel::reload(const Ops::Context &ctx)
{
    beginResetModel();
    m_backups = Ops::listBackups(ctx);
    endResetModel();
    Q_EMIT countChanged();
}

int HistoryModel::count() const
{
    return int(m_backups.size());
}
