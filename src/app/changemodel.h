// SPDX-License-Identifier: GPL-3.0-or-later
//
// The differences of the file under review, one row per region, together with
// the user's choice for each change ("mine" = keep the current text, "theirs"
// = take the new default). Builds the merged result from those choices.

#pragma once

#include "scanner.h"

#include <QAbstractListModel>
#include <QQmlEngine>

class ChangeModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App.changes")

    Q_PROPERTY(int changeCount READ changeCount NOTIFY changed)
    Q_PROPERTY(int upstreamCount READ upstreamCount NOTIFY changed)
    Q_PROPERTY(int localCount READ localCount NOTIFY changed)
    Q_PROPERTY(int decisionCount READ decisionCount NOTIFY changed)
    Q_PROPERTY(int takingNewCount READ takingNewCount NOTIFY choicesChanged)
    Q_PROPERTY(bool resultIsMine READ resultIsMine NOTIFY choicesChanged)
    Q_PROPERTY(bool resultIsNew READ resultIsNew NOTIFY choicesChanged)
    Q_PROPERTY(bool customized READ customized NOTIFY choicesChanged)

public:
    enum Role {
        KindRole = Qt::UserRole + 1, // unchanged | upstream | local | conflict | unknown
        IsChangeRole,
        MineLinesRole,
        TheirsLinesRole,
        MineLineRole, // 1-based
        TheirsLineRole,
        ChoiceRole, // "mine" | "theirs"
        RecommendedRole,
        CommentOnlyRole,
        LineCountRole, // for unchanged regions
    };
    Q_ENUM(Role)

    explicit ChangeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void load(const Entry &entry);
    void clear();

    Q_INVOKABLE void setChoice(int row, const QString &choice);
    Q_INVOKABLE void useRecommended();
    Q_INVOKABLE void chooseAll(const QString &choice);
    Q_INVOKABLE QString resultText() const;

    int changeCount() const;
    int upstreamCount() const;
    int localCount() const;
    int decisionCount() const;
    int takingNewCount() const;
    bool resultIsMine() const;
    bool resultIsNew() const;
    bool customized() const;

Q_SIGNALS:
    void changed();
    void choicesChanged();

private:
    struct Row {
        Merge::Region region;
        bool commentOnly = false;
        bool takeTheirs = false;
        bool recommendTheirs = false;
    };
    QList<Row> m_rows;
    bool m_keepOnly = false;
    bool m_trailingNewline = true;
    QStringList m_mine;
    QStringList m_theirs;

    QStringList resultLines() const;
};
