// SPDX-License-Identifier: GPL-3.0-or-later
//
// One side of a change: the lines with their line numbers. Lines that don't
// exist on the other side are tinted by what happens to them: green if they
// end up in the file (this side is chosen), red if they are dropped.

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: pane

    property string title
    property var lines: []
    property var otherLines: []
    property int firstLine: 1
    property bool selected: false
    // Long blocks (e.g. a whole new mirror list) are cut short until expanded.
    property int limit: 60
    property bool expanded: false
    readonly property var shownLines: expanded || lines.length <= limit ? lines : lines.slice(0, limit)

    readonly property var otherSet: {
        const s = {};
        for (const l of otherLines)
            s[l] = true;
        return s;
    }

    implicitHeight: column.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Kirigami.Theme.backgroundColor
    border.width: selected ? 2 : 1
    border.color: selected ? Kirigami.Theme.highlightColor : Qt.alpha(Kirigami.Theme.textColor, 0.15)

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            QQC2.Label {
                text: pane.title
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            QQC2.Label {
                visible: pane.selected
                text: i18nc("@info badge", "will be used")
                color: Kirigami.Theme.highlightColor
                font: Kirigami.Theme.smallFont
            }
        }

        QQC2.Label {
            visible: pane.lines.length === 0
            text: i18nc("@info", "(nothing here)")
            font.italic: true
            opacity: 0.6
        }

        Repeater {
            model: pane.shownLines
            delegate: Rectangle {
                required property int index
                required property string modelData
                readonly property bool differs: !pane.otherSet[modelData]

                Layout.fillWidth: true
                implicitHeight: lineRow.implicitHeight
                radius: 2
                color: differs ? (pane.selected ? Kirigami.Theme.positiveBackgroundColor : Kirigami.Theme.negativeBackgroundColor)
                               : "transparent"

                RowLayout {
                    id: lineRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.Label {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                        Layout.alignment: Qt.AlignTop
                        horizontalAlignment: Text.AlignRight
                        text: pane.firstLine + index
                        font.family: "monospace"
                        opacity: 0.5
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: modelData.length > 0 ? modelData : " "
                        font.family: "monospace"
                        wrapMode: Text.WrapAnywhere
                        textFormat: Text.PlainText
                    }
                }
            }
        }

        QQC2.Button {
            visible: !pane.expanded && pane.lines.length > pane.limit
            flat: true
            icon.name: "arrow-down"
            text: i18ncp("@action:button", "Show 1 more line", "Show %1 more lines", pane.lines.length - pane.limit)
            onClicked: pane.expanded = true
        }
    }
}
