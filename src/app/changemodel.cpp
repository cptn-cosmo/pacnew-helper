// SPDX-License-Identifier: GPL-3.0-or-later

#include "changemodel.h"
#include "fileinfo.h"

using namespace Qt::StringLiterals;

namespace
{
QString kindId(Merge::Kind kind)
{
    switch (kind) {
    case Merge::Kind::Unchanged:
        return u"unchanged"_s;
    case Merge::Kind::Upstream:
        return u"upstream"_s;
    case Merge::Kind::Local:
        return u"local"_s;
    case Merge::Kind::Conflict:
        return u"conflict"_s;
    case Merge::Kind::Unknown:
        return u"unknown"_s;
    }
    return {};
}

// What a careful administrator would pick by default.
bool recommendTheirs(const Merge::Region &r, bool commentOnly)
{
    switch (r.kind) {
    case Merge::Kind::Upstream:
        return true; // the package updated a part the user never touched
    case Merge::Kind::Local:
    case Merge::Kind::Conflict:
        return false; // the user's own setting wins
    case Merge::Kind::Unknown:
        // Without the previous default we can't tell who changed what:
        // take pure additions and comment updates, keep everything else.
        if (r.mine.isEmpty())
            return true;
        return commentOnly && !r.theirs.isEmpty();
    case Merge::Kind::Unchanged:
        return false;
    }
    return false;
}
} // namespace

ChangeModel::ChangeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ChangeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant ChangeModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const Row &row = m_rows.at(index.row());
    const Merge::Region &r = row.region;
    switch (role) {
    case KindRole:
        return kindId(r.kind);
    case IsChangeRole:
        return r.kind != Merge::Kind::Unchanged;
    case MineLinesRole:
        return r.mine;
    case TheirsLinesRole:
        return r.theirs;
    case MineLineRole:
        return int(r.mineLine + 1);
    case TheirsLineRole:
        return int(r.theirsLine + 1);
    case ChoiceRole:
        return row.takeTheirs ? u"theirs"_s : u"mine"_s;
    case RecommendedRole:
        return row.recommendTheirs ? u"theirs"_s : u"mine"_s;
    case CommentOnlyRole:
        return row.commentOnly;
    case LineCountRole:
        return int(r.mine.size());
    }
    return {};
}

QHash<int, QByteArray> ChangeModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {IsChangeRole, "isChange"},
        {MineLinesRole, "mineLines"},
        {TheirsLinesRole, "theirsLines"},
        {MineLineRole, "mineLine"},
        {TheirsLineRole, "theirsLine"},
        {ChoiceRole, "choice"},
        {RecommendedRole, "recommended"},
        {CommentOnlyRole, "commentOnly"},
        {LineCountRole, "lineCount"},
    };
}

void ChangeModel::load(const Entry &entry)
{
    beginResetModel();
    m_rows.clear();
    m_keepOnly = FileInfo::lookup(entry.path).policy == FileInfo::Policy::KeepOnly;
    m_mine = entry.live;
    m_theirs = entry.theirs;
    m_trailingNewline = entry.theirs.isEmpty() ? entry.liveTrailingNewline : entry.newTrailingNewline;
    if (entry.readable && entry.liveExists) {
        for (const Merge::Region &r : entry.regions()) {
            Row row;
            row.region = r;
            row.commentOnly = r.kind != Merge::Kind::Unchanged && Merge::isCommentOnly(r);
            row.recommendTheirs = !m_keepOnly && recommendTheirs(r, row.commentOnly);
            row.takeTheirs = row.recommendTheirs;
            m_rows.append(row);
        }
    }
    endResetModel();
    Q_EMIT changed();
    Q_EMIT choicesChanged();
}

void ChangeModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_mine.clear();
    m_theirs.clear();
    endResetModel();
    Q_EMIT changed();
    Q_EMIT choicesChanged();
}

void ChangeModel::setChoice(int row, const QString &choice)
{
    if (row < 0 || row >= m_rows.size() || m_rows[row].region.kind == Merge::Kind::Unchanged)
        return;
    const bool theirs = choice == u"theirs" && !m_keepOnly;
    if (m_rows[row].takeTheirs == theirs)
        return;
    m_rows[row].takeTheirs = theirs;
    Q_EMIT dataChanged(index(row), index(row), {ChoiceRole});
    Q_EMIT choicesChanged();
}

void ChangeModel::useRecommended()
{
    for (int i = 0; i < m_rows.size(); ++i)
        m_rows[i].takeTheirs = m_rows[i].recommendTheirs;
    if (!m_rows.isEmpty())
        Q_EMIT dataChanged(index(0), index(int(m_rows.size()) - 1), {ChoiceRole});
    Q_EMIT choicesChanged();
}

void ChangeModel::chooseAll(const QString &choice)
{
    const bool theirs = choice == u"theirs" && !m_keepOnly;
    for (Row &row : m_rows) {
        if (row.region.kind != Merge::Kind::Unchanged)
            row.takeTheirs = theirs;
    }
    if (!m_rows.isEmpty())
        Q_EMIT dataChanged(index(0), index(int(m_rows.size()) - 1), {ChoiceRole});
    Q_EMIT choicesChanged();
}

QStringList ChangeModel::resultLines() const
{
    QStringList out;
    for (const Row &row : m_rows)
        out += row.takeTheirs ? row.region.theirs : row.region.mine;
    return out;
}

QString ChangeModel::resultText() const
{
    return Merge::joinLines(resultLines(), m_trailingNewline);
}

int ChangeModel::changeCount() const
{
    return int(std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &r) {
        return r.region.kind != Merge::Kind::Unchanged;
    }));
}

int ChangeModel::upstreamCount() const
{
    return int(std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &r) {
        return r.region.kind == Merge::Kind::Upstream;
    }));
}

int ChangeModel::localCount() const
{
    return int(std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &r) {
        return r.region.kind == Merge::Kind::Local;
    }));
}

int ChangeModel::decisionCount() const
{
    return int(std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &r) {
        return r.region.kind == Merge::Kind::Conflict || r.region.kind == Merge::Kind::Unknown;
    }));
}

int ChangeModel::takingNewCount() const
{
    return int(std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &r) {
        return r.region.kind != Merge::Kind::Unchanged && r.takeTheirs;
    }));
}

bool ChangeModel::resultIsMine() const
{
    return resultLines() == m_mine;
}

bool ChangeModel::resultIsNew() const
{
    return resultLines() == m_theirs;
}

bool ChangeModel::customized() const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [](const Row &r) {
        return r.takeTheirs != r.recommendTheirs;
    });
}
