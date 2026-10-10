# Frontend aplikacji teleoperacji — Operator HUD (Qt 6 / QML)

Opis interfejsu operatora aplikacji do trackingu (`modelDoTrackingu/`): co widać na ekranie,
z jakimi obiektami C++ rozmawia i co jest zaszyte na sztywno w kodzie.

> **Skąd ta wiedza:** wyłącznie z czytania kodu `modelDoTrackingu/Main.qml` (i `Main2.qml`)
> oraz ze zrzutu ekranu `obraz_2026-10-08_102447941.png`. Źródła C++ samej aplikacji
> (`RobotClient`, `VideoReceiver`) **nie są w repozytorium** — są w archiwach
> `TrackingRobot*.zip` / `TrackingRobotV3.7z`. Architektura ogólna:
> [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md).

## Pliki

| Plik | Co jest w środku |
|---|---|
| `modelDoTrackingu/Main.qml` | Główne (i jedyne) okno aplikacji — cały interfejs operatora |
| `modelDoTrackingu/Main2.qml` | Kopia `Main.qml` z **jedną** różnicą: `visibility: Window.Maximized` (okno startowe zmaksymalizowane) |
| `obraz_2026-10-08_102447941.png` | Zrzut ekranu HUD w stanie „ŁĄCZENIE..." (brak połączenia z robotem) |

## Układ okna

Okno `1280 × 760`, tytuł **„Rokae Helios | Operator HUD"**, podzielone na dwie kolumny
(`RowLayout`, marginesy 20 px, odstęp 20 px).

### Lewa kolumna — podgląd kamery

- `VideoItem` (`objectName: "cameraDisplay"`, `anchors.fill`) — tutaj renderowany jest obraz
  z kamery robota. To jedyne miejsce w interfejsie, gdzie widać wideo.
- Nakładka „brak połączenia": ciemny prostokąt z `BusyIndicator` i tekstem
  `robotClient.statusText` (duże litery) — widoczna, gdy `robotClient.isConnected == false`.
  Na zrzucie ekranu pokazuje **„ŁĄCZENIE..."**.
- Wskaźnik **„LIVE STREAM"** (pulsująca czerwona kropka + tekst), widoczny tylko po połączeniu.

### Prawa kolumna — panel dowodzenia (stała szerokość 360 px)

1. Nagłówek: **„ROKAE COMMAND"** / podtytuł „System Telemetrii Operatora".
2. Wiersz **„LINK STATUS:"** + aktualny status połączenia (`robotClient.statusText`, duże litery).
3. Karta **„KINEMATYKA OPERATORA"** — cztery wiersze telemetrii (czcionka monospace, żeby
   cyfry nie skakały):

   | Etykieta w HUD | Właściwość QML | Kolor |
   |---|---|---|
   | Prawy łokieć | `videoReceiver.rightElbowAngle` | akcent (cyjan) |
   | Lewy łokieć | `videoReceiver.leftElbowAngle` | akcent (cyjan) |
   | Obrót głowy (Yaw) | `videoReceiver.headYaw` | ostrzeżenie (żółty) |
   | Pochylenie tułowia | `videoReceiver.torsoRoll` | ostrzeżenie (żółty) |

   Kąty łokci pokazują się z jednym miejscem po przecinku, a gdy wartość jest `≤ 0`
   wyświetla się `---.-°` (czyli „brak pomiaru"). Yaw i pochylenie tułowia formatowane
   przez `toFixed(1)`.
4. Przyciski (wypchnięte na dół panelu):
   - **„INICJALIZUJ POŁĄCZENIE" / „ZAKOŃCZ SESJĘ"** — połączenie i rozłączenie z robotem.
   - **„EMERGENCY STOP"** (czerony, wyższy od pozostałych) — awaryjne zatrzymanie.

## Powiązanie z kodem C++

Interfejs odwołuje się do dwóch obiektów, które muszą być „wstrzyknięte" z C++ do kontekstu QML
(np. przez `setContextProperty`) — **to wnioskowanie z kodu QML, nie z kod źródłowego C++**:

| Obiekt w QML | Używane właściwości / metody | Znaczenie |
|---|---|---|
| `robotClient` | `statusText`, `isConnected`, `connectToRobot("10.111.169.242", 9090)`, `disconnectFromRobot()`, `emergencyStop()` | klient WebSocket do rosbridge na Jetsonie |
| `videoReceiver` | `rightElbowAngle`, `leftElbowAngle`, `headYaw`, `torsoRoll` | wyniki detekcji pozy operatora |

> ⚠️ **Adres i port są zaszyte na sztywno w QML:** `robotClient.connectToRobot("10.111.169.242", 9090)`.
> Zmiana adresu Jetsona wymaga przebudowania aplikacji — nie ma pola wprowadzania adresu w interfejsie.

## Paleta kolorów

Zdefiniowana jako właściwości na początku pliku (łatwa zmiana motywu):

| Nazwa | Wartość | Zastosowanie |
|---|---|---|
| `colorBg` | `#0b0c10` | tło okna |
| `colorPanel` | `#151720` | panele |
| `colorAccent` | `#00e5ff` | akcent, telemetria, przycisk połączenia |
| `colorSuccess` | `#00ff66` | (zdefiniowany, w obecnej wersji nieużywany) |
| `colorWarning` | `#ffcc00` | yaw / pochylenie tułowia |
| `colorDanger` | `#ff2a2a` | E-STOP, rozłączenie |
| `colorText` / `colorTextMuted` | `#ffffff` / `#8a8d98` | tekst |

## Co HUD **nie** zawiera

- pola do wpisania adresu/portu robota (patrz uwaga powyżej),
- podglądu strumienia głębi (tylko obraz kolorowy — patrz
  [`07-sensory-kamery.md`](07-sensory-kamery.md) §4),
- wskaźników ramion robota (HUD pokazuje kąty **operatora**, nie robota),
- obsługi mikrofonu i głośników — to osobne ścieżki
  ([`08-sensory-mikrofon.md`](08-sensory-mikrofon.md), [`09-sensory-glosniki.md`](09-sensory-glosniki.md)).

## Do sprawdzenia / do zrobienia

- [ ] jaki jest dokładny typ `VideoItem` (Qt Multimedia) i czy klatki dokleja `VideoReceiver`,
      czy osobny wątek `VideoWorker` — patrz [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §2.1,
- [ ] co fizycznie robi `robotClient.emergencyStop()` — zatrzymuje tylko wysyłkę komend,
      czy woła też `op=7` / serwis zatrzymania robota,
- [ ] dlaczego są dwie wersje pliku (`Main.qml`, `Main2.qml`) i która jest używana w buildzie.
