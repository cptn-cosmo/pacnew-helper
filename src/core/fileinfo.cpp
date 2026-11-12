// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileinfo.h"

#include <KLocalizedString>

#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace FileInfo
{

namespace
{
Info accounts(const QString &what)
{
    Info i;
    i.title = what;
    i.description = i18n("This file stores the user accounts and groups of this computer, including yours.");
    i.advice = i18n("Replacing it would delete accounts and could lock you out. New system accounts are created automatically, "
                    "so keeping your version is always the right choice here.");
    i.policy = Policy::KeepOnly;
    i.risk = Risk::High;
    return i;
}
} // namespace

Info lookup(const QString &path)
{
    const QString name = QFileInfo(path).fileName();

    if (path == u"/etc/passwd")
        return accounts(i18n("User accounts"));
    if (path == u"/etc/shadow")
        return accounts(i18n("User passwords"));
    if (path == u"/etc/group")
        return accounts(i18n("User groups"));
    if (path == u"/etc/gshadow")
        return accounts(i18n("Group passwords"));

    Info i;
    if (path == u"/etc/pacman.conf") {
        i.title = i18n("Package manager settings");
        i.description = i18n("Controls how software is installed and updated: which repositories are used, parallel downloads, colours and more.");
        i.advice = i18n("Repository sections you added (for example extra repositories) are part of your own changes and are kept.");
    } else if (name == u"mirrorlist" || name.endsWith(u"-mirrorlist")) {
        i.title = i18n("Download servers");
        i.description = i18n("The list of servers software updates are downloaded from.");
        i.advice = i18n("Your version may be sorted by speed. Keeping it is fine; the new version mainly brings an updated list of servers.");
    } else if (path == u"/etc/makepkg.conf" || path.startsWith(u"/etc/makepkg.conf.d/")) {
        i.title = i18n("Package build settings");
        i.description = i18n("Settings used when software is compiled on this computer (for example from the AUR).");
    } else if (path == u"/etc/fstab") {
        i.title = i18n("Drives mounted at startup");
        i.description = i18n("Lists which drives and partitions are connected automatically when the computer starts.");
        i.advice = i18n("A wrong entry here can stop the computer from starting. Keep your own entries.");
        i.risk = Risk::High;
    } else if (path == u"/etc/crypttab") {
        i.title = i18n("Encrypted drives");
        i.description = i18n("Lists encrypted drives that are unlocked at startup.");
        i.advice = i18n("A wrong entry here can stop the computer from starting. Keep your own entries.");
        i.risk = Risk::High;
    } else if (isSudoers(path)) {
        i.title = i18n("Administrator permissions");
        i.description = i18n("Controls who may run commands as administrator (sudo).");
        i.advice = i18n("A mistake here can take away your administrator rights. The result is checked before it is saved.");
        i.risk = Risk::High;
    } else if (path == u"/etc/mkinitcpio.conf" || path.startsWith(u"/etc/mkinitcpio.d/")) {
        i.title = i18n("Startup image settings");
        i.description = i18n("Controls what goes into the image that starts the system (drivers, encryption, hooks).");
        i.afterApply = i18n("The startup image is rebuilt automatically with the next kernel update. To use the change right away, run “sudo mkinitcpio -P”.");
        i.risk = Risk::High;
    } else if (path == u"/etc/default/grub") {
        i.title = i18n("Boot menu settings");
        i.description = i18n("Settings for the GRUB boot menu.");
        i.afterApply = i18n("Run “sudo grub-mkconfig -o /boot/grub/grub.cfg” to apply the change to the boot menu.");
        i.risk = Risk::High;
    } else if (path == u"/etc/locale.gen") {
        i.title = i18n("Languages and regional formats");
        i.description = i18n("Lists the languages and regional formats that are prepared for use.");
        i.advice = i18n("The languages you enabled are part of your own changes and are kept.");
        i.afterApply = i18n("Run “sudo locale-gen” if you enabled new languages.");
    } else if (path == u"/etc/resolv.conf") {
        i.title = i18n("Name server settings");
        i.description = i18n("Tells the computer which servers translate web addresses. On most systems another program manages this file.");
        i.advice = i18n("If it is managed by another program (for example systemd-resolved or NetworkManager), keep your version.");
    } else if (path == u"/etc/hosts") {
        i.title = i18n("Local host names");
        i.description = i18n("Fixed names for addresses on this computer or your network.");
    } else if (path == u"/etc/ssh/sshd_config" || path.startsWith(u"/etc/ssh/sshd_config.d/")) {
        i.title = i18n("Remote login (SSH server)");
        i.description = i18n("Controls who may log in to this computer over the network.");
        i.risk = Risk::High;
    } else if (path == u"/etc/ssh/ssh_config") {
        i.title = i18n("SSH client settings");
        i.description = i18n("Default settings for connecting to other computers over SSH.");
    } else if (path.startsWith(u"/etc/systemd/")) {
        i.title = i18n("System service settings");
        i.description = i18n("Settings for systemd, which starts and manages system services.");
    } else if (path.startsWith(u"/etc/pam.d/")) {
        i.title = i18n("Login and authentication rules");
        i.description = i18n("Controls how logging in, unlocking and password checks work.");
        i.advice = i18n("A mistake here can make it impossible to log in.");
        i.risk = Risk::High;
    } else if (path == u"/etc/environment") {
        i.title = i18n("System-wide environment");
        i.description = i18n("Variables set for every user and program.");
    } else if (path == u"/etc/shells") {
        i.title = i18n("Allowed login shells");
        i.description = i18n("Lists the command shells users may log in with.");
    } else if (path == u"/etc/nsswitch.conf") {
        i.title = i18n("Name lookup order");
        i.description = i18n("Defines where the system looks up user names, host names and similar information.");
        i.risk = Risk::High;
    } else {
        i.title = name;
        i.description = i18n("A system configuration file.");
    }
    return i;
}

bool isSudoers(const QString &path)
{
    return path == u"/etc/sudoers" || path.startsWith(u"/etc/sudoers.d/");
}

} // namespace FileInfo
