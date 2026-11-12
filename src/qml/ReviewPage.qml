// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import gay.geeks.pacnewhelper.app

Kirigami.ScrollablePage {
    id: page

    signal previewRequested

    readonly property var info: App.current
    readonly property var changes: App.changes
    readonly property bool writable: info.readable && !info.managed && !info.keepOnly && !info.identical
    readonly property bool hasChanges: changes.changeCount > 0

    title: info.title ?? ""

    actions: [
        Kirigami.Action {
            icon.name: "dialog-ok-apply"
            text: page.changes.resultIsMine ? i18nc("@action:button", "Keep My Version") : i18nc("@action:button", "Apply")
            tooltip: i18n("Save the file with the choices below")
            visible: page.writable && page.hasChanges
            enabled: !App.busy
            onTriggered: applicationWindow().confirmChange("apply")
        },
        Kirigami.Action {
            icon.name: "document-preview"
            text: i18nc("@action:button", "Preview Result")
            visible: page.writable && page.hasChanges
            onTriggered: page.previewRequested()
        },
        Kirigami.Action {
            icon.name: "edit-undo"
            text: i18nc("@action:button", "Reset to Recommended")
            visible: page.writable && page.changes.customized
            displayHint: Kirigami.DisplayHint.AlwaysHide
            onTriggered: page.changes.useRecommended()
        },
        Kirigami.Action {
            icon.name: "user-home"
            text: i18nc("@action:button", "Keep My Version Completely")
            visible: page.info.readable !== false
            enabled: !App.busy
            displayHint: Kirigami.DisplayHint.AlwaysHide
            onTriggered: applicationWindow().confirmChange("keep")
        },
        Kirigami.Action {
            icon.name: "document-new"
            text: i18nc("@action:button", "Use the New Version Completely")
            visible: page.writable
            enabled: !App.busy
            displayHint: Kirigami.DisplayHint.AlwaysHide
            onTriggered: applicationWindow().confirmChange("replace")
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        // --- what is this file, what happened -------------------------------
        QQC2.Label {
            Layout.fillWidth: true
            text: page.info.description ?? ""
            wrapMode: Text.Wrap
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: page.info.package
                  ? i18nc("%1 is a path, %2 a package name", "%1 · from the package %2", page.info.path, page.info.package)
                  : page.info.path ?? ""
            font.family: "monospace"
            opacity: 0.7
            elide: Text.ElideMiddle
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.info.readable && !page.info.identical
            type: Kirigami.MessageType.Information
            text: page.info.package
                  ? i18n("An update of %1 brought a new version of this file. Your file was left untouched; the new version was saved next to it as %2.",
                         page.info.package, page.info.fileName + ".pacnew")
                  : i18n("A software update brought a new version of this file. Your file was left untouched; the new version was saved next to it as %1.",
                         page.info.fileName + ".pacnew")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.info.identical === true
            type: Kirigami.MessageType.Positive
            text: i18n("The new version is exactly the same as yours. The extra copy can be removed safely.")
            actions: Kirigami.Action {
                icon.name: "edit-clear-all"
                text: i18nc("@action:button", "Remove Extra Copy")
                enabled: !App.busy
                onTriggered: applicationWindow().confirmChange("keep")
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.info.managed === true
            type: Kirigami.MessageType.Information
            text: page.info.symlink
                  ? i18n("This file is managed by another program (it is a link to a file that program keeps up to date). It is best left as it is. "
                         + "Keeping your version simply removes the extra copy.")
                  : i18n("This file is written by a program, as its first lines say: “%1”. That program keeps it up to date, so it is best left as it is. "
                         + "Keeping your version simply removes the extra copy.", page.info.generatedBy ?? "")
            actions: Kirigami.Action {
                icon.name: "user-home"
                text: i18nc("@action:button", "Keep My Version")
                enabled: !App.busy
                onTriggered: applicationWindow().confirmChange("keep")
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.info.readable === false
            type: Kirigami.MessageType.Warning
            text: i18n("This file is protected. To compare the two versions, unlock it with your administrator password.")
            actions: Kirigami.Action {
                icon.name: "object-unlocked"
                text: i18nc("@action:button", "Unlock")
                enabled: !App.busy
                onTriggered: App.unlock()
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: (page.info.advice ?? "").length > 0 && !page.info.identical && !page.info.managed
            type: page.info.highRisk ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
            text: page.info.advice ?? ""
            actions: page.info.keepOnly ? [keepOnlyAction] : []

            Kirigami.Action {
                id: keepOnlyAction
                icon.name: "user-home"
                text: i18nc("@action:button", "Keep My Version")
                enabled: !App.busy
                onTriggered: applicationWindow().confirmChange("keep")
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.writable && page.hasChanges && !page.info.hasBase
            type: Kirigami.MessageType.Information
            text: i18n("The previous version of this file is no longer available, so it can't be told which differences are your own changes. "
                       + "Additions from the new version are taken, everything else keeps your version. Please check the highlighted differences.")
        }

        // --- summary --------------------------------------------------------
        GridLayout {
            Layout.fillWidth: true
            visible: page.writable && page.hasChanges
            columns: page.width > Kirigami.Units.gridUnit * 30 ? 3 : 1
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.largeSpacing

            StatTile {
                Layout.fillWidth: true
                value: page.changes.upstreamCount
                label: i18n("Updates from the package")
                hint: i18n("taken over")
                iconName: "system-software-update"
            }
            StatTile {
                Layout.fillWidth: true
                value: page.changes.localCount
                label: i18n("Your own changes")
                hint: i18n("kept")
                iconName: "user-home"
            }
            StatTile {
                Layout.fillWidth: true
                value: page.changes.decisionCount
                label: i18n("Need your decision")
                hint: page.changes.decisionCount > 0 ? i18n("your version is kept unless you choose otherwise") : i18n("nothing to decide")
                iconName: "dialog-question"
                attention: page.changes.decisionCount > 0
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: page.writable && page.hasChanges
            spacing: Kirigami.Units.largeSpacing

            QQC2.Button {
                icon.name: "dialog-ok-apply"
                text: page.changes.resultIsMine ? i18nc("@action:button", "Keep My Version") : i18nc("@action:button", "Apply These Choices")
                highlighted: true
                enabled: !App.busy
                onClicked: applicationWindow().confirmChange("apply")
            }
            QQC2.Button {
                icon.name: "document-preview"
                text: i18nc("@action:button", "Preview Result")
                onClicked: page.previewRequested()
            }
            Item {
                Layout.fillWidth: true
            }
            QQC2.BusyIndicator {
                visible: App.busy
                running: visible
                implicitHeight: Kirigami.Units.iconSizes.medium
            }
        }

        // --- the changes ----------------------------------------------------
        Kirigami.Heading {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            visible: page.hasChanges && page.info.readable
            level: 2
            text: i18ncp("@title", "1 difference", "%1 differences", page.changes.changeCount)
        }

        Repeater {
            model: page.info.readable ? page.changes : null

            delegate: Loader {
                id: rowLoader

                required property int index
                required property string kind
                required property bool isChange
                required property var mineLines
                required property var theirsLines
                required property int mineLine
                required property int theirsLine
                required property string choice
                required property string recommended
                required property bool commentOnly
                required property int lineCount

                Layout.fillWidth: true
                sourceComponent: isChange ? changeCard : unchangedRow

                Component {
                    id: changeCard
                    ChangeCard {
                        row: rowLoader.index
                        kind: rowLoader.kind
                        mineLines: rowLoader.mineLines
                        theirsLines: rowLoader.theirsLines
                        mineLine: rowLoader.mineLine
                        theirsLine: rowLoader.theirsLine
                        choice: rowLoader.choice
                        recommended: rowLoader.recommended
                        commentOnly: rowLoader.commentOnly
                        editable: page.writable
                        wide: page.width > Kirigami.Units.gridUnit * 40
                    }
                }
                Component {
                    id: unchangedRow
                    QQC2.Label {
                        horizontalAlignment: Text.AlignHCenter
                        text: i18ncp("@info", "⋯ 1 line is the same in both versions ⋯", "⋯ %1 lines are the same in both versions ⋯", rowLoader.lineCount)
                        font: Kirigami.Theme.smallFont
                        opacity: 0.6
                    }
                }
            }
        }
    }
}
