# helios_probe

Węzeł ROS 2 do odczytu stanu ramion humanoida Rokae Helios przez xCore SDK (C++).

## Stan końcowy

`helios_probe` łączy się z kontrolerem ramienia przez xCore SDK 0.7.1 i **tylko czyta dane**: model, liczbę osi, wersję kontrolera, zasilanie, tryb pracy i stan pracy. Następnie publikuje pozycje stawów na topicu `helios/joint_states` (`sensor_msgs/JointState`, 10 Hz, radiany).

Węzeł niczego nie włącza i niczego nie porusza.

### Potwierdzone urządzenia

| Adres | Urządzenie | Wynik |
|---|---|---|
| `192.168.71.160` | prawa ręka `AR5-5_0.7R-W4C4A2` | działa, 7 osi |
| `192.168.71.161` | lewa ręka `AR5-5_0.7L-W4C4A2` | działa, 7 osi |

Oba kontrolery: `3.2.1.C224.20260731`, zasilanie `off`, tryb `manual`, stan pracy `idle` (0).

### Pozostałe adresy w sieci

| Adres | Urządzenie | Uwagi |
|---|---|---|
| `192.168.71.254` | korpus z głową (`TaiHu`) | 4 osie + 2 osie dodatkowe głowy; **nieobsłużone**, patrz problem 8 |
| `192.168.71.50` | podstawa | brak połączenia SDK, patrz problem 9 |
| `192.168.71.51` | cały ORIN | komputer, na którym uruchamiany jest węzeł |

## Wnioski

### Klasa robota dla rąk

- **Obie ręce (`.160` prawa, `.161` lewa) obsługuje klasa `rokae::xMateErProRobot`** (`Cobot<7>`, czyli `Robot_T<WorkType::collaborative, 7>`), 7 osi.
- Klasa `rokae::xMateRobot` (6 osi) **nie działa**: SDK odrzuca połączenie błędem `Robot instance type does not match`.
- Tak samo robi oficjalny sterownik Rokae (`rokae_hardware_interface.cpp`): dla 7 osi używa `xMateErProRobot`, dla pozostałych `xMateRobot`.
- Nazwa modelu (`AR5-5_0.7R/L-W4C4A2`) **nie mówi wprost o liczbie osi**. Liczbę osi trzeba brać z kontrolera (`robotInfo().joint_num`), a nie z nazwy.
- Obie ręce mają ten sam kontroler (`3.2.1.C224.20260731`), więc ten sam kod i te same biblioteki obsługują obie.

### Klasy dostępne w SDK 0.7.1

| Klasa | Osie | Rodzaj |
|---|---|---|
| `xMateRobot` | 6 | współpracujący (`Cobot<6>`) |
| `xMateCr5Robot` | 5 | współpracujący (`Cobot<5>`) |
| `xMateErProRobot` | 7 | współpracujący (`Cobot<7>`), **używana dla rąk Heliosa** |
| `StandardRobot` | 6 | przemysłowy (`IndustrialRobot<6>`) |
| `PCB3Robot` | 3 | przemysłowy (`IndustrialRobot<3>`) |
| `PCB4Robot` | 4 | przemysłowy (`IndustrialRobot<4>`) |

Żadna z nich nie jest potwierdzona dla korpusu z głową (`TaiHu`).

### Pozostałe wnioski

- **Korpus z głową (`.254`, `TaiHu`):** klasa nieustalona. Kandydat do sprawdzenia to `PCB4Robot` (jedyna 4-osiowa), ale nie wiadomo, czy pasuje. Osie dodatkowe głowy w SDK występują jako parametry szyny, a nie jako osobny mechanizm dwóch osi.
- **MoveIt:** w workspace są konfiguracje `rokae_xMateAR5L_moveit_config` i `rokae_xMateAR5R_moveit_config`, o nazwach zgodnych z modelami rąk (AR5, L i R). **Nie sprawdzono**, czy opisują one ramiona o 7 stawach i czy pasują limity, więc nie należy ich używać do ruchu bez weryfikacji.
- **Odpowiedź na problem z przykładami `rokae_example`:** to programy klienckie MoveIt, więc uruchamia się je przez launch, który podnosi `move_group`, a nie gołą binarką.
- **Ustawienia sieci:** ramiona i korpus są w podsieci `192.168.71.x`. Odczyt działa bez podawania `local_ip`.

## Konfiguracja, która działa

- **Lokalizacja pakietu:** `~/rokae_ws/src/rokae_ros2-main/helios_probe/` (`package.xml`, `CMakeLists.txt`, `src/helios_probe.cpp`)
- **SDK:** `rokae_hardware/sdk` w wersji 0.7.1 (aarch64). Starsze kopie 0.5.x w `~/vision_file` nie są używane.
- **Klasa robota:** `rokae::xMateErProRobot` (`-DHELIOS_DOF=7`)
- **Linkowanie (statyczne):** `libxCoreSDK.a` + `libxMateModel.a` + `orocos-kdl` (z systemu) + `Eigen3::Eigen` + `Threads::Threads`, definicja `XMATEMODEL_LIB_SUPPORTED`. Tak samo linkuje pakiet `rokae_hardware`.

### Budowanie

```bash
cd ~/rokae_ws
colcon build --packages-select helios_probe --cmake-clean-cache --cmake-args \
  -DXCORE_SDK_DIR=/home/std/rokae_ws/src/rokae_ros2-main/rokae_hardware/sdk \
  -DHELIOS_DOF=7
source install/setup.zsh
```

### Uruchomienie

```bash
ros2 run helios_probe helios_probe --ros-args -p robot_ip:=192.168.71.160
```

Parametry:

| Parametr | Domyślnie | Opis |
|---|---|---|
| `robot_ip` | `192.168.0.160` | adres kontrolera ramienia |
| `local_ip` | `""` | adres lokalnego interfejsu sieciowego (opcjonalny) |

Dwie ręce naraz (osobne przestrzenie nazw):

```bash
ros2 run helios_probe helios_probe --ros-args -r __ns:=/right -p robot_ip:=192.168.71.160
ros2 run helios_probe helios_probe --ros-args -r __ns:=/left  -p robot_ip:=192.168.71.161
```

Odczyt pozycji stawów:

```bash
ros2 topic echo /right/helios/joint_states
```

## Problemy i rozwiązania

| # | Problem | Przyczyna | Rozwiązanie | Status |
|---|---|---|---|---|
| 1 | `joint_position_control` z `rokae_example` kończy się błędem `Could not find parameter robot_description` | To klient MoveIt, a nie samodzielny program. Wymaga URDF, SRDF i działającego `move_group`, czyli uruchomienia przez launch. W workspace są konfiguracje MoveIt dla ramion xMate (CR, AR5L/R, ER, Pro, SR). Nie ma osobnej konfiguracji dla całego Heliosa, a to, czy AR5L/AR5R pasują do jego rąk, nie zostało zweryfikowane. | Odpuszczone. Zamiast tego własny węzeł na samym SDK, bez MoveIt. | obejście |
| 2 | Kompilacja: `fatal error: Eigen/Core: No such file or directory` | Katalog `sdk/` z `rokae_hardware` nie ma podkatalogu `external/` z Eigenem. | `find_package(Eigen3 REQUIRED)` i `Eigen3::Eigen` w linkowaniu (Eigen jest w `/usr/include/eigen3`). | działa |
| 3 | Linkowanie: tysiące `undefined reference to KDL::...`, `Model::...`, `ConvertFrameArray` | Biblioteka SDK zależy od KDL i od modelu robota Rokae (`libxMateModel.a`), których nie podano linkerowi. | Dolinkować `libxMateModel.a`, `orocos-kdl` z systemu i dodać `-DXMATEMODEL_LIB_SUPPORTED`. | działa |
| 4 | To samo po zamianie na `libxCoreSDK.so.0.7.1` | Wersja dynamiczna też wymaga KDL i modelu. Zmiana `.a` na `.so` nie pomaga. | Zostać przy wersji statycznej (problem 3). Dowiązanie `libxCoreSDK.so.0` (SONAME) potrzebne tylko przy linkowaniu dynamicznym. | nie pomogło |
| 5 | `source install/setup.bash` w zsh daje błędy, `not found: local_setup.zsh` | Powłoka to zsh, a po nieudanym buildzie pakiet nie jest zainstalowany. | Używać `source install/setup.zsh` po **udanym** buildzie. | działa |
| 6 | Po zmianach w CMake build używa starych ustawień | Cache CMake w colcon. | Dodawać `--cmake-clean-cache` przy zmianach w `CMakeLists.txt`. | działa |
| 7 | `Robot instance type does not match with the connected robot AR5-5_0.7R-W4C4A2` | Użyta klasa `xMateRobot` (6 osi) nie zgadza się z modelem. Kontroler zgłasza 7 osi. Nazwa modelu nie mówi wprost o liczbie osi. | Zbudować z `-DHELIOS_DOF=7`, co daje `xMateErProRobot`. Tak samo robi sterownik Rokae dla 7 osi. | działa |
| 8 | `.254` (`TaiHu`): `Robot instance type does not match with the connected robot TaiHu` | Korpus z głową to inny typ urządzenia niż ramię xMate. W SDK 0.7.1 jedyna klasa 4-osiowa to `PCB4Robot`. Osie dodatkowe głowy w SDK występują tylko jako parametry szyny. | Nierozwiązane, odłożone. Do sprawdzenia w przyszłości: `PCB4Robot`, inna wersja SDK, pytanie do Rokae. | otwarte |
| 9 | `.50` (podstawa): `network: network connection` | Brak połączenia SDK na tym adresie. Możliwe, że urządzenie w ogóle nie mówi protokołem xCore (przypuszczenie). | Nie dotyczy węzła. | nie wymaga akcji |

## Czego jeszcze nie sprawdzono

- Wartości z topicu `helios/joint_states` (w logach widać było tylko dane startowe).
- Jednoczesnej pracy dwóch instancji (`/right` i `/left`).
- Włączania zasilania z poziomu SDK i jakiegokolwiek ruchu. Wymaga osobnej rozwagi: zasilenie zwalnia hamulce, więc ramię musi mieć wolną przestrzeń i dostęp do E-stop.
- Korpusu z głową (`TaiHu`) i osi dodatkowych.

## Przydatne odnośniki

- Repozytorium SDK: https://github.com/RokaeRobot/xCoreSDK-CPP
- Dokumentacja SDK: https://docs.rokae.com/en/docs/SDK/cpp
- Korespondencja API SDK z Robot Assist: https://docs.rokae.com/en/docs/SDK/sdk_assist
- Pakiet ROS 2 Rokae: `RokaeRobot/rokae_ros2`
