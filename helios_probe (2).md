# helios_probe

Węzeł ROS 2 do odczytu stanu i sterowania ramionami oraz torsem humanoida Rokae Helios przez xCore SDK (C++).

## Kompilacja

```bash
cd ~/rokae_ws

colcon build --packages-select helios_probe --cmake-clean-cache --cmake-args \
  -DXCORE_SDK_DIR=/home/std/rokae_ws/src/rokae_ros2-main/rokae_hardware/sdk \
  -DHELIOS_DOF=7

source install/setup.zsh
```

## Uruchomienie

**WAŻNE:** Do uruchomienia komunikacji w trybie RT (`RtCommand`, potrzebne do ruchu) trzeba podać zarówno `robot_ip`, jak i `local_ip` (adres IP komputera w tej samej podsieci).

```bash
# Prawa ręka (odczyt)
ros2 run helios_probe helios_probe --ros-args \
  -p robot_ip:=192.168.71.160 \
  -p local_ip:=192.168.71.51

# Ruch ramieniem (test)
ros2 run helios_probe helios_mover --ros-args \
  -p robot_ip:=192.168.71.160 \
  -p local_ip:=192.168.71.51

# Odczyt torsu (TaiHu)
ros2 run helios_probe helios_torso_read

# Odczyt osi zewnętrznych (głowa) — do sprawdzenia
ros2 run helios_probe helios_head_read
```

## Stan końcowy

`helios_probe` łączy się z kontrolerami ramion i torsu przez xCore SDK 0.7.1. Odczytuje: model, liczbę osi, wersję kontrolera, zasilanie, tryb pracy i stan pracy. Publikuje pozycje stawów na topicu `helios/joint_states` (`sensor_msgs/JointState`, 10 Hz, radiany).

Dodatkowo powstały programy pomocnicze do:

- ruchu ramieniem (`helios_mover`) — potwierdzony ruch testowy działa,
- diagnostyki torsu i identyfikacji klasy sterującej (`helios_torso_probe`),
- odczytu pozycji stawów torsu (`helios_torso_read`),
- odczytu osi zewnętrznych, tj. głowy (`helios_head_read`).

## Potwierdzone urządzenia

| Adres | Urządzenie | Klasa SDK | Osie | Wynik |
|---|---|---|---:|---|
| `192.168.71.160` | prawa ręka AR5-5_0.7R-W4C4A2 | `rokae::xMateErProRobot` | 7 | działa, ruch potwierdzony |
| `192.168.71.161` | lewa ręka AR5-5_0.7L-W4C4A2 | `rokae::xMateErProRobot` | 7 | działa (odczyt) |
| `192.168.71.254` | tors TaiHu | `rokae::PCB4Robot` | 4 + osie zewnętrzne (głowa) | odczyt działa |
| `192.168.71.50` | podstawa | brak połączenia SDK | — | patrz problem 9 |
| `192.168.71.51` | Orin (komputer) | — | — | komputer roboczy, na nim uruchamiane węzły |

**Kontrolery ramion:** `3.2.1.C224.20260731`  
**Kontroler torsu:** `3.2.0.10.NoSafetyboard_V3`

## Wnioski

### Klasa robota dla rąk

Obie ręce (`.160` prawa, `.161` lewa) obsługuje klasa `rokae::xMateErProRobot` (`Cobot<7>`, czyli `Robot_T<WorkType::collaborative, 7>`), 7 osi.

Klasa `rokae::xMateRobot` (6 osi) nie działa: SDK odrzuca połączenie błędem `Robot instance type does not match`.

Tak samo robi oficjalny sterownik Rokae (`rokae_hardware_interface.cpp`): dla 7 osi używa `xMateErProRobot`, dla pozostałych `xMateRobot`.

Nazwa modelu (`AR5-5_0.7R/L-W4C4A2`) nie mówi wprost o liczbie osi. Liczbę osi trzeba brać z kontrolera (`robotInfo().joint_num`), a nie z nazwy.

Obie ręce mają ten sam kontroler (`3.2.1.C224.20260731`), więc ten sam kod i te same biblioteki obsługują obie.

### Klasa robota dla torsu z głową (TaiHu)

Tors (`.254`) obsługuje klasa `rokae::PCB4Robot` (`IndustrialRobot<4>`, klasa przemysłowa 4-osiowa).

Klasa zaakceptowana metodą "bruteforce probe" — po kolei testowane wszystkie prekompilowane klasy z SDK. Tylko `PCB4Robot` się połączyła.

Kontroler zgłasza 4 osie główne (`joint_num = 4`).

Głowa (2 osie pan/tilt) jest podłączona jako osie zewnętrzne (external axes, po chińsku 外部关节). W SDK Rokae są to standardowe osie dodatkowe konfigurowane w kontrolerze (np. tor jezdny, obrotnik, itp.).

Osie zewnętrzne występują w SDK w polu `std::vector<double> external` w strukturach `JointPosition` oraz `CartesianPosition` (patrz `data_types.h`, linie ~412 i ~444).

Nie znaleziono żadnych informacji o torsie ani głowie w istniejącym kodzie workspace (grep nie znalazł TaiHu, `192.168.71.254`, torso).

## Klasy dostępne w SDK 0.7.1

| Klasa | Osie | Rodzaj | Zastosowanie w Heliosie |
|---|---:|---|---|
| `xMateRobot` | 6 | współpracujący (`Cobot<6>`) | — |
| `xMateCr5Robot` | 5 | współpracujący (`Cobot<5>`) | — |
| `xMateErProRobot` | 7 | współpracujący (`Cobot<7>`) | ramiona (`.160`, `.161`) |
| `StandardRobot` | 6 | przemysłowy (`IndustrialRobot<6>`) | — |
| `PCB3Robot` | 3 | przemysłowy (`IndustrialRobot<3>`) | — |
| `PCB4Robot` | 4 | przemysłowy (`IndustrialRobot<4>`) | tors TaiHu (`.254`) |

> **Uwaga o szablonach:** Nie da się użyć surowego szablonu `rokae::Cobot<4>` ani `rokae::IndustrialRobot<N>` z dowolnym `N`. Biblioteka statyczna `libxCoreSDK.a` zawiera prekompilowany kod wyłącznie dla klas wypisanych powyżej. Próba użycia innych DoF kończy się błędem linkera (`undefined reference to rokae::Robot_T<...>::connectToRobot`).

## Ruch ramieniem (Motion Control)

Klasa robota `xMateErProRobot` nie posiada metod ruchu typu `moveJ` bezpośrednio.

Ruch trzeba wykonywać przez dedykowany kontroler ruchu (Motion Controller) — dla trybu real-time to `RtMotionController`.

### Poprawna sekwencja uruchamiania ruchu

```cpp
robot.setOperateMode(rokae::OperateMode::automatic, ec) // tryb auto (wymagany do sterowania z PC)
robot.setPowerState(true, ec) // włączenie serwomotorów
robot.setMotionControlMode(rokae::MotionControlMode::RtCommand, ec) // tryb czasu rzeczywistego
auto rtCon = robot.getRtMotionController().lock() // pobranie kontrolera
rtCon->MoveJ(speed_ratio, q_start, q_target) // właściwa metoda ruchu, z wielkiej litery M
```

Typ pozycji stawów: `std::array<double, DoF>`, np. `std::array<double, 7>` dla ramion.

## Komunikacja sieciowa

- Kanał TCP (zarządzanie, `connectToRobot`, `setPowerState`, `jointPos`) — działa bez podawania `local_ip`.
- Kanał UDP (motion control real-time, port `1338`) — wymaga podania `local_ip` równego adresowi karty sieciowej komputera w podsieci robota.
- Bez `local_ip` lub z błędnym `local_ip` błąd: `realtime: 1338 Failed to open UDP socket. Cannot assign requested address (EADDRNOTAVAIL)` lub `Invalid argument (EINVAL)`.
- Sprawdzenie IP komputera: `ip -4 addr show` — Orin ma `192.168.71.51`.
- Włączenie silników nie potwierdza działania UDP. Silniki załącza kanał TCP, ale ruch wymaga UDP. Częste źródło nieporozumień.

## Pozostałe wnioski

- **MoveIt:** w workspace są konfiguracje `rokae_xMateAR5L_moveit_config` i `rokae_xMateAR5R_moveit_config`, o nazwach zgodnych z modelami rąk (AR5, L i R). Nie sprawdzono, czy opisują ramiona o 7 stawach i czy pasują limity, więc nie należy ich używać do ruchu bez weryfikacji.
- **Odpowiedź na problem z przykładami `rokae_example`:** to programy klienckie MoveIt, więc uruchamia się je przez launch, który podnosi `move_group`, a nie gołą binarką.
- **Ustawienia sieci:** ramiona, tors i Orin są w podsieci `192.168.71.x`.

## Konfiguracja, która działa

- Lokalizacja pakietu: `~/rokae_ws/src/rokae_ros2-main/helios_probe/` (`package.xml`, `CMakeLists.txt`, `src/*.cpp`)
- SDK: `rokae_hardware/sdk` w wersji `0.7.1` (aarch64). Starsze kopie `0.5.x` w `~/vision_file` nie są używane.
- Klasa robota (ramiona): `rokae::xMateErProRobot` (`-DHELIOS_DOF=7`)
- Klasa robota (tors): `rokae::PCB4Robot` (jawnie w kodzie)
- Linkowanie (statyczne): `libxCoreSDK.a` + `libxMateModel.a` + `orocos-kdl` (z systemu) + `Eigen3::Eigen` + `Threads::Threads`, definicja `XMATEMODEL_LIB_SUPPORTED`.

### Struktura plików w pakiecie

```text
helios_probe/
├── CMakeLists.txt
├── package.xml
└── src/
    ├── helios_probe.cpp          # odczyt stanu ramienia
    ├── helios_mover.cpp          # ruch testowy ramienia (MoveJ)
    ├── helios_torso_probe.cpp    # diagnostyka klasy dla torsu (bruteforce)
    ├── helios_torso_read.cpp     # odczyt pozycji 4 stawów torsu
    └── helios_head_read.cpp      # odczyt osi zewnętrznych (głowa) — WIP
```

## Budowanie

```bash
cd ~/rokae_ws
colcon build --packages-select helios_probe --cmake-clean-cache --cmake-args \
  -DXCORE_SDK_DIR=/home/std/rokae_ws/src/rokae_ros2-main/rokae_hardware/sdk \
  -DHELIOS_DOF=7
source install/setup.zsh
```

## Uruchomienie

```bash
# Odczyt ramienia
ros2 run helios_probe helios_probe --ros-args -p robot_ip:=192.168.71.160

# Odczyt obu rąk równocześnie
ros2 run helios_probe helios_probe --ros-args -r __ns:=/right -p robot_ip:=192.168.71.160
ros2 run helios_probe helios_probe --ros-args -r __ns:=/left  -p robot_ip:=192.168.71.161

# Ruch ramieniem (potrzebny local_ip!)
ros2 run helios_probe helios_mover --ros-args \
  -p robot_ip:=192.168.71.160 \
  -p local_ip:=192.168.71.51

# Diagnostyka torsu
ros2 run helios_probe helios_torso_probe

# Odczyt torsu (4 stawy)
ros2 run helios_probe helios_torso_read

# Odczyt głowy (osie zewnętrzne)
ros2 run helios_probe helios_head_read
```

## Parametry węzła `helios_probe` i `helios_mover`

| Parametr | Domyślnie | Opis |
|---|---|---|
| `robot_ip` | `192.168.0.160` | adres kontrolera ramienia |
| `local_ip` | `""` | adres lokalnego interfejsu (wymagany dla motion control RT!) |
| `speed_ratio` | `0.05` | prędkość ruchu jako ułamek maksymalnej (0.05 = 5%) — tylko dla `helios_mover` |

### Odczyt pozycji stawów

```bash
ros2 topic echo /right/helios/joint_states
```

## Problemy i rozwiązania

| # | Problem | Przyczyna | Rozwiązanie | Status |
|---:|---|---|---|---|
| 1 | `joint_position_control` z `rokae_example` kończy się błędem `Could not find parameter robot_description` | To klient MoveIt, a nie samodzielny program. Wymaga URDF, SRDF i działającego `move_group`, czyli uruchomienia przez launch. W workspace są konfiguracje MoveIt dla ramion xMate, ale nie ma osobnej konfiguracji dla całego Heliosa. | Odpuszczone. Zamiast tego własny węzeł na samym SDK, bez MoveIt. | obejście |
| 2 | Kompilacja: `fatal error: Eigen/Core: No such file or directory` | Katalog `sdk/` z `rokae_hardware` nie ma podkatalogu `external/` z Eigenem. | `find_package(Eigen3 REQUIRED)` i `Eigen3::Eigen` w linkowaniu (Eigen jest w `/usr/include/eigen3`). Nowo dodawane binarki też muszą to mieć! | działa |
| 3 | Linkowanie: tysiące `undefined reference to KDL::..., Model::..., ConvertFrameArray` | Biblioteka SDK zależy od KDL i od modelu robota Rokae (`libxMateModel.a`), których nie podano linkerowi. | Dolinkować `libxMateModel.a`, `orocos-kdl` z systemu i dodać `-DXMATEMODEL_LIB_SUPPORTED`. | działa |
| 4 | To samo po zamianie na `libxCoreSDK.so.0.7.1` | Wersja dynamiczna też wymaga KDL i modelu. Zmiana `.a` na `.so` nie pomaga. | Zostać przy wersji statycznej (problem 3). | nie pomogło |
| 5 | `source install/setup.bash` w zsh daje błędy, `not found: local_setup.zsh` | Powłoka to zsh, a po nieudanym buildzie pakiet nie jest zainstalowany. | Używać `source install/setup.zsh` po udanym buildzie. | działa |
| 6 | Po zmianach w CMake build używa starych ustawień | Cache CMake w colcon. | Dodawać `--cmake-clean-cache` przy zmianach w `CMakeLists.txt`. | działa |
| 7 | `Robot instance type does not match with the connected robot AR5-5_0.7R-W4C4A2` | Użyta klasa `xMateRobot` (6 osi) nie zgadza się z modelem. Kontroler zgłasza 7 osi. Nazwa modelu nie mówi wprost o liczbie osi. | Zbudować z `-DHELIOS_DOF=7`, co daje `xMateErProRobot`. | działa |
| 8 | `.254 (TaiHu): Robot instance type does not match with the connected robot TaiHu` | Korpus z głową to inny typ urządzenia niż ramię xMate. Trzeba użyć `PCB4Robot` (4 DoF, industrial). | Zmienić klasę na `rokae::PCB4Robot`. Głowa (2 osie) jest jako osie zewnętrzne (external). | działa |
| 9 | `.50 (podstawa): network: network connection` | Brak połączenia SDK na tym adresie. Możliwe, że urządzenie w ogóle nie mówi protokołem xCore. | Nie dotyczy węzła. | nie wymaga akcji |
| 10 | `error: 'MoveJ' is not a member of 'rokae::xMateErProRobot'` | Metody ruchu nie znajdują się w klasie robota, tylko w kontrolerze ruchu. Poza tym `moveJ` vs `MoveJ` (wielkość liter). | Pobrać kontroler: `robot.getRtMotionController().lock()` i wywołać `rtCon->MoveJ(...)` (z wielkiej M). | działa |
| 11 | `'Other' is not a member of 'rokae::MotionControlMode'` | Enum `MotionControlMode` nie ma wartości `Other`. Próba przywrócenia stanu w destruktorze była błędem. | Usunąć wywołanie `setMotionControlMode(MotionControlMode::Other)`. Samo `disconnectFromRobot(ec)` wystarczy. | działa |
| 12 | `realtime: 1338 Failed to open UDP socket. Invalid argument` | SDK dostało pusty `local_ip = ""`. System nie potrafi zbindować socketu UDP. | Podać `local_ip` na sztywno przez parametr ROS lub przez wpis w kodzie. | działa |
| 13 | `realtime: 1338 Failed to open UDP socket. Cannot assign requested address` | Adres `local_ip` nie jest przypisany do żadnej karty sieciowej komputera. | Sprawdzić `ip -4 addr` i podać dokładny adres komputera z podsieci robota. | działa |
| 14 | `undefined reference to rokae::Robot_T<...>::connectToRobot` przy użyciu `Cobot<4>` | Biblioteka `libxCoreSDK.a` nie ma prekompilowanego kodu dla dowolnych szablonów. Tylko gotowe klasy (`xMateRobot`, `PCB4Robot` itd.) są dostępne. | Używać wyłącznie prekompilowanych klas z listy w `robot.h`. | działa |
| 15 | `error: conversion from std::array<double, 4> to non-scalar type rokae::JointPosition requested` | `robot.jointPos(ec)` zwraca `std::array`, a nie `JointPosition`. | Użyć metody `robot.jointPosition(ec)` (bez s na końcu) lub podobnej — do zweryfikowania w `robot.h`. | w toku |

## Metoda "bruteforce probe" do identyfikacji nieznanych urządzeń Rokae

Jeśli SDK odrzuca połączenie z komunikatem `Robot instance type does not match...`, użyj programu `helios_torso_probe`, który po kolei próbuje wszystkie dostępne w SDK gotowe klasy robotów. Ta metoda pozwoliła zidentyfikować, że tors TaiHu jest obsługiwany przez `PCB4Robot`.

Program testuje tylko konkretne, prekompilowane klasy (nie surowe szablony) — inaczej wystąpi błąd linkera.

## Czego jeszcze nie sprawdzono / co jest w toku

- Odczyt pozycji głowy (osie zewnętrzne): trwają prace nad `helios_head_read`. Do naprawy: właściwa metoda w SDK zwracająca `rokae::JointPosition` (z polami `joints` i `external`). Prawdopodobnie `robot.jointPosition(ec)`.
- Ruch torsem: analogicznie do ramion, powinno działać przez `PCB4Robot.getRtMotionController().lock()->MoveJ(...)`. Wymaga przetestowania.
- Ruch głową (osiami zewnętrznymi): metoda ruchu do zbadania — być może przez specjalną strukturę pozycji z polem `external`.
- Jednoczesnej pracy dwóch instancji (`/right` i `/left`).
- Podstawy (`.50`) — czy w ogóle jest sterowana przez Rokae xCore.

## Przydatne odnośniki

- Repozytorium SDK: https://github.com/RokaeRobot/xCoreSDK-CPP
- Dokumentacja SDK: https://docs.rokae.com/en/docs/SDK/cpp
- Korespondencja API SDK z Robot Assist: https://docs.rokae.com/en/docs/SDK/sdk_assist
- Pakiet ROS 2 Rokae: RokaeRobot/rokae_ros2
