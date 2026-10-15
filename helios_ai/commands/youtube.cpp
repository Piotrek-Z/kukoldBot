#include "youtube.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "rokae/robot.h"
#include "rokae/data_types.h"

namespace commands {

using json = nlohmann::json;

namespace {

constexpr const char* YOUTUBE_API_URL =
    "https://www.googleapis.com/youtube/v3/search";
constexpr const char* AUDIO_DEVICE = "alsa/plughw:CARD=REF,DEV=0";

size_t writeCallback(void* contents, size_t size, size_t nmemb,
                     std::string* output) {
    const size_t total = size * nmemb;
    output->append(static_cast<char*>(contents), total);
    return total;
}

std::string urlEncode(CURL* curl, const std::string& value) {
    char* encoded = curl_easy_escape(curl, value.c_str(),
                                     static_cast<int>(value.size()));
    if (!encoded) return {};

    std::string result(encoded);
    curl_free(encoded);
    return result;
}

// Uruchamia mpv i zwraca jego PID (albo -1 przy b³êdzie).
// Po PID-zie poznajemy koniec piosenki (patrz mpvAlive).
pid_t playWithMpv(const std::string& url) {
    const pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "[YouTube] Nie uda³o siê uruchomiæ procesu mpv.\n";
        return -1;
    }

    if (pid == 0) {
        execlp(
            "mpv",
            "mpv",
            "--no-video",
            "--audio-device=alsa/plughw:CARD=REF,DEV=0",
            "--cache=yes",
            "--cache-secs=30",
            "--cache-pause=yes",
            "--ytdl-format=251",
            url.c_str(),
            static_cast<char*>(nullptr)
        );

        std::cerr << "[YouTube] Nie znaleziono programu mpv.\n";
        _exit(127);
    }

    std::cout << "[YouTube] Uruchomiono mpv, PID: " << pid << "\n";
    return pid;
}

// ============================================================================
// Taniec do muzyki
//
// Po odpaleniu piosenki robot ustawia rêce w pozie tanecznej, buja tu³owiem
// do koñca utworu, a potem wraca rêkami do pozycji spoczynkowych i tu³owiem
// do zera. Ca³oœæ leci w w¹tku komendy (odczepionym w llm_commands), wiêc
// odpowiedŸ g³osowa "Jasne, odtwarzam" idzie od razu, bez czekania.
//
// Przep³yw jest równoleg³y: rêce szykuj¹ siê i id¹ w pozê od razu, a tu³ów
// przygotowuje siê w osobnym w¹tku w tle i do³¹cza do bujania, jak wstanie.
// Dziêki temu jego d³ugie próby zasilania nie opóŸniaj¹ r¹k.
//
// Przygotowanie modu³ów (recoverState, tryb, NrtCommand, zasilanie) wzorowane
// na wave.cpp. Odpytywanie pozycji zamiast queryEventInfo (nie kompiluje siê
// na GCC 11) - jak w pos_zero.cpp.
// ============================================================================

// Adresy modu³ów (jak w pos_zero.cpp / wave.cpp).
const std::string LEWA_IP  = "192.168.71.161";
const std::string PRAWA_IP = "192.168.71.160";
const std::string TULOW_IP = "192.168.71.254";
const std::string LOCAL_IP = "192.168.71.51";

constexpr double PI = 3.141592653589793;

// Prêdkoœci: taniec ma byæ p³ynny, nie sportowy.
constexpr double ARM_SPEED_MM_S   = 150.0;  // rêce: poza taneczna i powrót
constexpr double TORSO_SPEED_MM_S = 100.0;  // tu³ów: powrót do zera
constexpr double SWAY_SPEED_MM_S  = 60.0;   // tu³ów: bujanie (wolno i miêkko)
constexpr double MOVE_ZONE_MM     = 0.0;    // dok³adne dojœcie do punktu

constexpr rokae::OperateMode DANCE_OPERATE_MODE = rokae::OperateMode::automatic;

// Zasilanie: kilka prób z czyszczeniem alarmów pomiêdzy. Firmware tu³owia
// potrafi odrzuciæ pierwsze setPowerState po starcie kontrolera.
constexpr int POWER_ATTEMPTS         = 4;  // rêce (szybkie, nie blokuj¹ innych)
constexpr int TORSO_ATTEMPTS_PER_MODE = 2;  // tu³ów próbuje w 2 trybach (patrz ni¿ej)
constexpr int POWER_ATTEMPT_WAIT_MS  = 3000;
constexpr int POWER_POLL_MS          = 200;

// Ruch: luŸne okno startu + jedna automatyczna powtórka ca³ego ruchu.
// Kontroler œwie¿o po w³¹czeniu zasilania potrafi przyj¹æ komendê i nie ruszyæ.
constexpr int MOVE_START_TIMEOUT_MS = 10000;
constexpr int MOVE_TIMEOUT_MS       = 60000;
constexpr int MOVE_POLL_MS          = 100;
constexpr int MOVE_ATTEMPTS         = 2;
constexpr double MOVING_EPS_DEG     = 0.02;
constexpr int STABLE_POLLS_REQUIRED = 3;
constexpr double ARRIVED_TOLERANCE_DEG = 1.0;  // "ju¿ na miejscu" - ruch pomijany

// Oddech po w³¹czeniu zasilania, zanim pójdzie pierwsza komenda ruchu.
constexpr int ARM_SETTLE_MS   = 1000;
constexpr int TORSO_SETTLE_MS = 2000;  // tyle czeka wave.cpp

// Bujanie tu³owiem: który staw za któr¹ oœ i jak mocno.
//
// T1..T4 to joints[0..3]. Jeœli robot buja siê nie w tê stronê (np. na boki
// zamiast przód/ty³), zamieñ te dwa indeksy miejscami i przebuduj.
constexpr std::size_t SWAY_PITCH_JOINT = 2;  // przód/ty³
constexpr std::size_t SWAY_ROLL_JOINT  = 0;  // lewo/prawo
constexpr double SWAY_AMP_DEG = 3.0;         // "leciutko" - amplituda bujania

// Pozy w stopniach.
constexpr std::array<double, 7> RIGHT_DANCE_DEG = {52.27, 91.18, 40.87, 128.43, 34.39, -20.82, 28.47};
constexpr std::array<double, 7> RIGHT_REST_DEG  = {-0.55, 70.16, 74.67, 33.71, 24.83, -1.93, -1.18};
constexpr std::array<double, 7> LEFT_DANCE_DEG  = {2.44, 61.38, -159.32, 121.52, 173.04, 8.45, 0.31};
constexpr std::array<double, 7> LEFT_REST_DEG   = {-1.48, 82.05, -102.83, 42.32, 98.31, 9.35, 20.25};
constexpr std::array<double, 4> TORSO_ZERO_DEG  = {0.0, 0.0, 0.0, 0.0};

template <std::size_t N>
std::vector<double> deg2radVec(const std::array<double, N>& deg) {
    std::vector<double> rad(N);
    for (std::size_t i = 0; i < N; ++i) {
        rad[i] = deg[i] * PI / 180.0;
    }
    return rad;
}

// Wykrywanie API draga (jak w pos_zero.cpp): enableDrag/disableDrag s¹
// w cobotach, a np. PCB4Robot (tu³ów) ich nie ma.
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
        case rokae::PowerState::estop: return "estop (wciœniêty wy³¹cznik awaryjny)";
        case rokae::PowerState::gstop: return "gstop (otwarta os³ona bezpieczeñstwa)";
        default:                       return "nieznany";
    }
}

std::string operateModeName(rokae::OperateMode mode) {
    switch (mode) {
        case rokae::OperateMode::manual:    return "manual";
        case rokae::OperateMode::automatic: return "automatic";
        default: return "nieznany(" + std::to_string(static_cast<int>(mode)) + ")";
    }
}

std::string operationStateName(rokae::OperationState state) {
    switch (state) {
        case rokae::OperationState::idle:          return "idle (stoi)";
        case rokae::OperationState::moving:        return "moving (w ruchu)";
        case rokae::OperationState::drag:          return "drag (DRAG W£¥CZONY)";
        case rokae::OperationState::jog:           return "jog";
        case rokae::OperationState::jogging:       return "jogging (jog w ruchu)";
        case rokae::OperationState::rtControlling: return "rtControlling (sterowanie RT)";
        case rokae::OperationState::rlProgram:     return "rlProgram (program RL)";
        default:
            return "inny(" + std::to_string(static_cast<int>(state)) + ")";
    }
}

// Zasilanie silników - bez niego kontroler nie przyjmie ruchu.
// Próbuje wielokrotnie: tu³ów (PCB4) odrzuca czasem pierwsze setPowerState.
// Uwaga: stan "on" te¿ przechodzi przez pêtlê (odœwie¿enie + weryfikacja),
// bo kontroler potrafi³ zg³osiæ "on", a po kilkunastu sekundach byæ "off".
template <typename Robot>
bool ensureMotorPowerOn(Robot& robot, const std::string& moduleName, int attempts) {
    std::error_code ec;
    const auto state = robot.powerState(ec);

    if (!ec) {
        std::cout << "[Dance] " << moduleName
                  << " - zasilanie: " << powerStateName(state) << "\n";

        if (state == rokae::PowerState::estop) {
            std::cerr << "[Dance] " << moduleName
                      << " - E-STOP wciœniêty. Zwolnij wy³¹cznik awaryjny i powtórz.\n";
            return false;
        }

        if (state == rokae::PowerState::gstop) {
            std::cerr << "[Dance] " << moduleName
                      << " - otwarta os³ona bezpieczeñstwa. Zamknij j¹ i powtórz.\n";
            return false;
        }
    } else {
        std::cerr << "[Dance] " << moduleName
                  << " - powerState(): " << ec.message() << "\n";
    }

    for (int attempt = 1; attempt <= attempts; ++attempt) {
        std::cout << "[Dance] " << moduleName << " - w³¹czam zasilanie silników"
                  << " (próba " << attempt << "/" << attempts << ")...\n";

        if (attempt > 1) {
            // Mo¿e zalega alarm blokuj¹cy za³¹czenie - wyczyœæ przed ponowieniem.
            ec.clear();
            robot.recoverState(1, ec);  // kasowanie stanu awaryjnego (jak w drag_on)
            ec.clear();
            robot.clearServoAlarm(ec);
        }

        ec.clear();
        robot.setPowerState(true, ec);

        if (ec) {
            std::cerr << "[Dance] " << moduleName
                      << " - setPowerState(true): " << ec.message() << "\n";
        }

        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(POWER_ATTEMPT_WAIT_MS);
        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(POWER_POLL_MS));

            ec.clear();
            const auto current = robot.powerState(ec);
            if (!ec && current == rokae::PowerState::on) {
                std::cout << "[Dance] " << moduleName << " - zasilanie w³¹czone.\n";
                return true;
            }
        }
    }

    std::cerr << "[Dance] " << moduleName << " - zasilanie nie w³¹czy³o siê po "
              << attempts << " próbach.\n";
    return false;
}

// Jednolinijkowy zrzut stanu modu³u - diagnostyka, gdy tu³ów odmawia zasilania.
template <typename Robot>
void logModuleSnapshot(Robot& robot, const std::string& moduleName) {
    std::error_code ec;
    const auto mode = robot.operateMode(ec);
    const std::string modeStr = ec ? ("b³¹d: " + ec.message()) : operateModeName(mode);

    ec.clear();
    const auto power = robot.powerState(ec);
    const std::string powerStr = ec ? ("b³¹d: " + ec.message()) : powerStateName(power);

    ec.clear();
    const auto operation = robot.operationState(ec);
    const std::string operationStr =
        ec ? ("b³¹d: " + ec.message()) : operationStateName(operation);

    std::cout << "[Dance] " << moduleName << " - stan: tryb=" << modeStr
              << ", zasilanie=" << powerStr << ", praca=" << operationStr << "\n";
}

// Przy w³¹czonym dragu kontroler nie przyjmie komendy ruchu.
template <typename Robot>
void disableDragIfActive(Robot& robot, const std::string& moduleName) {
    if constexpr (!HasDisableDrag<Robot>::value) {
        return;  // np. tu³ów (PCB4Robot): brak API draga - nic nie robimy.
    } else {
        std::error_code ec;
        const auto state = robot.operationState(ec);

        if (ec || state != rokae::OperationState::drag) {
            return;
        }

        std::cout << "[Dance] " << moduleName
                  << " - drag by³ w³¹czony, wy³¹czam przed ruchem...\n";

        ec.clear();
        robot.disableDrag(ec);

        if (ec) {
            std::cerr << "[Dance] " << moduleName
                      << " - disableDrag: " << ec.message() << "\n";
        }
    }
}

template <typename Robot>
void abortMotion(Robot& robot, const std::string& moduleName) {
    std::error_code ec;
    robot.stop(ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - stop(): " << ec.message() << "\n";
    }

    ec.clear();
    robot.moveReset(ec);   // odrzuca niewykonane komendy

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - moveReset(): " << ec.message() << "\n";
    }
}

// Czeka na koniec ruchu: najpierw a¿ robot ruszy, potem a¿ stanie.
// shouldAbort() pozwala przerwaæ czekanie w po³owie (koniec piosenki).
// Celowo nie u¿ywamy queryEventInfo() - patrz komentarz w pos_zero.cpp.
template <typename Robot, typename JointArray, typename AbortPred>
bool waitUntilStable(Robot& robot, const std::string& moduleName,
                     const JointArray& startPosition, AbortPred shouldAbort) {
    const auto overallDeadline = std::chrono::steady_clock::now()
                               + std::chrono::milliseconds(MOVE_TIMEOUT_MS);
    const auto startDeadline   = std::chrono::steady_clock::now()
                               + std::chrono::milliseconds(MOVE_START_TIMEOUT_MS);

    bool moving = false;
    int stablePolls = 0;
    JointArray previous = startPosition;

    while (std::chrono::steady_clock::now() < overallDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(MOVE_POLL_MS));

        if (shouldAbort()) {
            return false;
        }

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
                std::cerr << "[Dance] " << moduleName
                          << " - robot nie ruszy³ w "
                          << (MOVE_START_TIMEOUT_MS / 1000)
                          << " s (kontroler odrzuci³ komendê?).\n";
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

    std::cerr << "[Dance] " << moduleName
              << " - przekroczono maksymalny czas ruchu ("
              << (MOVE_TIMEOUT_MS / 1000) << " s).\n";
    return false;
}

// Pod³¹czenie i przygotowanie rêki do ruchu. Kolejnoœæ kroków jak
// w wave.cpp (przygotujRece): recoverState -> alarmy -> tryb -> zasilanie.
bool prepareArm(rokae::xMateErProRobot& robot, const std::string& moduleName) {
    std::error_code ec;

    std::cout << "[Dance] " << moduleName << " - ³¹czenie z kontrolerem...\n";
    robot.connectToRobot(ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - b³¹d po³¹czenia: " << ec.message() << "\n";
        return false;
    }

    ec.clear();
    robot.recoverState(1, ec);  // zdejmuje blokady/stany ochronne kontrolera

    if (ec) {
        std::cout << "[Dance] " << moduleName
                  << " - recoverState(1): " << ec.message() << "\n";
    }

    ec.clear();
    robot.clearServoAlarm(ec);

    if (ec) {
        std::cout << "[Dance] " << moduleName
                  << " - clearServoAlarm: " << ec.message() << "\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    ec.clear();
    robot.setOperateMode(DANCE_OPERATE_MODE, ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - setOperateMode: " << ec.message() << "\n";
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ec.clear();
    robot.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - setMotionControlMode(NrtCommand): " << ec.message() << "\n";
        return false;
    }

    if (!ensureMotorPowerOn(robot, moduleName, POWER_ATTEMPTS)) {
        return false;
    }

    // Serwa œwie¿o po za³¹czeniu potrzebuj¹ chwili, zanim przyjm¹ ruch.
    std::this_thread::sleep_for(std::chrono::milliseconds(ARM_SETTLE_MS));

    disableDragIfActive(robot, moduleName);
    return true;
}

// Pod³¹czenie i przygotowanie tu³owia.
//
// Kontroler PCB4 odmawia zasilania w trybie automatic - próbujemy tam
// najpierw (2 próby), a potem w manual (2 próby): kombinacja manual +
// recoverState(1) nie by³a jeszcze testowana i mo¿e byæ t¹ w³aœciw¹.
//
// Uwagi z testów na ¿ywym robocie:
// - recoverState(2) i (3) ten firmware odrzuca ("argument invalid") - pomijamy,
// - setTeachPendantMode(false) koñczy siê "General failure" - nie wo³amy.
bool prepareTorso(rokae::PCB4Robot& robot, const std::string& moduleName) {
    std::error_code ec;

    std::cout << "[Dance] " << moduleName << " - ³¹czenie z kontrolerem...\n";
    robot.connectToRobot(ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - b³¹d po³¹czenia: " << ec.message() << "\n";
        return false;
    }

    ec.clear();
    robot.recoverState(1, ec);  // kasowanie stanu awaryjnego (????)

    if (ec) {
        std::cout << "[Dance] " << moduleName
                  << " - recoverState(1): " << ec.message() << "\n";
    }

    ec.clear();
    robot.clearServoAlarm(ec);

    if (ec) {
        std::cout << "[Dance] " << moduleName
                  << " - clearServoAlarm: " << ec.message() << "\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    logModuleSnapshot(robot, moduleName);

    ec.clear();
    robot.setOperateMode(rokae::OperateMode::automatic, ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - setOperateMode(automatic): " << ec.message() << "\n";
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ec.clear();
    robot.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - setMotionControlMode(NrtCommand): " << ec.message() << "\n";
        return false;
    }

    if (ensureMotorPowerOn(robot, moduleName, TORSO_ATTEMPTS_PER_MODE)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(TORSO_SETTLE_MS));
        return true;
    }

    // Drugi rzut: tryb manual + œwie¿y recover - tego jeszcze nie próbowaliœmy.
    std::cout << "[Dance] " << moduleName
              << " - prze³¹czam w tryb manual i próbujê dalej...\n";

    ec.clear();
    robot.setOperateMode(rokae::OperateMode::manual, ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - setOperateMode(manual): " << ec.message() << "\n";
        return false;
    }

    ec.clear();
    robot.recoverState(1, ec);
    ec.clear();
    robot.clearServoAlarm(ec);

    if (ensureMotorPowerOn(robot, moduleName, TORSO_ATTEMPTS_PER_MODE)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(TORSO_SETTLE_MS));
        return true;
    }

    logModuleSnapshot(robot, moduleName);
    return false;
}

// Przed ka¿dym ruchem (zw³aszcza powrotem po minutach bezczynnoœci):
// dopnij tryb i zasilanie. Naprawia, nie psuje.
template <typename Robot>
bool reEnsureMotionReady(Robot& robot, const std::string& moduleName) {
    std::error_code ec;
    robot.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - setMotionControlMode(NrtCommand): " << ec.message() << "\n";
        return false;
    }

    ec.clear();
    const auto state = robot.powerState(ec);

    if (!ec && state == rokae::PowerState::on) {
        return true;
    }

    std::cout << "[Dance] " << moduleName << " - dopinam zasilanie przed ruchem...\n";
    return ensureMotorPowerOn(robot, moduleName, POWER_ATTEMPTS);
}

// Jeden ruch absolutny w stawach. Cel w radianach.
template <typename Robot, typename AbortPred>
bool moveJointsTo(Robot& robot, const std::string& moduleName,
                  const std::vector<double>& targetRad, double speed,
                  AbortPred shouldAbort) {
    if (!reEnsureMotionReady(robot, moduleName)) {
        return false;
    }

    std::error_code ec;
    const auto startPosition = robot.jointPos(ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - jointPos(): " << ec.message() << "\n";
        return false;
    }

    if (startPosition.size() != targetRad.size()) {
        std::cerr << "[Dance] " << moduleName << " - niezgodna liczba stawów: robot ma "
                  << startPosition.size() << ", cel ma " << targetRad.size() << ".\n";
        return false;
    }

    // Ju¿ na miejscu (np. powrót do zera, gdy bujanie skoñczy³o siê na zerze):
    // nie wysy³aj komendy, bo "brak ruchu" wygl¹da³by jak odrzucenie i mielibyœmy
    // 20 s timeoutów. Tak samo robi pos_zero (ZERO_TOLERANCE_DEG).
    if (maxAbsDiffDeg(startPosition, targetRad) <= ARRIVED_TOLERANCE_DEG) {
        std::cout << "[Dance] " << moduleName << " - ju¿ na miejscu, ruch pominiêty.\n";
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
        std::cerr << "[Dance] " << moduleName
                  << " - moveAppend: " << ec.message() << "\n";
        return false;
    }

    ec.clear();
    robot.moveStart(ec);

    if (ec) {
        std::cerr << "[Dance] " << moduleName
                  << " - moveStart: " << ec.message() << "\n";
        return false;
    }

    if (!waitUntilStable(robot, moduleName, startPosition, shouldAbort)) {
        return false;  // przerwano (koniec piosenki) albo timeout - powód ju¿ wypisany
    }

    ec.clear();
    const auto finalPosition = robot.jointPos(ec);

    if (!ec) {
        const double deviation = maxAbsDiffDeg(finalPosition, targetRad);
        std::cout << "[Dance] " << moduleName << " - doszed³ (odchy³ka "
                  << std::fixed << std::setprecision(2) << deviation << "°): ";
        printJoints(finalPosition);
        std::cout << "\n";
    }

    return true;
}

// Ruch z automatyczn¹ powtórk¹: kontroler œwie¿o po starcie potrafi
// zignorowaæ pierwsz¹ komendê (przyj¹æ bez b³êdu i nie ruszyæ).
template <typename Robot, typename AbortPred>
bool moveJointsToRetry(Robot& robot, const std::string& moduleName,
                       const std::vector<double>& targetRad, double speed,
                       AbortPred shouldAbort) {
    for (int attempt = 1; attempt <= MOVE_ATTEMPTS; ++attempt) {
        if (shouldAbort()) {
            return false;
        }
        if (attempt > 1) {
            std::cout << "[Dance] " << moduleName << " - powtarzam ruch (próba "
                      << attempt << "/" << MOVE_ATTEMPTS << ")...\n";
        }
        if (moveJointsTo(robot, moduleName, targetRad, speed, shouldAbort)) {
            return true;
        }
        abortMotion(robot, moduleName);
        if (shouldAbort()) {
            return false;  // koniec piosenki, nie b³¹d - nie ma co powtarzaæ
        }
    }
    return false;
}

// Czy mpv (piosenka) jeszcze gra. Przy okazji zbiera zombie po zakoñczeniu.
bool mpvAlive(pid_t pid) {
    if (pid <= 0) {
        return false;
    }
    int status = 0;
    const pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == 0) {
        return true;   // nadal gra
    }
    if (result < 0 && errno == EINTR) {
        return true;   // przerwane - sprawdzimy nastêpnym razem
    }
    return false;      // zakoñczy³ (zebrany) albo inny b³¹d
}

// Sprz¹ta po mpv w tle (¿eby nie wisia³ jako zombie), nie blokuj¹c tañca.
void reapInBackground(pid_t pid) {
    std::thread([pid] {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
    }).detach();
}

// Jeden cykl bujania: przód, œrodek, ty³, œrodek, lewo, œrodek, prawo, œrodek.
std::vector<std::vector<double>> swayWaypoints() {
    const double amp = SWAY_AMP_DEG * PI / 180.0;
    std::vector<double> zero(4, 0.0), fwd(4, 0.0), back(4, 0.0);
    std::vector<double> left(4, 0.0), right(4, 0.0);
    fwd[SWAY_PITCH_JOINT] = amp;
    back[SWAY_PITCH_JOINT] = -amp;
    left[SWAY_ROLL_JOINT] = amp;
    right[SWAY_ROLL_JOINT] = -amp;
    return {fwd, zero, back, zero, left, zero, right, zero};
}

void danceDuringSong(pid_t mpvPid) {
    // Dwa tañce naraz walczy³yby o te same stawy - drugi gra tylko muzykê.
    static std::atomic<bool> danceActive{false};
    bool expected = false;
    if (!danceActive.compare_exchange_strong(expected, true)) {
        std::cout << "[Dance] Taniec ju¿ trwa - gram tylko muzykê.\n";
        reapInBackground(mpvPid);
        return;
    }
    struct FlagReset {
        std::atomic<bool>& flag;
        ~FlagReset() { flag.store(false); }
    };
    FlagReset reset{danceActive};

    std::cout << "[Dance] Zaczynam taniec do piosenki (mpv PID " << mpvPid << ").\n";

    auto lewa = std::make_unique<rokae::xMateErProRobot>(LEWA_IP, LOCAL_IP);
    auto prawa = std::make_unique<rokae::xMateErProRobot>(PRAWA_IP, LOCAL_IP);
    auto tulow = std::make_unique<rokae::PCB4Robot>(TULOW_IP);

    // Przygotowanie równoleg³e: rêce id¹ w pozê od razu, tu³ów szykuje siê
    // w tle i do³¹cza do bujania, jak wstanie. Jego próby zasilania (nieraz
    // kilkunastosekundowe) nie blokuj¹ r¹k.
    bool lewaOk = false, prawaOk = false;
    std::thread prepLeft([&] { lewaOk = prepareArm(*lewa, "Lewa rêka"); });
    std::thread prepRight([&] { prawaOk = prepareArm(*prawa, "Prawa rêka"); });
    std::atomic<bool> torsoPrepDone{false}, torsoOk{false};
    std::thread torsoPrepThread([&] {
        torsoOk = prepareTorso(*tulow, "Tu³ów");
        torsoPrepDone = true;
    });
    prepLeft.join();
    prepRight.join();

    if (!lewaOk && !prawaOk) {
        std::cout << "[Dance] Rêce nie odpowiadaj¹ - tañczy sam tu³ów (jeœli wstanie).\n";
    }

    auto neverAbort = [] { return false; };

    // Rêce w pozê taneczn¹ (równolegle, jak w wave.cpp).
    std::thread leftThread, rightThread;
    if (lewaOk) {
        leftThread = std::thread([&] {
            moveJointsToRetry(*lewa, "Lewa rêka", deg2radVec(LEFT_DANCE_DEG),
                              ARM_SPEED_MM_S, neverAbort);
        });
    }
    if (prawaOk) {
        rightThread = std::thread([&] {
            moveJointsToRetry(*prawa, "Prawa rêka", deg2radVec(RIGHT_DANCE_DEG),
                              ARM_SPEED_MM_S, neverAbort);
        });
    }
    if (leftThread.joinable()) {
        leftThread.join();
    }
    if (rightThread.joinable()) {
        rightThread.join();
    }

    // Bujanie tu³owiem do koñca piosenki. Tu³ów do³¹cza z opóŸnieniem,
    // jak tylko skoñczy siê przygotowywaæ; rêce trzymaj¹ pozê przez ca³y czas.
    const auto songOver = [mpvPid] { return !mpvAlive(mpvPid); };
    const std::vector<std::vector<double>> waypoints = swayWaypoints();
    bool torsoJoined = false;     // tu³ów wykona³ choæ jeden krok bujania
    bool torsoDead = false;       // pad³ w trakcie - rêce tañcz¹ dalej same
    bool torsoAnnounced = false;
    while (mpvAlive(mpvPid)) {
        if (!torsoPrepDone.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        if (!torsoAnnounced) {
            torsoAnnounced = true;
            if (torsoOk.load()) {
                std::cout << "[Dance] Tu³ów gotowy - bujam tu³owiem...\n";
            } else {
                std::cout << "[Dance] Tu³ów niedostêpny - tañcz¹ same rêce.\n";
            }
        }
        if (!torsoOk.load() || torsoDead) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        for (const auto& waypoint : waypoints) {
            if (!mpvAlive(mpvPid)) {
                break;
            }
            if (!moveJointsToRetry(*tulow, "Tu³ów", waypoint, SWAY_SPEED_MM_S, songOver)) {
                abortMotion(*tulow, "Tu³ów");  // zatrzymaj urwany krok w miejscu
                if (mpvAlive(mpvPid)) {
                    std::cerr << "[Dance] Tu³ów zg³osi³ b³¹d w trakcie bujania"
                              << " - tañcz¹ same rêce.\n";
                    torsoDead = true;
                }
                break;
            }
            torsoJoined = true;
        }
    }
    std::cout << "[Dance] Koniec piosenki - wracam do pozycji.\n";

    // Powrót: tu³ów do zera (tylko jeœli buja³), rêce do spoczynkowych.
    std::thread torsoBack, leftBack, rightBack;
    const bool returnTorso = torsoJoined && torsoOk.load() && !torsoDead;
    if (returnTorso) {
        torsoBack = std::thread([&] {
            moveJointsToRetry(*tulow, "Tu³ów", deg2radVec(TORSO_ZERO_DEG),
                              TORSO_SPEED_MM_S, neverAbort);
        });
    }
    if (lewaOk) {
        leftBack = std::thread([&] {
            moveJointsToRetry(*lewa, "Lewa rêka", deg2radVec(LEFT_REST_DEG),
                              ARM_SPEED_MM_S, neverAbort);
        });
    }
    if (prawaOk) {
        rightBack = std::thread([&] {
            moveJointsToRetry(*prawa, "Prawa rêka", deg2radVec(RIGHT_REST_DEG),
                              ARM_SPEED_MM_S, neverAbort);
        });
    }
    if (torsoBack.joinable()) {
        torsoBack.join();
    }
    if (leftBack.joinable()) {
        leftBack.join();
    }
    if (rightBack.joinable()) {
        rightBack.join();
    }
    torsoPrepThread.join();

    std::cout << "[Dance] Taniec zakoñczony.\n";
}

} // namespace

void youtube(const std::string& query) {
    if (query.empty()) {
        std::cerr << "[YouTube] Puste zapytanie.\n";
        return;
    }

    const char* apiKey = std::getenv("YOUTUBE_API_KEY");
    if (!apiKey || std::string(apiKey).empty()) {
        std::cerr << "[YouTube] Brak zmiennej œrodowiskowej YOUTUBE_API_KEY.\n";
        std::cerr << "[YouTube] Ustaw j¹ poleceniem: export YOUTUBE_API_KEY=...\n";
        return;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[YouTube] Nie mo¿na uruchomiæ CURL.\n";
        return;
    }

    const std::string encodedQuery = urlEncode(curl, query);
    if (encodedQuery.empty()) {
        curl_easy_cleanup(curl);
        std::cerr << "[YouTube] Nie mo¿na zakodowaæ zapytania.\n";
        return;
    }

    const std::string url = std::string(YOUTUBE_API_URL) +
        "?part=snippet&q=" + encodedQuery +
        "&type=video&maxResults=1&order=relevance&key=" + apiKey;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "HeliosAI/1.0");

    const CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        std::cerr << "[YouTube] B³¹d po³¹czenia: "
                  << curl_easy_strerror(result) << "\n";
        return;
    }

    try {
        const json data = json::parse(response);

        if (data.contains("error")) {
            std::cerr << "[YouTube] API zwróci³o b³¹d: "
                      << data["error"].dump() << "\n";
            return;
        }

        if (!data.contains("items") || data["items"].empty()) {
            std::cout << "[YouTube] Nie znaleziono wyniku dla: "
                      << query << "\n";
            return;
        }

        const auto& item = data["items"][0];
        const std::string videoId = item["id"]["videoId"];
        const std::string title = item["snippet"]["title"];
        const std::string channel = item["snippet"]["channelTitle"];
        const std::string videoUrl =
            "https://www.youtube.com/watch?v=" + videoId;

        std::cout << "[YouTube] Znaleziono: " << title
                  << " — " << channel << "\n";
        std::cout << "[YouTube] Odtwarzam: " << videoUrl << "\n";

        const pid_t mpvPid = playWithMpv(videoUrl);
        if (mpvPid < 0) {
            return;  // b³¹d ju¿ wypisany w playWithMpv
        }
        danceDuringSong(mpvPid);
    } catch (const std::exception& error) {
        std::cerr << "[YouTube] B³¹d odpowiedzi JSON: "
                  << error.what() << "\n";
    }
}

} // namespace commands
