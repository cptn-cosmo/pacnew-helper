// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import gay.geeks.pacnewhelper.app

Kirigami.AbstractCard {
    id: card

    required property int row
    required property string kind
    required property var mineLines
    required property var theirsLines
    required property int mineLine
    required property int theirsLine
    required property string choice
    required property string recommended
    required property bool commentOnly
    property bool editable: true
    property bool wide: true

    readonly property bool needsDecision: kind === "conflict" || kind === "unknown"

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
                source: card.kind === "upstream" ? "system-software-update"
                      : card.kind === "local" ? "user-home"
                      : "dialog-question"
            }
            Kirigami.Heading {
                level: 4
                text: card.kind === "upstream" ? i18nc("@title", "Update from the package")
                    : card.kind === "local" ? i18nc("@title", "Your own change")
                    : card.kind === "conflict" ? i18nc("@title", "Changed on both sides")
                    : i18nc("@title", "Difference")
            }
            Kirigami.Chip {
                visible: card.commentOnly
                text: i18nc("@info badge", "Only comments")
                closable: false
                checkable: false
            }
            Kirigami.Chip {
                visible: card.needsDecision && card.editable
                text: i18nc("@info badge", "Your decision")
                closable: false
                checkable: false
            }
            Item {
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: i18nc("@info", "line %1", card.mineLine)
                font: Kirigami.Theme.smallFont
                opacity: 0.6
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.85
            text: {
                let t;
                if (card.kind === "upstream")
                    t = i18n("Only the new version changed this part; you never touched it. Taking it over is recommended.");
                else if (card.kind === "local")
                    t = i18n("This part was changed on this computer; the package did not change it. Keeping it is recommended.");
                else if (card.kind === "conflict")
                    t = i18n("Both this computer and the package changed this part. Your version is kept unless you choose the new one.");
                else if (card.mineLines.length === 0)
                    t = i18n("The new version adds this. Taking it over is recommended.");
                else if (card.theirsLines.length === 0)
                    t = i18n("Only your file contains this. Keeping it is recommended.");
                else
                    t = i18n("The two versions differ here. Your version is kept unless you choose the new one.");
                if (card.commentOnly)
                    t += " " + i18n("Only comments or empty lines differ, so either choice works the same.");
                return t;
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: card.wide ? 2 : 1
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.largeSpacing

            DiffPane {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                title: i18nc("@title", "Your current file")
                lines: card.mineLines
                otherLines: card.theirsLines
                firstLine: card.mineLine
                selected: card.choice === "mine"
            }
            DiffPane {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                title: i18nc("@title", "New version")
                lines: card.theirsLines
                otherLines: card.mineLines
                firstLine: card.theirsLine
                selected: card.choice === "theirs"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: card.editable
            spacing: Kirigami.Units.smallSpacing

            QQC2.ButtonGroup {
                id: choiceGroup
            }
            QQC2.Button {
                checkable: true
                checked: card.choice === "mine"
                QQC2.ButtonGroup.group: choiceGroup
                icon.name: "user-home"
                text: card.recommended === "mine" ? i18nc("@action:button", "Keep Mine (recommended)") : i18nc("@action:button", "Keep Mine")
                onClicked: App.changes.setChoice(card.row, "mine")
            }
            QQC2.Button {
                checkable: true
                checked: card.choice === "theirs"
                QQC2.ButtonGroup.group: choiceGroup
                icon.name: "system-software-update"
                text: card.recommended === "theirs" ? i18nc("@action:button", "Use New (recommended)") : i18nc("@action:button", "Use New")
                onClicked: App.changes.setChoice(card.row, "theirs")
            }
        }
    }
}
