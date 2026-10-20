import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ============================================================================
// ChatPanel - okienko czatu z Heliosem.
//
// Użycie (w Main.qml):
//   ChatPanel {
//       id: chatPanel
//       Layout.fillWidth: true
//       Layout.fillHeight: true
//       onSendRequested: function(text) { ... logika w Main ... }
//   }
//
// API:
//   signal sendRequested(string text) - użytkownik wysłał wiadomość
//   function addMessage(sender, text) - sender: "user" | "helios" | "system"
//   function clearChat()              - czyści historię
//
// Kolory pobiera z okna głównego (rootWindow.color*), żeby zmiana palety
// w jednym miejscu działała w całej aplikacji.
// ============================================================================
Rectangle {
    id: chatRoot
    color: "#f7f8fa"
    radius: 8
    border.color: "#dfe2e7"
    border.width: 1
    clip: true

    signal sendRequested(string text)

    // Stan stubowy (docelowo przyjdzie z core'a przez IPC):
    property bool listening: true
    property string heliosStatus: "OFFLINE"

    // Paleta (przekazywana z Main.qml - osobne pliki QML nie widzą się nawzajem):
    property color colorText: "#1c1e21"
    property color colorTextMuted: "#6b7280"
    property color colorAccent: "#e60012"
    property color colorSuccess: "#16a34a"

    function timeNow() {
        return Qt.formatTime(new Date(), "hh:mm")
    }

    function addMessage(sender, text) {
        chatModel.append({"sender": sender, "text": text, "time": timeNow()})
    }

    function clearChat() {
        chatModel.clear()
    }

    function sendCurrent() {
        var text = chatInput.text.trim()
        if (text.length === 0) {
            return
        }
        if (text.length > 500) {
            text = text.substring(0, 500)
        }
        chatInput.clear()
        addMessage("user", text)
        sendRequested(text)
    }

    ListModel {
        id: chatModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // ---- nagłówek: tytuł + status ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "HELIOS CHAT"
                color: chatRoot.colorText
                font.pixelSize: 14
                font.bold: true
                font.letterSpacing: 1

            }
            Button {
                id: btnWyczysc
                text: "WYCZYŚĆ CZAT" //przenosze

                onClicked: {
                    chatPanel.clearChat()
                }
            }
 

            Item { Layout.fillWidth: true }

            // Kropka nasłuchu
            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: chatRoot.listening ? chatRoot.colorSuccess : chatRoot.colorTextMuted
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                text: chatRoot.listening ? "SŁUCHA" : "GŁUCHY"
                color: chatRoot.listening ? chatRoot.colorSuccess : chatRoot.colorTextMuted
                font.pixelSize: 11
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            // Pigułka statusu połączenia z Heliosem
            Rectangle {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 20
                radius: 10
                color: "transparent"
                border.color: chatRoot.colorAccent
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: chatRoot.heliosStatus
                    color: chatRoot.colorAccent
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#dfe2e7" }

        // ---- historia wiadomości ----
        ListView {
            id: chatList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: chatModel
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            onCountChanged: Qt.callLater(chatList.positionViewAtEnd)

            delegate: Item {
                width: chatList.width
                height: bubbleBox.height

                RowLayout {
                    id: bubbleBox
                    width: parent.width
                    spacing: 0

                    // Wiadomości Heliosa i systemowe do lewej, użytkownika do prawej
                    Item { visible: model.sender === "user"; Layout.fillWidth: true }

                    ColumnLayout {
                        spacing: 2
                        Layout.maximumWidth: parent.width * 0.85

                        Rectangle {
                            Layout.fillWidth: true
                            // Dymek systemowy: sam tekst bez ramki
                            color: model.sender === "system" ? "transparent" : (model.sender === "user" ? chatRoot.colorAccent : "#ffffff")
                            radius: 8
                            border.color: model.sender === "system" ? "transparent" : (model.sender === "user" ? chatRoot.colorAccent : "#dfe2e7")
                            border.width: 1

                            // TextEdit tylko do odczytu zamiast Text: tekst w dymku
                            // można zaznaczać myszą i kopiować Ctrl+C (jak wszędzie).
                            TextEdit {
                                id: bubbleText
                                anchors.fill: parent
                                anchors.margins: 8
                                text: model.text
                                readOnly: true
                                selectByMouse: true
                                textFormat: TextEdit.PlainText
                                color: model.sender === "system" ? chatRoot.colorTextMuted : (model.sender === "user" ? "white" : chatRoot.colorText)
                                selectionColor: model.sender === "user" ? "#ffffff" : "#b9d4f5"
                                selectedTextColor: model.sender === "user" ? chatRoot.colorAccent : chatRoot.colorText
                                font.pixelSize: model.sender === "system" ? 11 : 13
                                font.italic: model.sender === "system"
                                wrapMode: TextEdit.WordWrap
                            }

                            // Szerokość: naturalna, maks. 85% listy. Bez tego dymki
                            // zwijałyby się do zera (tekst jest zakotwiczony).
                            implicitWidth: Math.min(bubbleText.implicitWidth + 16,
                                                    chatList.width * 0.85)
                            implicitHeight: bubbleText.implicitHeight
                                    + (model.sender === "system" ? 4 : 16)
                        }

                        Text {
                            visible: model.sender !== "system"
                            text: (model.sender === "user" ? "Ty  •  " : "Helios  •  ") + model.time
                            color: chatRoot.colorTextMuted
                            font.pixelSize: 10
                            Layout.alignment: model.sender === "user" ? Qt.AlignRight : Qt.AlignLeft
                        }
                    }

                    Item { visible: model.sender !== "user"; Layout.fillWidth: true }
                }
            }
        }

        // ---- szybkie komendy (chipsy) ----
        Flow {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: ["Zacznij śledzenie", "Pomachaj", "Statystyki", "Nagrywaj", "Stop"]

                Button {
                    text: modelData
                    font.pixelSize: 11
                    font.bold: true
                    topPadding: 6
                    bottomPadding: 6
                    leftPadding: 10
                    rightPadding: 10

                    contentItem: Text {
                        text: parent.text
                        color: chatRoot.colorAccent
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? "#fdeef0" : "transparent"
                        border.color: chatRoot.colorAccent
                        border.width: 1
                    }

                    onClicked: {
                        chatInput.text = modelData
                        chatRoot.sendCurrent()
                    }
                }
            }
        }

        // ---- pole wpisu + wyślij ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: chatInput
                Layout.fillWidth: true
                placeholderText: "Napisz do Heliosa..."
                placeholderTextColor: chatRoot.colorTextMuted
                color: chatRoot.colorText
                font.pixelSize: 13
                selectByMouse: true

                background: Rectangle {
                    radius: 6
                    color: "#ffffff"
                    border.color: chatInput.activeFocus ? chatRoot.colorAccent : "#c9ccd3"
                    border.width: 1
                }

                Keys.onReturnPressed: chatRoot.sendCurrent()
                Keys.onEnterPressed: chatRoot.sendCurrent()
            }

            Button {
                id: btnSend
                text: "WYŚLIJ"
                font.pixelSize: 12
                font.bold: true
                Layout.preferredWidth: 90
                Layout.preferredHeight: chatInput.height

                contentItem: Text {
                    text: parent.text
                    color: "#1c1e21"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: btnSend.pressed ? "#a8adb5" : (btnSend.hovered ? "#b9bcc4" : "#c9ccd3")
                }

                onClicked: chatRoot.sendCurrent()
            }
        }
    }
}
