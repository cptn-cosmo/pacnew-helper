// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.delegates as Delegates
import gay.geeks.pacnewhelper.app

Kirigami.ScrollablePage {
    id: page

    property string pendingId

    title: i18nc("@title", "History")

    Component.onCompleted: App.reloadHistory()

    header: QQC2.Label {
        padding: Kirigami.Units.largeSpacing
        wrapMode: Text.Wrap
        text: i18n("Before every change, the previous files are backed up. Undo restores a file exactly as it was before that change.")
    }

    ListView {
        id: list
        model: App.history

        delegate: Delegates.RoundedItemDelegate {
            id: delegate

            required property string backupId
            required property string path
            required property string time
            required property string actionText

            width: ListView.view.width
            down: false

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: delegate.actionText
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: delegate.path
                        font.family: "monospace"
                        opacity: 0.75
                        elide: Text.ElideMiddle
                    }
                    QQC2.Label {
                        text: delegate.time
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                }
                QQC2.Button {
                    icon.name: "edit-undo"
                    text: i18nc("@action:button", "Undo")
                    enabled: !App.busy
                    onClicked: {
                        page.pendingId = delegate.backupId;
                        undoDialog.subtitle = i18n("%1 is put back exactly as it was before this change.", delegate.path);
                        undoDialog.open();
                    }
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "view-history"
            text: i18n("No changes yet")
            explanation: i18n("Files you update with Pacnew Helper will be listed here.")
        }
    }

    Kirigami.PromptDialog {
        id: undoDialog
        title: i18nc("@title:dialog", "Undo this change?")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: App.restoreBackup(page.pendingId)
    }
}
