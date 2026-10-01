#include <iostream>
#include <chrono>
#include <thread>
#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <memory>
#include "rokae/robot.h"
#include "rokae/data_types.h"

// Struktura przechowująca pozycję Torsu (4 osie) oraz Głowy (2 osie zewnętrzne)
struct TorsoPose {
    std::array<double, 4> joints; // j1-j4 torsu (stopnie)
    std::array<double, 2> head;   // pan/tilt głowy (stopnie)
};

// ====================================================================
// FUNKCJE POMOCNICZE
// ====================================================================

void wyczyscBufor() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::array<double, 7> deg2rad7(const std::array<double, 7>& deg) {
    std::array<double, 7> rad;
    for (size_t i = 0; i < 7; i++) rad[i] = deg[i] * M_PI / 180.0;
    return rad;
}

std::string trybPracyToString(rokae::OperateMode mode) {
    switch (mode) {
        case rokae::OperateMode::manual: return "MANUAL (Ręczny/Kluczyk w lewo)";
        case rokae::OperateMode::automatic: return "AUTOMATIC (Automatyczny/Kluczyk w prawo)";
        default: return "UNKNOWN";
    }
}

// ====================================================================
// PUNKTY TRAJEKTORII (ze stopni Robot Assist)
// ====================================================================

// --- LEWA RĘKA (192.168.71.161) - 7 osi ---
const std::vector<std::array<double, 7>> LEWA_RECE_JOINT_DEG = {
    {-18.53700, 87.43741, -112.00451, 32.77294, 95.16150, 5.14149, 31.47747},
    {-62.11220, 81.76084, -50.00089, 133.80490, 115.12505, 23.40126, 17.11380},
    {-48.75222, 86.42663, -99.68442, 112.55556, 104.40226, 26.36305, 34.79392},
    {-48.75228, 86.42666, -99.68436, 112.55553, 104.40233, 26.36299, 34.79393},
    {-48.75226, 86.42666, -99.68436, 112.55554, 104.40232, 26.36301, 34.79394},
    {-48.75228, 86.42667, -99.68435, 112.55553, 104.40232, 26.36299, 34.79394},
};

// --- PRAWA RĘKA (192.168.71.160) - 7 osi ---
const std::vector<std::array<double, 7>> PRAWA_RECE_JOINT_DEG = {
    {31.57353, 92.47008, 95.09949, 68.55369, -3.01022, 15.08918, 4.09287},
    {-18.96046, 0.15258, 65.67573, 33.63637, 14.49540, 26.17101, 0.92587},
    {5.37921, 72.00226, 12.36546, 87.51457, -4.34218, 15.21070, 3.40150},
    {16.84373, 82.65137, 27.08224, 72.53241, 2.71246, 15.19909, 1.50636},
    {54.64069, 103.64353, 59.53290, 32.41992, -4.19701, 54.60930, 3.54601},
    {38.24495, 96.13247, 32.27489, 36.47362, -30.50856, 55.50552, 23.30014},
    {44.01989, 92.42045, 27.49159, 38.28033, 11.03788, 15.26256, 9.97995},
};

// --- TUŁÓW TaiHu (192.168.71.254) - 4 osie główne + 2 osie głowy ---
const std::vector<TorsoPose> TULOW_TRAJEKTORIA = {
    {{0.09892, 1.48937, -6.36630, -0.77294}, {10.98284, -0.42329}},
    {{-2.71312, -1.72648, -10.79361, 4.32171}, {37.18333, 6.15642}},
    {{-2.71194, -5.38175, -21.71761, -12.02508}, {-13.63789, 15.81666}},
    {{-3.46750, 5.38588, -2.72656, -3.60056}, {4.14445, -2.90474}}
};

// Pozycje bezpieczne
const std::array<double, 7> POZYCJA_ZERO_7 = {0, 0, 0, 0, 0, 0, 0};
const double PREDKOSC = 0.4;

// ====================================================================
// FUNKCJE: Przygotowanie ramion i tułowia
// ====================================================================
bool przygotujRece(rokae::xMateErProRobot& robot, const std::string& nazwa) {
    std::error_code ec;
    std::cout << "\n[" << nazwa << "] Przygotowanie do pracy...\n";
    
    robot.recoverState(1, ec);
    robot.clearServoAlarm(ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    robot.setOperateMode(rokae::OperateMode::automatic, ec);
    robot.setMotionControlMode(rokae::MotionControlMode::RtCommand, ec);
    robot.setPowerState(true, ec);
    
    if (ec) {
        std::cerr << "  [" << nazwa << "] BŁĄD zasilania: " << ec.message() << "\n";
        return false;
    }
    std::cout << "  [" << nazwa << "] Zasilanie WŁĄCZONE.\n";
    return true;
}

bool przygotujTulow(rokae::PCB4Robot& robot, const std::string& nazwa) {
    std::error_code ec;
    std::cout << "\n[" << nazwa << "] Przygotowanie do pracy...\n";
    
    // Odczyt aktualnego trybu fizycznego stacyjki szafy torsu
    rokae::OperateMode fizyczny_tryb = robot.operateMode(ec);
    std::cout << "  [" << nazwa << "] Fizyczna pozycja kluczyka na szafie: " 
              << trybPracyToString(fizyczny_tryb) << "\n";

    // 1. Rozszerzona procedura odblokowania obwodów bezpieczeństwa szafy przemysłowej
    std::cout << "  [" << nazwa << "] Resetowanie obwodów bezpieczeństwa (E-Stop + Safeguard + Collision)...\n";
    robot.recoverState(1, ec); // E-Stop Recovery
    robot.recoverState(2, ec); // Safeguard (Safety Gate) Recovery
    robot.recoverState(3, ec); // Collision Recovery
    
    // 2. Czyszczenie alarmów serw
    robot.clearServoAlarm(ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 3. Wymuszenie trybu automatycznego (wymagany do ruchu z PC)
    robot.setOperateMode(rokae::OperateMode::automatic, ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 4. Załączenie styczników Power ON
    std::cout << "  [" << nazwa << "] Załączanie styczników wysokiego napięcia... ";
    std::cout.flush();
    robot.setPowerState(true, ec);
    
    if (ec) {
        std::cerr << "BŁĄD: " << ec.message() << "\n";
        std::cerr << "  -> Wskazówka: Jeśli fizyczny kluczyk na szafie Torsu jest w trybie MANUAL,\n";
        std::cerr << "     przekręć go w prawo na AUTOMATIC i spróbuj ponownie.\n";
        return false;
    }
    
    // Bezpieczny czas na naładowanie szyny zasilającej DC
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "ZASILONE (styczniki zamknięte pomyślnie).\n";
    return true;
}

// ====================================================================
// FUNKCJA POMOCNICZA: Czekanie na zakończenie ruchu torsu
// ====================================================================
void czekajNaTulow(rokae::PCB4Robot& robot, const std::string& nazwa) {
    std::error_code ec;
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    bool w_ruchu = true;
    auto start_time = std::chrono::steady_clock::now();
    
    while (w_ruchu && !ec) {
        rokae::OperationState stan = robot.operationState(ec);
        if (stan != rokae::OperationState::moving) {
            w_ruchu = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(15)) {
            std::cerr << "  [" << nazwa << "] OSTRZEŻENIE: Timeout ruchu!\n";
            break;
        }
    }
}

// ====================================================================
// FUNKCJE: Wykonanie trajektorii
// ====================================================================
void wykonajTrajektorieRece(rokae::xMateErProRobot& robot, 
                             const std::vector<std::array<double, 7>>& punkty_deg,
                             const std::string& nazwa) 
{
    if (!przygotujRece(robot, nazwa)) return;

    std::error_code ec;
    auto rtCon = robot.getRtMotionController().lock();
    if (!rtCon) {
        std::cerr << "[" << nazwa << "] Brak kontrolera RT!\n";
        return;
    }
    
    std::cout << "\n[" << nazwa << "] Trajektoria (" << punkty_deg.size() 
              << " punktów, prędkość " << (int)(PREDKOSC * 100) << "%)\n";
    
    for (size_t i = 0; i < punkty_deg.size(); i++) {
        std::cout << "  [" << nazwa << "] Punkt " << (i + 1) 
                  << "/" << punkty_deg.size() << "...";
        std::cout.flush();
        
        auto target_rad = deg2rad7(punkty_deg[i]);
        auto current_pos = robot.jointPos(ec);
        rtCon->MoveJ(PREDKOSC, current_pos, target_rad);
        
        std::cout << " OK\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    std::cout << "[" << nazwa << "] Trajektoria zakończona.\n";
}

void wykonajTrajektorieTulow(rokae::PCB4Robot& robot,
                              const std::vector<TorsoPose>& punkty,
                              const std::string& nazwa)
{
    if (!przygotujTulow(robot, nazwa)) return;

    std::error_code ec;
    std::cout << "\n[" << nazwa << "] Rozpoczynanie trajektorii (" << punkty.size() 
              << " punktów, prędkość " << (int)(PREDKOSC * 100) << "%)...\n";
    
    robot.moveReset(ec);

    for (size_t i = 0; i < punkty.size(); i++) {
        std::cout << "  [" << nazwa << "] Punkt " << (i + 1) << "/" << punkty.size() << "...";
        std::cout.flush();

        rokae::JointPosition target_pos;
        target_pos.joints.resize(4);
        target_pos.external.resize(2);

        for (size_t j = 0; j < 4; j++) {
            target_pos.joints[j] = punkty[i].joints[j] * M_PI / 180.0;
        }
        for (size_t j = 0; j < 2; j++) {
            target_pos.external[j] = punkty[i].head[j] * M_PI / 180.0;
        }

        rokae::MoveAbsJCommand cmd(target_pos, PREDKOSC * 100.0);

        std::string cmdID;
        robot.moveAppend(cmd, cmdID, ec);
        if (ec) {
            std::cerr << " BŁĄD moveAppend: " << ec.message() << "\n";
            return;
        }

        robot.moveStart(ec);
        if (ec) {
            std::cerr << " BŁĄD moveStart: " << ec.message() << "\n";
            return;
        }

        czekajNaTulow(robot, nazwa);

        if (ec) {
            std::cerr << " BŁĄD podczas ruchu torsu: " << ec.message() << "\n";
            return;
        }
        std::cout << " OK\n";
    }
    std::cout << "[" << nazwa << "] Trajektoria Torsu + Głowy zakończona.\n";
}

// ====================================================================
// FUNKCJE: Powrót do pozycji zerowej
// ====================================================================
void powrotDoZeraRece(rokae::xMateErProRobot& robot, const std::string& nazwa) {
    if (!przygotujRece(robot, nazwa)) return;

    std::error_code ec;
    auto rtCon = robot.getRtMotionController().lock();
    if (!rtCon) return;
    std::cout << "\n[" << nazwa << "] Powrót do pozycji zerowej...\n";
    rtCon->MoveJ(PREDKOSC, robot.jointPos(ec), POZYCJA_ZERO_7);
    std::cout << "[" << nazwa << "] W pozycji zerowej.\n";
}

void powrotDoZeraTulow(rokae::PCB4Robot& robot, const std::string& nazwa) {
    if (!przygotujTulow(robot, nazwa)) return;

    std::error_code ec;
    std::cout << "\n[" << nazwa << "] Powrót do pozycji zerowej...\n";

    robot.moveReset(ec);

    rokae::JointPosition target_pos;
    target_pos.joints = {0, 0, 0, 0};
    target_pos.external = {0, 0};

    rokae::MoveAbsJCommand cmd(target_pos, PREDKOSC * 100.0);

    std::string cmdID;
    robot.moveAppend(cmd, cmdID, ec);
    if (ec) return;

    robot.moveStart(ec);
    if (ec) return;

    czekajNaTulow(robot, nazwa);
    std::cout << "[" << nazwa << "] W pozycji zerowej.\n";
}

// ====================================================================
// MAIN
// ====================================================================
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    const std::string LEWA_IP  = "192.168.71.161";
    const std::string PRAWA_IP = "192.168.71.160";
    const std::string TULOW_IP = "192.168.71.254";
    const std::string LOCAL_IP = "192.168.71.51";
    
    std::cout << "===========================================================\n";
    std::cout << "   Rokae Helios – Program Prezentacji (Targi)\n";
    std::cout << "   Prędkość globalna: " << (int)(PREDKOSC * 100) << "%\n";
    std::cout << "===========================================================\n";
    std::cout << "Lewa ręka  : " << LEWA_IP  << "  (xMateErProRobot, 7 osi)\n";
    std::cout << "Prawa ręka : " << PRAWA_IP << "  (xMateErProRobot, 7 osi)\n";
    std::cout << "Tułów      : " << TULOW_IP << "  (PCB4Robot, 4 osie + 2 osie głowy)\n";
    std::cout << "Komputer   : " << LOCAL_IP << "\n";
    
    std::unique_ptr<rokae::xMateErProRobot> lewa, prawa;
    std::unique_ptr<rokae::PCB4Robot> tulow;
    
    try {
        // === POŁĄCZENIE ===
        std::cout << "\n[INIT] Łączenie z LEWĄ ręką...\n";
        lewa = std::make_unique<rokae::xMateErProRobot>(LEWA_IP, LOCAL_IP);
        std::error_code ec;
        lewa->connectToRobot(ec);
        if (ec) throw std::runtime_error("Lewa ręka: " + ec.message());
        std::cout << "[INIT] Lewa ręka POŁĄCZONA.\n";
        
        std::cout << "\n[INIT] Łączenie z PRAWĄ ręką...\n";
        prawa = std::make_unique<rokae::xMateErProRobot>(PRAWA_IP, LOCAL_IP);
        prawa->connectToRobot(ec);
        if (ec) throw std::runtime_error("Prawa ręka: " + ec.message());
        std::cout << "[INIT] Prawa ręka POŁĄCZONA.\n";
        
        std::cout << "\n[INIT] Łączenie z TUŁOWIEM (PCB4Robot)...\n";
        tulow = std::make_unique<rokae::PCB4Robot>(TULOW_IP);
        tulow->connectToRobot(ec);
        if (ec) {
            std::cerr << "[INIT] UWAGA: Tułów nie połączony: " << ec.message() << "\n";
            std::cerr << "       Będziesz mógł sterować tylko ramionami.\n";
            tulow.reset();
        } else {
            std::cout << "[INIT] Tułów POŁĄCZONY.\n";
        }
        
        // === MENU ===
        bool uruchomiony = true;
        while (uruchomiony) {
            std::cout << "\n============ MENU PREZENTACJI ============\n";
            std::cout << "[1] Przygotuj WSZYSTKO (Power ON + reset)\n";
            std::cout << "[2] Powrót WSZYSTKICH do pozycji ZEROWEJ\n";
            std::cout << "----- Pojedyncze trajektorie -----\n";
            std::cout << "[3] Trajektoria LEWĄ ręką (6 pkt)\n";
            std::cout << "[4] Trajektoria PRAWĄ ręką (7 pkt)\n";
            std::cout << "[5] Trajektoria TUŁOWIEM + GŁOWĄ (4 pkt)\n";
            std::cout << "----- Równoczesne -----\n";
            std::cout << "[6] OBIE RĘCE naraz\n";
            std::cout << "[7] WSZYSTKO naraz (ręce + tułów + głowa)\n";
            std::cout << "----- Prezentacja -----\n";
            std::cout << "[8] PEŁNA PREZENTACJA (zero->trajektorie->zero)\n";
            std::cout << "----- Bezpieczeństwo -----\n";
            std::cout << "[9] Awaryjny POWER OFF (wszystko)\n";
            std::cout << "[0] Wyjście\n";
            std::cout << "==========================================\n";
            std::cout << "Wybierz: ";
            
            int w;
            if (!(std::cin >> w)) { wyczyscBufor(); continue; }
            
            switch (w) {
                case 1: {
                    przygotujRece(*lewa, "LEWA");
                    przygotujRece(*prawa, "PRAWA");
                    if (tulow) przygotujTulow(*tulow, "TULOW");
                    break;
                }
                case 2: {
                    std::thread tL([&]{ powrotDoZeraRece(*lewa,  "LEWA"); });
                    std::thread tP([&]{ powrotDoZeraRece(*prawa, "PRAWA"); });
                    std::thread tT;
                    if (tulow) tT = std::thread([&]{ powrotDoZeraTulow(*tulow, "TULOW"); });
                    tL.join(); tP.join();
                    if (tT.joinable()) tT.join();
                    break;
                }
                case 3: {
                    wykonajTrajektorieRece(*lewa, LEWA_RECE_JOINT_DEG, "LEWA");
                    break;
                }
                case 4: {
                    wykonajTrajektorieRece(*prawa, PRAWA_RECE_JOINT_DEG, "PRAWA");
                    break;
                }
                case 5: {
                    if (!tulow) {
                        std::cout << "Tułów nie jest połączony!\n";
                        break;
                    }
                    wykonajTrajektorieTulow(*tulow, TULOW_TRAJEKTORIA, "TULOW");
                    break;
                }
                case 6: {
                    std::cout << "\n>>> OBIE RĘCE NARAZ <<<\n";
                    std::thread tL([&]{ wykonajTrajektorieRece(*lewa,  LEWA_RECE_JOINT_DEG,  "LEWA"); });
                    std::thread tP([&]{ wykonajTrajektorieRece(*prawa, PRAWA_RECE_JOINT_DEG, "PRAWA"); });
                    tL.join(); tP.join();
                    std::cout << ">>> Obie ręce zakończyły <<<\n";
                    break;
                }
                case 7: {
                    std::cout << "\n>>> WSZYSTKO NARAZ <<<\n";
                    std::thread tL([&]{ wykonajTrajektorieRece(*lewa,  LEWA_RECE_JOINT_DEG,  "LEWA"); });
                    std::thread tP([&]{ wykonajTrajektorieRece(*prawa, PRAWA_RECE_JOINT_DEG, "PRAWA"); });
                    std::thread tT;
                    if (tulow) tT = std::thread([&]{ wykonajTrajektorieTulow(*tulow, TULOW_TRAJEKTORIA, "TULOW"); });
                    tL.join(); tP.join();
                    if (tT.joinable()) tT.join();
                    std::cout << ">>> Wszystko zakończone <<<\n";
                    break;
                }
                case 8: {
                    std::cout << "\n>>> PEŁNA PREZENTACJA <<<\n";
                    std::cout << "UWAGA: Rozpocznie się ruch wszystkich elementów robota!\n";
                    std::cout << "Naciśnij Enter aby kontynuować...";
                    wyczyscBufor(); std::cin.ignore();
                    
                    // Krok 1: Przygotowanie
                    przygotujRece(*lewa, "LEWA");
                    przygotujRece(*prawa, "PRAWA");
                    if (tulow) przygotujTulow(*tulow, "TULOW");
                    
                    std::cout << "\n--- Powrót do pozycji zerowych ---\n";
                    {
                        std::thread t1([&]{ powrotDoZeraRece(*lewa,  "LEWA"); });
                        std::thread t2([&]{ powrotDoZeraRece(*prawa, "PRAWA"); });
                        std::thread t3;
                        if (tulow) t3 = std::thread([&]{ powrotDoZeraTulow(*tulow, "TULOW"); });
                        t1.join(); t2.join();
                        if (t3.joinable()) t3.join();
                    }
                    
                    std::cout << "\n--- Wykonywanie trajektorii ---\n";
                    {
                        std::thread t1([&]{ wykonajTrajektorieRece(*lewa,  LEWA_RECE_JOINT_DEG,  "LEWA"); });
                        std::thread t2([&]{ wykonajTrajektorieRece(*prawa, PRAWA_RECE_JOINT_DEG, "PRAWA"); });
                        std::thread t3;
                        if (tulow) t3 = std::thread([&]{ wykonajTrajektorieTulow(*tulow, TULOW_TRAJEKTORIA, "TULOW"); });
                        t1.join(); t2.join();
                        if (t3.joinable()) t3.join();
                    }
                    
                    std::cout << "\n--- Finalny powrót do zera ---\n";
                    {
                        std::thread t1([&]{ powrotDoZeraRece(*lewa,  "LEWA"); });
                        std::thread t2([&]{ powrotDoZeraRece(*prawa, "PRAWA"); });
                        std::thread t3;
                        if (tulow) t3 = std::thread([&]{ powrotDoZeraTulow(*tulow, "TULOW"); });
                        t1.join(); t2.join();
                        if (t3.joinable()) t3.join();
                    }
                    
                    std::cout << "\n>>> PREZENTACJA ZAKOŃCZONA <<<\n";
                    break;
                }
                case 9: {
                    std::cout << "\n[STOP] Wyłączanie wszystkiego...\n";
                    lewa->setPowerState(false, ec);
                    prawa->setPowerState(false, ec);
                    if (tulow) tulow->setPowerState(false, ec);
                    std::cout << "[STOP] Zasilanie WYŁĄCZONE.\n";
                    break;
                }
                case 0: {
                    std::cout << "\nZamykanie – wyłączanie silników...\n";
                    lewa->setPowerState(false, ec);
                    prawa->setPowerState(false, ec);
                    if (tulow) tulow->setPowerState(false, ec);
                    uruchomiony = false;
                    break;
                }
                default:
                    std::cout << "Nieznana opcja!\n";
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\nBŁĄD KRYTYCZNY: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Program zakończony pomyślnie.\n";
    return 0;
}
