# Tutorial: jak uruchamiać programy

Praktyczny przewodnik po wszystkich programach w repozytorium: co zbudować, jak uruchomić
i sprawdzić, że działa.

> **Legenda źródeł:** komendy oznaczone 📄 pochodzą z dokumentacji zespołu,
> 🔧 z czytania kodu, ❓ są do uzupełnienia (nie ma ich nigdzie zapisanych).

---

## 1. Mapa programów

| Program | Gdzie jest | Jak uruchamiać | Uwaga |
|---|---|---|---|
| `connect_test` | `connect_test-pl.cpp` (katalog główny) | konsola, argument = IP robota | panel sterowania jedną ręką |
| `prezentacja` | `prezentacja.cpp` (katalog główny) | konsola, bez argumentów | ramiona + tors + głowa |
| `helios_probe` | `helios_probe.cpp` (katalog główny) | węzeł ROS 2 | odczyt stanu ramienia |
| aplikacja trackingu | `modelDoTrackingu/` + archiwa `TrackingRobot*` | Qt Creator → `Ctrl+R` | źródła C++ tylko w archiwach |
| narzędzia bazy jezdnej | `sterowaniePodstawa/` (po rozpakowaniu `helios_amr.7z`) | `make`, potem `./baza.sh …` | osobny protokół |
| skrypt startowy robota | na Jetsonie: `~/start_teleop.sh` | bezpośrednio na robocie | ❓ nie ma go w repo |

---

## 2. Co musi być zainstalowane

| Do czego | Czego trzeba | Skąd wiemy |
|---|---|---|
| programy na SDK | xCore SDK 0.7.1 (aarch64) + Eigen3 + `orocos-kdl` | 📄 [`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md) |
| węzły ROS 2 | ROS 2 Humble, `colcon`, workspace `~/rokae_ws` | 📄 [`11`](11-teleoperacja-helios-probe.md), [`10`](10-teleoperacja-tracker.md) |
| aplikacja Windows | Qt 6 (MinGW 64-bit albo MSVC 2022), OpenCV 4 z `cv::dnn` | 📄 [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §2.1 |
| narzędzia bazy | kompilator C++17 (`make`), Python 3 (narzędzia pomocnicze) | 📄 [`14-baza-jezdna-dokumentacja.md`](14-baza-jezdna-dokumentacja.md) §10.7 |
| kamery / audio | pakiety `ros-humble-*` (lista w dokumentach) | 📄 [`07`](07-sensory-kamery.md), [`08`](08-sensory-mikrofon.md) |

---

## 3. `connect_test` — panel sterowania jedną ręką

### Budowa

> ❓ **W repo nie ma pliku buildowego** dla tego programu. Poniżej szablon oparty na sposobie
> linkowania opisanym dla węzła `helios_probe` (📄 [`11`](11-teleoperacja-helios-probe.md),
> sekcja „Linkowanie (statyczne)") — ścieżki do SDK trzeba uzupełnić u siebie.

```bash
SDK=~/rokae_ws/src/rokae_ros2-main/rokae_hardware/sdk   # ❓ dostosuj do swojego układu

g++ -std=c++17 -O2 -DXMATEMODEL_LIB_SUPPORTED \
    connect_test-pl.cpp -o connect_test \
    -I"$SDK/include" -I/usr/include/eigen3 \
    "$SDK/lib/libxCoreSDK.a" "$SDK/lib/libxMateModel.a" \
    -lorocos-kdl -lpthread
```

### Uruchomienie 🔧 (z kodu)

```bash
./connect_test <IP_robota> [IP_lokalne]
./connect_test 192.168.71.160        # prawa ręka
./connect_test 192.168.71.161        # lewa ręka
```

Po uruchomieniu pojawia się menu 0–7 (szczegółowy opis: [`13-programy-cpp.md`](13-programy-cpp.md) §1).
Najważniejsze: **opcja 1** włącza zasilanie, **opcja 4** włącza drag mode (ramię daje się
przeciągać ręką), **opcja 7** resetuje błędy/E-Stop, **opcja 0** bezpiecznie kończy program.

### Sprawdzenie, że działa

- w logu: `POŁĄCZONO POMYŚLNIE!` 🔧
- opcja 6 (test sieci) pokazuje skuteczność odczytu i opóźnienia 🔧

---

## 4. `prezentacja` — program prezentacyjny

Buduje się jak `connect_test` (ten sam SDK, te same biblioteki), uruchamia się **bez argumentów** 🔧.

```bash
g++ -std=c++17 -O2 -DXMATEMODEL_LIB_SUPPORTED \
    prezentacja.cpp -o prezentacja \
    -I"$SDK/include" -I/usr/include/eigen3 \
    "$SDK/lib/libxCoreSDK.a" "$SDK/lib/libxMateModel.a" \
    -lorocos-kdl -lpthread

./prezentacja
```

Adresy są **zaszyte w kodzie** 🔧: lewa `.161`, prawa `.160`, tors `.254`, komputer `.51`.
Jeśli pracujesz w innej sieci — patrz [`TUTORIAL-ZMIANA-SIECI.md`](TUTORIAL-ZMIANA-SIECI.md).

Menu 0–9, w tym **opcja 1** (przygotuj wszystko: reset + Power ON), **opcja 8** (pełna prezentacja),
**opcja 9** (awaryjny Power OFF). Pełny opis: [`13-programy-cpp.md`](13-programy-cpp.md) §2.

### Sprawdzenie, że działa

- log: `[INIT] Lewa ręka POŁĄCZONA.`, `[INIT] Prawa ręka POŁĄCZONA.`, `[INIT] Tułów POŁĄCZONY.` 🔧
- jeśli tors się nie połączy, program działa dalej z samymi ramionami (to nie jest błąd) 🔧

---

## 5. `helios_probe` — węzeł ROS 2

W repozytorium jest tylko plik źródłowy; pełny pakiet żyje w workspace na robocie
(📄 [`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md)).

### Budowa 📄

```bash
cd ~/rokae_ws
colcon build --packages-select helios_probe --cmake-clean-cache --cmake-args \
  -DXCORE_SDK_DIR=/home/std/rokae_ws/src/rokae_ros2-main/rokae_hardware/sdk \
  -DHELIOS_DOF=7
source install/setup.zsh
```

### Uruchomienie 📄

```bash
ros2 run helios_probe helios_probe --ros-args -p robot_ip:=192.168.71.160
```

| Parametr | Domyślnie | Uwaga |
|---|---|---|
| `robot_ip` | `192.168.0.160` | ⚠️ wartość domyślna **nie jest** adresem ramienia Heliosa — podawaj jawnie |
| `local_ip` | `""` | potrzebny tylko do ruchu (tryb RT), nie do samego odczytu |

### Sprawdzenie, że działa 📄

```bash
ros2 topic echo /helios/joint_states        # 10 Hz, kąty w radianach
ros2 topic hz /helios/joint_states
```

W logu powinny być: model, liczba osi, wersja kontrolera, zasilanie, tryb pracy, stan pracy 🔧.

---

## 6. Aplikacja do trackingu (Windows)

1. Rozpakuj archiwum z aplikacją (`TrackingRobot*.zip` / `TrackingRobotV3.7z` w `modelDoTrackingu/`).
   ❓ **Która wersja jest aktualna — nie jest nigdzie zapisane.**
2. Otwórz projekt w **Qt Creatorze**.
3. Upewnij się, że model `yolov8n-pose.onnx` jest w miejscu, którego szuka program ❓.
4. `Ctrl + R` (Uruchom).
5. Kliknij **POŁĄCZ Z ROBOTEM** — status ma zmienić się na zielony `POŁĄCZONY`.

📄 Szczegóły i pułapki: [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §5.3 i §7
(m.in. błędy DLL: `libwinpthread-1.dll`, `libtbb12.dll`, `libstdc++-6.dll` kopiuje się
do folderu z `.exe`).

⚠️ Adres robota jest **zaszyty w QML** (`10.111.169.242`, port 9090) 🔧 —
zmiana sieci wymaga przebudowania aplikacji: [`TUTORIAL-ZMIANA-SIECI.md`](TUTORIAL-ZMIANA-SIECI.md).

---

## 7. Narzędzia bazy jezdnej

Kod leży w `sterowaniePodstawa/` — w repo jest tylko archiwum `helios_amr.7z`.

```bash
cd sterowaniePodstawa
7z x helios_amr.7z        # ❓ strukturę katalogów po wypakowaniu trzeba sprawdzić
make                     # buduje wszystkie binarki (C++17, -O2 -Wall -Wextra)  📄
make test                # testy rdzenia: parser, koperta, patche, stan        📄
```

Najprostsze wejście to nakładka `baza.sh` 📄 [`14-baza-jezdna-dokumentacja.md`](14-baza-jezdna-dokumentacja.md) §10.6:

```bash
./baza.sh stan                  # podgląd stanu bazy (5 s)
./baza.sh przod 150 2           # jedź prosto 150 mm/s przez 2 s
./baza.sh lewo 200 1            # obrót w lewo 200 mrad/s przez 1 s
./baza.sh wasd                  # prowadzenie z klawiatury (W/S/A/D, SPACJA = stop)
./baza.sh stop                  # natychmiastowe zatrzymanie
./baza.sh podglad przod 100 1   # pokaż, co by wysłało — NIE wysyła
```

Adres bazy zmieniasz zmiennymi środowiskowymi (domyślnie `192.168.71.50:5002`) 📄:

```bash
AMR_IP=127.0.0.1 AMR_PORT=5003 ./baza.sh przod 100 1     # test bez robota
```

### Test bez robota 📄

```bash
make bench                                             # atrapa bazy na porcie 5003
AMR_IP=127.0.0.1 AMR_PORT=5003 ./baza.sh przod 100 1
```

---

## 8. Narzędzia pomocnicze (Python / JS) 📄

```bash
python3 tools/bench_server.py --port 5003 --auto-mission   # atrapa bazy
python3 tools/analyze_capture.py cap/panel_paste.log       # analiza zrzutu ramek
python3 tools/test_wasd_pty.py --keys w=1.5,=1.0,d=1.0 -- ./baza.sh wasd   # test klawiatury
```

`tools/panel_hook.js` wkleja się w konsoli DevTools przeglądarki, żeby zapisać ruch panelu
Matrix (`panel_capture.log`) — opis procedury: [`15-baza-jezdna-przewodnik.md`](15-baza-jezdna-przewodnik.md) §6.

---

## 9. Ściąga: jedno okno

```bash
# 1. zbuduj i uruchom węzeł odczytu (na robocie / po SSH do Jetsona)
source /opt/ros/humble/setup.zsh && source ~/rokae_ws/install/setup.zsh
ros2 run helios_probe helios_probe --ros-args -p robot_ip:=192.168.71.160

# 2. kamera + serwer wideo (dwa terminale, nie zamykaj pierwszego!)
ros2 launch orbbec_camera gemini_330_series.launch.py
ros2 run web_video_server web_video_server

# 3. aplikacja operatora (Windows, Qt Creator) → Ctrl+R → POŁĄCZ Z ROBOTEM

# 4. baza jezdna (opcjonalnie)
cd sterowaniePodstawa && ./baza.sh wasd
```

---

## 10. Czego w tym tutorialu brakuje

- [ ] gotowych plików buildowych dla `connect_test-pl.cpp` i `prezentacja.cpp`,
- [ ] wskazania, która wersja archiwum `TrackingRobot*` jest aktualna,
- [ ] instrukcji budowy aplikacji Windows od zera (z archiwum),
- [ ] potwierdzenia, że szablon kompilacji z p. 3 faktycznie działa (nikt go nie sprawdził).
