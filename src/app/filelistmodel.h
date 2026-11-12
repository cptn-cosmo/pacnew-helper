// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "scanner.h"

#include <QAbstractListModel>
#include <QQmlEngine>

class FileListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App.files")

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int simpleKeepCount READ simpleKeepCount NOTIFY countChanged)
    Q_PROPERTY(int attentionCount READ attentionCount NOTIFY countChanged)

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        FileNameRole,
        KindRole,
        TitleRole,
        PackageRole,
        StatusRole, // string id, see statusId()
        StatusTextRole,
        StatusIconRole,
        GroupRole, // "attention" or "ready"
        SummaryRole,
        HighRiskRole,
    };
    Q_ENUM(Role)

    explicit FileListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QList<Entry> entries);
    const QList<Entry> &entries() const;
    Entry *entry(int row);
    void updateEntry(int row);
    void removeEntry(int row);
    int indexOf(const QString &path, const QString &kind) const;

    int count() const;
    int simpleKeepCount() const;
    int attentionCount() const;

    static QString statusId(Entry::Status status);
    static QString statusText(const Entry &entry);
    static QString statusIcon(Entry::Status status);
    static QString summary(const Entry &entry);

Q_SIGNALS:
    void countChanged();

private:
    QList<Entry> m_entries;
};
