#include "stats.h"
#include <array>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <functional>
#include <sstream>
#include <utility>
#include "rokae/robot.h"
#include "rokae/data_types.h"

namespace commands {

static double rad2deg(double rad) {
    return rad * 180.0 / M_PI;
}

static std::string modeToString(rokae::OperateMode mode) {
    switch (mode) {
        case rokae::OperateMode::manual: return "MANUAL";
        case rokae::OperateMode::automatic: return "AUTOMATIC";
        default: return "UNKNOWN";
    }
}

static std::string powerStateToString(rokae::PowerState state) {
    switch (state) {
        case rokae::PowerState::on: return "ON - zasilanie silników włączone";
        case rokae::PowerState::off: return "OFF - zasilanie silników wyłączone";
        case rokae::PowerState::estop: return "E-STOP - wciśnięty wyłącznik awaryjny";
        case rokae::PowerState::gstop: return "GSTOP - otwarta osłona bezpieczeństwa";
        default: return "NIEZNANY";
    }
}

static std::string operationStateToString(rokae::OperationState state) {
    switch (state) {
        case rokae::OperationState::idle: return "idle - stoi";
        case rokae::OperationState::drag: return "drag - DRAG WŁĄCZONY";
        case rokae::OperationState::rtControlling: return "rtControlling - sterowanie czasu rzeczywistego";
        case rokae::OperationState::rlProgram: return "rlProgram - projekt RL";
        case rokae::OperationState::moving: return "moving - w ruchu";
        case rokae::OperationState::jogging: return "jogging - w ruchu (jog)";
        default: return "inny - wartość " + std::to_string(static_cast<int>(state));
    }
}

static std::function<void(const std::string&)> g_statsListener;

void setStatsListener(std::function<void(const std::string&)> listener) {
    g_statsListener = std::move(listener);
}

std::string statsReport() {
    std::ostringstream out;
    const std::string LEWA_IP  = "192.168.71.161";
    const std::string PRAWA_IP = "192.168.71.160";
    const std::string TULOW_IP = "192.168.71.254";
    const std::string LOCAL_IP = "192.168.71.51";

    std::error_code ec;

    // Do podsumowania na końcu raportu.
    bool leftPower = false,  leftDrag = false;
    bool rightPower = false, rightDrag = false;
    bool torsoPower = false, torsoDrag = false;
    int connected = 0;

    out << "\n======================================================================\n";
    out << "               📊 RAPORT STATUSU SYSTEMU ROBOTA HELIOS                \n";
    out << "======================================================================\n";

    // --- 1. LEWA RĘKA (xMateErProRobot) ---
    rokae::xMateErProRobot lewa(LEWA_IP, LOCAL_IP);
    ec.clear();
    lewa.connectToRobot(ec);
    if (ec) {
        out << "🔴 LEWA RĘKA: BRAK POŁĄCZENIA (" << ec.message() << ")\n";
    } else {
        ++connected;
        out << "🟢 LEWA RĘKA: Połączona\n";

        ec.clear();
        rokae::OperateMode mode = lewa.operateMode(ec);
        out << "   | Tryb pracy: " << modeToString(mode) << "\n";

        // NOWE: zasilanie silników
        ec.clear();
        rokae::PowerState power = lewa.powerState(ec);
        if (ec) {
            out << "   | Zasilanie: [BŁĄD] " << ec.message() << "\n";
        } else {
            leftPower = (power == rokae::PowerState::on);
            out << "   | Zasilanie: " << powerStateToString(power)
                      << " (wartość " << static_cast<int>(power) << ")\n";
        }

        // NOWE: stan pracy - w tym czy drag jest włączony
        ec.clear();
        rokae::OperationState operation = lewa.operationState(ec);
        if (ec) {
            out << "   | Stan pracy: [BŁĄD] " << ec.message() << "\n";
        } else {
            leftDrag = (operation == rokae::OperationState::drag);
            out << "   | Stan pracy: " << operationStateToString(operation)
                      << " (wartość " << static_cast<int>(operation) << ")\n";
        }

        out << "   | Drag: " << (leftDrag ? "✅ WŁĄCZONY" : "⭕ wyłączony") << "\n";

        ec.clear();
        std::array<double, 7> joints = lewa.jointPos(ec);
        if (!ec) {
            out << "   | Pozycje osi:\n   |   ";
            for (size_t i = 0; i < joints.size(); ++i) {
                out << "J" << i+1 << ": " << std::fixed << std::setprecision(2) 
                          << rad2deg(joints[i]) << "°   ";
            }
            out << "\n";
        } else {
            out << "   | [BŁĄD] Nie można odczytać pozycji osi: " << ec.message() << "\n";
        }
    }
    out << "----------------------------------------------------------------------\n";

    // --- 2. PRAWA RĘKA (xMateErProRobot) ---
    rokae::xMateErProRobot prawa(PRAWA_IP, LOCAL_IP);
    ec.clear();
    prawa.connectToRobot(ec);
    if (ec) {
        out << "🔴 PRAWA RĘKA: BRAK POŁĄCZENIA (" << ec.message() << ")\n";
    } else {
        ++connected;
        out << "🟢 PRAWA RĘKA: Połączona\n";

        ec.clear();
        rokae::OperateMode mode = prawa.operateMode(ec);
        out << "   | Tryb pracy: " << modeToString(mode) << "\n";

        ec.clear();
        rokae::PowerState power = prawa.powerState(ec);
        if (ec) {
            out << "   | Zasilanie: [BŁĄD] " << ec.message() << "\n";
        } else {
            rightPower = (power == rokae::PowerState::on);
            out << "   | Zasilanie: " << powerStateToString(power)
                      << " (wartość " << static_cast<int>(power) << ")\n";
        }

        ec.clear();
        rokae::OperationState operation = prawa.operationState(ec);
        if (ec) {
            out << "   | Stan pracy: [BŁĄD] " << ec.message() << "\n";
        } else {
            rightDrag = (operation == rokae::OperationState::drag);
            out << "   | Stan pracy: " << operationStateToString(operation)
                      << " (wartość " << static_cast<int>(operation) << ")\n";
        }

        out << "   | Drag: " << (rightDrag ? "✅ WŁĄCZONY" : "⭕ wyłączony") << "\n";

        ec.clear();
        std::array<double, 7> joints = prawa.jointPos(ec);
        if (!ec) {
            out << "   | Pozycje osi:\n   |   ";
            for (size_t i = 0; i < joints.size(); ++i) {
                out << "J" << i+1 << ": " << std::fixed << std::setprecision(2) 
                          << rad2deg(joints[i]) << "°   ";
            }
            out << "\n";
        } else {
            out << "   | [BŁĄD] Nie można odczytać pozycji osi: " << ec.message() << "\n";
        }
    }
    out << "----------------------------------------------------------------------\n";

    // --- 3. TUŁÓW + GŁOWA (PCB4Robot) ---
    rokae::PCB4Robot tulow(TULOW_IP);
    ec.clear();
    tulow.connectToRobot(ec);
    if (ec) {
        out << "🔴 TUŁÓW: BRAK POŁĄCZENIA (" << ec.message() << ")\n";
    } else {
        ++connected;
        out << "🟢 TUŁÓW: Połączony\n";

        ec.clear();
        rokae::OperateMode mode = tulow.operateMode(ec);
        out << "   | Tryb pracy torsu: " << modeToString(mode) << "\n";

        ec.clear();
        rokae::PowerState power = tulow.powerState(ec);
        if (ec) {
            out << "   | Zasilanie: [BŁĄD] " << ec.message() << "\n";
        } else {
            torsoPower = (power == rokae::PowerState::on);
            out << "   | Zasilanie: " << powerStateToString(power)
                      << " (wartość " << static_cast<int>(power) << ")\n";
        }

        ec.clear();
        rokae::OperationState operation = tulow.operationState(ec);
        if (ec) {
            out << "   | Stan pracy: [BŁĄD] " << ec.message() << "\n";
        } else {
            torsoDrag = (operation == rokae::OperationState::drag);
            out << "   | Stan pracy: " << operationStateToString(operation)
                      << " (wartość " << static_cast<int>(operation) << ")\n";
        }

        out << "   | Drag: " << (torsoDrag ? "✅ WŁĄCZONY" : "⭕ wyłączony") << "\n";

        ec.clear();
        std::array<double, 4> torso_joints = tulow.jointPos(ec);
        if (!ec) {
            out << "   | Osie Torsu:\n   |   ";
            for (size_t i = 0; i < torso_joints.size(); ++i) {
                out << "T" << i+1 << ": " << std::fixed << std::setprecision(2) 
                          << rad2deg(torso_joints[i]) << "°   ";
            }
            out << "\n";
        } else {
            out << "   | [BŁĄD] Nie można odczytać pozycji torsu: " << ec.message() << "\n";
        }

    }
    out << "----------------------------------------------------------------------\n";

    // --- PODSUMOWANIE ---
    out << "PODSUMOWANIE\n";
    out << "   | Moduły połączone: " << connected << "/3\n";
    out << "   | Zasilanie włączone: "
              << (leftPower + rightPower + torsoPower) << "/" << connected << " połączonych\n";

    if (!leftDrag && !rightDrag && !torsoDrag) {
        out << "   | Drag: wyłączony na wszystkich modułach\n";
    } else {
        out << "   | Drag włączony na: ";
        bool first = true;
        if (leftDrag)  { out << "LEWA RĘKA";  first = false; }
        if (rightDrag) { out << (first ? "" : ", ") << "PRAWA RĘKA"; first = false; }
        if (torsoDrag) { out << (first ? "" : ", ") << "TUŁÓW"; }
        out << "\n";
    }

    out << "======================================================================\n\n";
    return out.str();
}

void stats() {
    const std::string report = statsReport();
    std::cout << report;
    if (g_statsListener) {
        g_statsListener(report);
    }
}

} // namespace commands
