#include "full_move.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "rokae/robot.h"
#include "rokae/data_types.h"

#include "arms_moves.h"      // ręce - dokładnie tak jak ruszają się teraz
#include "../../helios_amr/include/amr_matrix.hpp"   // baza Helios (Matrix OS) - Wasz plik z helios_amr/include

namespace commands {

namespace {

using namespace std::chrono_literals;

// ============================================================================
// full_move - test całego robota: ręce -> tułów -> baza, PO KOLEI.
// Ręce: wołamy arms_move() bez zmian. Tułów i baza: krótkie sekwencje, które
// kończą się tam gdzie zaczęły (zero przemieszczenia) - do sprawdzenia, czy
// podsystemy żyją. Baza idzie ramkami z przechwycenia panelu (PROTOKOL.md),
// przez amr::MatrixClient - tak samo jak działało w amr_cmd/baza.sh.
//
// UWAGA (poprawka po awarii): wszystko leci SEKWENCYJNIE, bo SDK Rokae
// przy konstrukcji każdego obiektu robota rejestruje logger spdlog
// '_rokae_log_console_'. Dwie konstrukcje w tym samym czasie = "logger
// already exists" + abort (dokładnie tak padł pierwszy test z wątkami).
// W youtube.cpp roboty też są zawsze tworzone po kolei - trzymamy ten sam
// porządek: żaden obiekt robota nie powstaje, gdy inny jeszcze żyje.
// ============================================================================

// ------------------------------- tułów (PCB4Robot, jak w youtube.cpp) ------

const std::string TULOW_IP = "192.168.71.254";

constexpr double TULOW_SPEED   = 100.0;  // jak TORSO_SPEED_MM_S w youtube.cpp
constexpr double TULOW_EPS_DEG = 1.0;    // "na miejscu"

// T1 = joints[0] = roll (lewo/prawo), T3 = joints[2] = pitch (przód/tył).
constexpr std::array<std::array<double, 4>, 5> TULOW_POSES = {{
    { 0.0, 0.0,  5.0, 0.0},  // pitch przód
    { 0.0, 0.0, -5.0, 0.0},  // pitch tył
    { 5.0, 0.0,  0.0, 0.0},  // roll lewo
    {-5.0, 0.0,  0.0, 0.0},  // roll prawo
    { 0.0, 0.0,  0.0, 0.0},  // zero
}};

template <typename A, typename B>
double diffDeg(const A& a, const B& b) {
    double m = 0.0;
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        m = std::max(m, std::fabs((a[i] - b[i]) * 180.0 / M_PI));
    }
    return m;
}

bool prepareTorso(rokae::PCB4Robot& r) {
    std::error_code ec;
    std::cout << "[Tułów] łączenie z kontrolerem...\n";
    r.connectToRobot(ec);
    if (ec) {
        std::cerr << "[Tułów] błąd połączenia: " << ec.message() << "\n";
        return false;
    }
    ec.clear(); r.recoverState(1, ec);
    ec.clear(); r.clearServoAlarm(ec);
    std::this_thread::sleep_for(300ms);

    ec.clear();
    r.setOperateMode(rokae::OperateMode::automatic, ec);
    if (ec) {
        std::cerr << "[Tułów] setOperateMode: " << ec.message() << "\n";
        return false;
    }
    ec.clear();
    r.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);
    if (ec) {
        std::cerr << "[Tułów] NrtCommand: " << ec.message() << "\n";
        return false;
    }

    // Firmware tułowia potrafi odrzucić pierwsze setPowerState - kilka prób.
    for (int i = 1; i <= 4; ++i) {
        std::cout << "[Tułów] włączam zasilanie (próba " << i << "/4)...\n";
        if (i > 1) {
            ec.clear(); r.recoverState(1, ec);
            ec.clear(); r.clearServoAlarm(ec);
        }
        ec.clear();
        r.setPowerState(true, ec);
        for (int j = 0; j < 15; ++j) {
            std::this_thread::sleep_for(200ms);
            ec.clear();
            if (r.powerState(ec) == rokae::PowerState::on) {
                std::this_thread::sleep_for(1s);
                std::cout << "[Tułów] zasilanie OK.\n";
                return true;
            }
        }
    }
    std::cerr << "[Tułów] nie włączył zasilania - pomijam tułów.\n";
    return false;
}

// Jeden MoveAbsJ + czekanie na dojazd (bez powtórek - to sekwencja testowa).
bool torsoMoveTo(rokae::PCB4Robot& r, const std::array<double, 4>& deg) {
    std::vector<double> target(4);
    for (int i = 0; i < 4; ++i) target[i] = deg[i] * M_PI / 180.0;

    std::error_code ec;
    const auto start = r.jointPos(ec);
    if (ec) {
        std::cerr << "[Tułów] jointPos: " << ec.message() << "\n";
        return false;
    }
    if (diffDeg(start, target) <= TULOW_EPS_DEG) {
        return true;  // już na miejscu - nie wysyłaj pustego ruchu
    }

    rokae::JointPosition t;
    t.joints = target;
    ec.clear(); r.moveReset(ec);
    std::string id;
    ec.clear();
    r.moveAppend(rokae::MoveAbsJCommand(t, TULOW_SPEED, 0.0), id, ec);
    if (ec) {
        std::cerr << "[Tułów] moveAppend: " << ec.message() << "\n";
        return false;
    }
    ec.clear();
    r.moveStart(ec);
    if (ec) {
        std::cerr << "[Tułów] moveStart: " << ec.message() << "\n";
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(100ms);
        ec.clear();
        const auto cur = r.jointPos(ec);
        if (!ec && diffDeg(cur, target) <= TULOW_EPS_DEG) {
            std::cout << "[Tułów] doszedł.\n";
            return true;
        }
    }
    std::cerr << "[Tułów] timeout dojazdu.\n";
    return false;
}

void torsoTest() {
    rokae::PCB4Robot tulow(TULOW_IP);
    if (!prepareTorso(tulow)) return;
    for (const auto& pose : TULOW_POSES) {
        if (!torsoMoveTo(tulow, pose)) {
            std::cerr << "[Tułów] przerywam sekwencję.\n";
            return;
        }
    }
    std::cout << "[Tułów] OK - sekwencja testowa skończona.\n";
}

// ------------------------------- baza (Matrix OS) --------------------------
// Ramki z przechwycenia panelu (DOKUMENTACJA rozdz. 9.7 / PROTOKOL 6.5).
// seq i sesję podmienia sendReplay, więc w hexach są zera.
// WAŻNE (z amr_wasd): po każdym zatrzymaniu baza potrafi zgubić grant
// sterowania - dlatego op=6+op=111 leci ponownie przed KAŻDYM nowym ruchem
// (re-arm), a op=7 dopiero na sam koniec (później jogi nie działają).

const char* BAZA_OP6   = "08021500000000190000000000000000320410063800";  // przejmij sterowanie
const char* BAZA_OP111 = "080215000000001900000000000000003204106f3800";  // para do op=6
const char* BAZA_JOG   = "080215130000001931c44216a10100003209101018ca0220003800";  // szablon jogu
const char* BAZA_ZERO  = "0802150000000019000000000000000032081010180020003800";  // zera = stop
const char* BAZA_OP7   = "08021500000000190000000000000000320410073800";  // oddaj sterowanie

std::string frameFromHex(const char* hex) {
    std::string body;
    amr::hexDecode(hex, body);
    return body;
}

void baseTest() {
    amr::MatrixClient client(amr::MatrixClient::Options{});  // 192.168.71.50:5002, admin
    std::string err;
    if (!client.connect(err)) {
        std::cerr << "[Baza] połączenie: " << err << "\n";
        return;
    }

    // Stan bazy (x, y, kąt) - po każdej nodze pokażemy, czy faktycznie się ruszyła.
    amr::State last_state;
    bool have_state = false;
    const auto on_state = [&](const amr::State& s) {
        last_state = s;
        have_state = true;
    };

    for (int i = 0; i < 40 && !client.logged_in(); ++i) {
        client.pollAny(100, nullptr, on_state);
    }
    if (!client.logged_in()) {
        std::cerr << "[Baza] brak sesji w 4 s (panel Matrix zajęty? musi być zamknięty) - przerywam.\n";
        return;
    }
    std::cout << "[Baza] połączono.\n";

    const std::string op6 = frameFromHex(BAZA_OP6);
    const std::string op111 = frameFromHex(BAZA_OP111);
    const std::string zero = frameFromHex(BAZA_ZERO);
    const std::string op7 = frameFromHex(BAZA_OP7);
    std::string jogTemplate;
    amr::hexDecode(BAZA_JOG, jogTemplate);

    bool ok = true;
    bool any_moved = false;
    auto send = [&](const std::string& body) {
        if (!ok) return;
        if (!client.sendReplay(body)) {
            std::cerr << "[Baza] błąd wysyłki (połączenie padło?) - przerywam.\n";
            ok = false;
            return;
        }
        client.pollAny(20, nullptr, on_state);
    };

    // Jog: pole 6.3 = jazda mm/s (+ przód), 6.4 = obrót mrad/s (+ w lewo).
    auto jog = [&](double v, double w, double seconds) {
        if (!ok) return;
        send(op6);     // re-arm: baza mogła zgubić grant po poprzednim zatrzymaniu
        send(op111);
        std::vector<amr::Node> tree;
        std::string e;
        if (!amr::parseTree(jogTemplate, tree) ||
            !amr::applyPatch(tree, "6.3=" + std::to_string((long long)v), e) ||
            !amr::applyPatch(tree, "6.4=" + std::to_string((long long)w), e)) {
            std::cerr << "[Baza] nie złożyłem ramki jogu (" << e << ") - przerywam.\n";
            ok = false;
            return;
        }
        const std::string body = amr::serializeTree(tree);
        const int n = std::max(1, (int)std::lround(seconds * 20.0));  // ~20 Hz jak baza.sh
        const amr::State s0 = last_state;  // stan przed nogą
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < n && ok; ++i) {
            send(body);
            std::this_thread::sleep_until(t0 + (i + 1) * 50ms);  // stały rytm ~20 Hz
        }
        send(zero);  // jawnie zatrzymaj po ruchu (zera x2 jak w przykładzie z dokumentacji)
        send(zero);
        for (int i = 0; i < 4; ++i) client.pollAny(50, nullptr, on_state);  // niech stan dojdzie
        if (have_state) {
            const double dx = last_state.x_mm - s0.x_mm;
            const double dy = last_state.y_mm - s0.y_mm;
            double dyaw = last_state.yaw_rad - s0.yaw_rad;
            while (dyaw > M_PI) dyaw -= 2 * M_PI;
            while (dyaw < -M_PI) dyaw += 2 * M_PI;
            std::cout << "[Baza]   noga v=" << v << " w=" << w << ": dx=" << std::showpos << dx
                      << " mm, dy=" << dy << " mm, dkat=" << dyaw * 180.0 / M_PI
                      << " st." << std::noshowpos << "\n";
            if (std::fabs(dx) >= 5.0 || std::fabs(dy) >= 5.0 || std::fabs(dyaw) >= 0.02) {
                any_moved = true;
            }
        } else {
            std::cout << "[Baza]   noga v=" << v << " w=" << w << ": brak odczytów stanu\n";
        }
    };

    jog(100, 0, 1.0);     // przód ~8-10 cm (rampa ~0.2 s)
    jog(-100, 0, 1.0);    // wróć
    jog(0, 200, 0.4);     // obrót w lewo ~5°
    jog(0, -200, 0.4);    // wróć

    send(op7);  // oddaj sterowanie - dopiero na sam koniec (potem jogi nie działają!)
    send(op7);
    for (int i = 0; i < 5; ++i) client.pollAny(50, nullptr, on_state);
    client.close();

    if (!ok) {
        std::cerr << "[Baza] NIE OK - sekwencja przerwana.\n";
        return;
    }
    if (have_state && !any_moved) {
        std::cout << "[Baza] UWAGA: żadna noga nie dała ruchu wg stanu bazy - ramki poszły,"
                     " ale baza ich nie posłuchała (grant sterowania? panel otwarty?).\n";
    }
    std::cout << "[Baza] sekwencja testowa wysłana.\n";
}

} // namespace

void full_move() {
    std::cout << "[Full] Start: ręce -> tułów -> baza (po kolei).\n";
    arms_move();   // ręce bez zmian
    torsoTest();   // po rękach - roboty Rokae nie mogą powstać naraz
    baseTest();    // baza nie tyka SDK Rokae, ale też po kolei (pierwszy test)
    std::cout << "[Full] Wszystko skończone.\n";
}

} // namespace commands
