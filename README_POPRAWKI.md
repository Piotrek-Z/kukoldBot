# Poprawione pliki — co podmienić i dlaczego

Ten katalog zawiera **poprawione wersje Twoich plików pod tymi samymi nazwami**.
Podmieniasz plik po pliku, kompilujesz — nic nie trzeba przepisywać od zera.

```
poprawione/
├─ windows/    <- aplikacja Qt/QML na Windows (podmiana w Twoim projekcie)
└─ robot/      <- pliki na robocie (sterownik + mostek)
```

---

## 0. Najkrócej: co naprawia Twoje dwa objawy

| Objaw | Przyczyna w Twoim kodzie | Naprawione w |
|---|---|---|
| „nie może ruszyć ramienia za plecy” | `teleop_safety_bridge.cpp`: **J3 było stałe** (`target[2] = 2.95`), a J3 decyduje o płaszczyźnie łokcia. Twoje zrzuty 112940 vs 113041 różnią się **głównie J3 (178,0° vs −4,46°)** | nowy `teleop_safety_bridge.cpp` — bez zamrożonych stawów, z oficjalnymi limitami AR5R |
| „nie potrafi zgiąć łokcia w dół” | twarde obcięcia: J1 ∈ ±83,1° przy potrzebnym **−82,9°** i J4 ≤ 126,1° przy potrzebnym **126,7°** — stałeś na samej krawędzi, a J1 liczyło się z odległości dłoni **w pikselach** | `helios_teleop_mapper.cpp` (nowy rdzeń) + `robotclient.cpp` (7 kątów w radianach) + `posedetector.cpp` (koniec heurystyk 2D) |
| „robot gubi się”, 2–3 s opóźnienia | `rokae_driver7.cpp`: wątek + blokujące `MoveJ` na **każdą** wiadomość, `g_isMoving` odrzucało resztę (200–900 ms); `posedetector` bez NMS i bez trackingu; filtr z betą w jednostkach „pikseli myszy” | `rokae_driver7.cpp` (serwo RT 1 kHz) + `rokae_rt_servo.h` + `posedetector.cpp` + `one_euro_filter.h` |
| **E-STOP** ⚠️ | `robotclient.cpp::emergencyStop()` wysyłało **pięć zer** na temat sterujący = rozkaz „jechać do zera wszystkich stawów” (gwałtowny ruch!) | `robotclient.cpp` — E-STOP nic nie wysyła: rozbraja mostek + `/teleop/stop` + `/rokae_driver7/stop_rt` |

---

## 1. `windows/` — co podmienić

### Pliki **poprawione** (podmień 1:1)

| Plik | Co dokładnie zmienione |
|---|---|
| `posedetector.h` / `.cpp` | letterbox zamiast rozciągania do 640×640; **NMS** (było: „weź najlepszy wiersz”); **tracking operatora** między klatkami (było: wybór po odległości od środka kadru co klatkę — stąd przeskakiwanie między ludźmi); progi conf/kpt z configu (były zignorowane); **opcjonalna głębia** — `processFrame(frame, depth, fx, fy, cx, cy)` wypełnia `KeyPoint3D.z` [m]; heurystyki kątów 2D **usunięte** (to one nie umiały odróżnić „do przodu” od „w bok”) |
| `videoreceiver.h` / `.cpp` | `cv::VideoCapture` (buforujące!) → **`MjpegGrabber`** (zawsze najnowsza klatka, stare odrzucane); `VideoWorker` zastąpiony przez `AiWorker` z jednoslotową skrzynką klatek; lokalny potok: detekcja → geometria 3D → **mapowanie na 7 stawów**; nowy sygnał **`jointTargetsReady(QVector<double> q_rad)`**; KALIBRUJ/ENGAGE/DISENGAGE/STOP jako `Q_INVOKABLE`; właściwości QML `rightShoulderYaw/Pitch/Elbow` **zachowane** (teraz pokazują prawdziwe wartości z mapowania) |
| `robotclient.h` / `.cpp` | 7 kątów **[rad]** na `/teleop/joint_targets` (było: 5 liczb na `/teleop/joint_commands`); serwisy `/teleop/calibrate_neutral|engage|disengage|stop` i `/rokae_driver7/stop_rt`; telemetria zwrotna `/teleop/human_arm`, `/teleop/bridge_state`, `/rokae_driver7/joint_states`; **naprawiony E-STOP**; lokalny bezpiecznik limitów AR5R; `sendJointAngles()`/`sendCartesianTarget()` zostawione w API jako no-op z ostrzeżeniem (żeby stary kod się skompilował, ale już nic nie wysyłał po starym protokole) |
| `one_euro_filter.h` | poprawione jednostki (beta w 1/(jedn. sygnału) — w v1 0.004–0.008 dla kątów w stopniach = filtr prawie nie nadążał, pasmo 0.7 Hz ≈ **227 ms opóźnienia**); martwa strefa **nie blokuje** już filtra (działa na residuum, domyślnie wyłączona); sensowne `dt` (0/ujemne); `setOutputDeadband()` |
| `motionfilter.h` | filtr ze **stałą czasową** zamiast „alpha” (v1 zachowywał się inaczej przy 15 i przy 30 fps); deadband na residuum; opcjonalna klamra prędkości |
| `appconfig.h` | czyta `config.json` v2 (`topic_joint_targets`, `topic_human_arm`, sekcja `teleop`, `dir_beta`…), a stare klucze rozumie jako fallback; wszystkie pola z v1 zachowane (`topicJointCommands`, `controlPhase`, `model3DPath`, `depthCameraUrl`, `filterMinCutoff/Beta/DCutoff`…) |
| `main.cpp` | `allAnglesUpdated → sendJointAngles` zastąpione przez **`jointTargetsReady → sendJointTargets`**; telemetria robota → VideoReceiver (tryb A); usunięty `qmlRegisterType<VideoItem>` (podwójna rejestracja z `QML_ELEMENT`); zegar pozycji robota 100 ms; strumień kamery startuje z QML (jedno miejsce) |
| `Main.qml` | **tylko dwie wstawki**: przyciski `KALIBR.` / `ENGAGE`(→`DISENGAGE`) obok START/STOP/LOG oraz dwa wiersze telemetrii („Jakość danych AI”, „Stan teleop”). Układ, kolory i E-STOP bez zmian |
| `CMakeLists.txt` | źródła aplikacji przeniesione do modułu QML (w v1 były wymienione **dwa razy** → dwie kopie klas i podwójna rejestracja typów); doszły pliki `helios_*`, `mjpeg_grabber.*`; OpenCV bez zmian |

### Pliki **bez zmian** (kopiuję je tylko dla kompletności)
`videoitem.h/.cpp`, `helioslink.h/.cpp`, `sshcore.h/.cpp`, `cwel.h`, `ChatPanel.qml`, `LogWindow.qml`

### Pliki **nowe** (dodaj do projektu — bez nich się nie skompiluje)
| Plik | Rola |
|---|---|
| `helios_ar5r.h/.cpp` | model AR5R: limity, prędkości, kinematyka prosta (walidacja na Twoich 6 zrzutach: zgodność 0,01 mm) |
| `helios_human_arm.h/.cpp` | geometria ramienia człowieka **w układzie tułowia**, 3D (z głębią), walidacja jakości, filtry kierunku |
| `helios_teleop_mapper.h/.cpp` | mapowanie na **7 stawów**, względne (od ENGAGE), limity, limiter prędkości, filtr wyjścia |
| `helios_filters.h` | One-Euro (poprawiony) dla wektorów i skalarów używany przez rdzeń |
| `mjpeg_grabber.h/.cpp` | niskolatencyjny czytnik MJPEG (zamiast `cv::VideoCapture`) |

> `helios_*` to zwykły C++17 bez Qt/ROS/OpenCV — kompiluje się w Twoim MinGW tak samo jak resztą projektu (jest już na liście w nowym `CMakeLists.txt`).

---

## 2. `robot/` — co podmienić

| Plik | Status | Co zrobione |
|---|---|---|
| `rokae_driver7.cpp` | **poprawiony** (Twój plik, 1165 linii) | patrz lista niżej — sterowanie idzie przez serwo RT 1 kHz |
| `rokae_rt_servo.h` | **nowy**, wymagany | klasa `helios_rt::RokaeRtServo`: pętla 1 kHz (`startMove` + `setControlLoop` + `startLoop`), interpolacja do ostatniego celu z limitem prędkości, spinlock zamiast mutexa (brak inwersji priorytetów), watchdog 0,30 s → **zamrożenie** ostatniej pozycji, obcięcie do oficjalnych limitów AR5R, ochrona przed NaN/inf, tryb awaryjny `use_control_loop = false` (`sendCommand`) |
| `teleop_safety_bridge.cpp` | **poprawiony** | mostek robi **tylko bezpieczeństwo**: limity AR5R z marginesem, soft-start od aktualnej pozycji, watchdog 200 ms, limiter prędkości/przyspieszenia, kontrola nadążania (25°), serwisy `/teleop/stop`, publikuje stan na `/teleop/bridge_state`. **Mapowanie i stałe J3/J5/J6 usunięte** (to one blokowały ruch za plecy) — mapowaniem zajmuje się `helios_pose_node` na robocie (albo AiWorker na Windows w trybie B) |

### Dokładna lista zmian w `rokae_driver7.cpp`

1. `#include "rokae_rt_servo.h"` (linia 4).
2. **Usunięte**: `g_rtCon`, `g_isStreaming`, `g_carrotMutex`, `g_isMoving`, zakomentowany wariant `servoj` oraz cała funkcja `onCarrotTargetReceived` z `std::thread` + `MoveJ` + `}).detach()`.
3. **Wstawione**: `static std::unique_ptr<helios_rt::RokaeRtServo> g_servo;` i nowe `onCarrotTargetReceived`, które woła `g_servo->submit(target)` (log 1 Hz zamiast 1 wiadomość = 1 wątek) + ostrzeżenie, gdy serwo nieaktywne.
4. `stop_rt_callback`: `g_servo->stop()` (stop pętli RT) na początku — przed `robot.stop(...)`.
5. `movej_callback` / `movel_callback` / `movec_callback`: **odmawiają** wykonania, gdy serwo RT jest aktywne (komunikat: „najpierw /rokae_driver7/stop_rt”). Bez tego ruch planowany i serwo walczyłyby o napęd.
6. `state_monitor_worker`: 100 Hz → **25 Hz** (`sleep_for(40ms)`). Odczyt `robot.jointPos()` to zapytanie do sterownika — przy 100 Hz zabierało pasmo pętli RT 1 kHz.
7. `main()`: utworzenie serwa `std::make_unique<helios_rt::RokaeRtServo>(robot, 1.0, 0.30)` i zatrzymanie go przy zamykaniu, przed `setMotionControlMode(Idle)`.
8. Usunięty przypadkowy niecenzuralny komentarz (kosmetyka).

W `CMakeLists.txt` sterownika **nic nie trzeba dodawać** — `rokae_rt_servo.h` jest samodzielny (tabela Vmax i limity są w środku). Upewnij się tylko, że includuje ścieżkę, w której leży ten nagłówek.

---

## 3. Kolejność wdrożenia (zalecana, krok po kroku)

Na robocie — **najpierw patch sterownika, bo to on daje „real-time” i pozwala wrócić do v1, gdyby coś nie wyszło**:

```bash
# 1) sterownik
cp rokae_rt_servo.h <katalog_twojego_sterownika>/
#    podmień rokae_driver7.cpp
colcon build --packages-select <twoj_pakiet_sterownika>   # (albo catkin/make - jak robisz)
ros2 run <pkg> rokae_driver7
ros2 topic hz /rokae_driver7/joint_states      # ma być ~25 Hz

# 2) mostek (na robocie)
#    podmien teleop_safety_bridge.cpp i zbuduj razem z resztą swoich węzłów

# 3) Windows: podmień pliki z poprawione/reindows/ i zbuduj
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.6.0/mingw_64
cmake --build build -j
```

Następnie w aplikacji, **z ręką na E-STOPIE, przy `speed_scale = 0.2`**:
1. KALIBR. (operator przodem do robota, ramię luźno w dół, łokieć ~15°),
2. ENGAGE,
3. 6 ruchów kontrolnych z README (sekcja 8) — kolejno: do przodu, zgięcie łokcia, dłoń przed ciałem, dłoń za ciałem, w bok, „łokieć w dół”,
4. jeśli jakiś kierunek jest odwrotny — w `mapper.ini` `sign.jN = -1`, restart węzła/AI,
5. dopiero gdy wszystkie 6 przechodzą — `speed_scale = 0.65`.

---

## 4. Czego **nie** zmieniłem (świadomie)

* `ChatPanel.qml`, `LogWindow.qml`, `helioslink.*`, `sshcore.*` — czat/SSH działały, nie było powodu ich ruszać.
* Limity stawów: użyte **oficjalne** wartości AR5R (J1/J2/J3/J5 ±178°/±120°/±178°/±178°, J4 −60°…+145°, J6/J7 ±60°) z marginesem 6° w mostku.
* Endpointy (192.168.49.97, rosbridge 9090, MJPEG 8080) — bez zmian, nadpisywalne w `config.json`.
* `mediapipe_pose_3d.onnx` / tryb „cartesian_ik” — nieużywane w v1, zostawione jako pola configu (nie usuwam, żebyś nie stracił konfiguracji).

---

## 5. Jedna rzecz do zrobienia ręcznie na stanowisku

Kierunek „góra świata” w układzie bazowym robota (orientacja montażu) **nie da się wyliczyć
z odczytów momentów** — sprawdziłem to liczbowo: dopasowanie modelu statyki daje |Fg| = 3–18 N,
gdy same człony ważą ~96 N, więc kolumna `Torque(Nm)` z Monitora nie jest czystym momentem
grawitacyjnym. Dlatego mapowanie jest **względne** (od ENGAGE), a jeśli po kalibracji ruchy
wychodzą „obrócone”, poprawisz to jednym przebiegiem:

```bash
ros2 service call /teleop/teach_sample std_srvs/srv/Trigger     # po każdym z 6-8 ruchów barkiem
python3 tools/fit_mapping.py teach.csv                          # wypisze gotowe axis.* / sign.*
ros2 service call /teleop/save_config std_srvs/srv/Trigger
```
(zestaw „nowej architektury” z tego przebiegu: `/home/user/helios_teleop/` — tam jest też `tools/fit_mapping.py`
i pełny `README_PL.md`; ten katalog `poprawione/` to wersja „podmień moje pliki”).
