// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plain-language knowledge about well-known configuration files, plus the
// safety rules that both the app and the privileged helper enforce.

#pragma once

#include <QString>

namespace FileInfo
{

enum class Policy {
    Normal,
    // The live file must never be replaced or merged (user accounts,
    // passwords). Only "keep my version" is offered and accepted.
    KeepOnly,
};

enum class Risk {
    Low,
    // A mistake can stop the system from booting or lock the user out.
    High,
};

struct Info {
    QString title; // short, friendly name ("Package manager settings")
    QString description; // what the file does, one or two sentences
    QString advice; // extra hint shown before applying, may be empty
    QString afterApply; // what to do after changing it, may be empty
    Policy policy = Policy::Normal;
    Risk risk = Risk::Low;
};

// path is the absolute path of the live file, e.g. "/etc/pacman.conf".
Info lookup(const QString &path);

// True for files that must be validated with visudo before being written.
bool isSudoers(const QString &path);

} // namespace FileInfo
