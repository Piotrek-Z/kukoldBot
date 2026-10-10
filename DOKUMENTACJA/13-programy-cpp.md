# Programy C++ w repozytorium (xCore SDK)

Opis trzech programów konsolowych leżących w katalogu głównym repozytorium: do czego służą,
jak się obsługują i na jakich klasach SDK pracują.

> **Skąd ta wiedza:** wyłącznie z czytania kodu źródłowego (`.cpp`) w repozytorium.
> Nie ma do nich osobnej dokumentacji — ten plik ją tworzy. Zachowania, których nie da się
> wyczytać z kodu (np. faktyczne wyniki testów na robocie), są oznaczone jako „do sprawdzenia".

## Wspólne założenia

- Wszystkie trzy programy używają **xCore SDK 0.7.1** (`#include "rokae/robot.h"`).
- Ręce: klasa `rokae::xMateErProRobot` (7 osi).
- Tułów: klasa `rokae::PCB4Robot` (4 osie + 2 osie zewnętrzne głowy).
- Adresy (zaszyte w kodzie lub podawane jako argumenty): lewa ręka `192.168.71.161`,
  prawa ręka `192.168.71.160`, tułów `192.168.71.254`, komputer pokładowy `192.168.71.51`.
- **W repozytorium nie ma plików buildowych** (`CMakeLists.txt`, `Makefile`) dla tych programów
  — żeby zbudować, trzeba dodać własny projekt i podlinkować SDK. To jest luka do wypełnienia.

---

## 1. `connect_test-pl.cpp` — interaktywny panel sterowania jedną ręką

Program konsolowy do pojedynczego ramienia. Po uruchomieniu łączy się z robotem i pokazuje menu.

**Uruchomienie**

```bash
./connect_test <IP_robota> [IP_lokalne]
# np. ./connect_test 192.168.71.160        # prawa ręka
#     ./connect_test 192.168.71.161        # lewa ręka
```

**Menu (cyfry 0–7)**

| Opcja | Działanie | Uwagi z kodu |
|---:|---|---|
| 1 | Włącz zasilanie silników | `setOperateMode(automatic)` → `setMotionControlMode(RtCommand)` → `setPowerState(true)` |
| 2 | Wyłącz zasilanie silników | `setPowerState(false)`; wyłącza też drag mode |
| 3 | Ruch do pozycji zerowej | wymaga włączonego zasilania i wyłączonego drag mode; `MoveJ(0.2, bieżąca_pozycja, zero)` — prędkość 20% |
| 4 | Włącz drag mode | `setOperateMode(manual)` + `setPowerState(true)` + `enableDrag(jointSpace, freely)` — ręka daje się przeciągać, działa kompensacja grawitacji |
| 5 | Wyłącz drag mode | `disableDrag()` — ramię blokuje się w obecnej pozycji |
| 6 | Szybki test sieci | 10 s, cel 1000 Hz; mierzy skuteczność odczytu, liczbę utraconych pakietów, średnie i maksymalne opóźnienie odczytu `jointPos_m` |
| 7 | Reset błędów / limitów / E-STOP | `recoverState(1)` (wyjście z blokady E-Stop) → `clearServoAlarm()` → pauza 500 ms → `setOperateMode(manual)` + `setPowerState(true)` |
| 0 | Bezpieczne wyjście | wyłącza drag mode i zasilanie, kończy program |

**Warto wiedzieć**

- Program sam nie wymusza `local_ip` — bez niego niektóre operacje (np. tryb RT) mogą nie działać;
  patrz [`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md) (problemy 12–13).
- Opcja 7 wypisuje podpowiedź: jeśli zasilanie nie wraca, sprawdź czy grzybek E-Stop jest
  przekręcony i czy ramię nie opiera się o mechaniczny zderzak.
- Test sieci (opcja 6) celuje w 1000 Hz, co jest **ambitnym** celem — realny wynik zależy
  od obciążenia systemu i sieci. Do sprawdzenia na robocie.

---

## 2. `prezentacja.cpp` — program prezentacyjny (targi)

Największy z programów: steruje **jednocześnie** lewą ręką, prawą ręką i tułowiem z głową.
Globalna prędkość: **40%** (`PREDKOSC = 0.4`).

**Uruchomienie**

```bash
./prezentacja        # bez argumentów; adresy są zaszyte w kodzie
```

**Po uruchomieniu** program łączy się z trzema kontrolerami. Jeśli tułów się nie połączy,
program **działa dalej** z samymi ramionami (tułów = `nullptr`) — informacja, nie błąd krytyczny.

**Menu (cyfry 0–9)**

| Opcja | Działanie |
|---:|---|
| 1 | Przygotuj WSZYSTKO (reset + Power ON) |
| 2 | Powrót WSZYSTKICH do pozycji zerowej |
| 3 | Trajektoria LEWĄ ręką (6 punktów) |
| 4 | Trajektoria PRAWĄ ręką (7 punktów) |
| 5 | Trajektoria TUŁOWIEM + GŁOWĄ (4 punkty) |
| 6 | OBIE RĘCE naraz |
| 7 | WSZYSTKO naraz (ręce + tułów + głowa) |
| 8 | PEŁNA PREZENTACJA (zero → trajektorie → zero) |
| 9 | Awaryjny POWER OFF (wszystko) |
| 0 | Wyjście |

**Dwa różne sposoby ruchu w jednym programie**

| Element | API | Szczegóły |
|---|---|---|
| Ręce | `getRtMotionController().lock()->MoveJ(...)` | ruch w trybie czasu rzeczywistego; punkty w radianach, konwersja `deg2rad7` |
| Tułów + głowa | `moveReset()` → `moveAppend(MoveAbsJCommand)` → `moveStart()` | ruch przez kolejkę komend; `JointPosition.joints` (4 wartości) + `JointPosition.external` (2 wartości = głowa pan/tilt) |

Po każdym punkcie tułowia program czeka na koniec ruchu (`operationState != moving`),
z limitem 15 s i ostrzeżeniem o timeoutcie.

**Przygotowanie tułowia jest rozbudowane** (wynika z kodu):
odczyt fizycznej pozycji kluczyka na szafie (`operateMode`: MANUAL = kluczyk w lewo,
AUTOMATIC = w prawo), potem `recoverState(1)` (E-Stop), `recoverState(2)` (Safeguard / drzwi
bezpieczeństwa), `recoverState(3)` (Collision), `clearServoAlarm()`, wymuszenie trybu
automatycznego, `setPowerState(true)` i 2 sekundy oczekiwania na naładowanie szyny DC.
Jeśli kluczyk fizyczny stoi w MANUAL — program wypisze, żeby go przekręcić.

**Trajektorie** są zapisane w kodzie jako stałe tablice kątów w stopniach (6 punktów dla lewej
ręki, 7 dla prawej, 4 dla tułowia z głową). Są to wartości „z Robot Assist" — przed użyciem
warto je zweryfikować względem aktualnych limitów
([`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §2).

---

## 3. `helios_probe.cpp` — węzeł ROS 2 do odczytu stanu ramienia

Węzeł `rclcpp`: łączy się z kontrolerem, wypisuje informacje i publikuje pozycje stawów.

**Co robi**

1. `connectToRobot()` — połączenie z kontrolerem.
2. `robotInfo()` — model, liczba osi, wersja kontrolera, identyfikator.
3. `powerState()` — zasilanie (`on` / `off` / `estop` / `gstop` = drzwi bezpieczeństwa).
4. `operateMode()` — tryb pracy (`manual` / `automatic`).
5. `operationState()` — stan pracy (wartość enum).
6. Jeśli liczba osi == `HELIOS_DOF`, co 100 ms (10 Hz) publikuje
   `sensor_msgs/msg/JointState` na temat **`helios/joint_states`** (kąty w radianach).
   Jeśli liczby osi nie zgadzają się — węzeł tylko informuje i nie publikuje.

**Parametry**

| Parametr | Domyślnie | Znaczenie |
|---|---|---|
| `robot_ip` | `192.168.0.160` | adres kontrolera ramienia |
| `local_ip` | `""` (pusty) | adres lokalnego interfejsu; **wymagany dla ruchu**, do samego odczytu zwykle nie jest konieczny |

**Kompilacja z liczbą osi**

`HELIOS_DOF` definiuje klasę robota: `7` → `xMateErProRobot` (ręce Heliosa),
inna wartość → `xMateRobot` (6 osi). Domyślnie `7`.

**Uwaga:** to jest kopia źródła węzła z workspace robota
(`~/rokae_ws/src/rokae_ros2-main/helios_probe/`). Pełny pakiet opisany w
[`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md) ma więcej binarek
(`helios_mover`, `helios_torso_probe`, `helios_torso_read`, `helios_head_read`) — wersja
w repozytorium to tylko najprostszy odczyt ramienia.

---

## Czego w tym pliku nie ma (luki)

- [ ] plików buildowych dla `connect_test-pl.cpp` i `prezentacja.cpp`,
- [ ] informacji, czy `prezentacja.cpp` był kiedykolwiek uruchomiony na robocie i z jakim skutkiem,
- [ ] opisu, co dokładnie zwraca `recoverState(1/2/3)` — numery pochodzą z kodu, nazwy
      (E-Stop / Safeguard / Collision) to komentarze w kodzie, nie cytata z oficjalnej dokumentacji,
- [ ] źródła samej aplikacji do trackingu (Qt/QML + `RobotClient`, `VideoReceiver`) —
      jest tylko w archiwach `TrackingRobot*.zip`.
