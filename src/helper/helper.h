// SPDX-License-Identifier: GPL-3.0-or-later
//
// KAuth helper, runs as root. Each slot is one polkit-protected action; see
// gay.geeks.pacnewhelper.actions. All validation lives in core/operations.

#pragma once

#include <KAuth/ActionReply>

#include <QObject>
#include <QVariantMap>

class PacnewAuthHelper : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    KAuth::ActionReply apply(const QVariantMap &args);
    KAuth::ActionReply read(const QVariantMap &args);
    KAuth::ActionReply restore(const QVariantMap &args);
};
