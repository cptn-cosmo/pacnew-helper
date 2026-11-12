// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.delegates as Delegates
import gay.geeks.pacnewhelper.app

Kirigami.ScrollablePage {
    id: page

    signal openRequested(string path, string kind)
    signal historyRequested
    signal aboutRequested

    title: i18nc("@title", "Configuration Updates")

    actions: [
        Kirigami.Action {
            icon.name: "view-refresh"
            text: i18nc("@action:button", "Check Again")
            enabled: !App.scanning && !App.busy
            onTriggered: App.rescan()
        },
        Kirigami.Action {
            icon.name: "view-history"
            text: i18nc("@action:button", "History")
            onTriggered: page.historyRequested()
        },
        Kirigami.Action {
            icon.name: "help-about"
            text: i18nc("@action:button", "About Pacnew Helper")
            displayHint: Kirigami.DisplayHint.AlwaysHide
            onTriggered: page.aboutRequested()
        }
    ]

    header: ColumnLayout {
        spacing: 0

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: App.demoMode
            type: Kirigami.MessageType.Warning
            text: i18n("Demo mode: all changes are made in %1, not on your system.", App.root)
        }

        Kirigami.InlineMessage {
            id: intro
            property bool dismissed: false
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: !dismissed && list.count > 0
            type: Kirigami.MessageType.Information
            showCloseButton: true
            // Only a click on the close button counts, not the page being hidden.
            onVisibleChanged: if (!visible && page.visible && page.isCurrentPage) dismissed = true
            text: i18n("When a program is updated, it sometimes brings a new version of a settings file that was changed on this computer. "
                       + "Your file is never overwritten; the new version is saved next to it. "
                       + "Open each file below to bring in the improvements while keeping your own settings.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: App.files.simpleKeepCount > 0
            type: Kirigami.MessageType.Positive
            text: i18np("1 file needs no changes: it is the same, maintained by a program, or the update brings nothing new. "
                        + "It can be kept as it is and its extra copy removed.",
                        "%1 files need no changes: they are the same, maintained by a program, or the update brings nothing new. "
                        + "They can be kept as they are and their extra copies removed.",
                        App.files.simpleKeepCount)
            actions: Kirigami.Action {
                icon.name: "dialog-ok-apply"
                text: i18nc("@action:button", "Keep and Tidy Up")
                enabled: !App.busy
                onTriggered: App.keepAllUnchanged()
            }
        }
    }

    ListView {
        id: list
        model: App.files

        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: Kirigami.ListSectionHeader {
            required property string section
            width: ListView.view.width
            text: section === "attention" ? i18nc("@title:group", "Needs Your Attention") : i18nc("@title:group", "Ready")
        }

        delegate: Delegates.RoundedItemDelegate {
            id: delegate

            required property int index
            required property string path
            required property string kind
            required property string title
            required property string packageName
            required property string statusText
            required property string statusIcon
            required property string status
            required property string summary
            required property bool highRisk

            width: ListView.view.width
            onClicked: page.openRequested(path, kind)

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: delegate.statusIcon
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: Kirigami.Units.iconSizes.medium
                    color: delegate.status === "review" || delegate.status === "keeponly" ? Kirigami.Theme.neutralTextColor
                         : delegate.status === "automatic" || delegate.status === "identical" ? Kirigami.Theme.positiveTextColor
                         : Kirigami.Theme.textColor
                    isMask: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        QQC2.Label {
                            text: delegate.title
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        QQC2.Label {
                            visible: delegate.highRisk
                            text: i18nc("@info badge", "Important")
                            color: Kirigami.Theme.neutralTextColor
                            font: Kirigami.Theme.smallFont
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: delegate.kind === "pacsave" ? delegate.path + ".pacsave" : delegate.path
                        font.family: "monospace"
                        opacity: 0.75
                        elide: Text.ElideMiddle
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: delegate.summary.length > 0 ? delegate.statusText + " · " + delegate.summary : delegate.statusText
                        font: Kirigami.Theme.smallFont
                        opacity: 0.85
                        wrapMode: Text.Wrap
                    }
                }

                Kirigami.Icon {
                    source: "go-next-symbolic"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                    opacity: 0.6
                }
            }
        }

        Kirigami.LoadingPlaceholder {
            anchors.centerIn: parent
            visible: App.scanning && list.count === 0
            text: i18n("Looking for updated configuration files…")
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: !App.scanning && list.count === 0
            icon.name: "checkmark"
            text: i18n("Your system settings are up to date")
            explanation: i18n("There are no new configuration files to review.")
            helpfulAction: Kirigami.Action {
                icon.name: "view-refresh"
                text: i18nc("@action:button", "Check Again")
                onTriggered: App.rescan()
            }
        }
    }
}
