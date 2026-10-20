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

    // ---- stan layoutu ----
    // false = sam czat na pełny ekran (start aplikacji)
    // true  = kamera + panel boczny z czatem po prawej
    property bool trackingActive: false

    // Stan z core'a (sygnały HeliosLink): listening/muted/capturing/verifying/thinking/speaking/offline
    property string coreState: "offline"
    property bool streamOn: false
    property bool remoteOn: false
    property bool listening: true
    property var sentEchoes: ({})


    color: colorBg

    // Przełącza między czatem pełnoekranowym a trybem śledzenia.
    // Przy starcie śledzenia odpala też strumień i połączenie (autostart
    // przeniesiony z main.cpp - na starcie apki nic nie mieli w tle).
    function setTracking(on) {
        if (on === trackingActive) {
            return
        }
        trackingActive = on
        if (on) {
            if (!robotClient.isConnected) {
                robotClient.connectToRobot("192.168.49.97", 9090)
            }
            if (!streamOn) {
                videoReceiver.startStream("http://192.168.49.97:8080/stream?topic=/camera/color/image_raw") // kamera robota ("0" = laptop)
                streamOn = true
            }
            chatPanel.addMessage("system", "Śledzenie ruchów włączone.")
        } else {
            videoReceiver.stopStream()
            streamOn = false
            chatPanel.addMessage("system", "Śledzenie ruchów wyłączone.")
        }
    }

    // Etykieta stanu core'a na wiersz NASŁUCH.
    function stateLabel(s) {
        if (s === "listening") return "SŁUCHA"
        if (s === "muted") return "WYŁĄCZONY"
        if (s === "capturing") return "NAGRYWA…"
        if (s === "verifying") return "SPRAWDZA…"
        if (s === "thinking") return "MYŚLI…"
        if (s === "speaking") return "MÓWI…"
        return "OFFLINE"
    }

    // Komendy śledzenia w czacie (przełączają kamerę lokalnie + tekst idzie do core'a).
    function isTrackingCommand(text) {
        const lower = text.toLowerCase()
        return lower.includes("śledz") || lower.includes("sledz") ||
               lower.includes("track") || lower.includes("kamer") || lower.includes("camer")
    }
    function isStopCommand(text) {
        const lower = text.toLowerCase()
        return lower.includes("stop") || lower.includes("przestań") || lower.includes("przestan") ||
               lower.includes("koniec") || lower.includes("zatrzymaj")
    }

    // Komendy nagrywarki stawów ROBOTA (obsługa lokalna - tekst NIE idzie do core'a).
    // Start: "nagrywaj", "nagrywaj co 0.5", "nagrywanie" / Stop: "stop nagrywania",
    // "przestań nagrywać", "koniec nagrywania", "nie nagrywaj".
    // UWAGA: stop nagrywania sprawdzamy PRZED zwykłym stopem, żeby "stop
    // nagrywania" nie wyłączał śledzenia.
    function isRecordStopCommand(text) {
        const lower = text.toLowerCase()
        if (!lower.includes("nagryw") && !lower.includes("record")) return false
        return lower.includes("stop") || lower.includes("przestań") || lower.includes("przestan") ||
               lower.includes("koniec") || lower.includes("zatrzymaj") || lower.includes("zakończ") ||
               lower.includes("zakoncz") || lower.includes("wyłącz") || lower.includes("wylacz") ||
               lower.includes("nie nagryw")
    }
    function isRecordStartCommand(text) {
        const lower = text.toLowerCase()
        if (!lower.includes("nagryw") && !lower.includes("record")) return false
        return !isRecordStopCommand(text)
    }
    // Opcjonalny interwał w komendzie: "nagrywaj co 2" / "nagrywaj co 0,5 s".
    function recordInterval(text) {
        const m = text.toLowerCase().replace(",", ".").match(/co\s+(\d+(?:\.\d+)?)/)
        if (m) {
            const s = parseFloat(m[1])
            if (s >= 0.2 && s <= 60) return s
        }
        return 1.0
    }

    // Sygnały z core'a (HeliosLink) -> czat i statusy.
    Connections {
        target: heliosLink
        function onReplyReceived(text, ref) {
            chatPanel.addMessage("helios", text)
        }
        function onHeardReceived(text, ref, source) {
            if (ref !== 0 && rootWindow.sentEchoes[ref] !== undefined) {
                delete rootWindow.sentEchoes[ref]
                return
            }
            chatPanel.addMessage("user", text)
        }
        function onLogReceived(text) {
            chatPanel.addMessage("system", text)
        }
        function onEarStateChanged(state) {
            rootWindow.coreState = state
            rootWindow.listening = (state !== "muted" && state !== "offline")
        }
        function onConnectionChanged(connected) {
            chatPanel.heliosStatus = connected ? "LINK" : "OFFLINE"
            chatPanel.addMessage("system", connected ? "Połączono z corem Heliosa." : "Utracono połączenie z corem - ponawiam…")
        }
    }

    // Komunikaty nagrywarki (start/stop/błędy) -> czat.
    Connections {
        target: jointRecorder
        function onMessage(text) {
            chatPanel.addMessage("system", text)
        }
    }

    // Rozłączenie robota w trakcie nagrywania -> czat (próbki czekają na dane).
    Connections {
        target: robotClient
        function onConnectionChanged(connected) {
            if (!connected && jointRecorder.recording) {
                chatPanel.addMessage("system", "Utracono połączenie z robotem - nagrywanie czeka na dane (komenda stop nagrywania kończy plik).")
            }
        }
    }

    // Błędy SSH (np. brak klucza, robot nieosiągalny) -> czat.
    Connections {
        target: sshCore
        function onLastErrorChanged(error) {
            if (error !== "") {
                chatPanel.addMessage("system", "SSH: " + error)
            }
        }
        // Potwierdzenia klików START/STOP -> czat (zawsze, bez tłumienia).
        function onNotice(text) {
            chatPanel.addMessage("system", "SSH: " + text)
        }
    }

    Component.onCompleted: {
        chatPanel.addMessage("helios", "Cześć! Jestem Helios. Napisz coś albo powiedz moje imię.")
        chatPanel.addMessage("system", "Łączę z corem Heliosa…")
    }

    LogWindow {
        id: logWindow
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

                // Łącze GUI<->core (TCP do robota)
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "LINK GUI:"
                        color: colorTextMuted
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: heliosLink.connected ? "POŁĄCZONY" : "BRAK"
                        color: heliosLink.connected ? colorSuccess : colorDanger
                        font.bold: true
                        font.pixelSize: 14
                        font.letterSpacing: 1
                    }
                }

                // Stan nasłuchu (z core'a)
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
                        text: stateLabel(coreState)
                        color: coreState === "listening" ? colorSuccess : ((coreState === "muted" || coreState === "offline") ? colorTextMuted : colorWarning)
                        font.bold: true
                        font.pixelSize: 14
                        font.letterSpacing: 1
                    }
                }

                // Stan nagrywarki stawów robota
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "NAGRYWANIE:"
                        color: colorTextMuted
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: jointRecorder.recording ? (robotClient.hasJointState ? "● REC " + jointRecorder.sampleCount : "CZEKAM NA DANE…") : (jointRecorder.filePath !== "" ? "PLIK GOTOWY" : "BRAK")
                        color: jointRecorder.recording ? (robotClient.hasJointState ? colorDanger : colorWarning) : colorTextMuted
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

                    onSendRequested: function(text) {
                        if (isRecordStopCommand(text)) {
                            jointRecorder.stopRecording()
                            return
                        }
                        if (isRecordStartCommand(text)) {
                            jointRecorder.startRecording(recordInterval(text))
                            return
                        }
                        var echoId = heliosLink.sendChat(text)
                        if (echoId !== 0) {
                            sentEchoes[echoId] = true
                        } else {
                            chatPanel.addMessage("system", "Brak połączenia z corem - wiadomość nie wysłana.")
                        }
                        if (isTrackingCommand(text)) {
                            setTracking(true)
                        } else if (isStopCommand(text)) {
                            setTracking(false)
                        }
                    }
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
                            label: "Bark: sięganie (Yaw)"
                            value: videoReceiver.rightShoulderYaw.toFixed(1) + "°"
                            valColor: colorWarning
                        }
                        TelemetryRow {
                            label: "Bark: unoszenie (Pitch)"
                            value: videoReceiver.rightShoulderPitch.toFixed(1) + "°"
                            valColor: colorWarning
                        }
                        TelemetryRow {
                            label: "Prawy łokieć"
                            value: videoReceiver.rightElbowAngle > 0 ? videoReceiver.rightElbowAngle.toFixed(1) + "°" : "---.-°"
                            valColor: colorAccent
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
                            if (robotClient.isConnected) {
                                robotClient.disconnectFromRobot();
                            } else {
                                robotClient.connectToRobot("192.168.49.97", 9090);
                            }
                        }
                    }

                    // Przełącznik nasłuchu (stub - docelowo komenda do core'a)
                    SmallButton {
                        id: btnListen
                        text: listening ? "NASŁUCH: ON" : "NASŁUCH: OFF"
                        active: listening
                        activeColor: colorSuccess

                        onClicked: {
                            heliosLink.setListening(!listening)
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
                    // SmallButton {
                    //     id: btnPodstawa
                    //     text: "STERUJ PODSTAWĄ"

                    //     onClicked: {
                            
                    //     }
                    // }
                   SmallButton {

                        text: remoteOn ? "REMOTE CONTROLL ON" : "REMOTE CONTROLL OFF"

                        onClicked: {
                            remoteOn = !remoteOn;
                            sshCore.remoteToggle()
                        }
                       
}

Keys.onPressed: {
    switch (event.key) {
        case Qt.Key_W: case Qt.Key_Up:    sshCore.drivePress('w'); break;
        case Qt.Key_S: case Qt.Key_Down:  sshCore.drivePress('s'); break;
        case Qt.Key_A: case Qt.Key_Left:  sshCore.drivePress('a'); break;
        case Qt.Key_D: case Qt.Key_Right: sshCore.drivePress('d'); break;
        case Qt.Key_Space:                sshCore.driveStop(); return;
        default: return;
    }
    event.accepted = true;
}
Keys.onReleased: {
        switch (event.key) {
        case Qt.Key_W: case Qt.Key_Up:    sshCore.driveRelease('w'); break;
        case Qt.Key_S: case Qt.Key_Down:  sshCore.driveRelease('s'); break;
        case Qt.Key_A: case Qt.Key_Left:  sshCore.driveRelease('a'); break;
        case Qt.Key_D: case Qt.Key_Right: sshCore.driveRelease('d'); break;
        case Qt.Key_Space:                sshCore.driveStop(); return;
        default: return;
    }
    event.accepted = true;
}

                    // Start/stop nagrywania stawów robota (to samo co komendy czatu)
                    SmallButton {
                        id: btnRecord
                        text: jointRecorder.recording ? "■ STOP NAG." : "● NAGRYWAJ"
                        active: jointRecorder.recording
                        activeColor: colorDanger

                        onClicked: {
                            if (jointRecorder.recording) {
                                jointRecorder.stopRecording()
                            } else {
                                jointRecorder.startRecording(1.0)
                            }
                        }
                    }

                    // Otwiera ostatnie nagranie w domyślnym programie (np. Excel)
                    SmallButton {
                        id: btnOpenRec
                        text: "OTWÓRZ NAGRANIE"
                        active: jointRecorder.filePath !== "" || jointRecorder.recording
                        activeColor: colorText

                        onClicked: {
                            jointRecorder.openRecording()
                        }
                    }
                }

                // CORE NA ROBOCIE (start/stop przez SSH)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    color: "#f7f8fa"
                    radius: 8
                    border.color: "#dfe2e7"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        spacing: 8

                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            Layout.alignment: Qt.AlignVCenter
                            color: sshCore.coreState === "running" ? colorSuccess : (sshCore.coreState === "stopped" ? colorTextMuted : colorWarning)
                        }
                        Text {
                            text: "CORE:"
                            color: colorTextMuted
                            font.pixelSize: 12
                            font.bold: true
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Text {
                            text: sshCore.coreState.toUpperCase()
                            color: colorText
                            font.pixelSize: 12
                            font.bold: true
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Item { Layout.fillWidth: true }

                        component CoreButton: Button {
                            property color lineColor: colorTextMuted
                            Layout.preferredWidth: 64
                            Layout.preferredHeight: 32
                            font.pixelSize: 11
                            font.bold: true
                            contentItem: Text {
                                text: parent.text
                                color: parent.lineColor
                                font: parent.font
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 6
                                color: parent.hovered ? "#eef0f3" : "transparent"
                                border.color: parent.lineColor
                                border.width: 1
                            }
                        }
                        CoreButton { text: "START"; lineColor: colorSuccess; onClicked: sshCore.startCore() }
                        CoreButton { text: "STOP"; lineColor: colorDanger; onClicked: sshCore.stopCore() }
                        CoreButton { text: "LOG"; onClicked: logWindow.visible = !logWindow.visible }
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
                        robotClient.emergencyStop();
                    }
                }
            }
        }
    }
}
