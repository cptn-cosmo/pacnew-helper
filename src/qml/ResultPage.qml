// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import gay.geeks.pacnewhelper.app

Kirigami.ScrollablePage {
    id: page

    title: i18nc("@title", "Result")

    property string resultText: App.changes.resultText()

    Connections {
        target: App.changes
        function onChoicesChanged() {
            page.resultText = App.changes.resultText();
        }
    }

    actions: Kirigami.Action {
        icon.name: "dialog-ok-apply"
        text: i18nc("@action:button", "Apply")
        enabled: !App.busy
        onTriggered: applicationWindow().confirmChange("apply")
    }

    header: QQC2.Label {
        padding: Kirigami.Units.largeSpacing
        wrapMode: Text.Wrap
        text: i18n("This is how %1 will look with your choices.", App.current.path ?? "")
    }

    QQC2.TextArea {
        readOnly: true
        text: page.resultText
        textFormat: TextEdit.PlainText
        font.family: "monospace"
        wrapMode: TextEdit.NoWrap
        selectByMouse: true
    }
}
