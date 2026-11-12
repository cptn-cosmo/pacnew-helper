// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.AbstractCard {
    id: tile

    property int value: 0
    property string label
    property string hint
    property string iconName
    property bool attention: false

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            source: tile.iconName
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: Kirigami.Units.iconSizes.medium
            Layout.alignment: Qt.AlignTop
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Kirigami.Heading {
                level: 1
                text: tile.value
                color: tile.attention ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: tile.label
                font.bold: true
                wrapMode: Text.Wrap
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: tile.hint
                font: Kirigami.Theme.smallFont
                opacity: 0.7
                wrapMode: Text.Wrap
            }
        }
    }
}
