> 📄 **Na podstawie:** `Helios_tracker.md` · treść bez zmian (dopasowano nazwę pliku i linki wewnętrzne) · 10 października 2026

# Podręcznik Użytkownika i Dokumentacja Techniczna: System Teleoperacji i Śledzenia Ruchu (Motion Retargeting) Robota Rokae Helios

Kompleksowa dokumentacja systemu teleoperacji czasu rzeczywistego dla dwuramiennego robota mobilnego **Rokae Helios** z 7-osiowymi ramionami **Rokae xMate AR5-R**, kamerą 3D **Orbbec Gemini 335L**, aplikacją desktopową **C++ / Qt 6 / QML** na systemie Windows oraz warstwą bezpieczeństwa w **ROS 2 Humble**.

---

## Spis Treści

1. [Architektura Całego Systemu](#1-architektura-ca%C5%82ego-systemu)
   - [1.1 Schemat przepływu danych](#11-schemat-przep%C5%82ywu-danych)
   - [1.2 Topologia sieciowa urządzeń robota](#12-topologia-sieciowa-urz%C4%85dze%C5%84-robota)
2. [Aplikacja Desktopowa Windows (C++ / Qt 6 / QML / OpenCV)](#2-aplikacja-desktopowa-windows-c--qt-6--qml--opencv)
   - [2.1 Komponenty i technologie](#21-komponenty-i-technologie)
   - [2.2 Model AI (YOLOv8-Pose ONNX) i ekstrakcja stawów](#22-model-ai-yolov8-pose-onnx-i-ekstrakcja-staw%C3%B3w)
   - [2.3 Filtracja ruchu (EMA + Deadband)](#23-filtracja-ruchu-ema--deadband)
   - [2.4 Klient sieciowy WebSocket (ROSBridge)](#24-klient-sieciowy-websocket-rosbridge)
3. [Warstwa Bezpieczeństwa na Robocie (C++: teleop_safety_bridge)](#3-warstwa-bezpiecze%C5%84stwa-na-robocie-c-teleop_safety_bridge)
   - [3.1 Zabezpieczenie przed uderzeniem w bazę robota](#31-zabezpieczenie-przed-uderzeniem-w-baz%C4%99-robota)
   - [3.2 Zabezpieczenie przed kolizją rąk między sobą](#32-zabezpieczenie-przed-kolizj%C4%85-r%C4%85k-mi%C4%99dzy-sob%C4%85)
   - [3.3 Zapobieganie błędom przeciążeniowym Safe/Stop (Slew Rate Limiter)](#33-zapobieganie-b%C5%82%C4%99dom-przeci%C4%85%C5%BCeniowym-safestop-slew-rate-limiter)
   - [3.4 Strażnik zaniku sygnału (Watchdog 400 ms)](#34-stra%C5%BCnik-zaniku-sygna%C5%82u-watchdog-400-ms)
4. [Sterowanie Fizycznym Ramieniem (rokae_driver7)](#4-sterowanie-fizycznym-ramieniem-rokae_driver7)
   - [4.1 Dlaczego rokae_driver7 a nie rokae_driver?](#41-dlaczego-rokae_driver7-a-nie-rokae_driver)
   - [4.2 Usługa MoveJ i mapowanie kątów na 7 osi](#42-us%C5%82uga-movej-i-mapowanie-k%C4%85t%C3%B3w-na-7-osi)
5. [Instrukcja Uruchomienia Krok po Kroku](#5-instrukcja-uruchomienia-krok-po-kroku)
   - [5.1 Szybki start: Jeden skrypt (start_teleop.sh)](#51-szybki-start-jeden-skrypt-start_teleopsh)
   - [5.2 Uruchomienie ręczne (opcjonalnie w osobnych oknach)](#52-uruchomienie-r%C4%99czne-opcjonalnie-w-osobnych-oknach)
   - [5.3 Start aplikacji na Windowsie i wykonanie ruchu](#53-start-aplikacji-na-windowsie-i-wykonanie-ruchu)
6. [Ściąga Wszystkich Komend (Command Cheat Sheet)](#6-%C5%9Aci%C4%85ga-wszystkich-komend-command-cheat-sheet)
7. [Baza Problemów i Rozwiązań (Troubleshooting)](#7-baza-problem%C3%B3w-i-rozwi%C4%85za%C5%84-troubleshooting)

---

## 1. Architektura Całego Systemu

### 1.1 Schemat przepływu danych

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           ROBOT ROKAE HELIOS (JETSON)                           │
│                                                                                 │
│   [Kamera Orbbec Gemini 335L]                                                   │
│                │                                                                │
│                ▼                                                                │
│      [web_video_server: 8080]                                                   │
└────────────────┼────────────────────────────────────────────────────────────────┘
                 │ Strumień HTTP/MJPEG (Wi-Fi)
                 ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         KOMPUTER OPERATORA (WINDOWS)                            │
│                                                                                 │
│   1. VideoReceiver (Wątek w tle): Odbiór klatek 640x480                         │
│   2. PoseDetector (YOLOv8-Pose ONNX): Detekcja 11 kluczowych stawów człowieka   │
│   3. Obliczenia Kątowe: Kąt łokcia (Joint 4) + Kąt uniesienia barku (Joint 2)   │
│   4. MotionFilter (EMA + Deadband): Wygładzenie mikrodrgań i szumu              │
│   5. Interfejs QML: Podgląd szkieletu, panel telemetrii, wskaźniki i E-STOP     │
│   6. RobotClient (WebSocket): Wysyłanie paczek JSON do Jetsona                  │
└────────────────┼────────────────────────────────────────────────────────────────┘
                 │ ws://10.111.169.242:9090 (Pakiety /teleop/joint_commands)
                 ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           ROBOT ROKAE HELIOS (JETSON)                           │
│                                                                                 │
│   [rosbridge_websocket: 9090]                                                   │
│                │ Temat ROS 2: /teleop/joint_commands (std_msgs/Float64MultiArray)│
│                ▼                                                                │
│   [teleop_safety_bridge (C++)]                                                  │
│      ├── Watchdog 400 ms (zamraża robota przy braku Wi-Fi)                      │
│      ├── Wirtualna podłoga Z (blokada uderzenia w platformę bazy)               │
│      ├── Wirtualna ściana centralna (blokada krzyżowania rąk)                   │
│      └── Slew Rate Limiter (kroki max 0.06 rad/cykl, prędkość 25%)              │
│                │ Usługa ROS 2: /rokae_driver7/movej                             │
│                ▼                                                                │
│   [rokae_driver7] (Ethernet 192.168.71.51 -> 192.168.71.160)                   │
│                │                                                                │
│                ▼                                                                │
│   [Fizyczne Ramię Rokae xMate AR5-R] ──► Płynny ruch w przestrzeni rzeczywistej!│
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Topologia sieciowa urządzeń robota

Komputer pokładowy **NVIDIA Jetson** pełni rolę routera i bramy między siecią zewnętrzną Wi-Fi a wewnętrzną magistralą przemysłową Ethernet:

| Urządzenie | Model | Interfejs | Adres IP w systemie | Rola w projekcie |
| :--- | :--- | :--- | :--- | :--- |
| **Sieć Zewnętrzna (Wi-Fi)** | Karta bezprzewodowa | `wlP1p1s0` | **`10.111.169.242`** | Komunikacja z laptopem: wideo (8080) i WebSocket (9090) |
| **Jetson (Sieć wewnętrzna)** | Karta LAN | `eno1` | **`192.168.71.51`** | Adres lokalny komputera nadrzędnego |
| **Prawe Ramię (AR5-R)** | Rokae xMate AR5-R (7-osi) | Ethernet | **`192.168.71.160`** | Sterowane fizyczne ramię |
| **Lewe Ramię (AR5-L)** | Rokae xMate AR5-L (7-osi) | Ethernet | **`192.168.71.161`** | Drugie ramię robota |
| **Baza Kołowa (AMR)** | Podwozie mobilne | Ethernet | **`192.168.71.50`** | Sterownik podwozia robota |
| **Tors (Tułów)** | TaiHu (4 osie) | Ethernet | **`192.168.71.254`** | Sterownik osi obrotu i pochylenia tułowia |
| **Kamera 3D** | Orbbec Gemini 335L | USB 3.2 | `2bc5:0804` | Kamera wizyjna w głowie robota |

---

## 2. Aplikacja Desktopowa Windows (C++ / Qt 6 / QML / OpenCV)

### 2.1 Komponenty i technologie
* **Kompilator:** MinGW 64-bit lub MSVC 2022 (C++17).
* **Interfejs GUI:** **Qt 6 QML** (renderowany sprzętowo na GPU).
* **Wizja:** **OpenCV 4** z modułem `cv::dnn`.
* **Wielowątkowość:** Wątek roboczy `VideoWorker` odseparowany od wątku GUI `QQuickPaintedItem`, co zapobiega zamrażaniu interfejsu przy zmiennej jakości sieci bezprzewodowej.

### 2.2 Model AI (YOLOv8-Pose ONNX) i ekstrakcja stawów
Do śledzenia operatora wykorzystano zoptymalizowany model **`yolov8n-pose.onnx`** (13.5 MB):
* Obraz wejściowy skalowany jest do $640 \times 640$.
* Do sterowania wybrano **11 istotnych punktów**:
  * **Głowa:** Nos (`0`), Uszy (`3`, `4`) – determinują obrót głowy (Yaw).
  * **Prawa ręka:** Prawy bark (`6`), Prawy łokieć (`8`), Prawy nadgarstek (`10`).
  * **Lewa ręka:** Lewy bark (`5`), Lewy łokieć (`7`), Lewy nadgarstek (`9`).
  * **Tułów:** Pas biodrowy (`11`, `12`) oraz obręcz barkowa (`5`, `6`).

#### Matematyka Kątowa:
1. **Kąt łokcia (Joint 4):** Kąt w przestrzeni między wektorami $\vec{BA}$ (bark) i $\vec{BC}$ (nadgarstek):
   $$\theta = \arccos\left(\frac{\vec{v}_1 \cdot \vec{v}_2}{\|\vec{v}_1\| \|\vec{v}_2\|}\right) \cdot \frac{180^\circ}{\pi}$$
2. **Kąt uniesienia barku (Joint 2):** Kąt między wektorem tułowia (biodro $\rightarrow$ bark) a wektorem ramienia (bark $\rightarrow$ łokieć).

### 2.3 Filtracja ruchu (EMA + Deadband)
Aby zapobiec nieprzyjemnemu drżeniu serwomotorów robota, wprowadzono klasę `MotionFilter`:
* **Martwa strefa (Deadband = $0.8^\circ$):** Zmiany mniejsze niż $0.8^\circ$ są ignorowane.
* **Wykładnicza średnia krocząca (EMA z $\alpha = 0.25$):**
  $$\theta_{\text{filtrowany}} = \alpha \cdot \theta_{\text{nowy}} + (1 - \alpha) \cdot \theta_{\text{poprzedni}}$$

### 2.4 Klient sieciowy WebSocket (ROSBridge)
Aplikacja wysyła sformatowane ramki JSON protokołu ROSBridge na port `9090`:
```json
{
  "op": "publish",
  "topic": "/teleop/joint_commands",
  "msg": {
    "layout": { "dim": [], "data_offset": 0 },
    "data": [shoulder_deg, elbow_deg, head_yaw, torso_roll]
  }
}
```

---

## 3. Warstwa Bezpieczeństwa na Robocie (C++: teleop_safety_bridge)

Węzeł `teleop_safety_bridge` działa w pętli czasu rzeczywistego (25 Hz) na komputerze Jetson i realizuje 4 krytyczne filtry:

### 3.1 Zabezpieczenie przed uderzeniem w bazę robota
* Jeśli człowiek opuści rękę pionowo w dół, kąt barku jest programowo ograniczany do minimum $10^\circ$, a kąt łokcia do minimum $15^\circ$.
* Ramię robota nie ma fizycznej możliwości uderzenia chwytakiem w platformę kołową.

### 3.2 Zabezpieczenie przed kolizją rąk między sobą
* W przypadku skrzyżowania rąk na klatce piersiowej, węzeł bada kąt pochylenia tułowia i wymusza zachowanie minimalnego rozstawu ramion ($>15^\circ$ separacji od osi symetrii $Y=0$).

### 3.3 Zapobieganie błędom przeciążeniowym Safe/Stop (Slew Rate Limiter)
* Kontroler przemysłowy Rokae natychmiast wbija hamulce awaryjne, jeśli wykryje gwałtowny skok przyspieszenia.
* Węzeł bezpieczeństwa narzuca maksymalny krok na cykl:
  $$\Delta \theta_{\text{max}} = 0.06\text{ rad (ok. } 3.5^\circ \text{ na cykl 40 ms)}$$
* Nawet przy gwałtownym machnięciu ręką, robot płynnie i spokojnie podąża za celem z zadaną prędkością maksymalną **25%**.

### 3.4 Strażnik zaniku sygnału (Watchdog 400 ms)
* Węzeł mierzy czas od ostatniej ramki odebranej z Windowsa:
  ```cpp
  if (elapsedMs > 400) { return; } // Zamrożenie pozycji w miejscu
  ```
* Przy wyłączeniu aplikacji, kliknięciu „Rozłącz” lub zerwaniu zasięgu Wi-Fi, ramię natychmiast zatrzymuje się w stabilnej pozycji i nie wykonuje żadnych niekontrolowanych ruchów.

---

## 4. Sterowanie Fizycznym Ramieniem (rokae_driver7)

### 4.1 Dlaczego rokae_driver7 a nie rokae_driver?
* Standardowy `rokae_driver` obsługuje wyłącznie roboty 6-osiowe (`rokae::xMateRobot`).
* Ramię robota Helios to **7-osiowy robot redundantny: `AR5-5_0.7R-W4C4A2`**.
* Próba uruchomienia wersji 6-osiowej zwraca błąd: `Robot instance type does not match`.
* Należy bezwzględnie używać dedykowanego węzła: **`rokae_driver7`** (używającego klasy `rokae::xMateErProRobot`).

### 4.2 Usługa MoveJ i mapowanie kątów na 7 osi
Sterownik wystawia usługę **`/rokae_driver7/movej`** typu `rokae_msgs/srv/MoveJ`:
* **`joint_positions` (tablica 7 wartości w radianach):**
  * `joint1` (Obrót podstawy): `0.0 rad`
  * `joint2` (Uniesienie barku): **Sterowane dynamicznie przez kąt barku człowieka** ($0.0 - 1.1\text{ rad}$)
  * `joint3` (Rotacja ramienia): `0.0 rad`
  * `joint4` (Zgięcie łokcia): **Sterowane dynamicznie przez kąt łokcia człowieka** ($0.0 - 1.35\text{ rad}$)
  * `joint5` (Rotacja przedramienia): `0.0 rad`
  * `joint6` (Zgięcie nadgarstka): `0.0 rad`
  * `joint7` (Rotacja chwytaka): `0.0 rad`
* **`velocity`:** Wartość skalowania prędkości (np. `0.25` = 25% prędkości znamionowej).

---

## 5. Instrukcja Uruchomienia Krok po Kroku

### 5.1 Szybki start: Jeden skrypt (start_teleop.sh)

Zamiast otwierać kilka terminali, na Jetsonie przygotowano skrypt nadrzędny, który uruchamia wszystkie procesy w tle i bezpiecznie zamyka je przy wciśnięciu `Ctrl + C`:

```zsh
~/start_teleop.sh
```

W terminalu pojawi się sekwencja startowa:
```text
====================================================
    STARTOWANIE SYSTEMU TELEOPERACJI ROKAE HELIOS    
====================================================
[1/4] Uruchamianie kamery Orbbec Gemini 335L...
[2/4] Uruchamianie Web Video Server (port 8080)...
[3/4] Uruchamianie Rosbridge WebSocket (port 9090)...
[4/4] Uruchamianie Teleop Safety Bridge (C++)...
[Sterownik] Uruchamianie rokae_driver7 dla AR5-R (192.168.71.160)...
====================================================
   WSZYSTKIE USŁUGI DZIAŁAJĄ POPRAWNIE!
   Wciśnij Ctrl+C, aby bezpiecznie wyłączyć całość.
====================================================
```

---

### 5.2 Uruchomienie ręczne (opcjonalnie w osobnych oknach)

Jeśli chcesz mieć pełny podgląd logów każdego modułu osobno:

* **Terminal 1 (Kamera 3D):**
  ```zsh
  source /opt/ros/humble/setup.zsh
  ros2 launch orbbec_camera gemini_330_series.launch.py
  ```
* **Terminal 2 (Wideo HTTP 8080):**
  ```zsh
  source /opt/ros/humble/setup.zsh
  ros2 run web_video_server web_video_server
  ```
* **Terminal 3 (WebSocket 9090):**
  ```zsh
  source /opt/ros/humble/setup.zsh
  ros2 launch rosbridge_server rosbridge_websocket_launch.xml
  ```
* **Terminal 4 (Sterownik fizycznego ramienia):**
  ```zsh
  source /opt/ros/humble/setup.zsh
  source ~/rokae_ws/install/setup.zsh
  ros2 run rokae_hardware rokae_driver7 --ros-args -p robot_ip:=192.168.71.160 -p local_ip:=192.168.71.51
  ```
* **Terminal 5 (Węzeł Bezpieczeństwa C++):**
  ```zsh
  source /opt/ros/humble/setup.zsh
  source ~/rokae_ws/install/setup.zsh
  ros2 run rokae_hardware teleop_safety_bridge
  ```

---

### 5.3 Start aplikacji na Windowsie i wykonanie ruchu

1. Otwórz projekt `TrackingRobot` w **Qt Creatorze**.
2. Wciśnij **`Ctrl + R`** (Uruchom).
3. Kliknij przycisk **`POŁĄCZ Z ROBOTEM`** (status zmieni się na zielony `POŁĄCZONY`).
4. Upewnij się, że silniki robota mają zasilanie (Power On / styczniki załączone w RobotAssist lub na kasecie).
5. **Trzymaj fizyczny grzybek bezpieczeństwa w dłoni.**
6. Stań przed kamerą w odległości 1.5–2 metrów:
   * **Zegnij łokieć:** Robot zsynchronizuje zgięcie łokcia (Joint 4).
   * **Unieś rękę przed siebie:** Robot uniesie całe ramię w górę (Joint 2).

---

## 6. Ściąga Wszystkich Komend (Command Cheat Sheet)

| Czynność | Komenda |
| :--- | :--- |
| **Start całego systemu teleoperacji** | `~/start_teleop.sh` |
| **Kompilacja pakietów robota w workspace** | `cd ~/rokae_ws && colcon build --packages-select rokae_msgs rokae_hardware && source install/setup.zsh` |
| **Podgląd przychodzących komend z Windowsa** | `ros2 topic echo /teleop/joint_commands` |
| **Podgląd bezpiecznych stanów wysyłanych do robota** | `ros2 topic echo /joint_states` |
| **Podgląd fizycznych enkoderów ramienia** | `ros2 topic echo /rokae_driver7/joint_states --once` |
| **Odpytanie kontrolera o wersję sprzętową** | `ros2 service call /rokae_driver7/get_robot_info rokae_msgs/srv/GetRobotInfo` |
| **Ręczny test mikro-ruchu łokciem o 5° (MoveJ)** | `ros2 service call /rokae_driver7/movej rokae_msgs/srv/MoveJ "{joint_positions: [0.0, 0.0, 0.0, 0.087, 0.0, 0.0, 0.0], velocity: 0.05}"` |
| **Ping sterownika prawego ramienia** | `ping 192.168.71.160` |
| **Podgląd wideo z kamery robota w przeglądarce** | `http://10.111.169.242:8080/stream?topic=/camera/color/image_raw` |

---

## 7. Baza Problemów i Rozwiązań (Troubleshooting)

### Problem 1: `Robot instance type does not match with the connected robot AR5-5_0.7R-W4C4A2`
* **Przyczyna:** Uruchomiono węzeł `rokae_driver` (dla 6 osi) zamiast `rokae_driver7` (dla 7 osi).
* **Rozwiązanie:** Zawsze uruchamiaj wersję 7-osiową: `ros2 run rokae_hardware rokae_driver7 ...`.

---

### Problem 2: `The passed service type is invalid` przy wywoływaniu usług
* **Przyczyna:** W nowym oknie terminala załadowano tylko bazowy ROS (`/opt/ros/humble/setup.zsh`), zapominając o workspace robota.
* **Rozwiązanie:** Wykonaj: `source ~/rokae_ws/install/setup.zsh`.

---

### Problem 3: `Package 'rokae_hardware' not found` po kompilacji
* **Przyczyna:** Zmienne `AMENT_PREFIX_PATH` wskazują na nieaktualny folder instalacji lub pominięto pakiet `rokae_msgs`.
* **Rozwiązanie:** Zbuduj oba pakiety razem:
  ```zsh
  cd ~/rokae_ws
  colcon build --packages-select rokae_msgs rokae_hardware
  source install/setup.zsh
  ```

---

### Problem 4: Błędy DLL na Windowsie (`pthread_cond_timedwait64` / `clock_gettime64`)
* **Przyczyna:** Niezgodność środowiska uruchomieniowego między MinGW z Qt a MinGW UCRT z MSYS2.
* **Rozwiązanie:** Skopiuj biblioteki `libwinpthread-1.dll`, `libtbb12.dll`, `libstdc++-6.dll` bezpośrednio do folderu z plikiem `.exe` programu (`build/.../appTrackingRobot.exe`).

---

### Problem 5: Błąd `cv_bridge exception: [16UC1] is not a color format` w przeglądarce
* **Przyczyna:** Próba otwarcia strumienia głębi `/camera/depth/image_raw` jako wideo JPEG. Dane głębi to 16-bitowa tablica odległości w milimetrach, a nie kolor.
* **Rozwiązanie:** Do podglądu wideo używaj tematu barwnego `/camera/color/image_raw`.

---

### Problem 6: Błąd `Illegal attempt to connect to VideoReceiver that is in a different thread` w Qt
* **Przyczyna:** Obiekt zarejestrowany w QML został przeniesiony do `QThread` (`moveToThread`), co łamie zasadę Qt Thread Affinity.
* **Rozwiązanie:** Podział na dwa obiekty: `VideoWorker` (wykonujący pętlę AI w tle) oraz `VideoReceiver` (kontroler żyjący w głównym wątku GUI).

---

### Problem 7: Błąd `Connection to tcp://10.111.169.242:8080 failed: Error number -138`
* **Przyczyna:** Serwer wideo `web_video_server` nie został uruchomiony na robocie przed włączeniem aplikacji w Windowsie.
* **Rozwiązanie:** Uruchom skrypt `~/start_teleop.sh` na Jetsonie przed kliknięciem Run w Qt Creatorze.
