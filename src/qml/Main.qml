// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import org.kde.coreaddons as KCoreAddons
import gay.geeks.pacnewhelper.app

Kirigami.ApplicationWindow {
    id: root

    // Set from the command line (development aids, see main.cpp).
    property string startPath
    property string startPage
    property bool startHandled: false

    title: i18n("Pacnew Helper")

    width: Kirigami.Units.gridUnit * 56
    height: Kirigami.Units.gridUnit * 40
    minimumWidth: Kirigami.Units.gridUnit * 22
    minimumHeight: Kirigami.Units.gridUnit * 24

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.ToolBar
    pageStack.globalToolBar.showNavigationButtons: Kirigami.ApplicationHeaderStyle.ShowBackButton
    pageStack.columnView.columnResizeMode: Kirigami.ColumnView.SingleColumn

    pageStack.initialPage: FilesPage {
        onOpenRequested: (path, kind) => root.openFile(path, kind)
        onHistoryRequested: root.showHistory()
        onAboutRequested: root.showAbout()
    }

    function openFile(path: string, kind: string) {
        App.open(path, kind);
        while (pageStack.depth > 1)
            pageStack.pop();
        const status = App.current.status;
        if (kind === "pacsave" || status === "orphaned")
            pageStack.push(leftoverComponent);
        else
            pageStack.push(reviewComponent);
    }

    // Every change goes through the summary page; nothing is written before
    // the user approved it there.
    function confirmChange(action: string, arg: string) {
        pageStack.push(summaryComponent, { action: action, arg: arg ?? "" });
    }

    function showHistory() {
        while (pageStack.depth > 1)
            pageStack.pop();
        pageStack.push(historyComponent);
    }

    function showAbout() {
        while (pageStack.depth > 1)
            pageStack.pop();
        pageStack.push(aboutComponent);
    }

    Connections {
        target: App

        function onMessage(text: string, error: bool) {
            if (error) {
                errorDialog.subtitle = text;
                errorDialog.open();
            } else {
                root.showPassiveNotification(text);
            }
        }

        function onFinished(summary: string, afterApply: string) {
            while (root.pageStack.depth > 1)
                root.pageStack.pop();
            if (afterApply.length > 0) {
                doneDialog.subtitle = summary + "\n\n" + afterApply;
                doneDialog.open();
            } else {
                root.showPassiveNotification(summary);
            }
        }

        function onScanningChanged() {
            if (App.scanning || root.startHandled)
                return;
            root.startHandled = true;
            if (root.startPath.length > 0) {
                const kind = root.startPath.endsWith(".pacsave") ? "pacsave" : "pacnew";
                root.openFile(root.startPath.replace(/\.(pacnew|pacsave)$/, ""), kind);
            }
            if (root.startPage.startsWith("summary:")) {
                root.confirmChange(root.startPage.slice(8), "");
            } else if (root.startPage === "history") {
                root.showHistory();
            } else if (root.startPage === "about") {
                root.showAbout();
            }
        }
    }

    Component {
        id: reviewComponent
        ReviewPage {
            onPreviewRequested: root.pageStack.push(resultComponent)
        }
    }
    Component {
        id: leftoverComponent
        LeftoverPage {}
    }
    Component {
        id: resultComponent
        ResultPage {}
    }
    Component {
        id: summaryComponent
        SummaryPage {
            id: summaryPage
            onCancelled: root.pageStack.pop()
            onApproved: {
                // Single-file actions close their pages once finished (see
                // onFinished); bulk actions and undo report via a message.
                if (summaryPage.action === "keep-all" || summaryPage.action === "undo")
                    root.pageStack.pop();
            }
        }
    }
    Component {
        id: historyComponent
        HistoryPage {}
    }
    Component {
        id: aboutComponent
        FormCard.AboutPage {
            aboutData: KCoreAddons.AboutData
        }
    }

    Kirigami.PromptDialog {
        id: errorDialog
        title: i18nc("@title:dialog", "Nothing was changed")
        standardButtons: Kirigami.Dialog.Ok
    }

    Kirigami.PromptDialog {
        id: doneDialog
        title: i18nc("@title:dialog", "Done")
        standardButtons: Kirigami.Dialog.Ok
    }
}
