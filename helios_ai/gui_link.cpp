// ============================================================================
//  gui_link.cpp - implementacja łącza z GUI (protokół opisany w gui_link.h)
// ============================================================================
#include "gui_link.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace helios_gui {
namespace {

std::string envStr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return (value && *value) ? std::string(value) : fallback;
}

int envInt(const char* name, int fallback) {
    const std::string value = envStr(name, {});
    if (value.empty()) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

void closeFd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

// --- minimalny skaner JSON (tylko to, czego potrzebuje protokół) ------------
// Celowo bez nlohmann/json: ten plik ma się kompilować wszędzie bez zależności.
// GUI generuje poprawny JSON (QJsonDocument), więc wystarczy łagodny skaner.

// Szuka "key" i zwraca wskaźnik na początek wartości (tuż za dwukropkiem).
const char* findValue(const std::string& line, const char* key) {
    const std::string quoted = std::string("\"") + key + "\"";
    size_t pos = line.find(quoted);
    if (pos == std::string::npos) {
        return nullptr;
    }
    pos = line.find(':', pos + quoted.size());
    if (pos == std::string::npos) {
        return nullptr;
    }
    ++pos;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        ++pos;
    }
    return (pos < line.size()) ? line.c_str() + pos : nullptr;
}

// Czyta wartość tekstową w cudzysłowie (z obsługą sekwencji ucieczki).
bool readJsonString(const char* at, std::string* out, const char** end) {
    if (!at || *at != '"') {
        return false;
    }
    ++at;
    std::string result;
    while (*at && *at != '"') {
        if (*at == '\\') {
            ++at;
            switch (*at) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    // \uXXXX -> UTF-8 (wystarcza dla BMP, czyli m.in. polskich znaków)
                    unsigned code = 0;
                    for (int i = 0; i < 4; ++i) {
                        ++at;
                        if (!*at) {
                            return false;
                        }
                        code <<= 4;
                        if (*at >= '0' && *at <= '9') {
                            code += static_cast<unsigned>(*at - '0');
                        } else if (*at >= 'a' && *at <= 'f') {
                            code += static_cast<unsigned>(*at - 'a' + 10);
                        } else if (*at >= 'A' && *at <= 'F') {
                            code += static_cast<unsigned>(*at - 'A' + 10);
                        } else {
                            return false;
                        }
                    }
                    if (code < 0x80) {
                        result += static_cast<char>(code);
                    } else if (code < 0x800) {
                        result += static_cast<char>(0xC0 | (code >> 6));
                        result += static_cast<char>(0x80 | (code & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (code >> 12));
                        result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    break;
                }
                case '\0': return false;
                default: result += *at; break;  // łagodnie: nieznane \x -> x
            }
            if (*at) {
                ++at;
            }
        } else {
            result += *at;
            ++at;
        }
    }
    if (*at != '"') {
        return false;
    }
    ++at;
    *out = result;
    if (end) {
        *end = at;
    }
    return true;
}

bool readJsonBool(const char* at, bool* out) {
    if (!at) {
        return false;
    }
    if (std::strncmp(at, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if (std::strncmp(at, "false", 5) == 0) {
        *out = false;
        return true;
    }
    if (*at == '1') {
        *out = true;
        return true;
    }
    if (*at == '0') {
        *out = false;
        return true;
    }
    return false;
}

bool readJsonInt(const char* at, long long* out) {
    if (!at) {
        return false;
    }
    char* stop = nullptr;
    const long long value = std::strtoll(at, &stop, 10);
    if (stop == at) {
        return false;
    }
    *out = value;
    return true;
}

}  // namespace

GuiLink::GuiLink() = default;

GuiLink::~GuiLink() {
    stop();
}

bool GuiLink::start(const Callbacks& callbacks) {
    cb_ = callbacks;
    if (envStr("HELIOS_GUI", "1") == "0") {
        std::cout << "[GUI] lacze z GUI wylaczone (HELIOS_GUI=0)\n";
        return false;
    }
    const int port = envInt("HELIOS_GUI_PORT", 5123);
    const std::string sockPath = envStr("HELIOS_GUI_SOCK", "/tmp/helios.sock");
    tcpListen_ = setupTcp(port);
    if (!sockPath.empty()) {
        unixListen_ = setupUnix(sockPath);
    }
    if (tcpListen_ < 0 && unixListen_ < 0) {
        std::cout << "[GUI] UWAGA: nie udalo sie otworzyc zadnego gniazda - "
                     "core dziala dalej bez GUI.\n";
        return false;
    }
    enabled_ = true;
    running_ = true;
    thread_ = std::thread([this] { loop(); });
    return true;
}

void GuiLink::stop() {
    running_ = false;
    // shutdown wybudza select/recv w wątku - join nie wisi.
    if (client_ >= 0) {
        ::shutdown(client_, SHUT_RDWR);
    }
    if (tcpListen_ >= 0) {
        ::shutdown(tcpListen_, SHUT_RDWR);
    }
    if (unixListen_ >= 0) {
        ::shutdown(unixListen_, SHUT_RDWR);
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    closeFd(client_);
    closeFd(tcpListen_);
    closeFd(unixListen_);
}

int GuiLink::setupTcp(int port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cout << "[GUI] TCP socket: " << std::strerror(errno) << "\n";
        return -1;
    }
    int reuse = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cout << "[GUI] TCP bind :" << port << ": " << std::strerror(errno) << "\n";
        ::close(fd);
        return -1;
    }
    if (::listen(fd, 4) != 0) {
        std::cout << "[GUI] TCP listen: " << std::strerror(errno) << "\n";
        ::close(fd);
        return -1;
    }
    std::cout << "[GUI] nasluch TCP na 0.0.0.0:" << port << "\n";
    return fd;
}

int GuiLink::setupUnix(const std::string& path) {
    if (path.size() + 1 > sizeof(sockaddr_un::sun_path)) {
        std::cout << "[GUI] za dluga sciezka gniazda unix: " << path << "\n";
        return -1;
    }
    // Test, czy pod ścieżką żyje cudze gniazdo (żeby po awarii nie blokować startu).
    {
        const int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (probe >= 0) {
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
            if (::connect(probe, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
                ::close(probe);
                std::cout << "[GUI] gniazdo " << path << " jest zajete (zyje inny core?) - "
                             "pomijam unix-socket.\n";
                return -1;
            }
            ::close(probe);
            ::unlink(path.c_str());  // zbędny plik po awarii - sprzątamy
        }
    }
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cout << "[GUI] unix socket: " << std::strerror(errno) << "\n";
        return -1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cout << "[GUI] unix bind " << path << ": " << std::strerror(errno) << "\n";
        ::close(fd);
        return -1;
    }
    if (::listen(fd, 4) != 0) {
        std::cout << "[GUI] unix listen: " << std::strerror(errno) << "\n";
        ::close(fd);
        return -1;
    }
    ::chmod(path.c_str(), 0666);
    std::cout << "[GUI] nasluch unix na " << path << "\n";
    return fd;
}

bool GuiLink::sendAll(const std::string& data) {
    const char* ptr = data.c_str();
    size_t left = data.size();
    while (left > 0) {
        const ssize_t sent = ::send(client_, ptr, left, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (sent == 0) {
            return false;
        }
        ptr += sent;
        left -= static_cast<size_t>(sent);
    }
    return true;
}

bool GuiLink::sendLine(const std::string& jsonLine) {
    if (client_ < 0) {
        return false;
    }
    return sendAll(jsonLine + "\n");
}

void GuiLink::dropClient(const std::string& reason) {
    if (client_ < 0) {
        return;
    }
    std::cout << "[GUI] klient rozlaczony (" << reason << ")\n";
    closeFd(client_);
    readBuffer_.clear();
    if (cb_.onClientChange) {
        cb_.onClientChange(false);
    }
}

void GuiLink::acceptFrom(int listenFd, const char* label) {
    const int fd = ::accept(listenFd, nullptr, nullptr);
    if (fd < 0) {
        if (errno != EINTR) {
            std::cout << "[GUI] accept (" << label << "): " << std::strerror(errno) << "\n";
        }
        return;
    }
    if (client_ >= 0) {
        // Jedno GUI naraz - nadmiarowych grzecznie odrzucamy.
        const char busy[] = "{\"type\":\"error\",\"code\":\"busy\"}\n";
        ::send(fd, busy, sizeof(busy) - 1, MSG_NOSIGNAL);
        ::close(fd);
        return;
    }
    int flag = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));  // dla unix: nieszkodliwe
    // Keepalive: martwy klient (urwane WiFi, ubita apka bez FIN) puści slot
    // po ~25 s zamiast trzymać go w nieskończoność (polityka: jedno GUI naraz).
    // Dla gniazda unix te opcje zwracają błąd ignorowany jak wyżej.
    ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
    int keepIdle = 10, keepInterval = 5, keepProbes = 3;
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepProbes, sizeof(keepProbes));
    struct timeval timeout{};
    timeout.tv_sec = 2;
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    client_ = fd;
    readBuffer_.clear();
    lastState_.clear();  // wymusza wysłanie stanu zaraz po połączeniu
    std::cout << "[GUI] klient polaczony (" << label << ")\n";
    sendLine("{\"type\":\"hello\",\"version\":1,\"name\":\"helios-core\"}");
    if (cb_.onClientChange) {
        cb_.onClientChange(true);
    }
}

void GuiLink::handleLine(const std::string& line) {
    std::string type;
    if (!readJsonString(findValue(line, "type"), &type, nullptr)) {
        sendLine("{\"type\":\"error\",\"code\":\"bad_json\"}");
        return;
    }
    if (type == "chat") {
        std::string text;
        if (!readJsonString(findValue(line, "text"), &text, nullptr) || text.empty()) {
            sendLine("{\"type\":\"error\",\"code\":\"empty_text\"}");
            return;
        }
        if (text.size() > 2000) {
            text.resize(2000);
        }
        long long ref = 0;
        readJsonInt(findValue(line, "id"), &ref);  // brak id = 0, też dobrze
        std::cout << "[GUI] czat (id=" << ref << "): " << text << "\n";
        if (cb_.onChat) {
            cb_.onChat(text, ref);
        }
    } else if (type == "set_listening") {
        bool on = true;
        if (!readJsonBool(findValue(line, "on"), &on)) {
            sendLine("{\"type\":\"error\",\"code\":\"bad_on\"}");
            return;
        }
        if (cb_.onSetListening) {
            cb_.onSetListening(on);
        }
    } else if (type == "ping") {
        sendLine("{\"type\":\"pong\"}");
    } else {
        sendLine("{\"type\":\"error\",\"code\":\"unknown_type\"}");
    }
}

void GuiLink::loop() {
    while (running_) {
        fd_set readSet;
        FD_ZERO(&readSet);
        int maxFd = -1;
        auto watch = [&](int fd) {
            if (fd >= 0) {
                FD_SET(fd, &readSet);
                if (fd > maxFd) {
                    maxFd = fd;
                }
            }
        };
        watch(tcpListen_);
        watch(unixListen_);
        watch(client_);
        struct timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;  // 5 Hz: wysyłka stanu i opróżnianie kolejki
        const int ready = ::select(maxFd + 1, &readSet, nullptr, nullptr, &timeout);
        if (!running_) {
            break;
        }
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cout << "[GUI] select: " << std::strerror(errno) << "\n";
            continue;
        }
        if (tcpListen_ >= 0 && FD_ISSET(tcpListen_, &readSet)) {
            acceptFrom(tcpListen_, "tcp");
        }
        if (unixListen_ >= 0 && FD_ISSET(unixListen_, &readSet)) {
            acceptFrom(unixListen_, "unix");
        }
        if (client_ >= 0 && FD_ISSET(client_, &readSet)) {
            char chunk[4096];
            const ssize_t got = ::recv(client_, chunk, sizeof(chunk), 0);
            if (got <= 0) {
                dropClient(got == 0 ? "zamkniete przez GUI" : std::strerror(errno));
            } else {
                readBuffer_.append(chunk, static_cast<size_t>(got));
                if (readBuffer_.size() > 65536) {
                    dropClient("przepelniony bufor");
                } else {
                    size_t pos = 0;
                    while ((pos = readBuffer_.find('\n')) != std::string::npos) {
                        std::string line = readBuffer_.substr(0, pos);
                        readBuffer_.erase(0, pos + 1);
                        if (!line.empty() && line.back() == '\r') {
                            line.pop_back();
                        }
                        if (!line.empty() && client_ >= 0) {
                            handleLine(line);
                        }
                    }
                }
            }
        }
        // Wysyłka do GUI: najpierw zaległe (logi/odpowiedzi), potem stan przy zmianie.
        if (client_ >= 0 && cb_.tryPopOutbox) {
            std::string out;
            int sent = 0;
            while (sent < 50 && cb_.tryPopOutbox(out)) {
                if (!sendLine(out)) {
                    dropClient("blad wysylki");
                    break;
                }
                ++sent;
            }
        }
        if (client_ >= 0 && cb_.pollState) {
            const std::string state = cb_.pollState();
            if (!state.empty() && state != lastState_) {
                lastState_ = state;
                if (!sendLine(state)) {
                    dropClient("blad wysylki stanu");
                }
            }
        }
    }
}

}  // namespace helios_gui
