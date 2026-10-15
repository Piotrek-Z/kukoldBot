#include "arms_moves.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "rokae/robot.h"
#include "rokae/data_types.h"

namespace commands {

namespace {

// ============================================================================
// Sam ruch ramion - bez YouTube, bez mpv, bez tułowia.
//
// Sekwencja (patrz arms_move() na dole):
//   1. prawa ręka wjeżdża w pozę i trzyma ją przez cały cykl,
//   2. lewa ręka przełącza się szybko A -> B, 3 powtórzenia,
//   3. obie ręce wracają do pozycji spoczynkowych.
//
// Ruch lewej ręki: cały cykl A->B x3 idzie na JEDNYM moveStart jako partia
// MoveAbsJ - dla kontrolera to jedna trajektoria z punktami pośrednimi
// (w statusie ruchu jest wayPointIndex). Punkty pośrednie mają strefę
// BLEND_ZONE_MM, więc robot przejeżdża przez A/B płynnie, bez zatrzymywania;
// ostatni punkt ma zone=0 (dokładne dojście i postój na B).
//
// Dlaczego nie "podmiana celu w locie": tryb NRT nie pozwala zmodyfikować
// wysłanej komendy, a jedyne API z podmianą celu (rokae::FollowPosition)
// wymaga biblioteki xMateModel, która nie wspiera tego robota
// (AR5-5_0.7L-W4C4A2 -> "xMateModel library unsupported robot type").
// Strefa przejazdu (zone) daje ten sam efekt wizualny: płynny łuk w każdym
// punkcie, zero zatrzymań pośrednich.
//
// Przygotowanie modułów (recoverState, tryb, NrtCommand, zasilanie) i mechanika
// ruchu (moveJointsToRetry, waitUntilStable) jak w youtube.cpp - bez części
// muzycznej i bez bujania tułowiem.
// ============================================================================

// Adresy modułów (jak w youtube.cpp / pos_zero.cpp).
const std::string LEWA_IP  = "192.168.71.161";
const std::string PRAWA_IP = "192.168.71.160";
const std::string LOCAL_IP = "192.168.71.51";

constexpr double PI = 3.141592653589793;

// Tempo: ma być szybko (jak prosiłeś), ale wciąż płynnie. Podkręć/lub obniż
// tę jedną liczbę, jeśli na żywym robocie będzie za ostro albo za wolno.
constexpr double ARM_SPEED    = 1000.0;
constexpr double MOVE_ZONE_MM = 0.0;  // fine - dokładne dojście i postój

// Strefa przejazdu (tzw.转弯区) punktów pośrednich, w mm. 0 = fine (robot
// zatrzymuje się w punkcie); 1-200 = promień zaokrąglenia narożnika.
// Większy = płynniejszy przejazd, ale mocniejsze "cięcie" narożników -
// robot nie dojeżdża dokładnie w A/B. Ostatni punkt ścieżki zawsze dostaje
// MOVE_ZONE_MM, żeby na końcu stanąć dokładnie na miejscu.
constexpr double BLEND_ZONE_MM = 40.0;

constexpr rokae::OperateMode MOVE_OPERATE_MODE = rokae::OperateMode::automatic;

// Zasilanie: kilka prób z czyszczeniem alarmów pomiędzy. Firmware potrafi
// odrzucić pierwsze setPowerState po starcie kontrolera.
constexpr int POWER_ATTEMPTS        = 4;
constexpr int POWER_ATTEMPT_WAIT_MS = 3000;
constexpr int POWER_POLL_MS         = 200;

// Ruch: luźne okno startu + jedna automatyczna powtórka całego ruchu.
// Kontroler świeżo po włączeniu zasilania potrafi przyjąć komendę i nie ruszyć.
constexpr int MOVE_START_TIMEOUT_MS    = 10000;
constexpr int MOVE_TIMEOUT_MS          = 60000;
constexpr int MOVE_POLL_MS             = 100;
constexpr int MOVE_ATTEMPTS            = 2;
constexpr double MOVING_EPS_DEG        = 0.02;
constexpr int STABLE_POLLS_REQUIRED    = 3;
constexpr double ARRIVED_TOLERANCE_DEG = 1.0;  // "już na miejscu" - ruch pomijany

// Oddech po włączeniu zasilania, zanim pójdzie pierwsza komenda ruchu.
constexpr int ARM_SETTLE_MS = 1000;

// Ile razy lewa ręka wykonuje parę A -> B.
constexpr int REPETITIONS = 3;

// Pozy w stopniach (J1..J7).
constexpr std::array<double, 7> LEFT_A_DEG     = {-59.64, 79.56, -128.99, 97.88, 76.47, 13.10, 7.35};
constexpr std::array<double, 7> LEFT_B_DEG     = {-60.90, 86.37, -53.22, 110.79, 122.33, 11.58, 2.10};
constexpr std::array<double, 7> RIGHT_HOLD_DEG = {-0.55, 70.16, 74.67, 33.71, 24.83, -1.93, -1.18};
constexpr std::array<double, 7> RIGHT_REST_DEG = {-0.55, 70.16, 74.67, 33.71, 24.83, -1.93, -1.18};
constexpr std::array<double, 7> LEFT_REST_DEG  = {-1.48, 82.05, -102.83, 42.32, 98.31, 9.35, 20.25};

template <std::size_t N>
std::vector<double> deg2radVec(const std::array<double, N>& deg) {
    std::vector<double> rad(N);
    for (std::size_t i = 0; i < N; ++i) {
        rad[i] = deg[i] * PI / 180.0;
    }
    return rad;
}

// Wykrywanie API draga (jak w pos_zero.cpp): enableDrag/disableDrag są
// w cobotach, a np. PCB4Robot ich nie ma.
template <typename T, typename = void>
struct HasDisableDrag : std::false_type {};

template <typename T>
struct HasDisableDrag<T, std::void_t<
    decltype(std::declval<T&>().disableDrag(std::declval<std::error_code&>()))
>> : std::true_type {};

template <typename A, typename B>
double maxAbsDiffDeg(const A& first, const B& second) {
    double maximum = 0.0;
    const std::size_t count = std::min(first.size(), second.size());
    for (std::size_t i = 0; i < count; ++i) {
        maximum = std::max(maximum,
                           std::fabs((first[i] - second[i]) * 180.0 / PI));
    }
    return maximum;
}

template <typename JointArray>
void printJoints(const JointArray& joints) {
    std::cout << std::fixed << std::setprecision(2);
    for (std::size_t i = 0; i < joints.size(); ++i) {
        std::cout << "J" << i + 1 << ": "
                  << joints[i] * 180.0 / PI << "  ";
    }
    std::cout << std::defaultfloat;
}

const char* powerStateName(rokae::PowerState state) {
    switch (state) {
        case rokae::PowerState::on:    return "on";
        case rokae::PowerState::off:   return "off";
        case rokae::PowerState::estop: return "estop (wciśnięty wyłącznik awaryjny)";
        case rokae::PowerState::gstop: return "gstop (otwarta osłona bezpieczeństwa)";
        default:                       return "nieznany";
    }
}

// Zasilanie silników - bez niego kontroler nie przyjmie ruchu.
// Próbuje wielokrotnie: bywa, że pierwsze setPowerState jest odrzucane.
// Uwaga: stan "on" też przechodzi przez pętlę (odświeżenie + weryfikacja),
// bo kontroler potrafił zgłosić "on", a po kilkunastu sekundach był "off".
template <typename Robot>
bool ensureMotorPowerOn(Robot& robot, const std::string& moduleName, int attempts) {
    std::error_code ec;
    const auto state = robot.powerState(ec);

    if (!ec) {
        std::cout << "[Arms] " << moduleName
                  << " - zasilanie: " << powerStateName(state) << "\n";

        if (state == rokae::PowerState::estop) {
            std::cerr << "[Arms] " << moduleName
                      << " - E-STOP wciśnięty. Zwolnij wyłącznik awaryjny i powtórz.\n";
            return false;
        }

        if (state == rokae::PowerState::gstop) {
            std::cerr << "[Arms] " << moduleName
                      << " - otwarta osłona bezpieczeństwa. Zamknij ją i powtórz.\n";
            return false;
        }
    } else {
        std::cerr << "[Arms] " << moduleName
                  << " - powerState(): " << ec.message() << "\n";
    }

    for (int attempt = 1; attempt <= attempts; ++attempt) {
        std::cout << "[Arms] " << moduleName << " - włączam zasilanie silników"
                  << " (próba " << attempt << "/" << attempts << ")...\n";

        if (attempt > 1) {
            // Może zalega alarm blokujący załączenie - wyczyść przed ponowieniem.
            ec.clear();
            robot.recoverState(1, ec);  // kasowanie stanu awaryjnego (jak w youtube.cpp)
            ec.clear();
            robot.clearServoAlarm(ec);
        }

        ec.clear();
        robot.setPowerState(true, ec);

        if (ec) {
            std::cerr << "[Arms] " << moduleName
                      << " - setPowerState(true): " << ec.message() << "\n";
        }

        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(POWER_ATTEMPT_WAIT_MS);
        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(POWER_POLL_MS));

            ec.clear();
            const auto current = robot.powerState(ec);
            if (!ec && current == rokae::PowerState::on) {
                std::cout << "[Arms] " << moduleName << " - zasilanie włączone.\n";
                return true;
            }
        }
    }

    std::cerr << "[Arms] " << moduleName << " - zasilanie nie włączyło się po "
              << attempts << " próbach.\n";
    return false;
}

// Przy włączonym dragu kontroler nie przyjmie komendy ruchu.
template <typename Robot>
void disableDragIfActive(Robot& robot, const std::string& moduleName) {
    if constexpr (!HasDisableDrag<Robot>::value) {
        return;  // brak API draga - nic nie robimy.
    } else {
        std::error_code ec;
        const auto state = robot.operationState(ec);

        if (ec || state != rokae::OperationState::drag) {
            return;
        }

        std::cout << "[Arms] " << moduleName
                  << " - drag był włączony, wyłączam przed ruchem...\n";

        ec.clear();
        robot.disableDrag(ec);

        if (ec) {
            std::cerr << "[Arms] " << moduleName
                      << " - disableDrag: " << ec.message() << "\n";
        }
    }
}

template <typename Robot>
void abortMotion(Robot& robot, const std::string& moduleName) {
    std::error_code ec;
    robot.stop(ec);

    if (ec) {
        std::cerr << "[Arms] " << moduleName
                  << " - stop(): " << ec.message() << "\n";
    }

    ec.clear();
    robot.moveReset(ec);   // odrzuca niewykonane komendy

    if (ec) {
        std::cerr << "[Arms] " << moduleName
                  << " - moveReset(): " << ec.message() << "\n";
    }
}

// Czeka na koniec ruchu: najpierw aż robot ruszy, potem aż stanie.
// Celowo nie używamy queryEventInfo() - patrz komentarz w pos_zero.cpp.
template <typename Robot, typename JointArray>
bool waitUntilStable(Robot& robot, const std::string& moduleName,
                     const JointArray& startPosition) {
    const auto overallDeadline = std::chrono::steady_clock::now()
                               + std::chrono::milliseconds(MOVE_TIMEOUT_MS);
    const auto startDeadline   = std::chrono::steady_clock::now()
                               + std::chrono::milliseconds(MOVE_START_TIMEOUT_MS);

    bool moving = false;
    int stablePolls = 0;
    JointArray previous = startPosition;

    while (std::chrono::steady_clock::now() < overallDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(MOVE_POLL_MS));

        std::error_code ec;
        const auto current = robot.jointPos(ec);

        if (ec) {
            continue;
        }

        const double delta = maxAbsDiffDeg(current, previous);
        previous = current;

        if (!moving) {
            if (delta > MOVING_EPS_DEG) {
                moving = true;
            } else if (std::chrono::steady_clock::now() > startDeadline) {
                std::cerr << "[Arms] " << moduleName
                          << " - robot nie ruszył w "
                          << (MOVE_START_TIMEOUT_MS / 1000)
                          << " s (kontroler odrzucił komendę?).\n";
                return false;
            }
            continue;
        }

        if (delta <= MOVING_EPS_DEG) {
            if (++stablePolls >= STABLE_POLLS_REQUIRED) {
                return true;
            }
        } else {
            stablePolls = 0;
        }
    }

    std::cerr << "[Arms] " << moduleName
              << " - przekroczono maksymalny czas ruchu ("
              << (MOVE_TIMEOUT_MS / 1000) << " s).\n";
    return false;
}

// Podłączenie i przygotowanie ręki do ruchu. Kolejność kroków jak
// w youtube.cpp: recoverState -> alarmy -> tryb -> zasilanie.
bool prepareArm(rokae::xMateErProRobot& robot, const std::string& moduleName) {
    std::error_code ec;

    std::cout << "[Arms] " << moduleName << " - łączenie z kontrolerem...\n";
    robot.connectToRobot(ec);

    if (ec) {
        std::cerr << "[Arms] " << moduleName
                  << " - błąd połączenia: " << ec.message() << "\n";
        return false;
    }

    ec.clear();
    robot.recoverState(1, ec);  // zdejmuje blokady/stany ochronne kontrolera

    if (ec) {
        std::cout << "[Arms] " << moduleName
                  << " - recoverState(1): " << ec.message() << "\n";
    }

    ec.clear();
    robot.clearServoAlarm(ec);

    if (ec) {
        std::cout << "[Arms] " << moduleName
                  << " - clearServoAlarm: " << ec.message() << "\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    ec.clear();
    robot.setOperateMode(MOVE_OPERATE_MODE, ec);

    if (ec) {
        std::cerr << "[Arms] " << moduleName
                  << " - setOperateMode: " << ec.message() << "\n";
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ec.clear();
    robot.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);

    if (ec) {
        std::cerr << "[Arms] " << moduleName
                  << " - setMotionControlMode(NrtCommand): " << ec.message() << "\n";
        return false;
    }

    if (!ensureMotorPowerOn(robot, moduleName, POWER_ATTEMPTS)) {
        return false;
    }

    // Serwa świeżo po załączeniu potrzebują chwili, zanim przyjmą ruch.
    std::this_thread::sleep_for(std::chrono::milliseconds(ARM_SETTLE_MS));

    disableDragIfActive(robot, moduleName);
    return true;
}

// Przed każdym ruchem: dopnij tryb i zasilanie. Naprawia, nie psuje.
template <typename Robot>
bool reEnsureMotionReady(Robot& robot, const std::string& moduleName) {
    std::error_code ec;
    robot.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);

    if (ec) {
        std::cerr << "[Arms] " << moduleName
                  << " - setMotionControlMode(NrtCommand): " << ec.message() << "\n";
        return false;
    }

    ec.clear();
    const auto state = robot.powerState(ec);

    if (!ec && state == rokae::PowerState::on) {
        return true;
    }

    std::cout << "[Arms] " << moduleName << " - dopinam zasilanie przed ruchem...\n";
    return ensureMotorPowerOn(robot, moduleName, POWER_ATTEMPTS);
}

// Jeden ruch absolutny w stawach. Cel w radianach.
template <typename Robot>
bool moveJointsTo(Robot& robot, const std::string& moduleName,
                  const std::vector<double>& targetRad, double speed) {
    if (!reEnsureMotionReady(robot, moduleName)) {
        return false;
    }

    std::error_code ec;
    const auto startPosition = robot.jointPos(ec);

    if (ec) {
        std::cerr << "[Arms] " << moduleName
                  << " - jointPos(): " << ec.message() << "\n";
        return false;
    }

    if (startPosition.size() != targetRad.size()) {
        std::cerr << "[Arms] " << moduleName << " - niezgodna liczba stawów: robot ma "
                  << startPosition.size() << ", cel ma " << targetRad.size() << ".\n";
        return false;
    }

    // Już na miejscu: nie wysyłaj komendy, bo "brak ruchu" wyglądałby jak
    // odrzucenie i mielibyśmy timeouty. Tak samo robi pos_zero.
    if (maxAbsDiffDeg(startPosition, targetRad) <= ARRIVED_TOLERANCE_DEG) {
        std::cout << "[Arms] " << moduleName << " - już na miejscu, ruch pominięty.\n";
        return true;
    }

    rokae::JointPosition target;
    target.joints.resize(targetRad.size());
    for (std::size_t i = 0; i < targetRad.size(); ++i) {
        target.joints[i] = targetRad[i];
    }
    const rokae::MoveAbsJCommand command(target, speed, MOVE_ZONE_MM);

    ec.clear();
    robot.moveReset(ec);

    std::string cmdID;
    ec.clear();
    robot.moveAppend(command, cmdID, ec);

    if (ec) {
        std::cerr << "[Arms] " << moduleName
                  << " - moveAppend: " << ec.message() << "\n";
        return false;
    }

    ec.clear();
    robot.moveStart(ec);

    if (ec) {
        std::cerr << "[Arms] " << moduleName
                  << " - moveStart: " << ec.message() << "\n";
        return false;
    }

    if (!waitUntilStable(robot, moduleName, startPosition)) {
        return false;  // timeout - powód już wypisany
    }

    ec.clear();
    const auto finalPosition = robot.jointPos(ec);

    if (!ec) {
        const double deviation = maxAbsDiffDeg(finalPosition, targetRad);
        std::cout << "[Arms] " << moduleName << " - doszedł (odchyłka "
                  << std::fixed << std::setprecision(2) << deviation << "°): ";
        printJoints(finalPosition);
        std::cout << "\n";
    }

    return true;
}

// Ruch z automatyczną powtórką: kontroler świeżo po starcie potrafi
// zignorować pierwszą komendę (przyjąć bez błędu i nie ruszyć).
template <typename Robot>
bool moveJointsToRetry(Robot& robot, const std::string& moduleName,
                       const std::vector<double>& targetRad, double speed) {
    for (int attempt = 1; attempt <= MOVE_ATTEMPTS; ++attempt) {
        if (attempt > 1) {
            std::cout << "[Arms] " << moduleName << " - powtarzam ruch (próba "
                      << attempt << "/" << MOVE_ATTEMPTS << ")...\n";
        }
        if (moveJointsTo(robot, moduleName, targetRad, speed)) {
            return true;
        }
        abortMotion(robot, moduleName);
    }
    return false;
}

// Cała ścieżka w stawach na JEDNYM moveStart. Partia MoveAbsJ jest dla
// kontrolera jedną trajektorią z punktami pośrednimi: punkty pośrednie
// (wszystkie oprócz ostatniego) mają strefę BLEND_ZONE_MM - robot przejeżdża
// przez nie płynnie, bez zatrzymywania; ostatni punkt ma MOVE_ZONE_MM
// (dokładne dojście i postój). Na tej samej zasadzie działała stara wersja
// "osobny Move na każdy punkt" - tu tylko wszystko leci w jednym locie.
// W środku: ta sama logika powtórek co w moveJointsToRetry.
template <typename Robot>
bool moveJointsSmooth(Robot& robot, const std::string& moduleName,
                      const std::vector<std::vector<double>>& pointsRad,
                      double speed) {
    if (pointsRad.empty()) {
        return false;
    }

    for (int attempt = 1; attempt <= MOVE_ATTEMPTS; ++attempt) {
        if (attempt > 1) {
            std::cout << "[Arms] " << moduleName << " - powtarzam całą sekwencję (próba "
                      << attempt << "/" << MOVE_ATTEMPTS << ")...\n";
        }

        if (!reEnsureMotionReady(robot, moduleName)) {
            continue;
        }

        std::error_code ec;
        const auto startPosition = robot.jointPos(ec);

        if (ec) {
            std::cerr << "[Arms] " << moduleName
                      << " - jointPos(): " << ec.message() << "\n";
            abortMotion(robot, moduleName);
            continue;
        }

        const std::vector<double>& finalTarget = pointsRad.back();

        if (startPosition.size() != finalTarget.size()) {
            std::cerr << "[Arms] " << moduleName << " - niezgodna liczba stawów: robot ma "
                      << startPosition.size() << ", cel ma " << finalTarget.size() << ".\n";
            return false;  // błąd konfiguracji - powtórka nie pomoże
        }

        // Cała ścieżka już zrobiona (np. poprzedni cykl skończył się na B):
        // nie wysyłaj komendy, żeby "brak ruchu" nie wyglądał jak odrzucenie.
        if (maxAbsDiffDeg(startPosition, finalTarget) <= ARRIVED_TOLERANCE_DEG) {
            std::cout << "[Arms] " << moduleName << " - już na miejscu, sekwencja pominięta.\n";
            return true;
        }

        // Jedna partia = jedna trajektoria z punktami pośrednimi.
        std::vector<rokae::MoveAbsJCommand> cmds;
        cmds.reserve(pointsRad.size());
        for (std::size_t i = 0; i < pointsRad.size(); ++i) {
            const rokae::JointPosition target(pointsRad[i]);
            const bool last = (i + 1 == pointsRad.size());
            cmds.emplace_back(target, speed, last ? MOVE_ZONE_MM : BLEND_ZONE_MM);
        }

        ec.clear();
        robot.moveReset(ec);

        std::string cmdID;
        ec.clear();
        robot.moveAppend(cmds, cmdID, ec);

        if (ec) {
            std::cerr << "[Arms] " << moduleName
                      << " - moveAppend: " << ec.message() << "\n";
            abortMotion(robot, moduleName);
            continue;
        }

        ec.clear();
        robot.moveStart(ec);

        if (ec) {
            std::cerr << "[Arms] " << moduleName
                      << " - moveStart: " << ec.message() << "\n";
            abortMotion(robot, moduleName);
            continue;
        }

        if (!waitUntilStable(robot, moduleName, startPosition)) {
            abortMotion(robot, moduleName);
            continue;
        }

        ec.clear();
        const auto finalPosition = robot.jointPos(ec);

        if (!ec) {
            const double deviation = maxAbsDiffDeg(finalPosition, finalTarget);
            std::cout << "[Arms] " << moduleName << " - doszedł (odchyłka "
                      << std::fixed << std::setprecision(2) << deviation << "°): ";
            printJoints(finalPosition);
            std::cout << "\n";
        }

        return true;
    }

    std::cerr << "[Arms] " << moduleName << " - sekwencja nie powiodła się po "
              << MOVE_ATTEMPTS << " próbach.\n";
    return false;
}

} // namespace

void arms_move() {
    auto lewa  = std::make_unique<rokae::xMateErProRobot>(LEWA_IP, LOCAL_IP);
    auto prawa = std::make_unique<rokae::xMateErProRobot>(PRAWA_IP, LOCAL_IP);

    // Przygotowanie równoległe obu rąk (jak w youtube.cpp).
    bool lewaOk = false, prawaOk = false;
    std::thread prepLeft([&]  { lewaOk  = prepareArm(*lewa,  "Lewa ręka"); });
    std::thread prepRight([&] { prawaOk = prepareArm(*prawa, "Prawa ręka"); });
    prepLeft.join();
    prepRight.join();

    if (!lewaOk && !prawaOk) {
        std::cerr << "[Arms] Żadna ręka nie odpowiada - kończę.\n";
        return;
    }

    // 1. Prawa ręka wjeżdża w pozę i trzyma ją przez cały cykl lewej
    //    ("prawa ręka cały czas" - dlatego czekamy, aż dojedzie).
    if (prawaOk) {
        moveJointsToRetry(*prawa, "Prawa ręka", deg2radVec(RIGHT_HOLD_DEG), ARM_SPEED);
    }

    // 2. Lewa ręka: A -> B, REPETITIONS razy szybko. Cała ścieżka na jednym
    //    moveStart - przejazd przez punkty bez zatrzymywania (patrz moveJointsSmooth).
    if (lewaOk) {
        std::vector<std::vector<double>> leftPath;
        leftPath.reserve(REPETITIONS * 2);
        for (int i = 0; i < REPETITIONS; ++i) {
            leftPath.push_back(deg2radVec(LEFT_A_DEG));
            leftPath.push_back(deg2radVec(LEFT_B_DEG));
        }
        moveJointsSmooth(*lewa, "Lewa ręka", leftPath, ARM_SPEED);
    }

    // 3. Powrót do pozycji spoczynkowych (równolegle).
    std::cout << "[Arms] Koniec sekwencji - wracam do pozycji spoczynkowych.\n";
    std::thread leftBack, rightBack;
    if (lewaOk) {
        leftBack = std::thread([&] {
            moveJointsToRetry(*lewa, "Lewa ręka", deg2radVec(LEFT_REST_DEG), ARM_SPEED);
        });
    }
    if (prawaOk) {
        rightBack = std::thread([&] {
            moveJointsToRetry(*prawa, "Prawa ręka", deg2radVec(RIGHT_REST_DEG), ARM_SPEED);
        });
    }
    if (leftBack.joinable()) {
        leftBack.join();
    }
    if (rightBack.joinable()) {
        rightBack.join();
    }

    std::cout << "[Arms] Gotowe.\n";
}

} // namespace commands
