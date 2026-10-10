# Tutorial: zmiana sieci / innego IP

Co zmienić, gdy robot pracuje w innej sieci (inne IP, inny router, inna hala, inny hotspot).
Najczęstsza przyczyna „nic nie działa" po przewiezieniu robota w inne miejsce.

> **Legenda:** 📄 z dokumentacji zespołu · 🔧 z czytania kodu · ❓ do uzupełnienia.

---

## 1. Najpierw dowiedz się, jaki jest nowy układ sieci

Na komputerze pokładowym (Jetson) 📄 [`06-siec-wifi.md`](06-siec-wifi.md):

```bash
ip a                    # wszystkie interfejsy i adresy
nmcli device show       # szczegóły połączeń (Wi-Fi, Ethernet)
```

Na robocie ważne są **dwa** adresy Jetsona:
- **zewnętrzny** (Wi-Fi) — z niego korzysta laptop operatora,
- **wewnętrzny** (Ethernet do kontrolerów) — z niego korzystają ramiona, tors i baza.

W dokumentacji było to 📄: zewnętrzny `10.111.169.242`, wewnętrzny `192.168.71.51`
([`04`](04-robot-rozwiazywanie-problemow.md) §1, [`10`](10-teleoperacja-tracker.md) §1.2).
**W nowej sieci prawdopodobnie oba się zmienią** (przynajmniej ten zewnętrzny).

> 💡 **Wskazówka:** adres wewnętrzny (`192.168.71.x`) można często zostawić bez zmian —
> kontrolery mają swoje stałe adresy, a Jetson dostaje adres statyczny w tej podsieci.
> Wtedy zmienia się tylko adres zewnętrzny i odpowiednio trasa na laptopie.

---

## 2. Pełna lista miejsc, w których jest adres

To jest sedno tego tutorialu — **tu adresy są zapisane na sztywno albo przekazywane jako parametr**.

| # | Element | Gdzie jest adres | Jak go zmienić | Jeśli zapomnisz |
|---:|---|---|---|---|
| 1 | Trasa na laptopie (Windows) | komenda `route add` | 📄 [`04`](04-robot-rozwiazywanie-problemow.md) §6.4 | `TTL expired in transit` przy pingowaniu kontrolerów |
| 2 | Routing na Jetsonie | `sysctl` + `iptables` | 📄 [`04`](04-robot-rozwiazywanie-problemow.md) §6.3 | laptop nie dociera do kontrolerów |
| 3 | Wi-Fi na Jetsonie | `nmcli dev wifi connect` | 📄 [`06-siec-wifi.md`](06-siec-wifi.md) | brak połączenia z robotem |
| 4 | `helios_probe` / `rokae_driver7` | parametry ROS `robot_ip`, `local_ip` | 📄 [`10`](10-teleoperacja-tracker.md) §5.2, [`11`](11-teleoperacja-helios-probe.md) | `Could not connect` / `The passed service type is invalid` |
| 5 | Aplikacja Windows (HUD) | **na sztywno w QML** 🔧 | edycja `Main.qml` + przebudowanie | `Connection to tcp://…:8080 failed: Error number -138` 📄 [`10`](10-teleoperacja-tracker.md) problem 7 |
| 6 | `prezentacja` | **na sztywno w C++** 🔧 | edycja stałych `LEWA_IP`/`PRAWA_IP`/`TULOW_IP`/`LOCAL_IP` + przebudowanie | program łączy się ze starymi adresami |
| 7 | `connect_test` | argument wiersza poleceń 🔧 | `./connect_test <nowe_ip>` | błąd połączenia od razu |
| 8 | Narzędzia bazy jezdnej | zmienne `AMR_IP` / `AMR_PORT` 📄 | `AMR_IP=… AMR_PORT=… ./baza.sh …` | brak odpowiedzi z bazy |
| 9 | Podgląd wideo w przeglądarce | adres w URL | `http://<nowe_ip>:8080/stream?topic=…` 📄 [`07`](07-sensory-kamery.md) §3.3 | brak obrazu |
| 10 | RobotAssist (na laptopie) | ustawienia połączenia w programie | ustawić adresy kontrolerów w GUI | brak połączenia z kontrolerem |
| 11 | Skrypt startowy `~/start_teleop.sh` | ❓ nie ma go w repo | edycja na robocie | ❓ |
| 12 | Ta dokumentacja | pliki `01`, `04`, `06`, `10`–`14` | poprawić wartości | kolejna osoba czyta stare adresy |

---

## 3. Procedura krok po kroku

### Krok 1 — sprawdź, co się zmieniło

```bash
# na Jetsonie
ip a
nmcli device show
```

Zapisz sobie: nowy adres zewnętrzny Jetsona, nowy adres wewnętrzny (jeśli się zmienił),
maskę podsieci i adres bramy.

### Krok 2 — ustaw Wi-Fi na Jetsonie (jeśli trzeba)

📄 [`06-siec-wifi.md`](06-siec-wifi.md):

```bash
sudo nmcli dev wifi connect "NAZWA_SIECI" password 'HASŁO'
```

> **W haśle muszą być pojedyncze cudzysłowy** — z podwójnymi nie działają znaki specjalne. 📄

### Krok 3 — włącz routing na Jetsonie

📄 [`04`](04-robot-rozwiazywanie-problemow.md) §6.3 (w PuTTY, na Jetsonie):

```bash
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -t nat -A POSTROUTING -j MASQUERADE
```

Trwały zapis (żeby przetrwało restart): §6.5 tego samego dokumentu.

### Krok 4 — dodaj trasę na laptopie (CMD jako Administrator)

📄 [`04`](04-robot-rozwiazywanie-problemow.md) §6.4:

```cmd
route add 192.168.71.0 mask 255.255.255.0 <NOWY_IP_JETSONA>
route -p add 192.168.71.0 mask 255.255.255.0 <NOWY_IP_JETSONA>    # zapisane na stałe
```

Jeśli zmieniła się **wewnętrzna** podsieć kontrolerów (nie tylko adres Jetsona), w obu
miejscach podstawiasz nową podsieć zamiast `192.168.71.0/24`.

### Krok 5 — sprawdź, że sieć działa

```cmd
ping <NOWY_IP_JETSONA>
ping 192.168.71.160        # prawe ramię
ping 192.168.71.161        # lewe ramię
ping 192.168.71.254        # tors
ping 192.168.71.50         # baza jezdna
```

Wszystkie mają odpowiadać. Jeśli nie — wróć do kroku 3/4 (albo do
[`04`](04-robot-rozwiazywanie-problemow.md) §6.2, „TTL expired in transit").

### Krok 6 — uruchom węzły z nowymi parametrami

📄 [`10`](10-teleoperacja-tracker.md) §5.2:

```bash
ros2 run rokae_hardware rokae_driver7 --ros-args \
  -p robot_ip:=192.168.71.160 -p local_ip:=<NOWY_WEWNĘTRZNY_IP_JETSONA>
```

> ⚠️ `local_ip` musi być adresem **komputera** w podsieci robota. Bez niego (albo z błędnym)
> ruch nie zadziała: `realtime: 1338 Failed to open UDP socket` 📄
> [`11`](11-teleoperacja-helios-probe.md) problemy 12–13. Włączenie silników **nie** jest
> dowodem, że UDP działa — to częste źródło nieporozumień.

### Krok 7 — przebuduj to, co ma adres na sztywno

| Program | Co zrobić |
|---|---|
| aplikacja Windows | zmienić adres w `Main.qml` 🔧, przebudować w Qt Creatorze |
| `prezentacja` | zmienić stałe w `prezentacja.cpp` 🔧, przebudować |
| `connect_test` | nic — adres podajesz jako argument 🔧 |
| narzędzia bazy | nic — wystarczy `AMR_IP`/`AMR_PORT` 📄 |

### Krok 8 — zaktualizuj dokumentację

Popraw wartości w: `01-przeglad-projektu.md`, `04-robot-rozwiazywanie-problemow.md` (§1, §6),
`06-siec-wifi.md`, `10-teleoperacja-tracker.md` (§1.2, §5), `11-teleoperacja-helios-probe.md`,
`13-programy-cpp.md`, `14-baza-jezdna-dokumentacja.md` (jeśli baza zmieniła adres),
a w przyszłości także `12-teleoperacja-frontend-hud.md` (adres w QML).

---

## 4. Checklist „nowe miejsce, nowa sieć"

- [ ] `ip a` na Jetsonie — znam nowy adres zewnętrzny i wewnętrzny
- [ ] Wi-Fi połączone (`nmcli`) albo kabel wpięty
- [ ] `ip_forward` + `iptables` włączone na Jetsonie
- [ ] trasa dodana na laptopie (`route -p add`)
- [ ] ping do Jetsona, ramion, torsu i bazy przechodzi
- [ ] `rokae_driver7` / `helios_probe` uruchomione z poprawnym `local_ip`
- [ ] aplikacja Windows ma nowy adres (albo została przebudowana)
- [ ] podgląd wideo działa: `http://<ip>:8080/stream?topic=/camera/color/image_raw`
- [ ] narzędzia bazy mają ustawione `AMR_IP`/`AMR_PORT`
- [ ] wartości w dokumentacji poprawione

---

## 5. Moja rekomendacja (opinia, nie fakt)

🔧 **Przestańmy trzymać adresy na sztywno w kodzie.** Dziś adres Jetsona jest wpisany
w `Main.qml`, a adresy wszystkich trzech kontrolerów — w `prezentacja.cpp`. Zmiana sieci
oznacza wtedy edycję kodu i przebudowanie aplikacji, choć wystarczyłoby:

- pole „adres robota" w interfejsie operatora (albo plik konfiguracyjny obok `.exe`),
- parametry wiersza poleceń albo zmienne środowiskowe dla programów konsolowych
  (jak już jest zrobione w `connect_test` i w narzędziach bazy przez `AMR_IP`),
- jeden plik `konfiguracja.md`/`.env` z wszystkimi adresami, z którego korzystają
  i programy, i dokumentacja.

To nie jest zmiana na teraz — ale warto mieć ją na liście, zanim robot pojedzie
w kolejne nowe miejsce.
