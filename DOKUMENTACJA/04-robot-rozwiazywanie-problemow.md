> 📄 **Na podstawie:** `DOKUMENTACJA_ROKAE_HELIOS_ROZWIAZYWANIE_PROBLEMOW.md` · wersja z poprawek — zmiany wymienione w bloku „✏️ Poprawki" pod nagłówkiem · 10 października 2026

# Przewodnik Rozwiązywania Problemów i Konfiguracji: Rokae Helios (xCore / RobotAssist / Sieć i ROS)

Kompendium wiedzy technicznej, konfiguracji bezpieczeństwa oraz rozwiązywania problemów ze sterownikiem **Rokae xCore (RobotAssist 5.x)**, robotem dwuramiennym **Rokae Helios** (tors 4-osiowy `TaiHu`) oraz integracją sieciową z komputerem nadrzędnym **NVIDIA Jetson (ROS 2)** i systemem Windows.

---

## Spis Treści

1. [Architektura Systemu i Prawidłowe Nazewnictwo](#1-architektura-systemu-i-prawid%C5%82owe-nazewnictwo)
2. [Moduł Bezpieczeństwa i Limity Kątowe (Soft Limits / Joint Limits)](#2-modu%C5%82-bezpiecze%C5%84stwa-i-limity-k%C4%85towe-soft-limits--joint-limits)
   - [2.1 Gdzie znajduje się Soft Limit w RobotAssist?](#21-gdzie-znajduje-si%C4%99-soft-limit-w-robotassist)
   - [2.2 Błąd [10051]: Given angles exceed mechanical limits](#22-b%C5%82%C4%85d-10051-given-angles-exceed-mechanical-limits)
   - [2.3 Błąd złych kątów przy załączaniu limitu](#23-b%C5%82%C4%85d-z%C5%82ych-k%C4%85t%C3%B3w-przy-za%C5%82%C4%85czaniu-limitu)
   - [2.4 Prawidłowa procedura zapisu i zatwierdzenia sumy kontrolnej](#24-prawid%C5%82owa-procedura-zapisu-i-zatwierdzenia-sumy-kontrolnej)
3. [Obsługa Stanów Robota i Błędy Napędów (Servo Alarms)](#3-obs%C5%82uga-stan%C3%B3w-robota-i-b%C5%82%C4%99dy-nap%C4%99d%C3%B3w-servo-alarms)
   - [3.1 Błąd [31015]: Axis 1 alarm code (0x3120) – Mains/DC-bus Undervoltage](#31-b%C5%82%C4%85d-31015-axis-1-alarm-code-0x3120--mainsdc-bus-undervoltage)
   - [3.2 Dlaczego Clear Alarm nie pomaga i jak odblokować robota?](#32-dlaczego-clear-alarm-nie-pomaga-i-jak-odblokowa%C4%87-robota)
   - [3.3 Różnice między trybem Manual a Automatic](#33-r%C3%B3%C5%BCnice-mi%C4%99dzy-trybem-manual-a-automatic)
4. [Tworzenie Punktów i Programowanie (RL Editor)](#4-tworzenie-punkt%C3%B3w-i-programowanie-rl-editor)
   - [4.1 Prawidłowe nauczanie punktów (jointtarget vs robtarget)](#41-prawid%C5%82owe-nauczanie-punkt%C3%B3w-jointtarget-vs-robtarget)
   - [4.2 Pułapka osiowa: Błąd liczby osi (np. punkty j6 na 4-osiowym torsie)](#42-pu%C5%82apka-osiowa-b%C5%82%C4%85d-liczby-osi-np-punkty-j6-na-4-osiowym-torsie)
   - [4.3 Błąd [61018]: Program not synchronized to the controller](#43-b%C5%82%C4%85d-61018-program-not-synchronized-to-the-controller)
   - [4.4 Błąd [13020]: All RL tasks have been stopped](#44-b%C5%82%C4%85d-13020-all-rl-tasks-have-been-stopped)
5. [Diagnostyka Usług Sieciowych Robota (Controller Service vs Upgrade Service)](#5-diagnostyka-us%C5%82ug-sieciowych-robota-controller-service-vs-upgrade-service)
   - [5.1 Przyczyny stanu: Controller Service: Disconnected / Upgrade Service: Connected](#51-przyczyny-stanu-controller-service-disconnected--upgrade-service-connected)
   - [5.2 Procedura zdalnego restartu (Soft Reboot) i restartu sprzętowego](#52-procedura-zdalnego-restartu-soft-reboot-i-restartu-sprz%C4%99towego)
6. [Topologia Sieciowa, Routing i Jednoczesny Dostęp do Internetu](#6-topologia-sieciowa-routing-i-jednoczesny-dost%C4%99p-do-internetu)
   - [6.1 Architektura połączeń (Laptop ↔ Jetson/ROS ↔ Kontrolery xCore)](#61-architektura-po%C5%82%C4%85cze%C5%84-laptop--jetsonros--kontrolery-xcore)
   - [6.2 Problem: TTL expired in transit i pętla routingu](#62-problem-ttl-expired-in-transit-i-p%C4%99tla-routingu)
   - [6.3 Konfiguracja Jetsona (Linux ip_forward + iptables NAT)](#63-konfiguracja-jetsona-linux-ip_forward--iptables-nat)
   - [6.4 Konfiguracja Windowsa (Statyczna trasa route add)](#64-konfiguracja-windowsa-statyczna-trasa-route-add)
   - [6.5 Trwały zapis konfiguracji (autostart po restarcie)](#65-trwa%C5%82y-zapis-konfiguracji-autostart-po-restarcie)
7. [Zestawienie Oficjalnej Dokumentacji Technicznej](#7-zestawienie-oficjalnej-dokumentacji-technicznej)

---

## 1. Architektura Systemu i Prawidłowe Nazewnictwo

Robot **Rokae Helios** składa się z kilku niezależnych jednostek sterujących połączonych siecią Ethernet:
* **Tors (Torso):** Zgłasza się w systemie pod nazwą modelu **`TaiHu`**. Posiada **4 osie obrotowe (J1, J2, J3, J4)**. Domyślny adres IP kontrolera torsu to `192.168.71.254`.
* **Ramiona (Arms):** Dwa 7-osiowe manipulatory elastyczne xMate. Lewe i prawe ramię posiadają własne kontrolery xCore — adresy `192.168.71.161` (lewe) i `192.168.71.160` (prawe).
* **Baza jezdna (AMR):** Podwozie kołowe z własnym sterownikiem — adres `192.168.71.50` (port `5002`, osobny protokół — patrz [`14-baza-jezdna-dokumentacja.md`](14-baza-jezdna-dokumentacja.md)).
* **Komputer nadrzędny (NVIDIA Jetson / Orin / Xavier):** System Linux Ubuntu (`tegra aarch64`), na którym uruchamiany jest stos ROS 2. Posiada interfejs Wi-Fi/Ethernet w podsieci zewnętrznej (np. `10.111.169.242`) oraz interfejs wewnętrzny spięty z kontrolerami xCore w podsieci `192.168.71.0/24` — adres wewnętrzny `192.168.71.51`.

> ✏️ **Poprawki 10.10.2026 (ten plik):**
> 1. Adresy ramion: wcześniej tu było `192.168.71.50` (prawe) i `192.168.71.51` (lewe).
>    Poprawiono na `.160` / `.161` — tak jest w [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §1.2,
>    w [`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md) oraz w kodzie
>    `prezentacja.cpp`. Adres `.50` należy do **bazy jezdnej**, a `.51` do **komputera pokładowego**.
> 2. Dopisano adres bazy jezdnej (`.50`) i adres wewnętrzny Jetsona (`.51`), których wcześniej
>    w tym rozdziale nie było — bez nich łatwo pomylić, który adres do czego należy.
> 3. Poprawiono diagram w §6.1 (tam były te same błędne adresy ramion oraz `eth0: 192.168.71.1`).
> 4. W przykładzie z „TTL expired in transit" (§6.2) pingowany był adres `192.168.71.51`,
>    który po poprawce oznacza komputer pokładowy — zamieniono na adres prawego ramienia
>    (`192.168.71.160`), żeby przykład był zgodny z resztą dokumentacji.

---

## 2. Moduł Bezpieczeństwa i Limity Kątowe (Soft Limits / Joint Limits)

### 2.1 Gdzie znajduje się Soft Limit w RobotAssist?

W oprogramowaniu RobotAssist (xCore) nie występuje bezpośrednio etykieta o nazwie „Soft Limit” dla głównych osi manipulatora.
* **Gdzie szukać:** Górne menu -> **`Safety`** -> lewe menu boczne -> **`Joint Limit`** -> sekcja **`Joint Position(°)`**.
* W oficjalnej chińskiej dokumentacji funkcja ta nazywa się dosłownie **`软限位` (Soft Limit)**.
* **Uwaga:** Zakładka `Setting -> Extra Axis` zawiera pole o nazwie *Soft Limit*, ale odnosi się ono **wyłącznie do osi zewnętrznych** (np. tor jezdny, obrotnik pozycjonera), a nie do głównych osi robota/torsu.

### 2.2 Błąd [10051]: Given angles exceed mechanical limits

```text
[10051]: Failed to set joint position limit, The given angles exceed mechanical limits
```

#### Przyczyna
Kontroler bezpieczeństwa xCore sprawdza, czy wprowadzany zakres programowy ściśle mieści się w fabrycznych, fizycznych odbojnikach mechanicznych manipulatora.
Dla torsu `TaiHu` mechaniczne limity (wyświetlane po prawej stronie w nawiasach kwadratowych `[...]`) wynoszą:

| Oś | Zakres mechaniczny `[min, max]` | Typowy błąd | Prawidłowy zakres Soft Limit |
| :---: | :---: | :---: | :---: |
| **J1** | **`[-68, 3]`** | Wpisanie górnego limitu np. `89` lub `90` | **`-68` do `3`** |
| **J2** | **`[-175, 49]`** | Wpisanie górnego limitu np. `141` | **`-175` do `49`** |
| **J3** | **`[-128, 88]`** | Wpisanie wartości poza przedziałem | **`-128` do `88`** |
| **J4** | **`[-90, 180]`** | Wpisanie wartości poza przedziałem | **`-90` do `180`** |

Wpisanie wartości `141` dla J2 lub `89` dla J1 skutkuje natychmiastowym odrzuceniem konfiguracji z błędem `10051`.

### 2.3 Błąd złych kątów przy załączaniu limitu

Kontroler odrzuci próbę włączenia limitu, jeśli:
1. **Aktualna pozycja osi wykracza poza zadawany zakres:** Jeśli J1 stoi fizycznie na $+1.5^\circ$, a w limitach wpiszesz zakres $[-68, 0]$, system zablokuje operację. Zawsze sprawdź bieżące kąty w oknie **`Monitor`** (ikona oka u góry).
2. **Limit dolny nie jest mniejszy od górnego:** Wartość *Lower Limit* musi być bezwzględnie mniejsza od *Upper Limit*.
3. **Tryb zredukowany (Reduced) jest szerszy niż normalny:** Zakres w kolumnie `Reduced Joint Pos` musi być równy lub węższy niż w kolumnie `Joint Pos`.

### 2.4 Prawidłowa procedura zapisu i zatwierdzenia sumy kontrolnej

1. Przejdź do: **`Safety` -> `Joint Limit`**.
2. Na dole ekranu w polu **`Safety Login :`** wpisz domyślne hasło:
   ```text
   safety
   ```
   i kliknij **`Unlock`**.
3. Wpisz wartości w tabeli `Joint Position(°)`:
   * J1: `-68` do `3` (Reduced: `-67` do `2`)
   * J2: `-175` do `49` (Reduced: `-174` do `48`)
   * J3: `-128` do `88` (Reduced: `-127` do `87`)
   * J4: `-90` do `180` (Reduced: `-89` do `179`)
4. Przestaw suwak **`Enable`** w wierszu *Joint Position(°)* na pozycję włączoną (niebieską).
5. Kliknij przycisk **`Confirm`** w prawym dolnym rogu.
6. W wyskakującym oknie **`Security Check`** upewnij się, że statusy limitów mają wartość aktywną i kliknij **`Confirm / OK`**.
7. Wygeneruje się nowa 4-znakowa suma kontrolna bezpieczeństwa (np. `40C9`), wyświetlana w prawym górnym rogu ekranu.

---

## 3. Obsługa Stanów Robota i Błędy Napędów (Servo Alarms)

### 3.1 Błąd [31015]: Axis 1 alarm code (0x3120) – Mains/DC-bus Undervoltage

```text
[31015]: Servo alarm occurs, and the error code reported by servo is: Axis 1 alarm code(0x3120)
```

#### Przyczyna
W standardzie napędów przemysłowych (CiA 402 / EtherCAT) kod błędu **`0x3120`** oznacza **Mains under-voltage / DC bus under-voltage** (spadek lub zanik napięcia na szynie DC zasilania silników serwonapędu).

Do błędu tego dochodzi w trzech sytuacjach:
1. **Przełączenie trybu z Manual na Auto:** Zgodnie z normami bezpieczeństwa kontroler xCore w chwili przełączenia trybu natychmiast fizycznie odcina stycznikiem napięcie zasilania silników. Serwo widzi zanik zasilania i zatrzaskuje alarm `0x3120`.
2. **Wciśnięty wyłącznik bezpieczeństwa (E-Stop):** Rozwiera obwód siłowy napędów przy nadal aktywnej elektronice logicznej.
3. **Spadek napięcia zasilania głównego:** Zbyt niski poziom naładowania baterii Heliosa lub odcięcie zasilacza buforowego torsu.

### 3.2 Dlaczego Clear Alarm nie pomaga i jak odblokować robota?

Alarm `0x3120` jest zatrzaskiwany w pamięci nieulotnej samego sterownika serwonapędu osi 1. Przycisk `Clear Alarm` w RobotAssist czyści wyłącznie bufor zdarzeń oprogramowania na komputerze – dopóki driver sprzętowy widzi brak napięcia lub stan błędu, natychmiast odsyła alarm z powrotem.

#### Procedura usunięcia błędu 0x3120:
1. Zamknij okno dialogowe błędu klikając **`OK`**.
2. Upewnij się, że robot znajduje się w trybie **`Manual`** (ikona na dolnym pasku stanu).
3. Sprawdź, czy żaden fizyczny wyłącznik awaryjny (E-Stop) nie jest wciśnięty.
4. **Wykonaj restart zasilania (Power Cycle):**
   * Wyłącz zasilanie robota/torsu głównym wyłącznikiem.
   * Odczekaj **15–20 sekund**, aby kondensatory szyny DC rozładowały się do zera.
   * Włącz zasilanie ponownie.
5. Po podniesieniu systemu chwyć kasetę i wciśnij 3-pozycyjny przycisk zezwalający (Deadman) do pozycji środkowej. Usłyszysz kliknięcie styczników, a ikona zasilania silnika zmieni stan na aktywny.

### 3.3 Różnice między trybem Manual a Automatic

| Funkcja / Stan | Tryb Ręczny (Manual) | Tryb Automatyczny (Auto) |
| :--- | :--- | :--- |
| **Załączenie zasilania** | Przycisk Deadman (pozycja środkowa) | Przycisk zasilania w GUI / wejście System IO |
| **Ruch ręczny (JOG)** | **Dozwolony** (maks. 250 mm/s) | **Zablokowany** |
| **Edycja kodu programu** | **Dozwolona** | **Zablokowana** (*Non-manual mode prohibits editing*) |
| **Wykonanie programu** | Wymaga trzymania Deadman + Run | Wystarczy jednokrotne naciśnięcie Run |
| **Wygrodzenie bezpieczeństwa** | Drzwi mogą być otwarte | Wymaga zamknięcia pętli Safety Gate |

---

## 4. Tworzenie Punktów i Programowanie (RL Editor)

### 4.1 Prawidłowe nauczanie punktów (jointtarget vs robtarget)

Dla torsu robota Helios (`TaiHu`) najpewniejszym sposobem poruszania jest sterowanie w przestrzeni osiowej za pomocą instrukcji **`MoveAbsJ`** oraz punktów typu **`jointtarget`**.

#### Krok po kroku przez Points List:
1. Załącz zasilanie robota w trybie Manual.
2. W prawym panelu JOG wybierz tryb osiowy (**Axis**) i ustaw osie J1–J4 w żądanej pozycji roboczej.
3. W lewym menu wejdź w **`Program` -> `Points List`**.
4. Kliknij przycisk dodawania **`+` (New)**.
5. Wybierz typ danych: **`jointtarget`**.
6. Nadaj unikalną nazwę, np. `p_start`, `p_lewo`, `p_prawo`.
7. Zaznacz utworzony punkt i kliknij przycisk **`Teach` / `ModPos`**.
8. System przepisze dokładne, bieżące kąty fizycznych osi torsu do punktu. Gwarantuje to brak konfliktów z limitami programowymi.
9. Kliknij **`Save`**.

### 4.2 Pułapka osiowa: Błąd liczby osi (np. punkty j6 na 4-osiowym torsie)

* Tors robota ma **4 osie** (J1–J4).
* Użycie w programie punktów wygenerowanych dla robota 6- lub 7-osiowego (np. `j6`, `j7` widoczne na listingu):
  ```text
  MoveAbsJ(j6, v1000, z50, tool0, wobj0); ! BŁĄD!
  ```
  spowoduje natychmiastowe wyrzucenie błędu:
  ```text
  [50512]: Axis quantity mismatch
  ```
* **Zasada:** W programie dla torsu odwołuj się wyłącznie do punktów nauczonych na modelu torsu (posiadających 4 współrzędne kątowe).

### 4.3 Błąd [61018]: Program not synchronized to the controller

```text
[61018]: The program is not synchronized to the controller, and starting the program via system IO, register function code, or external communication is prohibited.
```

* **Przyczyna:** Kod w edytorze tekstowym RobotAssist został zmieniony, ale nie został skompilowany i przesłany do sterownika czasu rzeczywistego.
* **Naprawa:**
  1. W edytorze kodu kliknij ikonę kompilacji/sprawdzenia składni (ptaszek $\checkmark$).
  2. Kliknij ikonę zapisu/synchronizacji ($\circlearrowright$ lub dyskietka).
  3. Kliknij przycisk **`PPtoMain`** (strzałka resetu wskaźnika do funkcji głównej). Spowoduje to natychmiastowe załadowanie nowego kodu do pamięci wykonawczej kontrolera.

### 4.4 Błąd [13020]: All RL tasks have been stopped

```text
[13020]: All RL tasks have been stopped. Please click PPtoMain or check the error message
```

* **Przyczyny:**
  1. Program doszedł do instrukcji `ENDPROC` i zakończył cykl pracy (normalne zachowanie w trybie *Single Cycle*).
  2. W trybie Manual puszczono przycisk zezwalający (Deadman) lub przycisk `Run`.
* **Naprawa:**
  * Kliknij **`PPtoMain`**, aby cofnąć wskaźnik wykonania na początek.
  * Jeśli program ma działać nieprzerwanie, przełącz ikonę cyklu na dolnym pasku na **Continuous / Loop** ($\circlearrowleft$).
  * Upewnij się, że podczas całego ruchu w trybie ręcznym przycisk Deadman jest stale wciśnięty w pozycji środkowej.

---

## 5. Diagnostyka Usług Sieciowych Robota (Controller Service vs Upgrade Service)

### 5.1 Przyczyny stanu: Controller Service: Disconnected / Upgrade Service: Connected

```text
Robot Service Connection
Controller Service: Disconnected
Upgrade Service:    Connected to [192.168.71.254:4567]
```

W kontrolerze Rokae xCore działają dwa niezależne procesy:
1. **Upgrade Service (port 4567):** Bazowa usługa systemowa Linuxa – działa zawsze, gdy system operacyjny żyje. Odpowiada za wgrywanie firmware'u, backupy i procedury ratunkowe.
2. **Controller Service:** Główny proces sterowania czasem rzeczywistym, planowania trajektorii i magistrali napędowej EtherCAT.

Gdy Controller Service ma status *Disconnected*, oznacza to, że **proces czasu rzeczywistego uległ awarii (crash), zawiesił się po krytycznym błędzie napędu (np. 0x3120) lub został zatrzymany przez watchdog**.

### 5.2 Procedura zdalnego restartu (Soft Reboot) i restartu sprzętowego

Dopóki proces nie zostanie zrestartowany, klikanie `Connect` nic nie zmieni.

#### Sposób A: Zdalny restart z poziomu RobotAssist (wykorzystując aktywne Upgrade Service)
1. W górnym menu wejdź w **`Option`**.
2. W lewym menu wybierz **`Software Upgrade`** (lub *Upgrade / Konserwacja*).
3. Kliknij przycisk **`Reboot Robot`** (`重启机器人`).
4. Potwierdź komunikat. System operacyjny torsu zrestartuje się i automatycznie uruchomi usługę `Controller Service`.
5. Po około 45 sekundach kliknij **`Connect`** w oknie połączenia.

#### Sposób B: Twardy restart zasilania (Power Cycle)
1. Wyłącz zasilanie zasilacza/baterii torsu.
2. Odczekaj 20 sekund.
3. Włącz zasilanie i odczekaj około 1 minutę na start wszystkich procesów.

---

## 6. Topologia Sieciowa, Routing i Jednoczesny Dostęp do Internetu

### 6.1 Architektura połączeń

```
[ Router Wi-Fi / Hotspot ]  (Internet, np. 10.111.169.1)
         ▲
         │ (Wi-Fi)
         ▼
[ Komputer Pokładowy Jetson ] (wlan0: 10.111.169.242)
         │
         │ (Wewnętrzny switch / eth0: 192.168.71.51)
         ▼
 ┌───────────────────────┬───────────────────────┬───────────────────────┐
 │                       │                       │                       │
[Tors Helios]           [Prawe Ramię]           [Lewe Ramię]            [Baza jezdna]
192.168.71.254          192.168.71.160          192.168.71.161          192.168.71.50

         ▲
         │ (Wi-Fi: 10.111.169.189)
[ Laptop Inżynierski ] (Windows)
```

### 6.2 Problem: TTL expired in transit i pętla routingu

Gdy z poziomu Windowsa wpisujesz:
```cmd
ping 192.168.71.160
```
otrzymujesz:
```text
Reply from 10.0.11.37: TTL expired in transit.
```
**Dlaczego?**  
Twój laptop ma podłączenie do sieci lokalnej przez router Wi-Fi (`10.0.11.37`). Windows nie posiada w tablicy routingu wpisu dla podsieci `192.168.71.0/24`. Wysyła więc pakiet do domyślnej bramy (routera Wi-Fi), a ten nie znając tej sieci, odbija go w pętli aż do wygaśnięcia licznika skoków TTL (*Time To Live*).

### 6.3 Konfiguracja Jetsona (Linux ip_forward + iptables NAT)

Aby Jetson przekazywał pakiety z sieci zewnętrznej Wi-Fi do wewnętrznej podsieci kontrolerów xCore, zaloguj się na niego przez PuTTY (`std@10.111.169.242`) i wykonaj:

1. **Włączenie routingu IP w jądrze systemu:**
   ```bash
   sudo sysctl -w net.ipv4.ip_forward=1
   ```
2. **Włączenie translacji adresów (NAT / Masquerade):**
   ```bash
   sudo iptables -t nat -A POSTROUTING -j MASQUERADE
   ```

### 6.4 Konfiguracja Windowsa (Statyczna trasa route add)

Poinformuj system Windows, że brama do sieci `192.168.71.0/24` znajduje się pod adresem IP Jetsona (`10.111.169.242`):

1. Otwórz **Wiersz Poleceń (CMD)** jako **Administrator**.
2. Wpisz polecenie:
   ```cmd
   route add 192.168.71.0 mask 255.255.255.0 10.111.169.242
   ```
3. *Opcjonalnie (zapis na stałe po restarcie komputera):* dodaj przełącznik `-p`:
   ```cmd
   route -p add 192.168.71.0 mask 255.255.255.0 10.111.169.242
   ```

### 6.5 Trwały zapis konfiguracji (autostart po restarcie)

#### Trwały routing na Jetsonie:
Aby po wyłączeniu robota Jetson nie zapomniał reguł routingu:

1. Otwórz plik konfiguracyjny sysctl:
   ```bash
   sudo nano /etc/sysctl.conf
   ```
   Odkomentuj (usuń znak `#`) lub dopisz linię:
   ```text
   net.ipv4.ip_forward = 1
   ```
   Zapisz plik (`Ctrl+O`, `Enter`, `Ctrl+X`).

2. Zainstaluj pakiet trwałego zapisu iptables:
   ```bash
   sudo apt-get update && sudo apt-get install -y iptables-persistent
   sudo netfilter-persistent save
   ```

---

## 7. Zestawienie Oficjalnej Dokumentacji Technicznej

* **Główny Portal Dokumentacji Technicznej Rokae Docs:**  
  [https://docs.rokae.com/](https://docs.rokae.com/)
* **Instrukcja Obsługi Systemu Sterowania xCore:**  
  [https://docs.rokae.com/docs/xCore](https://docs.rokae.com/docs/xCore)
* **Konfiguracja Funkcji Bezpieczeństwa (Limity osi, Soft Limits, Hasło `safety`):**  
  [https://docs.rokae.com/docs/xCore/安全功能](https://docs.rokae.com/docs/xCore/%E5%AE%89%E5%85%A8%E5%8A%9F%E8%83%BD)
* **Podstawy Obsługi Interfejsu RobotAssist / xCore HMI:**  
  [https://docs.rokae.com/docs/xCore/hmi-简介](https://docs.rokae.com/docs/xCore/hmi-%E7%AE%80%E4%BB%8B)
* **Podstawowe Operacje Sterownika (Załączanie zasilania, tryby, JOG):**  
  [https://docs.rokae.com/docs/xCore/控制系统基础操作](https://docs.rokae.com/docs/xCore/%E6%8E%A7%E5%88%B6%E7%B3%BB%E7%BB%9F%E5%9F%BA%E7%A1%80%E6%93%8D%E4%BD%9C)
* **Oficjalny Rejestr Kodów Błędów i Procedury Rozwiązywania Problemów (Troubleshooting):**  
  [https://docs.rokae.com/docs/xCore/故障排查](https://docs.rokae.com/docs/xCore/%E6%95%85%E9%9A%9C%E6%8E%92%E6%9F%A5)
* **Centrum Pobierania Oprogramowania (RobotAssist, xCore, pakiety aktualizacyjne):**  
  [https://docs.rokae.com/docs/DownLoad](https://docs.rokae.com/docs/DownLoad)
* **Karta Techniczna i Specyfikacja Robota Rokae Helios:**  
  [https://www.rokae.com/en/product/show/596/Wheeled-Dual-Arm-Robot-Helios.html](https://www.rokae.com/en/product/show/596/Wheeled-Dual-Arm-Robot-Helios.html)
