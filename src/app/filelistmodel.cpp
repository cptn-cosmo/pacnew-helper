// SPDX-License-Identifier: GPL-3.0-or-later

#include "filelistmodel.h"
#include "fileinfo.h"

#include <KLocalizedString>

#include <QFileInfo>

using namespace Qt::StringLiterals;

FileListModel::FileListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const Entry &e = m_entries.at(index.row());
    const auto status = e.status();
    switch (role) {
    case PathRole:
        return e.path;
    case FileNameRole:
        return QFileInfo(e.path).fileName();
    case KindRole:
        return e.kind;
    case TitleRole:
    case Qt::DisplayRole:
        return FileInfo::lookup(e.path).title;
    case PackageRole:
        return e.package;
    case StatusRole:
        return statusId(status);
    case StatusTextRole:
        return statusText(e);
    case StatusIconRole:
        return statusIcon(status);
    case GroupRole:
        switch (status) {
        case Entry::Status::NeedsReview:
        case Entry::Status::Protected:
        case Entry::Status::KeepOnly:
            return u"attention"_s;
        default:
            return u"ready"_s;
        }
    case SummaryRole:
        return summary(e);
    case HighRiskRole:
        return FileInfo::lookup(e.path).risk == FileInfo::Risk::High;
    }
    return {};
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {FileNameRole, "fileName"},
        {KindRole, "kind"},
        {TitleRole, "title"},
        {PackageRole, "packageName"},
        {StatusRole, "status"},
        {StatusTextRole, "statusText"},
        {StatusIconRole, "statusIcon"},
        {GroupRole, "group"},
        {SummaryRole, "summary"},
        {HighRiskRole, "highRisk"},
    };
}

void FileListModel::setEntries(QList<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    // Things that need the user first, then the rest; alphabetical within.
    std::stable_sort(m_entries.begin(), m_entries.end(), [](const Entry &a, const Entry &b) {
        auto rank = [](const Entry &e) {
            switch (e.status()) {
            case Entry::Status::NeedsReview:
            case Entry::Status::Protected:
            case Entry::Status::KeepOnly:
                return 0;
            default:
                return 1;
            }
        };
        return rank(a) < rank(b);
    });
    endResetModel();
    Q_EMIT countChanged();
}

const QList<Entry> &FileListModel::entries() const
{
    return m_entries;
}

Entry *FileListModel::entry(int row)
{
    return row >= 0 && row < m_entries.size() ? &m_entries[row] : nullptr;
}

void FileListModel::updateEntry(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;
    Q_EMIT dataChanged(index(row), index(row));
    Q_EMIT countChanged();
}

void FileListModel::removeEntry(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;
    beginRemoveRows({}, row, row);
    m_entries.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
}

int FileListModel::indexOf(const QString &path, const QString &kind) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].path == path && m_entries[i].kind == kind)
            return i;
    }
    return -1;
}

int FileListModel::count() const
{
    return int(m_entries.size());
}

int FileListModel::simpleKeepCount() const
{
    return int(std::count_if(m_entries.cbegin(), m_entries.cend(), [](const Entry &e) {
        return e.canSimplyKeep();
    }));
}

int FileListModel::attentionCount() const
{
    return int(std::count_if(m_entries.cbegin(), m_entries.cend(), [](const Entry &e) {
        const auto s = e.status();
        return s == Entry::Status::NeedsReview || s == Entry::Status::Protected || s == Entry::Status::KeepOnly;
    }));
}

QString FileListModel::statusId(Entry::Status status)
{
    switch (status) {
    case Entry::Status::Identical:
        return u"identical"_s;
    case Entry::Status::Automatic:
        return u"automatic"_s;
    case Entry::Status::NeedsReview:
        return u"review"_s;
    case Entry::Status::KeepOnly:
        return u"keeponly"_s;
    case Entry::Status::Managed:
        return u"managed"_s;
    case Entry::Status::Protected:
        return u"protected"_s;
    case Entry::Status::Leftover:
        return u"leftover"_s;
    case Entry::Status::Orphaned:
        return u"orphaned"_s;
    }
    return {};
}

QString FileListModel::statusText(const Entry &e)
{
    switch (e.status()) {
    case Entry::Status::Identical:
        return i18nc("@info:status", "No changes, extra copy can be removed");
    case Entry::Status::Automatic:
        if (e.upstream == 0)
            return i18nc("@info:status", "Nothing new, only your own changes differ");
        return i18nc("@info:status", "Ready to update automatically");
    case Entry::Status::NeedsReview:
        return i18ncp("@info:status", "1 change needs your decision", "%1 changes need your decision", e.conflicts + e.unknown);
    case Entry::Status::KeepOnly:
        return i18nc("@info:status", "Keep your version");
    case Entry::Status::Managed:
        return i18nc("@info:status", "Maintained by a program, keep your version");
    case Entry::Status::Protected:
        return i18nc("@info:status", "Protected, unlock to review");
    case Entry::Status::Leftover:
        return i18nc("@info:status", "Saved settings of a removed program");
    case Entry::Status::Orphaned:
        return i18nc("@info:status", "Original file is missing");
    }
    return {};
}

QString FileListModel::statusIcon(Entry::Status status)
{
    switch (status) {
    case Entry::Status::Identical:
        return u"edit-clear-all"_s;
    case Entry::Status::Automatic:
        return u"emblem-ok-symbolic"_s;
    case Entry::Status::NeedsReview:
        return u"dialog-warning-symbolic"_s;
    case Entry::Status::KeepOnly:
        return u"security-high-symbolic"_s;
    case Entry::Status::Managed:
        return u"emblem-symbolic-link"_s;
    case Entry::Status::Protected:
        return u"object-locked"_s;
    case Entry::Status::Leftover:
        return u"edit-delete-symbolic"_s;
    case Entry::Status::Orphaned:
        return u"dialog-question-symbolic"_s;
    }
    return {};
}

QString FileListModel::summary(const Entry &e)
{
    if (!e.readable || !e.liveExists || e.identical || e.kind == u"pacsave")
        return {};
    QStringList parts;
    if (e.upstream > 0)
        parts << i18ncp("@info", "1 update from the package", "%1 updates from the package", e.upstream);
    if (e.local > 0)
        parts << i18ncp("@info", "1 of your changes kept", "%1 of your changes kept", e.local);
    return parts.join(u" · "_s);
}
