import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Okno podglądu logu core'a (strumień na żywo przez SSH).
// Strumień startuje przy otwarciu okna i gaśnie przy zamknięciu.
Window {
    id: logWindow
    width: 860
    height: 520
    minimumWidth: 500
    minimumHeight: 300
    title: "Helios Core - log (" + sshCore.sshTarget + ")"
    color: "#101216"

    onVisibleChanged: sshCore.setLogStreaming(logWindow.visible)
    onClosing: function(close) {
        close.accepted = false
        logWindow.visible = false
    }

    Connections {
        target: sshCore
        function onLogReceived(line) {
            logArea.append(line)
            if (logArea.lineCount > 800) {
                const idx = logArea.text.indexOf("\n")
                if (idx >= 0) {
                    logArea.remove(0, idx + 1)
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "LOG CORE'A NA ŻYWO"
                color: "#e8eaed"
                font.pixelSize: 13
                font.bold: true
                font.letterSpacing: 1
            }
            Item { Layout.fillWidth: true }
            Text {
                text: sshCore.coreState.toUpperCase()
                color: "#8a8d98"
                font.pixelSize: 11
                font.bold: true
            }
            Button {
                text: "WYCZYŚĆ"
                font.pixelSize: 11
                onClicked: logArea.clear()
            }
            Button {
                text: "ZAMKNIJ"
                font.pixelSize: 11
                onClicked: logWindow.visible = false
            }
        }

        TextArea {
            id: logArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            readOnly: true
            selectByMouse: true
            wrapMode: TextArea.NoWrap
            font.family: "Courier New"
            font.pixelSize: 12
            color: "#e8eaed"
            background: Rectangle { color: "#101216" }
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}
        }
    }
}
