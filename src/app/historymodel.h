// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "operations.h"

#include <QAbstractListModel>
#include <QQmlEngine>

class HistoryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App.history")

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PathRole,
        TimeRole,
        ActionRole,
        CanRestoreRole,
    };

    explicit HistoryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reload(const Ops::Context &ctx);
    int count() const;

Q_SIGNALS:
    void countChanged();

private:
    QList<Ops::Backup> m_backups;
};
