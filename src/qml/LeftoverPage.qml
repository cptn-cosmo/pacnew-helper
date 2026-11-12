// SPDX-License-Identifier: GPL-3.0-or-later
//
// .pacsave files (settings kept after a package was removed) and .pacnew files
// whose original file no longer exists.

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import gay.geeks.pacnewhelper.app

Kirigami.ScrollablePage {
    id: page

    readonly property var info: App.current
    readonly property bool pacsave: info.kind === "pacsave"

    title: info.title ?? ""

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.fillWidth: true
            text: page.info.path + "." + page.info.kind
            font.family: "monospace"
            opacity: 0.7
            elide: Text.ElideMiddle
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: page.pacsave
                  ? (page.info.liveExists
                     ? i18n("A program that used this file was removed or changed, and the settings you had were saved here. "
                            + "A new file is already in place, so these saved settings are usually no longer needed.")
                     : i18n("A program that used this file was removed, and the settings you had were saved here in case you install it again. "
                            + "If you don't plan to, they can be removed."))
                  : i18n("An update brought a new version of this file, but the original file no longer exists. "
                         + "You can put the new version in place or remove it.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.info.readable === false
            type: Kirigami.MessageType.Warning
            text: i18n("This file is protected. To see its contents, unlock it with your administrator password.")
            actions: Kirigami.Action {
                icon.name: "object-unlocked"
                text: i18nc("@action:button", "Unlock")
                enabled: !App.busy
                onTriggered: App.unlock()
            }
        }

        RowLayout {
            spacing: Kirigami.Units.largeSpacing

            QQC2.Button {
                icon.name: "edit-delete"
                text: page.pacsave ? i18nc("@action:button", "Remove Saved Settings") : i18nc("@action:button", "Remove New Version")
                highlighted: page.pacsave
                enabled: !App.busy
                onClicked: applicationWindow().confirmChange(page.pacsave ? "delete" : "keep")
            }
            QQC2.Button {
                visible: !page.info.liveExists
                icon.name: page.pacsave ? "edit-undo" : "document-new"
                text: page.pacsave ? i18nc("@action:button", "Put Back in Place") : i18nc("@action:button", "Put New Version in Place")
                highlighted: !page.pacsave
                enabled: !App.busy
                onClicked: applicationWindow().confirmChange(page.pacsave ? "restore-leftover" : "replace")
            }
        }

        Kirigami.Heading {
            level: 3
            visible: page.info.readable !== false
            text: i18nc("@title", "Contents")
        }

        QQC2.TextArea {
            Layout.fillWidth: true
            visible: page.info.readable !== false
            readOnly: true
            text: page.info.leftoverText ?? ""
            textFormat: TextEdit.PlainText
            font.family: "monospace"
            wrapMode: TextEdit.Wrap
        }
    }
}
