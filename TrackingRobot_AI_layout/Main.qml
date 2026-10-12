import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrackingRobot

Window {
    id: rootWindow
    width: 1280
    height: 760
    visible: true
    title: "Rokae Helios | Operator HUD"
    visibility: Window.Maximized

    // Globalna paleta kolorów dla łatwej zmiany (jasny motyw, styl Robot Assistanta)
    readonly property color colorBg: "#eef0f3"
    readonly property color colorPanel: "#ffffff"
    readonly property color colorAccent: "#e60012" // Czerwień Rokae
    readonly property color colorSuccess: "#16a34a" // Zieleń statusów
    readonly property color colorWarning: "#d97706"
    readonly property color colorDanger: "#e60012"
    readonly property color colorText: "#1c1e21"
    readonly property color colorTextMuted: "#6b7280"

    // ---- stan layoutu (TRYB DEMO: przełącza tylko guzik KAMERA) ----
    // false = sam czat na pełny ekran (start aplikacji)
    // true  = kamera + panel boczny z czatem po prawej
    property bool trackingActive: false

    // DEMO: statyczne true, guzik nie przełącza (docelowo z core'a po IPC)
    property bool listening: true


    color: colorBg

    // TRYB DEMO: przełącza TYLKO widoczność bloków (czysty layout).
    // Nic nie woła, nic nie startuje - docelowo dojdzie tu start/stop
    // strumienia i wykrywanie komendy śledzenia z czatu.
    function setTracking(on) {
        trackingActive = on
    }

    Component.onCompleted: {
        chatPanel.addMessage("helios", "Cześć! Jestem Helios.")
        chatPanel.addMessage("system", "Tryb demo - guziki i czat są klikalne, ale nic nie robią.")
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // ==========================================
        // LEWA STRONA: Ekran Kamery (Viewport)
        // Widoczny tylko w trybie śledzenia.
        // ==========================================
        Rectangle {
            id: videoContainer
            visible: trackingActive
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#050508"
            radius: 12
            border.color: "#d9dce1"
            border.width: 2
            clip: true

            // Ramka akcentująca (cyberpunkowy detal)
            Rectangle {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                width: 150
                height: 3
                color: colorAccent
            }

            VideoItem {
                id: liveDisplay
                objectName: "cameraDisplay"
                anchors.fill: parent
            }

            // Ekran ładowania/braku połączenia
            Rectangle {
                anchors.fill: parent
                color: "#f4f5f7"
                visible: !robotClient.isConnected

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 15

                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        running: !robotClient.isConnected
                        width: 60
                        height: 60
                    }

                    Text {
                        text: robotClient.statusText.toUpperCase()
                        color: colorAccent
                        font.pixelSize: 22
                        font.bold: true
                        font.letterSpacing: 2
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // Wskaźnik LIVE (Pill)
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 15
                width: 140
                height: 34
                color: "#f2ffffff"
                radius: 17
                border.color: "#d9dce1"
                border.width: 1
                visible: robotClient.isConnected

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    // Pulsująca czerwona kropka
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: colorDanger

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.2; duration: 800 }
                            NumberAnimation { to: 1.0; duration: 800 }
                        }
                    }

                    Text {
                        text: "LIVE STREAM"
                        color: colorText
                        font.pixelSize: 12
                        font.bold: true
                        font.letterSpacing: 1
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }
        }

        // ==========================================
        // PRAWA STRONA: Panel Dowodzenia + Czat
        // Na starcie (bez śledzenia) zajmuje cały ekran.
        // ==========================================
        Rectangle {
            id: sidePanel
            Layout.preferredWidth: 360
            Layout.fillWidth: !trackingActive
            Layout.fillHeight: true
            color: colorPanel
            radius: 12
            border.color: "#dfe2e7"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14

                // Nagłówek Panelu
                ColumnLayout {
                    spacing: 5
                    Layout.fillWidth: true

                    Text {
                        text: "ROKAE COMMAND"
                        color: colorText
                        font.pixelSize: 24
                        font.bold: true
                        font.letterSpacing: 1
                    }
                    Text {
                        text: "System Telemetrii Operatora"
                        color: colorAccent
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#dfe2e7" }

                // Wskaźnik statusu łącza
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "LINK STATUS:"
                        color: colorTextMuted
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: robotClient.statusText.toUpperCase()
                        color: robotClient.isConnected ? colorSuccess : colorDanger
                        font.bold: true
                        font.pixelSize: 14
                        font.letterSpacing: 1
                    }
                }

                // Wskaźnik nasłuchu (stub - docelowo z core'a)
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "NASŁUCH:"
                        color: colorTextMuted
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: listening ? "SŁUCHA" : "WYŁĄCZONY"
                        color: listening ? colorSuccess : colorTextMuted
                        font.bold: true
                        font.pixelSize: 14
                        font.letterSpacing: 1
                    }
                }

                // ==========================================
                // CZAT Z HELIOSEM (rozciąga się na wolne miejsce)
                // ==========================================
                ChatPanel {
                    id: chatPanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 220
                    listening: rootWindow.listening

                    colorText: rootWindow.colorText
                    colorTextMuted: rootWindow.colorTextMuted
                    colorAccent: rootWindow.colorAccent
                    colorSuccess: rootWindow.colorSuccess

                    // onSendRequested - do podpięcia, gdy czat ożyje
                }

                // ==========================================
                // KARTA TELEMETRII (tylko w trybie śledzenia)
                // ==========================================
                Rectangle {
                    visible: trackingActive
                    Layout.fillWidth: true
                    Layout.preferredHeight: telemetryLayout.height + 30
                    color: "#f7f8fa"
                    radius: 8
                    border.color: "#dfe2e7"

                    ColumnLayout {
                        id: telemetryLayout
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 15
                        spacing: 12

                        Text {
                            text: "KINEMATYKA OPERATORA"
                            color: colorTextMuted
                            font.pixelSize: 12
                            font.bold: true
                        }

                        // Komponent pomocniczy do wierszy danych
                        component TelemetryRow: RowLayout {
                            property string label
                            property string value
                            property color valColor

                            Layout.fillWidth: true
                            Text { text: label; color: colorText; font.pixelSize: 14 }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: value
                                color: valColor
                                font.pixelSize: 18
                                font.bold: true
                                font.family: "Courier New" // Monospace żeby liczby nie skakały
                            }
                        }

                        TelemetryRow {
                            label: "Prawy łokieć"
                            value: videoReceiver.rightElbowAngle > 0 ? videoReceiver.rightElbowAngle.toFixed(1) + "°" : "---.-°"
                            valColor: colorAccent
                        }
                        TelemetryRow {
                            label: "Lewy łokieć"
                            value: videoReceiver.leftElbowAngle > 0 ? videoReceiver.leftElbowAngle.toFixed(1) + "°" : "---.-°"
                            valColor: colorAccent
                        }
                        TelemetryRow {
                            label: "Obrót głowy (Yaw)"
                            value: videoReceiver.headYaw.toFixed(1) + "°"
                            valColor: colorText
                        }
                        TelemetryRow {
                            label: "Pochylenie tułowia"
                            value: videoReceiver.torsoRoll.toFixed(1) + "°"
                            valColor: colorText
                        }
                    }
                }

                // ==========================================
                // PRZYCISKI AKCJI
                // ==========================================
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 10

                    // Wspólny styl małego przycisku funkcyjnego
                    component SmallButton: Button {
                        property color activeColor: colorAccent
                        property bool active: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 12
                            font.bold: true
                            font.letterSpacing: 1
                            color: parent.active ? parent.activeColor : colorTextMuted
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: "transparent"
                            border.color: parent.active ? parent.activeColor : "#b9bcc4"
                            border.width: 2
                        }
                    }

                    // Przycisk Połączenia
                    Button {
                        id: btnConnect
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44

                        contentItem: Text {
                            text: robotClient.isConnected ? "ZAKOŃCZ SESJĘ" : "POŁĄCZ"
                            font.pixelSize: 12
                            font.bold: true
                            font.letterSpacing: 1
                            color: robotClient.isConnected ? colorDanger : "#ffffff"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 6
                            color: robotClient.isConnected ? "transparent" : (btnConnect.pressed ? "#166534" : (btnConnect.hovered ? "#15803d" : colorSuccess))
                            border.color: robotClient.isConnected ? colorDanger : "transparent"
                            border.width: robotClient.isConnected ? 2 : 0

                            Rectangle {
                                anchors.fill: parent
                                radius: 6
                                color: "#ffffff"
                                opacity: btnConnect.hovered ? 0.1 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }
                        }

                        onClicked: {
                            // DEMO: docelowo connect/disconnect
                        }
                    }

                    // Przełącznik nasłuchu (stub - docelowo komenda do core'a)
                    SmallButton {
                        id: btnListen
                        text: listening ? "NASŁUCH: ON" : "NASŁUCH: OFF"
                        active: listening
                        activeColor: colorSuccess

                        onClicked: {
                            // DEMO: docelowo przełącznik nasłuchu
                        }
                    }

                    // Ręczne pokazanie/ukrycie kamery
                    SmallButton {
                        id: btnCamera
                        text: trackingActive ? "UKRYJ KAMERĘ" : "POKAŻ KAMERĘ"
                        active: trackingActive

                        onClicked: {
                            setTracking(!trackingActive)
                        }
                    }

                    // Czyszczenie historii czatu
                    SmallButton {
                        id: btnClear
                        text: "WYCZYŚĆ CZAT"

                        onClicked: {
                            // DEMO: docelowo czyszczenie czatu
                        }
                    }
                }

                // E-STOP (bez zmian, na całą szerokość)
                Button {
                    id: btnEstop
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60

                    contentItem: Text {
                        text: "EMERGENCY STOP"
                        font.pixelSize: 18
                        font.bold: true
                        font.letterSpacing: 2
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 6
                        color: btnEstop.pressed ? "#8f000b" : colorDanger
                        border.color: "#b3000e"
                        border.width: 1

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 20
                            height: 3
                            color: "#ffffff"
                            opacity: 0.5
                        }
                    }

                    onClicked: {
                        // DEMO: docelowo emergencyStop()
                    }
                }
            }
        }
    }
}
