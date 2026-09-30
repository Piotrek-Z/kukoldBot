#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <atomic>
#include <iomanip>
#include <limits>
#include "rokae/robot.h"

// Funkcja pomocnicza do czyszczenia bufora wejściowego
void wyczyscBufor() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cout << "Użycie: " << argv[0] << " <IP_robota> [IP_lokalne]" << std::endl;
        std::cout << "  Prawa ręka Heliosa: 192.168.71.160" << std::endl;
        std::cout << "  Lewa ręka Heliosa : 192.168.71.161" << std::endl;
        return 1;
    }
    
    std::string robot_ip = argv[1];
    std::string local_ip = (argc == 3) ? argv[2] : "";
    
    std::cout << "===========================================================\n";
    std::cout << " Rokae Helios – Interaktywny Panel Sterowania (AR5, 7 DoF)\n";
    std::cout << "===========================================================\n";
    std::cout << "IP robota:  " << robot_ip << "\n";
    std::cout << "IP lokalne: " << (local_ip.empty() ? "(auto)" : local_ip) << "\n";
    
    try {
        // Inicjalizacja i połączenie z robotem
        std::cout << "Łączenie z robotem... ";
        rokae::xMateErProRobot robot(robot_ip, local_ip);
        std::error_code ec;
        robot.connectToRobot(ec);
        if (ec) throw std::runtime_error("Połączenie nieudane: " + ec.message());
        std::cout << "POŁĄCZONO POMYŚLNIE!\n\n";

        bool program_uruchomiony = true;
        bool zasilanie_on = false;
        bool drag_mode_on = false;

        while (program_uruchomiony) {
            std::cout << "\n=================== MENU STEROWANIA ===================\n";
            std::cout << "[1] Włącz zasilanie silników (Power ON) " << (zasilanie_on ? "[AKTYWNE]" : "") << "\n";
            std::cout << "[2] Wyłącz zasilanie silników (Power OFF)\n";
            std::cout << "[3] Ruch do pozycji pionowej / zerowej (MoveJ)\n";
            std::cout << "[4] URUCHOM DRAG MODE (Swobodne przeciąganie ręką) " << (drag_mode_on ? "[URUCHOMIONE!]" : "") << "\n";
            std::cout << "[5] Zablokuj ramię (Wyłącz Drag Mode)\n";
            std::cout << "[6] Uruchom szybki test sieci (10 sekund, 1000 Hz)\n";
            std::cout << "[7] RESETUJ BŁĘDY / LIMITY / E-STOP (Odblokuj ramię)\n";
            std::cout << "[0] Bezpieczne wyłączenie i wyjście z programu\n";
            std::cout << "=======================================================\n";
            std::cout << "Wybierz opcję: ";
            
            int wybor;
            if (!(std::cin >> wybor)) {
                std::cout << "Nieprawidłowy znak! Wybierz liczbę.\n";
                wyczyscBufor();
                continue;
            }

            switch (wybor) {
                case 1: { // Power ON
                    std::cout << "Włączanie zasilania silników...\n";
                    robot.setOperateMode(rokae::OperateMode::automatic, ec);
                    robot.setMotionControlMode(rokae::MotionControlMode::RtCommand, ec);
                    robot.setPowerState(true, ec);
                    if (ec) {
                        std::cout << "Błąd włączania zasilania: " << ec.message() << "\n";
                    } else {
                        std::cout << "Zasilanie WŁĄCZONE. Hamulce zwolnione.\n";
                        zasilanie_on = true;
                    }
                    break;
                }
                case 2: { // Power OFF
                    std::cout << "Wyłączanie zasilania silników...\n";
                    robot.setPowerState(false, ec);
                    if (ec) {
                        std::cout << "Błąd wyłączania zasilania: " << ec.message() << "\n";
                    } else {
                        std::cout << "Zasilanie WYŁĄCZONE. Hamulce zablokowane.\n";
                        zasilanie_on = false;
                        drag_mode_on = false;
                    }
                    break;
                }
                case 3: { // Ruch do zera
                    if (!zasilanie_on) {
                        std::cout << "Błąd: Najpierw włącz zasilanie silników [Opcja 1]!\n";
                        break;
                    }
                    if (drag_mode_on) {
                        std::cout << "Błąd: Wyłącz najpierw Drag Mode [Opcja 5]!\n";
                        break;
                    }
                    std::cout << "UWAGA: Ramię rozpocznie ruch! Upewnij się, że jest bezpiecznie.\n";
                    std::cout << "Naciśnij Enter, aby potwierdzić start ruchu...";
                    wyczyscBufor();
                    std::cin.ignore();

                    std::array<double, 7> zero_position = {0, 0, 0, 0, 0, 0, 0};
                    auto rtCon = robot.getRtMotionController().lock();
                    if (rtCon) {
                        std::cout << "Ruch w toku...\n";
                        rtCon->MoveJ(0.2, robot.jointPos(ec), zero_position);
                        std::cout << "Ruch zakończony.\n";
                    } else {
                        std::cout << "Błąd: Nie udało się uzyskać kontrolera RT.\n";
                    }
                    break;
                }
                case 4: { // Włącz Drag Mode
                    std::cout << "Uruchamianie trybu przeciągania (Drag Mode)...\n";
                    
                    // Do drag mode zaleca się tryb manualny
                    robot.setOperateMode(rokae::OperateMode::manual, ec);
                    robot.setPowerState(true, ec); // serwa muszą być zasilone, żeby działała kompensacja
                    
                    // Uruchomienie przeciągania w SDK Rokae (enableDrag)
                    robot.enableDrag(rokae::DragParameter::Space::jointSpace, 
                                     rokae::DragParameter::Type::freely, 
                                     ec, 
                                     true);
                    
                    if (ec) {
                        std::cout << "Błąd uruchamiania Drag Mode: " << ec.message() << "\n";
                    } else {
                        std::cout << "====================================================\n";
                        std::cout << " DRAG MODE AKTYWNY! Możesz teraz bezpiecznie\n";
                        std::cout << " przesuwać ramię ręką. Kompensacja grawitacji działa.\n";
                        std::cout << "====================================================\n";
                        drag_mode_on = true;
                        zasilanie_on = true;
                    }
                    break;
                }
                case 5: { // Wyłącz Drag Mode
                    std::cout << "Wyłączanie trybu przeciągania...\n";
                    robot.disableDrag(ec);
                    if (ec) {
                        std::cout << "Błąd wyłączania Drag Mode: " << ec.message() << "\n";
                    } else {
                        std::cout << "Drag Mode WYŁĄCZONY. Ramię zostało zablokowane w obecnej pozycji.\n";
                        drag_mode_on = false;
                    }
                    break;
                }
                case 6: { // Test sieciowy (10 sekund)
                    std::cout << "Uruchamianie szybkiego testu sieci (10 sekund)...\n";
                    if (!zasilanie_on) {
                        std::cout << "Włączam zasilanie na potrzeby testu...\n";
                        robot.setOperateMode(rokae::OperateMode::automatic, ec);
                        robot.setMotionControlMode(rokae::MotionControlMode::RtCommand, ec);
                        robot.setPowerState(true, ec);
                        zasilanie_on = true;
                    }

                    robot.startReceiveRobotState(std::chrono::milliseconds(1), {
                        rokae::RtSupportedFields::jointPos_m
                    });

                    const int TEST_DURATION_MS = 10000;
                    const int TARGET_FREQ = 1000;
                    const int CYCLE_US = 1000000 / TARGET_FREQ;

                    std::atomic<int> cycle_count{0};
                    std::atomic<int> success_count{0};
                    std::atomic<int> failed_count{0};
                    std::atomic<double> min_delay{10000.0};
                    std::atomic<double> max_delay{0.0};
                    std::atomic<double> total_delay{0.0};

                    auto test_start = std::chrono::steady_clock::now();
                    auto test_end = test_start + std::chrono::milliseconds(TEST_DURATION_MS);

                    while (std::chrono::steady_clock::now() < test_end) {
                        auto cycle_start = std::chrono::high_resolution_clock::now();
                        try {
                            std::array<double, 7> pos{};
                            auto read_start = std::chrono::high_resolution_clock::now();
                            int ret_ = robot.updateRobotState(std::chrono::milliseconds(1));
                            int ret = robot.getStateData(rokae::RtSupportedFields::jointPos_m, pos);
                            auto read_end = std::chrono::high_resolution_clock::now();

                            double delay_ms = std::chrono::duration<double, std::milli>(read_end - read_start).count();
                            bool success = ((ret_ > 0) && (ret == 0));

                            cycle_count++;
                            if (success) {
                                success_count++;
                                if (delay_ms < min_delay) min_delay = delay_ms;
                                if (delay_ms > max_delay) max_delay = delay_ms;
                                total_delay.store(total_delay.load() + delay_ms);
                            } else {
                                failed_count++;
                            }
                        } catch (...) {
                            failed_count++;
                            cycle_count++;
                        }

                        auto cycle_end = std::chrono::high_resolution_clock::now();
                        auto cycle_time = std::chrono::duration_cast<std::chrono::microseconds>(cycle_end - cycle_start);
                        int wait_us = CYCLE_US - cycle_time.count();
                        if (wait_us > 0) {
                            std::this_thread::sleep_for(std::chrono::microseconds(wait_us));
                        }
                    }

                    int total = cycle_count;
                    int success = success_count;
                    double avg_delay = (success > 0) ? total_delay / success : 0.0;

                    std::cout << "\n================ WYNIKI TESTU ================\n";
                    std::cout << "Skuteczność:           " << ((double)success / total * 100) << "%\n";
                    std::cout << "Utracone pakiety:      " << failed_count << "\n";
                    std::cout << "Średnie opóźnienie:    " << avg_delay << "ms\n";
                    std::cout << "Maksymalne opóźnienie: " << max_delay << "ms\n";
                    std::cout << "==============================================\n";
                    break;
                }
                case 7: { // RESET BŁĘDÓW / LIMITÓW / E-STOP
                    std::cout << "Uruchamianie procedury odblokowania ramienia...\n";
                    
                    // 1. Wyjście ze stanu blokady E-Stop (item = 1 oznacza急停恢复)
                    std::cout << "-> Próba resetu zatrzasku bezpieczeństwa E-Stop...\n";
                    robot.recoverState(1, ec);
                    if (ec) {
                        std::cout << "Informacja: E-Stop nie wymagał resetu lub fizyczny przycisk jest wciąż wciśnięty: " << ec.message() << "\n";
                    } else {
                        std::cout << "-> Blokada E-Stop została pomyślnie usunięta z kontrolera.\n";
                    }

                    // 2. Wyczyść alarmy serwonapędów w kontrolerze xCore (np. po przekroczeniu limitu osi)
                    robot.clearServoAlarm(ec);
                    if (ec) {
                        std::cout << "Błąd podczas czyszczenia rejestru alarmów: " << ec.message() << "\n";
                        break;
                    }
                    std::cout << "-> Alarmy serwonapędów zostały zresetowane.\n";

                    // 3. Krótka pauza (500ms) na rozbrojenie fizycznych przekaźników szafy
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                    // 4. Przełącz tryb z powrotem na manualny i włącz zasilanie serw
                    std::cout << "-> Próba ponownego włączenia zasilania silników...\n";
                    robot.setOperateMode(rokae::OperateMode::manual, ec);
                    robot.setPowerState(true, ec);
                    
                    if (ec) {
                        std::cout << "Błąd przywracania zasilania: " << ec.message() << "\n";
                        std::cout << "Wskazówka: Jeśli wyciągnąłeś grzybek E-Stop, upewnij się, że jest przekręcony,\n";
                        std::cout << "oraz że ramię nie opiera się fizycznie o twardy mechaniczny zderzak.\n";
                    } else {
                        std::cout << "====================================================\n";
                        std::cout << " Ramię zostało POMYŚLNIE odblokowane i zasilone!\n";
                        std::cout << " Hamulce zwolnione, ramię jest gotowe do pracy.\n";
                        std::cout << "====================================================\n";
                        zasilanie_on = true;
                        drag_mode_on = false; // Drag został zresetowany
                    }
                    break;
                }
                case 0: { // Wyjście
                    std::cout << "Zamykanie programu. Wyłączanie zasilania silników dla bezpieczeństwa...\n";
                    if (drag_mode_on) robot.disableDrag(ec);
                    robot.setPowerState(false, ec);
                    program_uruchomiony = false;
                    break;
                }
                default: {
                    std::cout << "Nieznana opcja! Wybierz numer od 0 do 7.\n";
                    break;
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Wystąpił błąd krytyczny: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Program zakończony pomyślnie.\n";
    return 0;
}
