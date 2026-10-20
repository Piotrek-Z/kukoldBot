# Tutorial: Programowanie Ruchu Ramion (7 DoF) i Torsu (4+2 DoF) w C++ (Rokae SDK)

> **Dla kogo jest ten dokument?**  
> Ten przewodnik powstał na bazie praktycznych doświadczeń i działającego kodu (`pos_zero.cpp`, `wave.cpp`, `gesty.cpp`) napisanego dla robota **Rokae Helios**. Wyjaśnia krok po kroku, jak bezpiecznie poruszać 7-osiowymi rękami oraz 4-osiowym torsem (wraz z 2-osiową głową) i **dlaczego przykłady z oficjalnej dokumentacji Rokae często nie działają lub prowadzą do błędów na tym konkretnym modelu.**

---

## Spis Treści

1. [Dlaczego oficjalna dokumentacja Rokae bywa myląca dla Heliosa?](#1-dlaczego-oficjalna-dokumentacja-rokae-bywa-myl%C4%85ca-dla-heliosa)
2. [Architektura w SDK: Adresy IP, Klasy C++ i Liczba Osi](#2-architektura-w-sdk-adresy-ip-klasy-c-i-liczba-osi)
3. [Zasady Ogólne: Jednostki, Wielowątkowość (Bug spdlog) i Kompilacja (Bug GCC 11)](#3-zasady-og%C3%B3lne-jednostki-wielow%C4%85tkowo%C5%9B%C4%87-bug-spdlog-i-kompilacja-bug-gcc-11)
4. [Krok 1: Bezpieczne Przygotowanie Ręki lub Torsu do Ruchu](#4-krok-1-bezpieczne-przygotowanie-r%C4%99ki-lub-torsu-do-ruchu)
5. [Krok 2: Prosty Ruch do Punktu – Tryb NRT (`MoveAbsJCommand`)](#5-krok-2-prosty-ruch-do-punktu--tryb-nrt-moveabsjcommand)
6. [Krok 3: Płynny Ruch Ciągły 7-osiowej Ręki – Pętla RT 1000 Hz (`1 - cos`)](#6-krok-3-p%C5%82ynny-ruch-ci%C4%85g%C5%82y-7-osiowej-r%C4%99ki--p%C4%99tla-rt-1000-hz-1---cos)
7. [Krok 4: Geometria 7 Osi – Pozycja Startowa vs Zero, Pułapka 344° i Lustro Rąk](#7-krok-4-geometria-7-osi--pozycja-startowa-vs-zero-pu%C5%82apka-344-i-lustro-r%C4%85k)
8. [Ściąga: Błędy Rokae SDK i Ich Rozwiązania](#8-%C5%9Bci%C4%85ga-b%C5%82%C4%99dy-rokae-sdk-i-ich-rozwi%C4%85zania)

---

## 1. Dlaczego oficjalna dokumentacja Rokae bywa myląca dla Heliosa?

Oficjalna dokumentacja Rokae xCore SDK jest pisana głównie pod pojedyncze, 6-osiowe ramiona przemysłowe lub coboty stojące na stole. W robocie humanoidalnym **Helios** trafisz na **7 kluczowych różnic**, których oficjalne przykłady nie uwzględniają:

| # | Co mówi oficjalna dokumentacja Rokae | Jak jest naprawdę w robocie Helios |
| :-: | :--- | :--- |
| **1** | Jeden typ robota w przykładach | Helios ma **dwie zupełnie różne klasy robotów**: ramiona to 7-osiowe coboty `rokae::xMateErProRobot`, a tułów to 4-osiowy robot przemysłowy `rokae::PCB4Robot` (który ma dodatkowo 2 osie zewnętrzne głowy!). |
| **2** | Konstruktor przyjmuje tylko IP robota | Dla **ramion** konstruktor wymaga **dwóch adresów IP**: `xMateErProRobot(IP_RAMIENIA, LOCAL_IP)` (bez adresu karty sieciowej Jetsona nie zadziała tryb RT). Dla **torsu** podaje się tylko jeden adres: `PCB4Robot(TULOW_IP)`. |
| **3** | Oczekiwanie na koniec ruchu przez `queryEventInfo()` | Na komputerze pokładowym Jetson (Ubuntu 22.04, **GCC 11**) użycie `queryEventInfo()` powoduje **twardy błąd kompilacji** (`static_assert` w `bits/hashtable.h` przez `std::unordered_map<std::string, std::any>`). Koniec ruchu trzeba sprawdzać własną pętlą odpytującą pozycję lub stan! |
| **4** | `setPowerState(true)` od razu włącza silniki | Fizyczne załączenie styczników trwa **od kilkuset milisekund do kilku sekund**. Wysłanie komendy ruchu od razu po `setPowerState(true)` kończy się błędem `motor power state unsupported operation`. Trzeba w pętli czekać na `PowerState::on`. |
| **5** | Możesz tworzyć obiekty dwóch robotów w osobnych wątkach | Konstruktor `rokae::Robot` rejestruje globalny logger `spdlog` o stałej nazwie `"_rokae_log_console_"`. Równoczesne utworzenie obiektu lewej i prawej ręki w dwóch wątkach zabija cały program (`SIGABRT`!). Tworzenie, łączenie i niszczenie obiektów **musi być chronione mutexem**. |
| **6** | Limity osi 7-osiowego xMate ER Pro to $\pm 170^\circ$ / $\pm 120^\circ$ | Ze względu na boczny montaż barków w Heliosie, sama **pozycja startowa prawej ręki ma `J1 = -173.06°`** (poza katalogowym $\pm 170^\circ$). Co więcej, pomylenie znaku `-171°` z `+171°` powoduje obrót barku o **344°**! |
| **7** | Do ruchu przez kilka punktów użyj `rtCon->MoveJ()` | `MoveJ` zatrzymuje rękę do zera w każdym punkcie, a przełączenie z pętli RT na `MoveJ` (np. żeby wrócić do pozycji startowej) często zawodzi i zostawia rękę w powietrzu. Płynny ruch (machanie, gesty) + powrót trzeba realizować w **jednej ciągłej pętli RT (co 1 ms)** z limitem przyspieszenia $\le 40^\circ/\text{s}^2$. |

---

## 2. Architektura w SDK: Adresy IP, Klasy C++ i Liczba Osi

Wewnątrz Heliosa każdy moduł wykonawczy ma własny kontroler w wewnętrznej podsieci Ethernet `192.168.71.0/24`.

### Wymagane nagłówki
```cpp
#include "rokae/robot.h"
#include "rokae/data_types.h"
```

### Stałe adresów IP i klasy obiektów

```cpp
// Adresy kontrolerów w wewnętrznej sieci robota
const std::string PRAWA_IP = "192.168.71.160"; // Prawa ręka (7 osi: J1..J7)
const std::string LEWA_IP  = "192.168.71.161"; // Lewa ręka  (7 osi: J1..J7)
const std::string TULOW_IP = "192.168.71.254"; // Tułów/Tors (4 osie: J1..J4 + 2 osie głowy)

// Adres IP karty sieciowej Jetsona w podsieci wewnętrznej (wymagany dla ramion!)
const std::string LOCAL_IP = "192.168.71.51";
```

### Tworzenie obiektów dla poszczególnych modułów

```cpp
// 1. Ramiona (7 DoF) - klasa xMateErProRobot (wymaga LOCAL_IP jako drugiego argumentu)
rokae::xMateErProRobot prawa(PRAWA_IP, LOCAL_IP);
rokae::xMateErProRobot lewa(LEWA_IP, LOCAL_IP);

// 2. Tułów + Głowa (4 + 2 DoF) - klasa PCB4Robot (tylko 1 argument IP!)
rokae::PCB4Robot tulow(TULOW_IP);
```

---

## 3. Zasady Ogólne: Jednostki, Wielowątkowość (Bug spdlog) i Kompilacja (Bug GCC 11)

### 3.1. Jednostki: Stopnie vs Radiany
* W programie **RobotAssist (GUI)** wszystkie kąty osi widzisz w **stopniach (`deg`)**.
* W **C++ SDK** wszystkie funkcje (`jointPos()`, `JointPosition`, `MoveAbsJCommand`, `MoveJ`) przyjmują i zwracają kąty **wyłącznie w radianach (`rad`)**!
* **Zasada:** Punkty w kodzie przechowuj czytelnie w stopniach i przeliczaj je pomocniczą funkcją przed wysłaniem do SDK:

```cpp
using A7 = std::array<double, 7>;

A7 deg2rad7(const A7& deg) {
    A7 rad{};
    for (size_t i = 0; i < 7; i++) {
        rad[i] = deg[i] * M_PI / 180.0;
    }
    return rad;
}
```

### 3.2. Bug wielowątkowości `spdlog` – jak sterować dwiema rękami naraz?
Jeśli chcesz uruchomić ruch lewej i prawej ręki równolegle w dwóch wątkach (`std::thread`), **nie wolno** tworzyć obiektów `xMateErProRobot` jednocześnie. Biblioteka Rokae tworzy wewnętrznie logger o stałej nazwie `"_rokae_log_console_"` i przy wyścigu wątków rzuca:
> `spdlog fatal error: logger with name '_rokae_log_console_' already exists` (`SIGABRT`)

**Rozwiązanie (wzorzec z `gesty.cpp`):** Użyj globalnego `std::mutex` wokół tworzenia, łączenia (`connectToRobot`) oraz niszczenia obiektu robota:

```cpp
std::mutex g_sdkMutex;

void watekRamienia(const char* ip) {
    std::unique_ptr<rokae::xMateErProRobot> robotPtr;
    // Strażnik RAII zapewniający bezpieczne zniszczenie obiektu pod mutexem
    struct Zniszcz {
        std::unique_ptr<rokae::xMateErProRobot>& p;
        ~Zniszcz() {
            std::lock_guard<std::mutex> lock(g_sdkMutex);
            p.reset();
        }
    } zniszcz{robotPtr};

    std::error_code ec;
    {
        std::lock_guard<std::mutex> lock(g_sdkMutex);
        robotPtr = std::make_unique<rokae::xMateErProRobot>(ip, LOCAL_IP);
        robotPtr->connectToRobot(ec);
    }
    if (ec) return;

    // Dalej w wątku możesz już wykonywać ruchy równolegle z drugą ręką!
}
```

### 3.3. Bug kompilacji `queryEventInfo()` na GCC 11
**Nigdy nie wywołuj `robot.queryEventInfo(...)`** w kodzie kompilowanym na Jetsonie (GCC 11). Nagłówek SDK używa niekompletnego typu w `std::unordered_map<std::string, std::any>`, co wywołuje błąd `static_assert` w bibliotece standardowej C++. Zamiast tego używaj funkcji odpytujących `jointPos()` lub `operationState()`, pokazanych w Kroku 2.

---

## 4. Krok 1: Bezpieczne Przygotowanie Ręki lub Torsu do Ruchu

Zanim wyślesz jakąkolwiek komendę ruchu, musisz przeprowadzić kontroler przez odpowiednią sekwencję startową. Pominięcie któregokolwiek kroku sprawi, że robot zignoruje komendę lub rzuci błąd.

### 4.1. Sprawdzenie, czy ręka nie jest już w ruchu (`OperationState::moving`)
Jeśli poprzedni ruch został przerwany lub w tle działa inna komenda, ręka ma stan `moving`. Próba zmiany trybu pracy lub zasilania w tym stanie zwróci błąd:  
`robot state (moving) unsupported operation`.

```cpp
bool upewnijSieZeRekaStoi(rokae::xMateErProRobot& robot, const std::string& nazwa) {
    auto stoi = [&]() {
        std::error_code e;
        const auto st = robot.operationState(e);
        return !e && st != rokae::OperationState::moving;
    };
    if (stoi()) return true;

    std::cerr << "[" << nazwa << "] Ręka w stanie 'moving' - zatrzymuję (stop + moveReset)...\n";
    std::error_code ec;
    robot.stop(ec);      ec.clear();
    robot.moveReset(ec); ec.clear();

    const auto koniec = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < koniec) {
        if (stoi()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false; // Po 10 s nadal w ruchu - nie ruszaj zasilaniem!
}
```

### 4.2. Włączenie zasilania silników z oczekiwaniem na styczniki (`PowerState::on`)
Wywołanie `robot.setPowerState(true, ec)` tylko **zleca** włączenie silników. Musisz zaczekać w pętli (do 10 s), aż `robot.powerState(ec)` faktycznie zwróci `rokae::PowerState::on`:

```cpp
template <typename Robot>
bool ensureMotorPowerOn(Robot& robot, const std::string& moduleName) {
    std::error_code ec;
    const auto state = robot.powerState(ec);

    if (!ec) {
        if (state == rokae::PowerState::on) return true;
        if (state == rokae::PowerState::estop) {
            std::cerr << "[" << moduleName << "] E-STOP wciśnięty! Zwolnij grzybek.\n";
            return false;
        }
        if (state == rokae::PowerState::gstop) {
            std::cerr << "[" << moduleName << "] Otwarta osłona bezpieczeństwa (gstop).\n";
            return false;
        }
    }

    ec.clear();
    robot.setPowerState(true, ec);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ec.clear();
        if (robot.powerState(ec) == rokae::PowerState::on && !ec) {
            return true; // Styczniki załączone, silniki gotowe!
        }
    }
    std::cerr << "[" << moduleName << "] Zasilanie nie włączyło się w 10 s.\n";
    return false;
}
```

### 4.3. Wyłączenie trybu wodzenia ręcznego (`drag`) – tylko dla ramion!
* Ramiona (`xMateErProRobot`) mają tryb `drag` (luźne prowadzenie ręką). Jeśli ktoś zostawił włączony `drag`, wywołanie `moveStart()` rzuci nieoczywisty błąd: **`task_space unsupported operation`**.
* Z kolei tułów (`PCB4Robot`) **nie posiada w ogóle metody `disableDrag()`** w swoim API!
* Co gorsza, czasami kontroler ramienia blokuje się w trybie `drag` i zwykłe `disableDrag()` zwraca błąd – wtedy trzeba **wyłączyć zasilanie silników, wywołać `disableDrag()` na wyłączonych silnikach i włączyć zasilanie ponownie** (sprawdzony przepis z `pos_zero.cpp`):

```cpp
// Wykrywanie w czasie kompilacji, czy dany typ robota ma metodę disableDrag()
template <typename T, typename = void>
struct HasDisableDrag : std::false_type {};

template <typename T>
struct HasDisableDrag<T, std::void_t<
    decltype(std::declval<T&>().disableDrag(std::declval<std::error_code&>()))
>> : std::true_type {};

template <typename Robot>
void disableDragIfActive(Robot& robot, const std::string& moduleName) {
    if constexpr (!HasDisableDrag<Robot>::value) {
        return; // Dla tułowia (PCB4Robot) nic nie robimy
    } else {
        std::error_code ec;
        if (robot.operationState(ec) != rokae::OperationState::drag || ec) return;

        ec.clear();
        robot.disableDrag(ec);
        if (ec) {
            // Procedura awaryjna: stop -> zasilanie OFF -> disableDrag -> zasilanie ON
            std::error_code e2;
            robot.stop(e2);                e2.clear();
            robot.moveReset(e2);           e2.clear();
            robot.setPowerState(false, e2);
            for (int i = 0; i < 40; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::error_code e3;
                if (robot.powerState(e3) == rokae::PowerState::off && !e3) break;
            }
            e2.clear();
            robot.disableDrag(e2);
            ensureMotorPowerOn(robot, moduleName);
        }
    }
}
```

### 4.4. Specyfika przygotowania Torsu (`PCB4Robot`)
Tors (`192.168.71.254`) wymaga wyczyszczenia stanów `recoverState(1..3)`, wyłączenia trybu kasety `setTeachPendantMode(false, ec)` i działa w trybie `NrtCommand` (odczekaj ok. 2 sekundy po włączeniu zasilania torsu):

```cpp
bool przygotujTulow(rokae::PCB4Robot& robot) {
    std::error_code ec;
    robot.recoverState(1, ec);
    robot.recoverState(2, ec);
    robot.recoverState(3, ec);
    robot.clearServoAlarm(ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    robot.setTeachPendantMode(false, ec);
    robot.setOperateMode(rokae::OperateMode::automatic, ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    robot.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);
    robot.setPowerState(true, ec);
    if (ec) return false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return true;
}
```

---

## 5. Krok 2: Prosty Ruch do Punktu – Tryb NRT (`MoveAbsJCommand`)

Tryb **NRT (`MotionControlMode::NrtCommand`)** jest najprostszy i najbezpieczniejszy, gdy chcesz po prostu przestawić ramię lub tułów z obecnej pozycji do jednego punktu docelowego (np. pozycja zero, pozycja startowa). Kontroler sam planuje trajektorię.

### 5.1. Ruch 7-osiowej ręki w trybie NRT (na bazie `pos_zero.cpp` i `wave.cpp`)

```cpp
std::error_code ec;
robot.setOperateMode(rokae::OperateMode::manual, ec); // lub automatic
ensureMotorPowerOn(robot, "Lewa ręka");
disableDragIfActive(robot, "Lewa ręka");

robot.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);

// 1. Zbuduj docelową pozycję 7 osi (w radianach!)
// Przykład A: Wszystkie 7 osi na 0.0 rad (pozycja zero)
rokae::JointPosition zeroTarget(7, 0.0);

// Przykład B: Konkretne kąty 7 osi ze stopni
std::array<double, 7> cel_rad = deg2rad7({2.22, 87.30, -89.40, 41.27, 93.61, 1.27, 39.44});
rokae::JointPosition targetPos;
targetPos.joints.assign(cel_rad.begin(), cel_rad.end());

// 2. Utwórz komendę MoveAbsJCommand (prędkość w mm/s, np. 100-200 mm/s, strefa 0.0 = dokładny dojazd)
rokae::MoveAbsJCommand command(targetPos, 200.0, 0.0);

// 3. Wyczyść kolejkę, dodaj komendę i wystartuj ruch
robot.moveReset(ec);
std::string cmdID;
robot.moveAppend(command, cmdID, ec);
robot.moveStart(ec);
```

### 5.2. Ruch Torsu (4 osie) i Głowy (2 osie zewnętrzne) w trybie NRT
> ⚠️ **Ważna różnica dla Torsu (`PCB4Robot`):**  
> Obiekt `rokae::JointPosition` dla torsu ma **dwa wektory**:
> * `target_pos.joints` – **4 osie tułowia** (`J1, J2, J3, J4`, w radianach),
> * `target_pos.external` – **2 osie głowy** (w radianach)!

```cpp
if (przygotujTulow(tulow)) {
    std::error_code ec;
    tulow.moveReset(ec);

    rokae::JointPosition target_pos;
    target_pos.joints   = {0.0, 0.0, 0.0, 0.0}; // 4 osie torsu (rad)
    target_pos.external = {0.0, 0.0};           // 2 osie głowy (rad)

    rokae::MoveAbsJCommand cmd(target_pos, 50.0); // prędkość np. 50 mm/s
    std::string cmdID;
    tulow.moveAppend(cmd, cmdID, ec);
    tulow.moveStart(ec);
}
```

### 5.3. Jak czekać na koniec ruchu NRT (zamiast niedziałającego `queryEventInfo`)?

Masz do wyboru dwa sprawdzone podejścia:

1. **Krótkie oczekiwanie po `operationState()` (z `wave.cpp`):**
   ```cpp
   // Najpierw daj kontrolerowi 150-500 ms na fizyczne rozpoczęcie ruchu!
   std::this_thread::sleep_for(std::chrono::milliseconds(500));
   const auto t0 = std::chrono::steady_clock::now();
   while (true) {
       std::error_code ec;
       if (robot.operationState(ec) != rokae::OperationState::moving || ec) break;
       std::this_thread::sleep_for(std::chrono::milliseconds(50));
       if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(60)) break;
   }
   ```
2. **Dokładne śledzenie kątów `jointPos()` (z `pos_zero.cpp`):**
   * Najpierw czekasz maksymalnie 5 sekund, aż zmiana kąta dowolnej osi przekroczy `0.02°` (dzięki temu wiesz, czy kontroler w ogóle przyjął komendę i ruszył).
   * Następnie czekasz, aż przez 3 kolejne odczyty (co 100 ms) zmiana kątów spadnie poniżej `0.02°`.

---

## 6. Krok 3: Płynny Ruch Ciągły 7-osiowej Ręki – Pętla RT 1000 Hz (`1 - cos`)

Gdy chcesz wykonać wieloetapowy, płynny ruch (np. machanie ręką w `wave.cpp` albo gestykulację podczas mowy w `gesty.cpp`), tryb NRT ani funkcja `rtCon->MoveJ()` się nie sprawdzą, ponieważ **zatrzymują rękę do zera w każdym punkcie pośrednim**.

Zamiast tego używa się **pętli czasu rzeczywistego (`MotionControlMode::RtCommand`)**, w której Twój callback C++ jest wywoływany przez kontroler **co 1 milisekundę (1000 Hz)** i zwraca natychmiastową pozycję wszystkich 7 osi.

### 6.1. Cztery żelazne zasady pętli RT w Heliosie (odkryte w praktyce!)

1. **Zwiększ tolerancję sieci RT (`setRtNetworkTolerance`) PRZED wejściem w `RtCommand`!**  
   Pętla RT wymaga pakietu co 1 ms. Gdy komputer pokładowy Jetson jest zajęty (np. działa mikrofon, Whisper STT, odtwarzanie audio lub LLM), pakiet potrafi się spóźnić o kilka milisekund. Domyślnie kontroler uznaje to za awarię sieci i **gwałtownie zatrzymuje rękę w losowym miejscu**.  
   Wywołaj `robot.setRtNetworkTolerance(60, ec)` **przed** `setMotionControlMode(RtCommand)`:
   ```cpp
   robot.recoverState(1, ec);
   robot.clearServoAlarm(ec);
   std::this_thread::sleep_for(std::chrono::milliseconds(300));
   robot.setOperateMode(rokae::OperateMode::automatic, ec);

   // Ustaw tolerancję utraty pakietów RT (np. 60%) PRZED przełączeniem w RtCommand!
   robot.setRtNetworkTolerance(60, ec);

   robot.setMotionControlMode(rokae::MotionControlMode::RtCommand, ec);
   ensureMotorPowerOn(robot, "PRAWA");
   ```

2. **Pierwsza komenda w pętli MUSI być równa aktualnej pozycji `robot.jointPos(ec)`!**  
   Jeśli w `t = 0` wyślesz do kontrolera inną pozycję niż ta, w której fizycznie stoi ramię, kontroler wyliczy nieskończoną prędkość i natychmiast odetnie zasilanie silników.

3. **Używaj interpolacji `1 - cos` i pilnuj limitu przyspieszenia ($\le 40^\circ/\text{s}^2$)!**  
   Aby ręka ruszała i hamowała łagodnie (prędkość = 0 na końcach odcinka), używaj współczynnika:
   $$s(t) = \frac{1 - \cos(\pi \cdot t / T)}{2} \in [0, 1]$$
   Szczytowe przyspieszenie na osi pokonującej kąt $\Delta\theta$ w czasie $T$ wynosi:
   $$a_{\max} = \frac{\Delta\theta \cdot \pi^2}{2 \cdot T^2}$$
   > ⚠️ **UWAGA:** Wewnętrzny czujnik bezpieczeństwa w kontrolerze ramienia Heliosa **przerywa ruch, gdy przyspieszenie przekroczy ok. $50^\circ/\text{s}^2$**!  
   Dlatego minimalny bezpieczny czas odcinka $T$ dla maksymalnego przyspieszenia $a_{\max} = 40^\circ/\text{s}^2$ wyliczaj ze wzoru (z `gesty.cpp`):
   ```cpp
   constexpr double MAX_ACC_RAD = 40.0 * M_PI / 180.0; // 40 deg/s^2
   constexpr double MIN_CZAS_S  = 2.0;

   double obliczBezpiecznyCzas(const A7& od_rad, const A7& do_rad) {
       double maxDelta = 0.0;
       for (size_t i = 0; i < 7; i++) {
           maxDelta = std::max(maxDelta, std::fabs(do_rad[i] - od_rad[i]));
       }
       const double t = std::sqrt(maxDelta * M_PI * M_PI / (2.0 * MAX_ACC_RAD));
       return std::max(MIN_CZAS_S, t);
   }
   ```

4. **Powrót do pozycji startowej wykonuj W TEJ SAMEJ pętli RT!**  
   W pierwszych wersjach kodu pętla RT kończyła się po machaniu w punkcie `A`, a powrót do pozycji startowej miał wykonać zwykły `MoveJ`. W praktyce przełączanie trybu po pętli RT często zawodziło i ręka zostawała podniesiona w powietrzu.  
   **Rozwiązanie:** Cały cykl (`Start -> A -> machanie A<->B -> powrót A->Start`) zaprogramuj w jednym callbacku RT i dopiero po dojechaniu z powrotem do `Start` wywołaj `cmd.setFinished()`.

### 6.2. Szablon kompletnej pętli RT (Dojazd $\rightarrow$ Ruch $\rightarrow$ Powrót)

```cpp
auto rtCon = robot.getRtMotionController().lock();
if (!rtCon) return false;

std::error_code ec;
const A7 start = robot.jointPos(ec); // Punkt startowy = aktualna fizyczna pozycja!
const A7 cel   = deg2rad7(CEL_DEG);

const double tTam    = obliczBezpiecznyCzas(start, cel);
const double tPowrot = obliczBezpiecznyCzas(cel, start);
double czas = 0.0;

std::function<rokae::JointPosition()> callback = [&]() {
    czas += 0.001; // Wywoływane dokładnie co 1 ms (1000 Hz)
    rokae::JointPosition cmd;
    cmd.joints.resize(7);

    if (czas < tTam) {
        // Faza 1: Płynny dojazd ze start do cel
        const double s = (1.0 - std::cos(M_PI * czas / tTam)) / 2.0;
        for (size_t i = 0; i < 7; i++)
            cmd.joints[i] = start[i] + (cel[i] - start[i]) * s;
    } else if (czas < tTam + tPowrot) {
        // Faza 2: Płynny powrót z cel do start
        const double tau = czas - tTam;
        const double s = (1.0 - std::cos(M_PI * tau / tPowrot)) / 2.0;
        for (size_t i = 0; i < 7; i++)
            cmd.joints[i] = cel[i] + (start[i] - cel[i]) * s;
    } else {
        // Faza 3: Koniec ruchu - jesteśmy z powrotem w start
        for (size_t i = 0; i < 7; i++) cmd.joints[i] = start[i];
        cmd.setFinished(); // Kończy startLoop()
    }
    return cmd;
};

try {
    rtCon->setControlLoop(callback);
    rtCon->startMove(rokae::RtControllerMode::jointPosition);
    rtCon->startLoop(true); // Blokuje wątek aż do wywołania cmd.setFinished()
} catch (const std::exception& e) {
    std::cerr << "Błąd w pętli RT: " << e.what() << "\n";
}
```

> 💡 **Co jeśli pętla RT zostanie przerwana błędem w połowie ruchu?**  
> W `wave.cpp` zastosowano funkcję ratunkową `powrotPrawejNrt()`, która:
> 1. Wywołuje `stop()` i `moveReset()`.
> 2. Wyłącza silniki `setPowerState(false)` i czeka 4 s na `PowerState::off`.
> 3. Wywołuje `clearServoAlarm()` oraz `recoverState(1)`.
> 4. Przełącza w tryb `manual`, włącza silniki `setPowerState(true)` (czekając na `PowerState::on`), ustawia `NrtCommand` i wysyła bezpieczny `MoveAbsJCommand` z prędkością `100 mm/s` do pozycji startowej.

---

## 7. Krok 4: Geometria 7 Osi – Pozycja Startowa vs Zero, Pułapka 344° i Lustro Rąk

### 7.1. Pozycja Zero (`0°`) to NIE jest Pozycja Startowa!
* **Pozycja Zero (`posZero`):** Wszystkie 7 osi ustawione na `0.0°`. Służy do kalibracji/transportu/diagnostyki.
* **Pozycja Startowa (`posStart`):** Naturalna pozycja spoczynkowa rąk wzdłuż ciała robota, z której startują wszystkie gesty i machanie:

```cpp
// Sprawdzone pozycje startowe ramion Heliosa (J1..J7 w stopniach):
const std::array<double, 7> LEWA_START_DEG  = {   2.22,  87.30,  -89.40, 41.27,  93.61,  1.27, 39.44};
const std::array<double, 7> PRAWA_START_DEG = {-173.06, -79.35, -103.52, 42.80, -84.28, -8.30, -2.92};
```

> ⚠️ **Zabezpieczenie z `gesty.cpp`:** Zanim uruchomisz trajektorię gestów, zawsze sprawdź, czy każda z 7 osi stoi w promieniu maksymalnie **`20°`** od `LEWA_START_DEG` / `PRAWA_START_DEG`. Jeśli robot stoi w pozycji zero (`0°`), bezpośredni ruch do punktów gestów poprowadzi ramię po nieprzewidywalnym torze!

### 7.2. Pułapka interpolacji w przestrzeni jointów ($\pm 180^\circ$ = obrót o $344^\circ$!)
Ramię w trybie osiowym (`jointPosition` / `MoveAbsJ`) interpoluje kąty **liniowo od wartości początkowej do końcowej**, a **nie** „najkrótszą drogą po okręgu”.

* **Przykład z życia (`wave.cpp`):**  
  W pozycji startowej prawa ręka ma oś barku **`J1 = -173.06°`**.  
  Jeśli uczysz punkt machania i wpiszesz **`J1 = +171.0°`** (co w przestrzeni 3D jest niemal tym samym kątem co `-189°`), ramię spróbuje przejechać z `-173.06°` do `+171.0°` przez zero, wykonując **pełny wymach barkiem o $344.06^\circ$**!  
  **Rozwiązanie:** Zawsze zachowuj spójny znak kąta blisko granicy $\pm 180^\circ$ (w `wave.cpp` użyto **`J1 = -171.0°`**, dzięki czemu oś J1 obraca się tylko o bezpieczne $2^\circ$).  
  Dodatkowo przed ruchem warto w kodzie policzyć $\max_i | \theta_{\text{cel}}[i] - \theta_{\text{start}}[i] |$ i wypisać ostrzeżenie, jeśli którykolwiek joint ma wykonać obrót $> 120^\circ$.

### 7.3. Jak automatycznie odbić punkty z Lewej ręki na Prawą (Lustrzana symetria 7 osi)?
Ponieważ lewe i prawe ramię są zamontowane symetrycznie po obu stronach korpusu, a ich pozycje startowe mają przeliczoną konfigurację barku, nie musisz ręcznie uczyć tych samych gestów dla obu rąk.

W `gesty.cpp` wyprowadzono wzór na przeliczenie dowolnego punktu lewej ręki na odpowiadający mu punkt prawej ręki względem ich pozycji startowych:

$$\text{Prawa}[i] = \text{PRAWA\_START}[i] + \text{ZNAK}[i] \cdot \left(\text{Lewa}[i] - \text{LEWA\_START}[i]\right)$$

gdzie wektor znaków dla osi `J1..J7` wynosi:
```cpp
// Osie J1, J2, J3, J5, J7 zmieniają znak (-1), a osie J4 (łokieć) i J6 (+1) zachowują znak:
const std::array<double, 7> ZNAK_PRAWA = {-1, -1, -1, +1, -1, +1, -1};

std::array<double, 7> odbijPunktNaPrawa(const std::array<double, 7>& lewa_deg) {
    std::array<double, 7> prawa_deg{};
    for (size_t i = 0; i < 7; i++) {
        prawa_deg[i] = PRAWA_START_DEG[i] + ZNAK_PRAWA[i] * (lewa_deg[i] - LEWA_START_DEG[i]);
    }
    return prawa_deg;
}
```
*(Przy pierwszym teście nowych punktów odbitych lustrzanie zawsze zmniejsz prędkość i trzymaj rękę na grzybku E-Stop!).*

---

## 8. Ściąga: Błędy Rokae SDK i Ich Rozwiązania

| Komunikat błędu / Objaw | Co oznacza / Dlaczego wystąpił | Jak naprawić w kodzie C++ |
| :--- | :--- | :--- |
| **`motor power state unsupported operation`** | Wysłano komendę ruchu (`moveAppend` / `moveStart` / start RT), zanim styczniki silników fizycznie się załączyły po `setPowerState(true)`. | Po `setPowerState(true, ec)` dodaj pętlę czekającą aż `robot.powerState(ec) == rokae::PowerState::on` (z timeoutem 10 s). |
| **`task_space unsupported operation`** przy `moveStart()` | Ramię znajduje się w trybie wodzenia ręcznego (`OperationState::drag`). | Wywołaj `robot.disableDrag(ec)`. Jeśli zwróci błąd, wyłącz zasilanie (`setPowerState(false)`), wywołaj `disableDrag(ec)` na wyłączonych silnikach i włącz zasilanie ponownie. |
| **`robot state (moving) unsupported operation`** | Próba wywołania `setPowerState` lub `setOperateMode`, gdy ramię jeszcze wykonuje poprzedni ruch (stan `OperationState::moving`). | Przed przygotowaniem ręki sprawdź `operationState()`. Jeśli jest `moving`, wywołaj `stop()` + `moveReset()` i zaczekaj w pętli aż stan zmieni się na `idle`. |
| **`spdlog fatal error: logger with name '_rokae_log_console_' already exists`** (`SIGABRT`) | Dwa wątki jednocześnie wywołały konstruktor `rokae::xMateErProRobot`. | Opakuj tworzenie (`make_unique`), `connectToRobot()` oraz niszczenie (`reset()`) obiektów robota wspólnym `std::mutex`. |
| **Błąd kompilacji w `bits/hashtable.h` (`static_assert`)** | Użyto `robot.queryEventInfo()` na kompilatorze GCC 11 (Ubuntu 22.04 na Jetsonie). | Nie używaj `queryEventInfo()`. Sprawdzaj koniec ruchu odpytując w pętli `robot.operationState(ec)` lub zmiany kątów z `robot.jointPos(ec)`. |
| **Ręka w pętli RT nagle zatrzymuje się w połowie ruchu (`hasMotionError: TAK`)** | 1) Zbyt duże przyspieszenie ($> 50^\circ/\text{s}^2$), **LUB**<br>2) Jetson był obciążony (audio/AI) i pakiet 1 ms się spóźnił. | 1) Wydłuż czas odcinka $T$ tak, by $a_{\max} = \frac{\Delta\theta \cdot \pi^2}{2 T^2} \le 40^\circ/\text{s}^2$.<br>2) Przed `setMotionControlMode(RtCommand)` wywołaj `robot.setRtNetworkTolerance(60, ec)`. |
| **Po zakończeniu machania w RT ręka nie wraca przez `MoveJ` i zostaje w górze** | Przełączenie kontrolera z pętli `RtCommand` (`jointPosition`) na `MoveJ` często się nie udaje bez pełnego resetu zasilania. | Włącz fazę powrotu do pozycji startowej bezpośrednio do callbacku pętli RT (`1 - cos` do punktu startowego) i dopiero tam wywołaj `cmd.setFinished()`. |
