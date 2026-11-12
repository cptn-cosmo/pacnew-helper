// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shown before anything is written: what will happen, and exactly how each
// file changes. Only "Approve" runs the action.

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import gay.geeks.pacnewhelper.app

Kirigami.ScrollablePage {
    id: page

    required property string action
    property string arg: ""
    // Taken once when the page opens, so it can't change under the user's eyes.
    readonly property var summary: App.preview(action, arg)

    signal approved
    signal cancelled

    title: i18nc("@title", "Check Before Saving")

    footer: QQC2.ToolBar {
        contentItem: RowLayout {
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                Layout.fillWidth: true
                text: i18n("Nothing has been changed yet.")
                opacity: 0.8
                elide: Text.ElideRight
            }
            QQC2.BusyIndicator {
                visible: App.busy
                running: visible
                implicitHeight: Kirigami.Units.iconSizes.medium
            }
            QQC2.Button {
                icon.name: "dialog-cancel"
                text: i18nc("@action:button", "Cancel")
                enabled: !App.busy
                onClicked: page.cancelled()
            }
            QQC2.Button {
                icon.name: "dialog-ok-apply"
                text: page.summary.actionText ?? i18nc("@action:button", "Approve and Save")
                highlighted: true
                enabled: !App.busy && (page.summary.items ?? []).length > 0
                onClicked: {
                    App.execute(page.action, page.arg);
                    page.approved();
                }
            }
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Heading {
            Layout.fillWidth: true
            level: 1
            text: page.summary.title ?? ""
            wrapMode: Text.Wrap
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.summary.highRisk === true
            type: Kirigami.MessageType.Warning
            text: i18n("This is an important system file. Please read the changes below carefully before approving.")
        }

        // --- what will happen ------------------------------------------------
        Kirigami.Heading {
            level: 3
            text: i18nc("@title", "What will happen")
        }

        Repeater {
            model: page.summary.bullets ?? []
            delegate: RowLayout {
                required property string modelData
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                    source: "arrow-right"
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: modelData
                    wrapMode: Text.Wrap
                }
            }
        }

        // --- per file ----------------------------------------------------------
        Repeater {
            model: page.summary.items ?? []

            delegate: Kirigami.AbstractCard {
                id: fileCard

                required property var modelData
                readonly property var rows: modelData.rows ?? []
                property bool expanded: false
                readonly property int limit: 200
                readonly property var shownRows: expanded || rows.length <= limit ? rows : rows.slice(0, limit)

                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: fileCard.modelData.path
                            font.family: "monospace"
                            font.bold: true
                            elide: Text.ElideMiddle
                        }
                        QQC2.Label {
                            visible: fileCard.modelData.added > 0
                            text: i18ncp("@info lines added", "+%1 line", "+%1 lines", fileCard.modelData.added)
                            color: Kirigami.Theme.positiveTextColor
                        }
                        QQC2.Label {
                            visible: fileCard.modelData.removed > 0
                            text: i18ncp("@info lines removed", "−%1 line", "−%1 lines", fileCard.modelData.removed)
                            color: Kirigami.Theme.negativeTextColor
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: fileCard.modelData.heading
                        opacity: 0.8
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: (fileCard.modelData.note ?? "").length > 0
                        text: fileCard.modelData.note ?? ""
                        wrapMode: Text.Wrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        visible: fileCard.rows.length > 0
                        implicitHeight: diffColumn.implicitHeight + Kirigami.Units.smallSpacing * 2
                        radius: Kirigami.Units.cornerRadius
                        color: Kirigami.Theme.backgroundColor
                        border.width: 1
                        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
                        Kirigami.Theme.colorSet: Kirigami.Theme.View
                        Kirigami.Theme.inherit: false

                        ColumnLayout {
                            id: diffColumn
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.smallSpacing
                            spacing: 0

                            Repeater {
                                model: fileCard.shownRows
                                delegate: Rectangle {
                                    required property var modelData
                                    readonly property string type: modelData.type

                                    Layout.fillWidth: true
                                    implicitHeight: rowLayout.implicitHeight
                                    color: type === "added" ? Kirigami.Theme.positiveBackgroundColor
                                         : type === "removed" ? Kirigami.Theme.negativeBackgroundColor
                                         : "transparent"

                                    RowLayout {
                                        id: rowLayout
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        spacing: Kirigami.Units.smallSpacing

                                        QQC2.Label {
                                            visible: type !== "gap"
                                            Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                                            Layout.alignment: Qt.AlignTop
                                            horizontalAlignment: Text.AlignRight
                                            // numbering of the file after saving; removed lines have none
                                            text: type === "removed" ? "" : modelData.newLine
                                            font.family: "monospace"
                                            opacity: 0.5
                                        }
                                        QQC2.Label {
                                            visible: type !== "gap"
                                            Layout.alignment: Qt.AlignTop
                                            text: type === "added" ? "+" : type === "removed" ? "−" : " "
                                            font.family: "monospace"
                                            font.bold: true
                                        }
                                        QQC2.Label {
                                            Layout.fillWidth: true
                                            text: type === "gap"
                                                  ? i18ncp("@info", "⋯ 1 unchanged line ⋯", "⋯ %1 unchanged lines ⋯", modelData.count)
                                                  : (modelData.text.length > 0 ? modelData.text : " ")
                                            horizontalAlignment: type === "gap" ? Text.AlignHCenter : Text.AlignLeft
                                            font.family: type === "gap" ? Kirigami.Theme.defaultFont.family : "monospace"
                                            opacity: type === "gap" ? 0.6 : 1
                                            wrapMode: Text.WrapAnywhere
                                            textFormat: Text.PlainText
                                        }
                                    }
                                }
                            }

                            QQC2.Button {
                                visible: !fileCard.expanded && fileCard.rows.length > fileCard.limit
                                flat: true
                                icon.name: "arrow-down"
                                text: i18nc("@action:button", "Show everything")
                                onClicked: fileCard.expanded = true
                            }
                        }
                    }
                }
            }
        }
    }
}
