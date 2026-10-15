#pragma once

// ============================================================================
//  gui_link.h - łącze core Heliosa z GUI (apka Qt na laptopie)
//
//  Transport: TCP (0.0.0.0:PORT, domyślnie 5123) + unix-socket (/tmp/helios.sock).
//  Protokół: JSON-linie (jedna linia = jeden obiekt JSON zakończony \n).
//  Bez Qt, bez ROS, bez nowych zależności - tylko POSIX + STL.
//
//  GUI -> core:
//    {"type":"chat","text":"...","id":7}   tekst z czatu (id: echo dla GUI)
//    {"type":"set_listening","on":false}   bramka nasłuchu słowa-klucza
//    {"type":"ping"}                       -> {"type":"pong"}
//
//  core -> GUI:
//    {"type":"hello","version":1,"name":"helios-core"}
//    {"type":"heard","text":"...","source":"voice|gui|keyboard","ref":7}
//    {"type":"reply","text":"...","ref":7}
//    {"type":"state","state":"listening|muted|capturing|verifying|thinking|speaking",...}
//    {"type":"log","text":"..."}            kopia linii z konsoli (bez szumu statystyk)
//    {"type":"error","code":"..."}          błąd protokołu (złe JSON, pusty tekst...)
//
//  Konfiguracja (zmienne środowiskowe):
//    HELIOS_GUI=0          całkowite wyłączenie łącza (tryb w 100% headless)
//    HELIOS_GUI_PORT=5123   port TCP (nasłuch na wszystkich interfejsach)
//    HELIOS_GUI_SOCK=/tmp/helios.sock   ścieżka gniazda unix (pusta = wyłączone)
//
//  Wątek: start() odpala jeden wątek z pętlą select(); stop() go kończy.
//  Jeden klient naraz - nadmiarowi dostają {"type":"error","code":"busy"}.
// ============================================================================

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace helios_gui {

struct Callbacks {
    // Tekst z czatu GUI (ref>0: potwierdzenie echa; ref==0: brak id).
    std::function<void(const std::string& text, long long ref)> onChat;
    // Żądanie włączenia/wyłączenia nasłuchu słowa-klucza.
    std::function<void(bool on)> onSetListening;
    // Gotowa linia JSON ze stanem (bez \n) albo "" gdy brak.
    // Wołane ~5x/s; wysyłane tylko przy zmianie.
    std::function<std::string()> pollState;
    // Pobiera jedną zaległą linię JSON do GUI (log/reply/heard). false = pusto.
    std::function<bool(std::string& out)> tryPopOutbox;
    // Powiadomienie o podłączeniu/odłączeniu GUI (do bramkowania logów).
    std::function<void(bool connected)> onClientChange;
};

class GuiLink {
public:
    GuiLink();
    ~GuiLink();

    GuiLink(const GuiLink&) = delete;
    GuiLink& operator=(const GuiLink&) = delete;

    // Odpala nasłuch i wątek. Zwraca false, gdy łącze wyłączone (HELIOS_GUI=0)
    // albo nie dało się otworzyć żadnego gniazda (core działa dalej bez GUI).
    bool start(const Callbacks& callbacks);
    void stop();
    bool enabled() const { return enabled_; }

private:
    int setupTcp(int port);
    int setupUnix(const std::string& path);
    void loop();
    void acceptFrom(int listenFd, const char* label);
    void handleLine(const std::string& line);
    bool sendAll(const std::string& data);
    bool sendLine(const std::string& jsonLine);
    void dropClient(const std::string& reason);

    Callbacks cb_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    bool enabled_ = false;
    int tcpListen_ = -1;
    int unixListen_ = -1;
    int client_ = -1;
    std::string readBuffer_;
    std::string lastState_;
};

}  // namespace helios_gui
