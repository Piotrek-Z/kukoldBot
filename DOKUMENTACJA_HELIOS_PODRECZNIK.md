# Podręcznik wdrożeniowy i programistyczny — Rokae Helios

**Przeznaczenie:** punkt startowy dla programistów i inżynierów przejmujących rozwój robota Rokae Helios. Zawiera procedury uruchomienia, konfigurację sieci, programowanie ramion i torsu, sterowanie bazą AMR, obsługę kamer/audio, złącze narzędziowe oraz katalog diagnostyki. Jest to dokumentacja techniczna systemu — nie kronika konkretnych zadań.

> **Bezpieczeństwo:** wszelkie testy ruchu wykonuj z wolną strefą pracy, małą prędkością i gotowym dostępem do E-Stop. Nie wyłączaj ani nie rozszerzaj limitów bezpieczeństwa, nie obchodź interlocków i nie podłączaj obciążeń do złącza M8 bez sprawdzenia parametrów elektrycznych. Komendy oraz przykłady w tym podręczniku wymagają oceny ryzyka przez osobę odpowiedzialną za stanowisko.

## Szybka nawigacja

1. [Mapa systemu i adresacja](#1-mapa-systemu-i-adresacja)
2. [Uruchomienie, SSH, Wi-Fi i routing](#2-uruchomienie-ssh-wi-fi-i-routing)
3. [RobotAssist, bezpieczeństwo i rozwiązywanie problemów](#3-robotassist-bezpieczenstwo-i-rozwiazywanie-problemow)
4. [Ruch ramion 7-DoF i torsu 4+2-DoF przez Rokae SDK](#4-ruch-ramion-7-dof-i-torsu-42-dof-przez-rokae-sdk)
5. [Baza jezdna AMR / Matrix OS](#5-baza-jezdna-amr--matrix-os)
6. [Kamery i strumieniowanie obrazu](#6-kamery-i-strumieniowanie-obrazu)
7. [Mikrofon, STT i integracja ROS 2](#7-mikrofon-stt-i-integracja-ros-2)
8. [Głośniki](#8-g%C5%82o%C5%9Bniki)
9. [Złącze narzędziowe M8](#9-z%C5%82%C4%85cze-narz%C4%99dziowe-m8)
10. [Źródła i dalsza dokumentacja](#10-%C5%BAr%C3%B3d%C5%82a-i-dalsza-dokumentacja)

## 1. Mapa systemu i adresacja

Helios składa się z osobnych kontrolerów ruchu i komputera Jetson. Ramię, tors, baza jezdna oraz Jetson mają odrębne interfejsy i oprogramowanie — nie należy zakładać, że steruje nimi jeden wspólny SDK.

| Element | Adres / interfejs | Sterowanie / uwagi |
|---|---|---|
| Jetson AGX Orin — Wi-Fi w `JM-TRONIK_5G` | `192.168.49.97/24` | SSH, ROS 2 Humble, usługi kamer i audio |
| Jetson — podsieć sterowników (`LOCAL_IP`) | `192.168.71.51` | drugi argument konstruktora SDK dla ramion; interfejs do sieci robotów |
| Baza AMR / Matrix OS | `192.168.71.50:5002` | binarny WebSocket + Protobuf; panel `http://192.168.71.50/#/homePage` |
| Prawe ramię xMate ER Pro, 7 osi | `192.168.71.160` | `rokae::xMateErProRobot(IP, LOCAL_IP)` |
| Lewe ramię xMate ER Pro, 7 osi | `192.168.71.161` | `rokae::xMateErProRobot(IP, LOCAL_IP)` |
| Tors TaiHu, 4 osie + 2 osie głowy | `192.168.71.254` | `rokae::PCB4Robot(IP)`; 4 osie w `joints`, 2 zewnętrzne w `external` |
| Głowica 3D | USB do Jetsona | Orbbec Gemini 335L |
| Kamery USB | `/dev/video8`, `/dev/video10`, `/dev/video12` | numery parzyste to strumienie obrazu |
| Mikrofon/głośniki | ALSA `REF` (często karta 3) | stabilniejsza nazwa: `plughw:CARD=REF,DEV=0` |

Adres Wi-Fi może zależeć od aktualnej konfiguracji sieci. Do diagnozy sprawdź go na Jetsonie przez `ip a` i `nmcli device show`; nie zakładaj, że adres Wi-Fi i `LOCAL_IP` są tym samym interfejsem.

## 2. Uruchomienie, SSH, Wi-Fi i routing

### 2.1 Włączenie robota

1. Podłącz robota do zasilania.
2. Poczekaj, aż robot wyda **dwa sygnały dźwiękowe**.
3. Kliknij przycisk na plecach **dokładnie jeden raz**.
4. Poczekaj, aż system się uruchomi i sparuje. Nie klikaj przycisku wielokrotnie podczas oczekiwania.

Przed ruchem sprawdź, czy nie jest aktywny E-Stop, czy stanowisko jest bezpieczne oraz czy tryb sterowania odpowiada operacji.

### 2.2 SSH i Wi-Fi

Połącz się z Jetsonem przez SSH (użytkownik i host zależą od konfiguracji; w przekazanych notatkach używany jest `std`). Aby połączyć Jetsona z siecią Wi-Fi z poziomu terminala: 

```bash
sudo nmcli dev wifi connect "Moje_Wifi" password 'HASŁO'
```

Hasło umieszczaj w **pojedynczych cudzysłowach**. Pozwala to uniknąć interpretacji znaków specjalnych przez powłokę, w tym `$`, `!` i `#`. Nie publikuj haseł produkcyjnych w repozytorium ani w logach.

Przydatne polecenia: 

```bash
ip a
nmcli device show
nmcli dev wifi list
nmcli connection show --active
```

Adres Jetsona w sieci `JM-TRONIK_5G` podany dla tego stanowiska: `192.168.49.97/24`.

### 2.3 Dostęp z laptopa do podsieci kontrolerów

Sterowniki są w podsieci `192.168.71.0/24`, odrębnej od Wi-Fi. Jeżeli laptop nie ma bezpośredniej trasy do tej podsieci, wykorzystaj Jetsona jako router. Na Jetsonie włącz forwarding: 

```bash
sudo sysctl -w net.ipv4.ip_forward=1
```

Przykład reguły NAT (dopasuj interfejsy i politykę firewalla do wdrożenia; poniższa szeroka reguła nie powinna być kopiowana bez przeglądu):

```bash
sudo iptables -t nat -A POSTROUTING -s 192.168.49.0/24 -d 192.168.71.0/24 -j MASQUERADE
```

Na Windowsie uruchom CMD jako administrator i dodaj trasę przez adres Wi-Fi Jetsona: 

```cmd
route add 192.168.71.0 mask 255.255.255.0 192.168.49.97
```

W razie potrzeby trwałość konfiguracji `ip_forward` można ustawić w `/etc/sysctl.conf`; reguły zapory/NAT należy utrwalać dopiero po sprawdzeniu, że routing działa i nie otwiera niepożądanego dostępu. Objaw `TTL expired in transit` zwykle oznacza brak poprawnej trasy lub pętlę routingu. Zweryfikuj `ip route`, trasę Windows (`route print`) oraz osiągalność Jetsona i sterowników.

---


# 3. RobotAssist, bezpieczeństwo i rozwiązywanie problemów

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
* **Ramiona (Arms):** Dwa 7-osiowe manipulatory xMate. Prawe ramię: `192.168.71.160`; lewe ramię: `192.168.71.161`.
* **Komputer nadrzędny (NVIDIA Jetson / Orin / Xavier):** System Linux Ubuntu (`tegra aarch64`), na którym uruchamiany jest stos ROS 2. Posiada interfejs Wi-Fi/Ethernet w podsieci zewnętrznej (np. `192.168.49.97`) oraz interfejs wewnętrzny spięty z kontrolerami xCore w podsieci `192.168.71.0/24`.

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
[ Sieć Wi-Fi JM-TRONIK_5G ]  (Jetson: 192.168.49.97/24)
         ▲
         │ (Wi-Fi)
         ▼
[ Komputer Pokładowy Jetson ] (wlan0: 192.168.49.97)
         │
         │ (Wewnętrzny switch / eth0: 192.168.71.1)
         ▼
 ┌───────────────────────┬───────────────────────┐
 │                       │                       │
[Tors Helios]           [Prawe Ramię]           [Lewe Ramię]
192.168.71.254          192.168.71.160          192.168.71.161

         ▲
         │ (Wi-Fi / trasa do Jetsona)
[ Laptop Inżynierski ] (Windows, w tej samej sieci routowalnej)
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

Aby Jetson przekazywał pakiety z sieci zewnętrznej Wi-Fi do wewnętrznej podsieci kontrolerów xCore, zaloguj się na niego przez SSH (`std@192.168.49.97`) i wykonaj:

1. **Włączenie routingu IP w jądrze systemu:**
   ```bash
   sudo sysctl -w net.ipv4.ip_forward=1
   ```
2. **Włączenie translacji adresów (NAT / Masquerade):**
   ```bash
   sudo iptables -t nat -A POSTROUTING -j MASQUERADE
   ```

### 6.4 Konfiguracja Windowsa (Statyczna trasa route add)

Poinformuj system Windows, że brama do sieci `192.168.71.0/24` znajduje się pod adresem IP Jetsona (`192.168.49.97`):

1. Otwórz **Wiersz Poleceń (CMD)** jako **Administrator**.
2. Wpisz polecenie:
   ```cmd
   route add 192.168.71.0 mask 255.255.255.0 192.168.49.97
   ```
3. *Opcjonalnie (zapis na stałe po restarcie komputera):* dodaj przełącznik `-p`:
   ```cmd
   route -p add 192.168.71.0 mask 255.255.255.0 192.168.49.97
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

---

# 4. Ruch ramion 7-DoF i torsu 4+2-DoF przez Rokae SDK

# Tutorial: Programowanie Ruchu Ramion (7 DoF) i Torsu (4+2 DoF) w C++ (Rokae SDK)

> **Dla kogo jest ten dokument?**  
> Ten przewodnik powstał na bazie praktycznych doświadczeń i działającego kodu (`pos_zero.cpp`, `wave.cpp`, `gesty.cpp`) napisanego dla robota **Rokae Helios**. Wyjaśnia krok po kroku, jak bezpiecznie poruszać 7-osiowymi rękami oraz 4-osiowym torsem (wraz z 2-osiową głową) i **dlaczego przykłady z oficjalnej dokumentacji Rokae często nie działają lub prowadzą do błędów na tym konkretnym modelu.**

---

## Spis Treści

1. [Dlaczego oficjalna dokumentacja Rokae bywa myląca dla Heliosa?](#1-dlaczego-oficjalna-dokumentacja-rokae-bywa-myl%C4%85ca-dla-heliosa)
2. [Architektura w SDK: Adresy IP, Klasy C++ i Liczba Osi](#2-architektura-w-sdk-adresy-ip-klasy-c-i-liczba-osi)
3. [Zasady Ogólne: Jednostki, Wielowątkowość (Bug spdlog) i Kompilacja (Bug GCC 11)](#3-zasady-og%C3%B3lne-jednostki-wielow%C4%85tkowo%C5%9B%C4%87-bug-spdlog-i-kompilacja-bug-gcc-11)
4. [Krok 1: Bezpieczne Przygotowanie Ręki lub Torsu do Ruchu](#4-krok-1-bezpieczne-przygotowanie-r%C4%99ki-lub-torsu-do-ruchu)
5. [Krok 2: Prosty Ruch do Punktu – Tryb NRT (`MoveAbsJCommand`)](#5-krok-2-prosty-ruch-do-punktu--tryb-nrt-moveabsjcommand)
6. [Krok 3: Płynny Ruch Ciągły 7-osiowej Ręki – Pętla RT 1000 Hz (`1 - cos`)](#6-krok-3-p%C5%82ynny-ruch-ci%C4%85g%C5%82y-7-osiowej-r%C4%99ki--p%C4%99tla-rt-1000-hz-1---cos)
7. [Krok 4: Geometria 7 Osi – Pozycja Startowa vs Zero, Pułapka 344° i Lustro Rąk](#7-krok-4-geometria-7-osi--pozycja-startowa-vs-zero-pu%C5%82apka-344-i-lustro-r%C4%85k)
8. [Ściąga: Błędy Rokae SDK i Ich Rozwiązania](#8-%C5%9Bci%C4%85ga-b%C5%82%C4%99dy-rokae-sdk-i-ich-rozwi%C4%85zania)

---

## 1. Dlaczego oficjalna dokumentacja Rokae bywa myląca dla Heliosa?

Oficjalna dokumentacja Rokae xCore SDK jest pisana głównie pod pojedyncze, 6-osiowe ramiona przemysłowe lub coboty stojące na stole. W robocie humanoidalnym **Helios** trafisz na **7 kluczowych różnic**, których oficjalne przykłady nie uwzględniają:

| # | Co mówi oficjalna dokumentacja Rokae | Jak jest naprawdę w robocie Helios |
| :-: | :--- | :--- |
| **1** | Jeden typ robota w przykładach | Helios ma **dwie zupełnie różne klasy robotów**: ramiona to 7-osiowe coboty `rokae::xMateErProRobot`, a tułów to 4-osiowy robot przemysłowy `rokae::PCB4Robot` (który ma dodatkowo 2 osie zewnętrzne głowy!). |
| **2** | Konstruktor przyjmuje tylko IP robota | Dla **ramion** konstruktor wymaga **dwóch adresów IP**: `xMateErProRobot(IP_RAMIENIA, LOCAL_IP)` (bez adresu karty sieciowej Jetsona nie zadziała tryb RT). Dla **torsu** podaje się tylko jeden adres: `PCB4Robot(TULOW_IP)`. |
| **3** | Oczekiwanie na koniec ruchu przez `queryEventInfo()` | Na komputerze pokładowym Jetson (Ubuntu 22.04, **GCC 11**) użycie `queryEventInfo()` powoduje **twardy błąd kompilacji** (`static_assert` w `bits/hashtable.h` przez `std::unordered_map<std::string, std::any>`). Koniec ruchu trzeba sprawdzać własną pętlą odpytującą pozycję lub stan! |
| **4** | `setPowerState(true)` od razu włącza silniki | Fizyczne załączenie styczników trwa **od kilkuset milisekund do kilku sekund**. Wysłanie komendy ruchu od razu po `setPowerState(true)` kończy się błędem `motor power state unsupported operation`. Trzeba w pętli czekać na `PowerState::on`. |
| **5** | Możesz tworzyć obiekty dwóch robotów w osobnych wątkach | Konstruktor `rokae::Robot` rejestruje globalny logger `spdlog` o stałej nazwie `"_rokae_log_console_"`. Równoczesne utworzenie obiektu lewej i prawej ręki w dwóch wątkach zabija cały program (`SIGABRT`!). Tworzenie, łączenie i niszczenie obiektów **musi być chronione mutexem**. |
| **6** | Limity osi 7-osiowego xMate ER Pro to $\pm 170^\circ$ / $\pm 120^\circ$ | Ze względu na boczny montaż barków w Heliosie, sama **pozycja startowa prawej ręki ma `J1 = -173.06°`** (poza katalogowym $\pm 170^\circ$). Co więcej, pomylenie znaku `-171°` z `+171°` powoduje obrót barku o **344°**! |
| **7** | Do ruchu przez kilka punktów użyj `rtCon->MoveJ()` | `MoveJ` zatrzymuje rękę do zera w każdym punkcie, a przełączenie z pętli RT na `MoveJ` (np. żeby wrócić do pozycji startowej) często zawodzi i zostawia rękę w powietrzu. Płynny ruch (machanie, gesty) + powrót trzeba realizować w **jednej ciągłej pętli RT (co 1 ms)** z limitem przyspieszenia $\le 40^\circ/\text{s}^2$. |

---

## 2. Architektura w SDK: Adresy IP, Klasy C++ i Liczba Osi

Wewnątrz Heliosa każdy moduł wykonawczy ma własny kontroler w wewnętrznej podsieci Ethernet `192.168.71.0/24`.

### Wymagane nagłówki
```cpp
#include "rokae/robot.h"
#include "rokae/data_types.h"
```

### Stałe adresów IP i klasy obiektów

```cpp
// Adresy kontrolerów w wewnętrznej sieci robota
const std::string PRAWA_IP = "192.168.71.160"; // Prawa ręka (7 osi: J1..J7)
const std::string LEWA_IP  = "192.168.71.161"; // Lewa ręka  (7 osi: J1..J7)
const std::string TULOW_IP = "192.168.71.254"; // Tułów/Tors (4 osie: J1..J4 + 2 osie głowy)

// Adres IP karty sieciowej Jetsona w podsieci wewnętrznej (wymagany dla ramion!)
const std::string LOCAL_IP = "192.168.71.51";
```

### Tworzenie obiektów dla poszczególnych modułów

```cpp
// 1. Ramiona (7 DoF) - klasa xMateErProRobot (wymaga LOCAL_IP jako drugiego argumentu)
rokae::xMateErProRobot prawa(PRAWA_IP, LOCAL_IP);
rokae::xMateErProRobot lewa(LEWA_IP, LOCAL_IP);

// 2. Tułów + Głowa (4 + 2 DoF) - klasa PCB4Robot (tylko 1 argument IP!)
rokae::PCB4Robot tulow(TULOW_IP);
```

---

## 3. Zasady Ogólne: Jednostki, Wielowątkowość (Bug spdlog) i Kompilacja (Bug GCC 11)

### 3.1. Jednostki: Stopnie vs Radiany
* W programie **RobotAssist (GUI)** wszystkie kąty osi widzisz w **stopniach (`deg`)**.
* W **C++ SDK** wszystkie funkcje (`jointPos()`, `JointPosition`, `MoveAbsJCommand`, `MoveJ`) przyjmują i zwracają kąty **wyłącznie w radianach (`rad`)**!
* **Zasada:** Punkty w kodzie przechowuj czytelnie w stopniach i przeliczaj je pomocniczą funkcją przed wysłaniem do SDK:

```cpp
using A7 = std::array<double, 7>;

A7 deg2rad7(const A7& deg) {
    A7 rad{};
    for (size_t i = 0; i < 7; i++) {
        rad[i] = deg[i] * M_PI / 180.0;
    }
    return rad;
}
```

### 3.2. Bug wielowątkowości `spdlog` – jak sterować dwiema rękami naraz?
Jeśli chcesz uruchomić ruch lewej i prawej ręki równolegle w dwóch wątkach (`std::thread`), **nie wolno** tworzyć obiektów `xMateErProRobot` jednocześnie. Biblioteka Rokae tworzy wewnętrznie logger o stałej nazwie `"_rokae_log_console_"` i przy wyścigu wątków rzuca:
> `spdlog fatal error: logger with name '_rokae_log_console_' already exists` (`SIGABRT`)

**Rozwiązanie (wzorzec z `gesty.cpp`):** Użyj globalnego `std::mutex` wokół tworzenia, łączenia (`connectToRobot`) oraz niszczenia obiektu robota:

```cpp
std::mutex g_sdkMutex;

void watekRamienia(const char* ip) {
    std::unique_ptr<rokae::xMateErProRobot> robotPtr;
    // Strażnik RAII zapewniający bezpieczne zniszczenie obiektu pod mutexem
    struct Zniszcz {
        std::unique_ptr<rokae::xMateErProRobot>& p;
        ~Zniszcz() {
            std::lock_guard<std::mutex> lock(g_sdkMutex);
            p.reset();
        }
    } zniszcz{robotPtr};

    std::error_code ec;
    {
        std::lock_guard<std::mutex> lock(g_sdkMutex);
        robotPtr = std::make_unique<rokae::xMateErProRobot>(ip, LOCAL_IP);
        robotPtr->connectToRobot(ec);
    }
    if (ec) return;

    // Dalej w wątku możesz już wykonywać ruchy równolegle z drugą ręką!
}
```

### 3.3. Bug kompilacji `queryEventInfo()` na GCC 11
**Nigdy nie wywołuj `robot.queryEventInfo(...)`** w kodzie kompilowanym na Jetsonie (GCC 11). Nagłówek SDK używa niekompletnego typu w `std::unordered_map<std::string, std::any>`, co wywołuje błąd `static_assert` w bibliotece standardowej C++. Zamiast tego używaj funkcji odpytujących `jointPos()` lub `operationState()`, pokazanych w Kroku 2.

---

## 4. Krok 1: Bezpieczne Przygotowanie Ręki lub Torsu do Ruchu

Zanim wyślesz jakąkolwiek komendę ruchu, musisz przeprowadzić kontroler przez odpowiednią sekwencję startową. Pominięcie któregokolwiek kroku sprawi, że robot zignoruje komendę lub rzuci błąd.

### 4.1. Sprawdzenie, czy ręka nie jest już w ruchu (`OperationState::moving`)
Jeśli poprzedni ruch został przerwany lub w tle działa inna komenda, ręka ma stan `moving`. Próba zmiany trybu pracy lub zasilania w tym stanie zwróci błąd:  
`robot state (moving) unsupported operation`.

```cpp
bool upewnijSieZeRekaStoi(rokae::xMateErProRobot& robot, const std::string& nazwa) {
    auto stoi = [&]() {
        std::error_code e;
        const auto st = robot.operationState(e);
        return !e && st != rokae::OperationState::moving;
    };
    if (stoi()) return true;

    std::cerr << "[" << nazwa << "] Ręka w stanie 'moving' - zatrzymuję (stop + moveReset)...\n";
    std::error_code ec;
    robot.stop(ec);      ec.clear();
    robot.moveReset(ec); ec.clear();

    const auto koniec = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < koniec) {
        if (stoi()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false; // Po 10 s nadal w ruchu - nie ruszaj zasilaniem!
}
```

### 4.2. Włączenie zasilania silników z oczekiwaniem na styczniki (`PowerState::on`)
Wywołanie `robot.setPowerState(true, ec)` tylko **zleca** włączenie silników. Musisz zaczekać w pętli (do 10 s), aż `robot.powerState(ec)` faktycznie zwróci `rokae::PowerState::on`:

```cpp
template <typename Robot>
bool ensureMotorPowerOn(Robot& robot, const std::string& moduleName) {
    std::error_code ec;
    const auto state = robot.powerState(ec);

    if (!ec) {
        if (state == rokae::PowerState::on) return true;
        if (state == rokae::PowerState::estop) {
            std::cerr << "[" << moduleName << "] E-STOP wciśnięty! Zwolnij grzybek.\n";
            return false;
        }
        if (state == rokae::PowerState::gstop) {
            std::cerr << "[" << moduleName << "] Otwarta osłona bezpieczeństwa (gstop).\n";
            return false;
        }
    }

    ec.clear();
    robot.setPowerState(true, ec);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ec.clear();
        if (robot.powerState(ec) == rokae::PowerState::on && !ec) {
            return true; // Styczniki załączone, silniki gotowe!
        }
    }
    std::cerr << "[" << moduleName << "] Zasilanie nie włączyło się w 10 s.\n";
    return false;
}
```

### 4.3. Wyłączenie trybu wodzenia ręcznego (`drag`) – tylko dla ramion!
* Ramiona (`xMateErProRobot`) mają tryb `drag` (luźne prowadzenie ręką). Jeśli ktoś zostawił włączony `drag`, wywołanie `moveStart()` rzuci nieoczywisty błąd: **`task_space unsupported operation`**.
* Z kolei tułów (`PCB4Robot`) **nie posiada w ogóle metody `disableDrag()`** w swoim API!
* Co gorsza, czasami kontroler ramienia blokuje się w trybie `drag` i zwykłe `disableDrag()` zwraca błąd – wtedy trzeba **wyłączyć zasilanie silników, wywołać `disableDrag()` na wyłączonych silnikach i włączyć zasilanie ponownie** (sprawdzony przepis z `pos_zero.cpp`):

```cpp
// Wykrywanie w czasie kompilacji, czy dany typ robota ma metodę disableDrag()
template <typename T, typename = void>
struct HasDisableDrag : std::false_type {};

template <typename T>
struct HasDisableDrag<T, std::void_t<
    decltype(std::declval<T&>().disableDrag(std::declval<std::error_code&>()))
>> : std::true_type {};

template <typename Robot>
void disableDragIfActive(Robot& robot, const std::string& moduleName) {
    if constexpr (!HasDisableDrag<Robot>::value) {
        return; // Dla tułowia (PCB4Robot) nic nie robimy
    } else {
        std::error_code ec;
        if (robot.operationState(ec) != rokae::OperationState::drag || ec) return;

        ec.clear();
        robot.disableDrag(ec);
        if (ec) {
            // Procedura awaryjna: stop -> zasilanie OFF -> disableDrag -> zasilanie ON
            std::error_code e2;
            robot.stop(e2);                e2.clear();
            robot.moveReset(e2);           e2.clear();
            robot.setPowerState(false, e2);
            for (int i = 0; i < 40; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::error_code e3;
                if (robot.powerState(e3) == rokae::PowerState::off && !e3) break;
            }
            e2.clear();
            robot.disableDrag(e2);
            ensureMotorPowerOn(robot, moduleName);
        }
    }
}
```

### 4.4. Specyfika przygotowania Torsu (`PCB4Robot`)
Tors (`192.168.71.254`) wymaga wyczyszczenia stanów `recoverState(1..3)`, wyłączenia trybu kasety `setTeachPendantMode(false, ec)` i działa w trybie `NrtCommand` (odczekaj ok. 2 sekundy po włączeniu zasilania torsu):

```cpp
bool przygotujTulow(rokae::PCB4Robot& robot) {
    std::error_code ec;
    robot.recoverState(1, ec);
    robot.recoverState(2, ec);
    robot.recoverState(3, ec);
    robot.clearServoAlarm(ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    robot.setTeachPendantMode(false, ec);
    robot.setOperateMode(rokae::OperateMode::automatic, ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    robot.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);
    robot.setPowerState(true, ec);
    if (ec) return false;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return true;
}
```

---

## 5. Krok 2: Prosty Ruch do Punktu – Tryb NRT (`MoveAbsJCommand`)

Tryb **NRT (`MotionControlMode::NrtCommand`)** jest najprostszy i najbezpieczniejszy, gdy chcesz po prostu przestawić ramię lub tułów z obecnej pozycji do jednego punktu docelowego (np. pozycja zero, pozycja startowa). Kontroler sam planuje trajektorię.

### 5.1. Ruch 7-osiowej ręki w trybie NRT (na bazie `pos_zero.cpp` i `wave.cpp`)

```cpp
std::error_code ec;
robot.setOperateMode(rokae::OperateMode::manual, ec); // lub automatic
ensureMotorPowerOn(robot, "Lewa ręka");
disableDragIfActive(robot, "Lewa ręka");

robot.setMotionControlMode(rokae::MotionControlMode::NrtCommand, ec);

// 1. Zbuduj docelową pozycję 7 osi (w radianach!)
// Przykład A: Wszystkie 7 osi na 0.0 rad (pozycja zero)
rokae::JointPosition zeroTarget(7, 0.0);

// Przykład B: Konkretne kąty 7 osi ze stopni
std::array<double, 7> cel_rad = deg2rad7({2.22, 87.30, -89.40, 41.27, 93.61, 1.27, 39.44});
rokae::JointPosition targetPos;
targetPos.joints.assign(cel_rad.begin(), cel_rad.end());

// 2. Utwórz komendę MoveAbsJCommand (prędkość w mm/s, np. 100-200 mm/s, strefa 0.0 = dokładny dojazd)
rokae::MoveAbsJCommand command(targetPos, 200.0, 0.0);

// 3. Wyczyść kolejkę, dodaj komendę i wystartuj ruch
robot.moveReset(ec);
std::string cmdID;
robot.moveAppend(command, cmdID, ec);
robot.moveStart(ec);
```

### 5.2. Ruch Torsu (4 osie) i Głowy (2 osie zewnętrzne) w trybie NRT
> ⚠️ **Ważna różnica dla Torsu (`PCB4Robot`):**  
> Obiekt `rokae::JointPosition` dla torsu ma **dwa wektory**:
> * `target_pos.joints` – **4 osie tułowia** (`J1, J2, J3, J4`, w radianach),
> * `target_pos.external` – **2 osie głowy** (w radianach)!

```cpp
if (przygotujTulow(tulow)) {
    std::error_code ec;
    tulow.moveReset(ec);

    rokae::JointPosition target_pos;
    target_pos.joints   = {0.0, 0.0, 0.0, 0.0}; // 4 osie torsu (rad)
    target_pos.external = {0.0, 0.0};           // 2 osie głowy (rad)

    rokae::MoveAbsJCommand cmd(target_pos, 50.0); // prędkość np. 50 mm/s
    std::string cmdID;
    tulow.moveAppend(cmd, cmdID, ec);
    tulow.moveStart(ec);
}
```

### 5.3. Jak czekać na koniec ruchu NRT (zamiast niedziałającego `queryEventInfo`)?

Masz do wyboru dwa sprawdzone podejścia:

1. **Krótkie oczekiwanie po `operationState()` (z `wave.cpp`):**
   ```cpp
   // Najpierw daj kontrolerowi 150-500 ms na fizyczne rozpoczęcie ruchu!
   std::this_thread::sleep_for(std::chrono::milliseconds(500));
   const auto t0 = std::chrono::steady_clock::now();
   while (true) {
       std::error_code ec;
       if (robot.operationState(ec) != rokae::OperationState::moving || ec) break;
       std::this_thread::sleep_for(std::chrono::milliseconds(50));
       if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(60)) break;
   }
   ```
2. **Dokładne śledzenie kątów `jointPos()` (z `pos_zero.cpp`):**
   * Najpierw czekasz maksymalnie 5 sekund, aż zmiana kąta dowolnej osi przekroczy `0.02°` (dzięki temu wiesz, czy kontroler w ogóle przyjął komendę i ruszył).
   * Następnie czekasz, aż przez 3 kolejne odczyty (co 100 ms) zmiana kątów spadnie poniżej `0.02°`.

---

## 6. Krok 3: Płynny Ruch Ciągły 7-osiowej Ręki – Pętla RT 1000 Hz (`1 - cos`)

Gdy chcesz wykonać wieloetapowy, płynny ruch (np. machanie ręką w `wave.cpp` albo gestykulację podczas mowy w `gesty.cpp`), tryb NRT ani funkcja `rtCon->MoveJ()` się nie sprawdzą, ponieważ **zatrzymują rękę do zera w każdym punkcie pośrednim**.

Zamiast tego używa się **pętli czasu rzeczywistego (`MotionControlMode::RtCommand`)**, w której Twój callback C++ jest wywoływany przez kontroler **co 1 milisekundę (1000 Hz)** i zwraca natychmiastową pozycję wszystkich 7 osi.

### 6.1. Cztery żelazne zasady pętli RT w Heliosie (odkryte w praktyce!)

1. **Zwiększ tolerancję sieci RT (`setRtNetworkTolerance`) PRZED wejściem w `RtCommand`!**  
   Pętla RT wymaga pakietu co 1 ms. Gdy komputer pokładowy Jetson jest zajęty (np. działa mikrofon, Whisper STT, odtwarzanie audio lub LLM), pakiet potrafi się spóźnić o kilka milisekund. Domyślnie kontroler uznaje to za awarię sieci i **gwałtownie zatrzymuje rękę w losowym miejscu**.  
   Wywołaj `robot.setRtNetworkTolerance(60, ec)` **przed** `setMotionControlMode(RtCommand)`:
   ```cpp
   robot.recoverState(1, ec);
   robot.clearServoAlarm(ec);
   std::this_thread::sleep_for(std::chrono::milliseconds(300));
   robot.setOperateMode(rokae::OperateMode::automatic, ec);

   // Ustaw tolerancję utraty pakietów RT (np. 60%) PRZED przełączeniem w RtCommand!
   robot.setRtNetworkTolerance(60, ec);

   robot.setMotionControlMode(rokae::MotionControlMode::RtCommand, ec);
   ensureMotorPowerOn(robot, "PRAWA");
   ```

2. **Pierwsza komenda w pętli MUSI być równa aktualnej pozycji `robot.jointPos(ec)`!**  
   Jeśli w `t = 0` wyślesz do kontrolera inną pozycję niż ta, w której fizycznie stoi ramię, kontroler wyliczy nieskończoną prędkość i natychmiast odetnie zasilanie silników.

3. **Używaj interpolacji `1 - cos` i pilnuj limitu przyspieszenia ($\le 40^\circ/\text{s}^2$)!**  
   Aby ręka ruszała i hamowała łagodnie (prędkość = 0 na końcach odcinka), używaj współczynnika:
   $$s(t) = \frac{1 - \cos(\pi \cdot t / T)}{2} \in [0, 1]$$
   Szczytowe przyspieszenie na osi pokonującej kąt $\Delta\theta$ w czasie $T$ wynosi:
   $$a_{\max} = \frac{\Delta\theta \cdot \pi^2}{2 \cdot T^2}$$
   > ⚠️ **UWAGA:** Wewnętrzny czujnik bezpieczeństwa w kontrolerze ramienia Heliosa **przerywa ruch, gdy przyspieszenie przekroczy ok. $50^\circ/\text{s}^2$**!  
   Dlatego minimalny bezpieczny czas odcinka $T$ dla maksymalnego przyspieszenia $a_{\max} = 40^\circ/\text{s}^2$ wyliczaj ze wzoru (z `gesty.cpp`):
   ```cpp
   constexpr double MAX_ACC_RAD = 40.0 * M_PI / 180.0; // 40 deg/s^2
   constexpr double MIN_CZAS_S  = 2.0;

   double obliczBezpiecznyCzas(const A7& od_rad, const A7& do_rad) {
       double maxDelta = 0.0;
       for (size_t i = 0; i < 7; i++) {
           maxDelta = std::max(maxDelta, std::fabs(do_rad[i] - od_rad[i]));
       }
       const double t = std::sqrt(maxDelta * M_PI * M_PI / (2.0 * MAX_ACC_RAD));
       return std::max(MIN_CZAS_S, t);
   }
   ```

4. **Powrót do pozycji startowej wykonuj W TEJ SAMEJ pętli RT!**  
   W pierwszych wersjach kodu pętla RT kończyła się po machaniu w punkcie `A`, a powrót do pozycji startowej miał wykonać zwykły `MoveJ`. W praktyce przełączanie trybu po pętli RT często zawodziło i ręka zostawała podniesiona w powietrzu.  
   **Rozwiązanie:** Cały cykl (`Start -> A -> machanie A<->B -> powrót A->Start`) zaprogramuj w jednym callbacku RT i dopiero po dojechaniu z powrotem do `Start` wywołaj `cmd.setFinished()`.

### 6.2. Szablon kompletnej pętli RT (Dojazd $\rightarrow$ Ruch $\rightarrow$ Powrót)

```cpp
auto rtCon = robot.getRtMotionController().lock();
if (!rtCon) return false;

std::error_code ec;
const A7 start = robot.jointPos(ec); // Punkt startowy = aktualna fizyczna pozycja!
const A7 cel   = deg2rad7(CEL_DEG);

const double tTam    = obliczBezpiecznyCzas(start, cel);
const double tPowrot = obliczBezpiecznyCzas(cel, start);
double czas = 0.0;

std::function<rokae::JointPosition()> callback = [&]() {
    czas += 0.001; // Wywoływane dokładnie co 1 ms (1000 Hz)
    rokae::JointPosition cmd;
    cmd.joints.resize(7);

    if (czas < tTam) {
        // Faza 1: Płynny dojazd ze start do cel
        const double s = (1.0 - std::cos(M_PI * czas / tTam)) / 2.0;
        for (size_t i = 0; i < 7; i++)
            cmd.joints[i] = start[i] + (cel[i] - start[i]) * s;
    } else if (czas < tTam + tPowrot) {
        // Faza 2: Płynny powrót z cel do start
        const double tau = czas - tTam;
        const double s = (1.0 - std::cos(M_PI * tau / tPowrot)) / 2.0;
        for (size_t i = 0; i < 7; i++)
            cmd.joints[i] = cel[i] + (start[i] - cel[i]) * s;
    } else {
        // Faza 3: Koniec ruchu - jesteśmy z powrotem w start
        for (size_t i = 0; i < 7; i++) cmd.joints[i] = start[i];
        cmd.setFinished(); // Kończy startLoop()
    }
    return cmd;
};

try {
    rtCon->setControlLoop(callback);
    rtCon->startMove(rokae::RtControllerMode::jointPosition);
    rtCon->startLoop(true); // Blokuje wątek aż do wywołania cmd.setFinished()
} catch (const std::exception& e) {
    std::cerr << "Błąd w pętli RT: " << e.what() << "\n";
}
```

> 💡 **Co jeśli pętla RT zostanie przerwana błędem w połowie ruchu?**  
> W `wave.cpp` zastosowano funkcję ratunkową `powrotPrawejNrt()`, która:
> 1. Wywołuje `stop()` i `moveReset()`.
> 2. Wyłącza silniki `setPowerState(false)` i czeka 4 s na `PowerState::off`.
> 3. Wywołuje `clearServoAlarm()` oraz `recoverState(1)`.
> 4. Przełącza w tryb `manual`, włącza silniki `setPowerState(true)` (czekając na `PowerState::on`), ustawia `NrtCommand` i wysyła bezpieczny `MoveAbsJCommand` z prędkością `100 mm/s` do pozycji startowej.

---

## 7. Krok 4: Geometria 7 Osi – Pozycja Startowa vs Zero, Pułapka 344° i Lustro Rąk

### 7.1. Pozycja Zero (`0°`) to NIE jest Pozycja Startowa!
* **Pozycja Zero (`posZero`):** Wszystkie 7 osi ustawione na `0.0°`. Służy do kalibracji/transportu/diagnostyki.
* **Pozycja Startowa (`posStart`):** Naturalna pozycja spoczynkowa rąk wzdłuż ciała robota, z której startują wszystkie gesty i machanie:

```cpp
// Sprawdzone pozycje startowe ramion Heliosa (J1..J7 w stopniach):
const std::array<double, 7> LEWA_START_DEG  = {   2.22,  87.30,  -89.40, 41.27,  93.61,  1.27, 39.44};
const std::array<double, 7> PRAWA_START_DEG = {-173.06, -79.35, -103.52, 42.80, -84.28, -8.30, -2.92};
```

> ⚠️ **Zabezpieczenie z `gesty.cpp`:** Zanim uruchomisz trajektorię gestów, zawsze sprawdź, czy każda z 7 osi stoi w promieniu maksymalnie **`20°`** od `LEWA_START_DEG` / `PRAWA_START_DEG`. Jeśli robot stoi w pozycji zero (`0°`), bezpośredni ruch do punktów gestów poprowadzi ramię po nieprzewidywalnym torze!

### 7.2. Pułapka interpolacji w przestrzeni jointów ($\pm 180^\circ$ = obrót o $344^\circ$!)
Ramię w trybie osiowym (`jointPosition` / `MoveAbsJ`) interpoluje kąty **liniowo od wartości początkowej do końcowej**, a **nie** „najkrótszą drogą po okręgu”.

* **Przykład z życia (`wave.cpp`):**  
  W pozycji startowej prawa ręka ma oś barku **`J1 = -173.06°`**.  
  Jeśli uczysz punkt machania i wpiszesz **`J1 = +171.0°`** (co w przestrzeni 3D jest niemal tym samym kątem co `-189°`), ramię spróbuje przejechać z `-173.06°` do `+171.0°` przez zero, wykonując **pełny wymach barkiem o $344.06^\circ$**!  
  **Rozwiązanie:** Zawsze zachowuj spójny znak kąta blisko granicy $\pm 180^\circ$ (w `wave.cpp` użyto **`J1 = -171.0°`**, dzięki czemu oś J1 obraca się tylko o bezpieczne $2^\circ$).  
  Dodatkowo przed ruchem warto w kodzie policzyć $\max_i | \theta_{\text{cel}}[i] - \theta_{\text{start}}[i] |$ i wypisać ostrzeżenie, jeśli którykolwiek joint ma wykonać obrót $> 120^\circ$.

### 7.3. Jak automatycznie odbić punkty z Lewej ręki na Prawą (Lustrzana symetria 7 osi)?
Ponieważ lewe i prawe ramię są zamontowane symetrycznie po obu stronach korpusu, a ich pozycje startowe mają przeliczoną konfigurację barku, nie musisz ręcznie uczyć tych samych gestów dla obu rąk.

W `gesty.cpp` wyprowadzono wzór na przeliczenie dowolnego punktu lewej ręki na odpowiadający mu punkt prawej ręki względem ich pozycji startowych:

$$\text{Prawa}[i] = \text{PRAWA\_START}[i] + \text{ZNAK}[i] \cdot \left(\text{Lewa}[i] - \text{LEWA\_START}[i]\right)$$

gdzie wektor znaków dla osi `J1..J7` wynosi:
```cpp
// Osie J1, J2, J3, J5, J7 zmieniają znak (-1), a osie J4 (łokieć) i J6 (+1) zachowują znak:
const std::array<double, 7> ZNAK_PRAWA = {-1, -1, -1, +1, -1, +1, -1};

std::array<double, 7> odbijPunktNaPrawa(const std::array<double, 7>& lewa_deg) {
    std::array<double, 7> prawa_deg{};
    for (size_t i = 0; i < 7; i++) {
        prawa_deg[i] = PRAWA_START_DEG[i] + ZNAK_PRAWA[i] * (lewa_deg[i] - LEWA_START_DEG[i]);
    }
    return prawa_deg;
}
```
*(Przy pierwszym teście nowych punktów odbitych lustrzanie zawsze zmniejsz prędkość i trzymaj rękę na grzybku E-Stop!).*

---

## 8. Ściąga: Błędy Rokae SDK i Ich Rozwiązania

| Komunikat błędu / Objaw | Co oznacza / Dlaczego wystąpił | Jak naprawić w kodzie C++ |
| :--- | :--- | :--- |
| **`motor power state unsupported operation`** | Wysłano komendę ruchu (`moveAppend` / `moveStart` / start RT), zanim styczniki silników fizycznie się załączyły po `setPowerState(true)`. | Po `setPowerState(true, ec)` dodaj pętlę czekającą aż `robot.powerState(ec) == rokae::PowerState::on` (z timeoutem 10 s). |
| **`task_space unsupported operation`** przy `moveStart()` | Ramię znajduje się w trybie wodzenia ręcznego (`OperationState::drag`). | Wywołaj `robot.disableDrag(ec)`. Jeśli zwróci błąd, wyłącz zasilanie (`setPowerState(false)`), wywołaj `disableDrag(ec)` na wyłączonych silnikach i włącz zasilanie ponownie. |
| **`robot state (moving) unsupported operation`** | Próba wywołania `setPowerState` lub `setOperateMode`, gdy ramię jeszcze wykonuje poprzedni ruch (stan `OperationState::moving`). | Przed przygotowaniem ręki sprawdź `operationState()`. Jeśli jest `moving`, wywołaj `stop()` + `moveReset()` i zaczekaj w pętli aż stan zmieni się na `idle`. |
| **`spdlog fatal error: logger with name '_rokae_log_console_' already exists`** (`SIGABRT`) | Dwa wątki jednocześnie wywołały konstruktor `rokae::xMateErProRobot`. | Opakuj tworzenie (`make_unique`), `connectToRobot()` oraz niszczenie (`reset()`) obiektów robota wspólnym `std::mutex`. |
| **Błąd kompilacji w `bits/hashtable.h` (`static_assert`)** | Użyto `robot.queryEventInfo()` na kompilatorze GCC 11 (Ubuntu 22.04 na Jetsonie). | Nie używaj `queryEventInfo()`. Sprawdzaj koniec ruchu odpytując w pętli `robot.operationState(ec)` lub zmiany kątów z `robot.jointPos(ec)`. |
| **Ręka w pętli RT nagle zatrzymuje się w połowie ruchu (`hasMotionError: TAK`)** | 1) Zbyt duże przyspieszenie ($> 50^\circ/\text{s}^2$), **LUB**<br>2) Jetson był obciążony (audio/AI) i pakiet 1 ms się spóźnił. | 1) Wydłuż czas odcinka $T$ tak, by $a_{\max} = \frac{\Delta\theta \cdot \pi^2}{2 T^2} \le 40^\circ/\text{s}^2$.<br>2) Przed `setMotionControlMode(RtCommand)` wywołaj `robot.setRtNetworkTolerance(60, ec)`. |
| **Po zakończeniu machania w RT ręka nie wraca przez `MoveJ` i zostaje w górze** | Przełączenie kontrolera z pętli `RtCommand` (`jointPosition`) na `MoveJ` często się nie udaje bez pełnego resetu zasilania. | Włącz fazę powrotu do pozycji startowej bezpośrednio do callbacku pętli RT (`1 - cos` do punktu startowego) i dopiero tam wywołaj `cmd.setFinished()`. |

---

# 5. Baza jezdna AMR / Matrix OS — referencja protokołu, biblioteki i CLI

# Dokumentacja — Helios AMR (baza jezdna, system Matrix OS)
### Referencja protokołu, biblioteki C++ i narzędzi

**Wersja:** 0.3 · **Data:** 2026-10-08 · **Zakres:** baza jezdna (ramiona: SDK ROKAE, poza zakresem)

Dokument **referencyjny**: opisuje interfejsy, pola i kontrakty. Można go czytać wyrywkowo.
Metodyka dochodzenia do tych ustaleń i katalog problemów: [`PRZEWODNIK.md`](PRZEWODNIK.md).

---

## 1. Konwencje dokumentu

### 1.1 Etykiety pewności

| Etykieta | Znaczenie |
|---|---|
| `[R]` | potwierdzone na prawdziwym robocie |
| `[P]` | ustalone z przechwyconego ruchu panelu Matrix |
| `[A]` | sprawdzone tylko na atrapie (`tools/bench_server.py`) |
| `[?]` | hipoteza lub brak weryfikacji — nie zakładać w kodzie produkcyjnym |

### 1.2 Konwencje zapisu

- **hex** — zawsze treść ramki („body"), **bez** 4-bajtowego prefiksu długości.
- **Ścieżka pola** — kropki od początku treści: `6.3` = pole 6 → podpole 3. Numeracja pól
  protobufa (1-based), zgodna z protobufem.
- **Jednostki SI**: mm, mm/s, mrad, mrad/s (1000 mrad/s = 57,3°/s), ms, Hz.
- **„treść" (ang. content)** — podkomunikat wewnątrz pola koperty (4, 5 lub 6).

### 1.3 Słownik

| Termin | Znaczenie |
|---|---|
| **koperta (envelope)** | zewnętrzny komunikat każdej ramki: rodzaj, seq, sesja, treść (rozdz. 4) |
| **body** | pełna treść ramki bez prefiksu długości; to zapisują narzędzia |
| **kanał komend** | pole 6 koperty — tu jadą komendy ruchu (`kind=2`), patrz rozdz. 6 |
| **opkod** | kod operacji w kanale komend (pole `6.2`): 6, 7, 16, 32, 111 |
| **grant sterowania** | prawo do wysyłania jogu; nadawane przez `op=6`+`op=111`, odbierane przez `op=7` |
| **re-arm** | ponowne wysłanie `op=6`+`op=111` przed nowym ruchem |
| **jog** | strumień komend prędkości (`op=16`) wysyłany cyklicznie |

---

## 2. Architektura i adresacja

```
[panel www]──┐
             ├──ws://192.168.71.50:5002──>[ baza Helios: Matrix OS ]
[amr_*]──────┘                                   ▲
   ▲                                             │
   └── opcjonalnie przez amr_proxy (MITM, 127.0.0.1:5002)
[atrapa bench_server.py  :5003]  ← do testów bez robota (inny port, ten sam protokół)
```

| Element | Rola | Adres domyślny |
|---|---|---|
| Baza Helios | urządzenie sterowane | `192.168.71.50:5002` (WebSocket) |
| Panel Matrix | oryginalny klient (web) | `http://192.168.71.50/#/homePage` |
| `amr_*` | nasi klienci (C++) | łączą się jak panel |
| `amr_proxy` | podsłuch dwukierunkowy | nasłuch `0.0.0.0:5002`, przekazanie na bazę |
| `bench_server.py` | atrapa bazy (testy) | `0.0.0.0:5003` |

### 2.1 Domyślne poświadczenia

Wartości poniżej są domyślne i przeznaczone do sieci robota; jeśli konfiguracja urządzenia na to pozwala, zmień je i ogranicz dostęp sieciowy. Nie używaj tego samego hasła poza odizolowaną siecią stanowiska.


| Parametr | Wartość domyślna |
|---|---|
| host / port | `192.168.71.50` / `5002` |
| użytkownik | `admin` |
| hasło | MD5 w hex: `21232f297a57a5a743894a0e4a801fc3` (MD5 z `admin`) |
| timeout połączenia | 3000 ms |
| heartbeat | 3000 ms (0 = wyłączony) |

Zmiana hasła: `echo -n 'haslo' | md5sum` → `--pass-md5`.

---

## 3. Warstwa sieciowa

### 3.1 Transport

| Cecha | Wartość | Status |
|---|---|---|
| protokół | WebSocket (`ws://`), ramki **binarne** | `[R]` |
| port | 5002 | `[R]` |
| kompresja / TLS | brak (nie zaobserwowano) | `[P]` |
| utrzymanie sesji | heartbeat (rozdz. 5.3) | `[P]` |

### 3.2 Ramkowanie

Każda wiadomość aplikacji = **protobuf poprzedzony 4-bajtową długością (u32, big-endian)**.

```
[ 4 B: długość N ][ N B: treść protobuf („body") ]
```

Narzędzia zapisują i przyjmują wyłącznie część „body" (bez prefiksu). Złożenie/rozłożenie
prefiksu robi warstwa transportowa (`amr::matrixFrame`, `WsClient`).

### 3.3 Kodowanie protobuf

| Typ drutu (wire) | Znaczenie |
|---|---|
| 0 | varint |
| 1 | fixed64 (8 B, little-endian) |
| 2 | długość + bajty (zagnieżdżony komunikat, tekst, bajty) |
| 5 | fixed32 (4 B, little-endian) |

Zasady istotne w tym protokole:
- varint: 7 bitów na bajt, najmłodsza grupa pierwsza;
- **liczby ujemne (int64)** mają wszystkie wyższe bity ustawione, więc np. `−266` zajmuje
  10 bajtów i „gołym okiem" wygląda jak liczba ~1,8·10¹⁹. Do interpretacji służy `as_i64` /
  `intField`; nie odczytywać varintów ręcznie.
- Wartości w tym protokole są **całkowite** (bez float); ułamków nie zaobserwowano.

---

## 4. Koperta (envelope)

Struktura każdej ramki w obie strony:

| Pole | Typ (wire) | Nazwa | Znaczenie |
|---|---|---|---|
| 1 | varint | `kind` | **0** = żądanie, **1** = odpowiedź, **2** = push/komenda |
| 2 | fixed32 | `seq` | numer sekwencji; klient nadaje własne, baza odsyła w odpowiedzi |
| 3 | fixed64 | `session` | identyfikator sesji nadany przez bazę; **0** przed zalogowaniem |
| 4 | bytes | `req` | treść żądania/pushu; podpole 1 = typ (rozdz. 5.4) |
| 5 | bytes | `resp` | treść odpowiedzi/pushu |
| 6 | bytes | `cmd` | **kanał komend** (rozdz. 6); podpole 2 = opkod |

Uwagi:
- Pole 6 pojawia się tylko w ramkach `kind=2` wysyłanych przez panel. Ramki `kind=0` (zapytania)
  i `kind=1` (odpowiedzi) używają pól 4/5.
- Wysyłając z przechwyconej ramki podmieniamy **wyłącznie** pola 2 i 3 (nasze `seq` i `session`);
  resztę zostawiamy bajt w bajt — to robi `MatrixClient::sendReplay`.
- `describeFrame()` zwraca skrót w postaci `kind=2 seq=19 sess=0x1a11... op=16 len=27`.

### 4.1 Identyfikacja ramki (`FrameInfo`)

| Pole | Znaczenie |
|---|---|
| `kind`, `seq`, `session` | jak wyżej |
| `type` | typ z treści żądania/pushu (`req.1` lub `resp.1`) |
| `op` | opkod z kanału komend (`cmd.2`) |
| `has_type`, `has_op` | czy dane pole wystąpiło (typ/opkod może nie istnieć) |

Selektor `--type N` w narzędziach dopasowuje ramkę **po `type` albo po `op`** — dlatego
`--type 16` działa zarówno dla jogu z kanału komend, jak i dla zadań z pola 4.

---

## 5. Sesja i uwierzytelnianie

### 5.1 Logowanie

Żądanie (`kind=0`, `seq=1`, `session=0`):

```
pole 4 = { 1: 0, 2: { 1: "<login>", 2: "<md5-hasła-hex>", 3: 30 } }
```

| Ścieżka | Znaczenie |
|---|---|
| `4.1` | kod żądania: **0** = logowanie |
| `4.2.1` | login |
| `4.2.2` | MD5 hasła (32 znaki hex, małe litery) |
| `4.2.3` | 30 — interpretacja nieznana (stałe w panelu) `[P]` `[?]` |

Odpowiedź bazy niesie **sesję** (pole 3 koperty) oraz payload stanu (pole 5). Od tego momentu
każda wysyłana ramka musi zawierać tę sesję.

### 5.2 Bootstrap po zalogowaniu

Do dokładnie tego samego (kolejność i kody) co panel `[P]`:

| Kolejność | Typ żądania | Uwaga |
|---|---|---|
| 1 | `1` | znaczenie nieustalone `[?]` |
| 2 | `17` | znaczenie nieustalone `[?]` (prawdopodobnie mapa/konfiguracja) |
| 3 | `4` | znaczenie nieustalone `[?]` |

Wysyłane raz, po pierwszej odpowiedzi (seq 1). Można wyłączyć: `Options::bootstrap_requests=false`.

### 5.3 Heartbeat

| Aspekt | Wartość |
|---|---|
| żądanie | `kind=0`, pole 4 = `{1: 18}` |
| częstotliwość panelu | co ~3 s (zaobserwowane na całym zrzucie) `[P]` |
| nasza konfiguracja | `Options::heartbeat_ms` (domyślnie 3000; 0 = nie wysyłaj) |
| reguła | wysyłany dopiero po ~3 s **bezczynności** (nie zaraz po logowaniu) |

### 5.4 Znane kody żądań

| Typ | Kierunek | Znaczenie | Status |
|---|---|---|---|
| 0 | klient → baza | logowanie | `[R]` |
| 1 | klient → baza | bootstrap (1. z trzech) | `[P]` [?] |
| 4 | klient → baza | bootstrap (3. z trzech) | `[P]` [?] |
| 17 | klient → baza | bootstrap (2. z trzech) | `[P]` [?] |
| 18 | klient → baza | heartbeat | `[P]` |
| 2 | baza → klient | push stanu (rozdz. 7) | `[R]` |
| 11 | baza → klient | odpowiedź na zadanie (`op=32`) | `[P]` |

---

## 6. Kanał komend (pole 6)

### 6.1 Struktura

```
kind=2, pole 6 = { 2: <opkod>, 7: 0, <parametry…> }
```

`kind=2` i pole 6 występują razem; parametrami są kolejne pola tego komunikatu.

### 6.2 Opkody

| Opkod | Nazwa | Parametry | Status |
|---|---|---|---|
| **16** | jog (strumień prędkości) | `3` = jazda, `4` = obrót, `7` = 0 | `[R]` |
| **6** | przejmij sterowanie | `7` = 0 | `[R]` (skutek) |
| **111** | (nieznany, para z `op=6`) | `7` = 0 | `[P]` |
| **7** | oddaj sterowanie | `7` = 0 | `[R]` (skutek) |
| **32** | zlecenie zadania | `9` = `{3, 4, 5, 11}`, `7` = 0 | `[P]` (adresowanie `[?]`) |

### 6.3 Pole jogu (`op=16`)

| Ścieżka | Typ | Jednostka | Znaczenie | Zakres bezp. |
|---|---|---|---|---|
| `6.2` | varint | — | opkod = 16 | — |
| `6.3` | varint | mm/s | jazda: **+ przód**, − tył | 100–300 `[R]` skala 1:1 |
| `6.4` | varint | mrad/s | obrót: **+ lewo**, − prawo | 100–300 |
| `6.7` | varint | — | 0 (stałe w każdej zaobserwowanej ramce) | — |

Kalibracja `[R]`: komenda `6.3=100` → ~103 mm/s w fazie stałej (1 jednostka = 1 mm/s);
rampa rozruchu bazy ~0,2 s (krótkie ruchy wypadają odpowiednio krótsze).

### 6.4 Przykładowe ramki (hex)

| Ramka | hex | Rozbiór |
|---|---|---|
| jog z panelu: przód 330 | `080215130000001931c44216a10100003209101018ca0220003800` | `kind=2, seq=19, sess, 6={2:16,3:330,4:0,7:0}` |
| jog: obrót w lewo 266 | `0802151f0000001931c44216a1010000320910101800208a023800` | `6={2:16,3:0,4:266,7:0}` |
| jog: obrót w prawo 266 | `080215450000001931c44216a101000032111010180020f6fdffffffffffffff013800` | `6={2:16,3:0,4:−266,7:0}` (10-bajtowy varint) |
| przejmij sterowanie | `08021500000000190000000000000000320410063800` | `6={2:6,7:0}` |
| (para `op=6`) | `080215000000001900000000000000003204106f3800` | `6={2:111,7:0}` |
| oddaj sterowanie | `08021500000000190000000000000000320410073800` | `6={2:7,7:0}` |
| stop (zera) | `0802150000000019000000000000000032081010180020003800` | `6={2:16,3:0,4:0,7:0}` |
| zadanie | `0802156b0000001931c44216a1010000320f102038004a09180220022a01035801` | `6={2:32,7:0,9:{3:2,4:2,5:03,11:1}}` |

W ramkach zapisanych w plikach (`cap/*.hex`) pola `seq` i `session` są zerowane — podmienia je
`sendReplay`.

### 6.5 Częstotliwości

| Źródło | Częstotliwość jogu | Status |
|---|---|---|
| panel Matrix | ~5 Hz (co ~0,2 s) | `[P]` |
| nasze narzędzia | 20 Hz (domyślnie), maks. 50 Hz (`--rate`, przycinane) | — |

---

## 7. Push stanu (typ 2)

Wysyłany cyklicznie (nie na żądanie) jako `kind=1`/`kind=2` z treścią w polu 5. Trafia też
w odpowiedzi na logowanie.

| Ścieżka | Typ | Jednostka | Znaczenie | Status |
|---|---|---|---|---|
| `3.2` | varint | — | faza zadania: **2** = spoczynek, **7** = jazda, **10** = start | `[R]` |
| `3.4.2` | varint | mm | pozycja x | `[R]` |
| `3.4.3` | varint | mm | pozycja y | `[R]` |
| `3.4.7` | varint | mrad | kąt (yaw) | `[R]` |
| `3.7.8` | varint | 10 mm | pozostały dystans do celu (1 jedn. = 1 cm) | `[P]` |
| `3.7.12[n]` | varint ×4 | mm | odcinek trasy (sx, sy, ex, ey) | `[P]` (kolejność pól `[?]`) |
| `3.10` | tekst | — | nazwa mapy (np. `Showroom`) | `[R]` |

Mapowanie na `amr::State` (biblioteka, rozdz. 9.7):

| Pole `State` | Źródło |
|---|---|
| `x_mm`, `y_mm`, `yaw_rad` | `3.4.2`, `3.4.3`, `3.4.7` (mrad → rad) |
| `dist_raw` | `3.7.8` |
| `phase` | `3.2` |
| `map` | `3.10` |
| `route` | `3.7.12[]` |
| `raw` | kopia całego komunikatu stanu |

Format linii HUD (`stateLine`): `x=   -10.0 mm  y=  -267.0 mm  kat=-1.5400  dystans=    0 cm  faza=2  trasa[0]  mapa=Showroom`.

---

## 8. Zadanie (`op=32`) i odpowiedź (typ 11)

**Zlecenie zadania** `[P]` (jedno zaobserwowane):

```
6 = { 2: 32, 7: 0, 9: { 3: 2, 4: 2, 5: 0x03, 11: 1 } }
```

Znaczenie pól `9.3/9.4/9.5/9.11`: **nieustalone** `[?]`. Prawdopodobnie wskazanie trasy/stacji.

**Odpowiedź** po ~8,5 s `[P]`:

```
kind=1, pole 5 = { 1: 11, 14: { 1: 1, 2: 0 } }
```

| Ścieżka | Znaczenie |
|---|---|
| `5.1` | kod odpowiedzi = 11 |
| `5.14.1`, `5.14.2` | `1`, `0` — interpretacja nieustalona `[?]` |

W panelu: wybór stacji + zatwierdzenie; w zrzucie brak pełnego adresowania celu.
**Do rozgryzienia potrzebny zrzut z dziennikiem działań** (patrz `PRZEWODNIK.md`, dodatek B).

---

## 9. Biblioteka `include/amr_matrix.hpp`

Nagłówek-only (C++17, bez zależności), wszystko w przestrzeni `amr`. `amr::kVersion` = `"0.3"`.

### 9.1 Warstwa surowych komunikatów

| Symbol | Sygnatura | Opis |
|---|---|---|
| `Field` | `{uint32_t num; int wire; uint64_t value; std::string bytes;}` | jedno pole protobufa |
| `readVarint` | `bool readVarint(std::string_view b, size_t& i, uint64_t& out)` | dekoder varint |
| `parseMessage` | `bool parseMessage(std::string_view b, std::vector<Field>& out)` | pełny podział komunikatu |
| `findField` | `const Field* findField(const std::vector<Field>& v, uint32_t num, int wire)` | wyszukanie pola o dokładnym numerze **i** typie drutu; `nullptr`, gdy brak |
| `intField` | `int64_t intField(const std::vector<Field>& v, uint32_t num)` | wartość pola varint jako **int64** (obsługa znaku); brak pola = 0 |
| `varintBytes` | `std::string varintBytes(uint64_t)` | odwrotność `readVarint` |

### 9.2 Drzewo i edycja (do podmian pól)

| Symbol | Sygnatura | Opis |
|---|---|---|
| `Node` | `{num, wire, val, str, kids}` | węzeł drzewa; `kids` = podpola |
| `parseTree` | `bool parseTree(std::string_view, std::vector<Node>&)` | body → drzewo |
| `serializeTree` | `std::string serializeTree(const std::vector<Node>&)` | drzewo → body |
| `encodeNode` | `std::string encodeNode(const Node&)` | jeden węzeł → bajty |
| `varintNode` / `stringNode` | `Node varintNode(num, int64_t)` / `Node stringNode(num, const std::string&)` | konstruktory |
| `applyPatch` | `bool applyPatch(std::vector<Node>&, const std::string& spec, std::string& err)` | patrz 9.2.1 |
| `dumpTree` / `dumpBody` | `std::string dumpBody(std::string_view)` | czytelny wydruk drzewa |
| `flattenTree` | `void flattenTree(...)` | drzewo → lista ścieżek |
| `diffBodies` | `std::vector<std::string> diffBodies(a, b, size_t limit = 32)` | różnice pól dwóch body |

#### 9.2.1 Gramatyka `applyPatch` (i `--set`)

```
spec := ścieżka '=' wartość
ścieżka := element ('.' element)*        element := NUMER [ ':' TYP ]
TYP   := v | i (varint, domyślny) | u | z (zigzag) | s (tekst) | f (float32) | d (float64)
```

- Brakujący element pośredni jest **tworzony** (jako podkomunikat, wire=2).
- Jeśli wskazane pole ma już podpola, podmiana kończy się błędem (nie nadpisujemy poddrzewa).
- Działanie jest **względne wobec przekazanego drzewa** — stąd dwa różne „punkty odniesienia"
  w narzędziach (rozdz. 10.4.1, tabela pułapki).

### 9.3 Identyfikacja i budowa ramek

| Symbol | Sygnatura | Opis |
|---|---|---|
| `frameInfo` | `FrameInfo frameInfo(std::string_view body)` | rozbiór koperty: kind/seq/session/type/op |
| `describeFrame` | `std::string describeFrame(std::string_view)` | jednowierszowy opis ramki |
| `encodeEnvelope` | `std::string encodeEnvelope(kind, seq, session, content_field, content)` | budowa koperty |
| `matrixHeader` / `matrixFrame` | — | nagłówek z sesją / pełna ramka z prefiksem długości |
| `loginFrame` | `std::string loginFrame(user, pass_md5_hex)` | gotowa ramka logowania (z prefiksem) |
| `requestFrame` | `std::string requestFrame(seq, session, type)` | gotowe zapytanie o typie `type` |
| `hexEncode` / `hexDecode` | `std::string hexEncode(...)` / `bool hexDecode(..., std::string&)` | hex ↔ bajty; `hexDecode` **pomija znaki nie-hex** (tolerancyjny), zwraca `false` przy nieparzystej liczbie cyfr |

### 9.4 Stan

| Symbol | Opis |
|---|---|
| `State` | `{x_mm, y_mm, yaw_rad, dist_raw, phase, map, route, raw}` + `operator==` |
| `RouteSegment` | `{sx, sy, ex, ey}` w mm (kolejność pól `[?]`) |
| `decodeState(payload, State&)` | payload pola 5 → `State` (zwraca `false`, gdy to nie stan) |
| `stateFromBody(body, State&)` | to samo, ale dla całego body |
| `stateLine(const State&)` | jednowierszowy HUD |

`decodeState` akceptuje payload tylko gdy `payload.1 == 2` (typ pushu stanu).

### 9.5 Wybór ramki z zapisu

```cpp
bool readCaptureSelect(const std::string& path, const std::string& dir /*"UP"|"DOWN"*/,
                       int type, int nth, std::string& hex_out, std::string& info, std::string& err);
```

Iteruje po zapisie, filtruje po kierunku i dopasowaniu `type` **lub** `op`, bierze `nth`-te
trafienie. Wypisuje `info` = `describeFrame`. Błąd np.:
`nie znalazlem ramki dir=UP type=16 nr 1 w cap/panel_paste.log`.

### 9.6 Klient

#### 9.6.1 `WsClient` (niski poziom)

| Metoda | Opis |
|---|---|
| `bool connect(host, port, origin, timeout_ms, std::string& err)` | połączenie (handshake HTTP Upgrade) |
| `bool sendBinary(const std::string&)` | surowa ramka binarna (już z prefiksem) |
| `Recv receive(std::string& msg, int timeout_ms)` | odbiór (`Recv::Ok/Timeout/Closed`) |
| `int nativeFd() const` | deskryptor do własnego `poll()` |
| `void shutdown()` | zamknięcie |

#### 9.6.2 `MatrixClient` (wysoki poziom)

| Metoda | Sygnatura | Opis |
|---|---|---|
| ctor | `explicit MatrixClient(Options)` | konfiguracja |
| `connect` | `bool connect(std::string& err)` | połączenie + wysłanie logowania |
| `logged_in` | `bool logged_in() const` | czy znamy sesję (`session != 0`) |
| `poll` | `Recv poll(int timeout_ms, const StateCb&)` | czekanie na ramki (tylko stan) |
| `pollAny` | `Recv pollAny(int timeout_ms, const FrameCb&, const StateCb&)` | każda ramka + stan; **wywołuje heartbeat** |
| `sendBody` | `bool sendBody(const std::string& body)` | wysyłka gotowego body (uzupełnia nagłówek) |
| `sendRequest` | `bool sendRequest(uint32_t type, const std::string& extra = {}, std::string* effective = nullptr)` | żądanie `{1:type} + extra` |
| `sendReplay` | `bool sendReplay(const std::string& captured_body, std::string* effective = nullptr)` | odtworzenie ramki: **podmienia tylko `seq` i `session`** |
| `close` | `void close()` | zamknięcie |
| `nativeFd` | `int nativeFd() const` | deskryptor (do `poll` razem ze stdin) |
| `options` | `const Options& options() const` | bieżące opcje |

Zachowania automatyczne:
- po pierwszej odpowiedzi (seq 1) wysyła bootstrap **1, 17, 4** (jeśli `bootstrap_requests`);
- `pollAny` wywołuje `maybeHeartbeat()` — `type=18` po `heartbeat_ms` bezczynności;
- każda odebrana ramka idzie do `on_frame`; payload stanu → `on_state`.

#### 9.6.3 `Options`

| Pole | Domyślnie | Znaczenie |
|---|---|---|
| `host` | `192.168.71.50` | adres bazy |
| `port` | `5002` | port |
| `user` | `admin` | login |
| `pass_md5` | MD5(`admin`) | hash hasła w hex |
| `connect_timeout_ms` | `3000` | timeout połączenia |
| `bootstrap_requests` | `true` | wysyłaj 1/17/4 po logowaniu |
| `heartbeat_ms` | `3000` | okres heartbeatu (0 = wyłącz) |

### 9.7 Minimalny przykład (wysyłka jogu)

```cpp
amr::MatrixClient::Options opt;
opt.host = "192.168.71.50";
amr::MatrixClient client(opt);
std::string err;
if (!client.connect(err)) { /* err */ }
while (!client.logged_in()) client.pollAny(100, nullptr, nullptr);

// op=6 i op=111: przejmij sterowanie (hex z cap/panel_op6_enable.hex, panel_op111.hex)
std::string f_enable, f_111, f_jog, f_zero, f_rel;
amr::hexDecode("08021500000000190000000000000000320410063800", f_enable);
amr::hexDecode("080215000000001900000000000000003204106f3800", f_111);
// jog: op=16, vx=150, wz=0 (budujemy od zera)
std::vector<amr::Node> six{ amr::varintNode(2, 16), amr::varintNode(3, 150), amr::varintNode(4, 0), amr::varintNode(7, 0) };
std::vector<amr::Node> env{ amr::varintNode(1, 2), amr::stringNode(6, amr::serializeTree(six)) };
f_jog = amr::serializeTree(env);
amr::hexDecode("0802150000000019000000000000000032081010180020003800", f_zero);  // zera
amr::hexDecode("08021500000000190000000000000000320410073800", f_rel);          // op=7

client.sendReplay(f_enable);
client.sendReplay(f_111);
for (int i = 0; i < 20; ++i) { client.sendReplay(f_jog); client.pollAny(50, nullptr, nullptr); }
client.sendReplay(f_zero);
client.sendReplay(f_zero);
client.sendReplay(f_rel);   // dopiero na koniec!
client.sendReplay(f_rel);
```

---

## 10. Narzędzia — referencja CLI

Wspólne dla wszystkich binarek: `--ip`, `--port`, `--user`, `--pass-md5`, `--help`/`-h`.
Kody wyjścia: **0** sukces · **1** błąd połączenia · **2** błąd użycia/argumentów/danych.

### 10.1 `amr_state_cli` — odczyt stanu

| Opcja | Typ | Domyślnie | Opis |
|---|---|---|---|
| `--seconds S` | liczba | 0 (do Ctrl-C) | ile sekund nasłuchiwać |
| `--all` | flaga | off | każda ramka stanu (bez odsiewania powtórek) |
| `--full` | flaga | off | pełny wydruk drzewa stanu (`state.raw`) |
| `--segments` | flaga | off | wypisz odcinki trasy |

### 10.2 `amr_probe` — podglądacz i próbnik

| Opcja | Typ | Domyślnie | Opis |
|---|---|---|---|
| `--seconds S` | liczba | 0 (z `--type`: 5) | czas nasłuchu |
| `--all` | flaga | off | każda ramka w całości (wyłącza tryb diff) |
| `--type N` | int | — | po zalogowaniu wyślij żądanie o kodzie N |
| `--set a.b=v` | string | — | pole treści żądania (powtarzalne; typy jak 9.2.1) |
| `--repeat N` | int | 1 | ile razy wysłać |
| `--rate H` | liczba | 10 | częstotliwość wysyłki [Hz] |
| `--hex HEX` | string | — | wyślij przechwyconą ramkę (podmienia seq/sesję) |
| `--interactive` | flaga | off | polecenia ze stdin: `type N [a.b=v …]`, `hex HEX`, `dump HEX`, `q` |
| `--dump-hex HEX` | string | — | dekodowanie **offline** (bez łączenia) i wyjście |
| `--dry-run` | flaga | off | pokaż, ale nie wysyłaj |

### 10.3 `amr_proxy` — podsłuch (MITM)

| Opcja | Domyślnie | Opis |
|---|---|---|
| `--listen HOST:PORT` | `0.0.0.0:5002` | gdzie nasłuchuje |
| `--remote HOST:PORT` | `192.168.71.50:5002` | adres bazy |
| `--save PLIK` | — | zapis wszystkich ramek (format: rozdz. 11) |
| `--down-full` | off | pełne drzewo ramek z bazy (poza stanem) |
| `--no-up-full` | off | nie pokazuj pełnego drzewa ramek z panelu |

Ctrl-C kończy pracę i drukuje podsumowanie typów (`UP kind type -> liczba`).
Uwaga operacyjna: uruchamiać jako root i kierować na niego tylko ruch przeglądarki, inaczej
proxy podsłuchuje sam siebie (patrz `PRZEWODNIK.md`, §6, droga B).

### 10.4 `amr_cmd` — wysyłka komend

| Opcja | Typ | Domyślnie | Opis |
|---|---|---|---|
| `--frame HEX\|@plik` | string | — | gotowa ramka (body) |
| `--type N` | int | — | zadanie budowane od zera: treść `{1:N, --set …}` |
| `--from-capture PLIK` | string | — | wybór ramki z zapisu |
| `--dir UP\|DOWN` | string | `UP` | kierunek w zapisie |
| `--nth K` | int | 1 | które trafienie wybrać |
| `--set a.b=v` | string | — | podmiana pola (powtarzalne) |
| `--pre-frame HEX\|@plik` | string | — | ramka wstępna, wysyłana raz przed komendą (powtarzalne) |
| `--stop-frame HEX\|@plik` | string | — | ramka stopu; **można podać kilka** (każda 2×) |
| `--stop-type M` | int | — | stop budowany od zera: treść `{1:M, --stop-set …}` |
| `--stop-set a.b=v` | string | — | pola ramki stopu budowanej od zera |
| `--seconds S` | liczba | 2 (gdy brak `--repeat`) | czas nadawania w pętli; `0` = jednorazowo |
| `--repeat N` | int | 0 (wyłączone) | dokładnie N ramek (nadrzędne nad `--seconds`) |
| `--rate H` | liczba | 20 | częstotliwość; >50 przycinane do 50 z ostrzeżeniem |
| `--hold S` | liczba | 1.0 | przy pojedynczej ramce: zwłoka przed stopem |
| `--echo-state` | flaga | off | drukuj stan bazy przy każdym odczycie |
| `--dry-run` | flaga | off | nic nie wysyła |
| `--yes` | flaga | off | pomija 3-sekundowe odliczanie |

#### 10.4.1 Punkt odniesienia ścieżek `--set` (częsta pomyłka)

| Źródło komendy | Względem czego liczy się ścieżka | Przykład |
|---|---|---|
| `--type N --set …` | **treść** zadania (`{1:N, …}`), opakowana potem w pole 4 | `--type 100 --set 2.1=250` |
| `--frame` / `--from-capture` | **całego body** przechwyconej ramki | `--set 6.3=150` |
| `--stop-type M --stop-set …` | treści ramki stopu | jak `--type` |

Przy ramkach panelu (kanał 6) zawsze używamy drugiej postaci (`--from-capture` + `--set 6.3=…`).

### 10.5 `amr_wasd` — prowadzenie z klawiatury

Źródło ramki (jedno): `--frame HEX|@plik`, `--from-capture PLIK [--type N] [--nth K]`, `--type N`.
Podmiany pól: `--set a.b=@@vx@@`, `--set a.b=@@wz@@` (**wymagane**; `@@vx@@`/`@@wz@@` to znaczniki
wartości bieżącej).

| Opcja | Domyślnie | Opis |
|---|---|---|
| `--dir UP\|DOWN`, `--nth K` | `UP`, 1 | wybór ramki z zapisu |
| `--pre-frame HEX\|@plik` | — | ramki wstępne (np. `op=6`, `op=111`); powtarzalne |
| `--stop-frame HEX\|@plik` | — | **chwilowy stop** (puszczenie klawisza / SPACJA); bez oddawania sterowania; powtarzalne |
| `--release-frame HEX\|@plik` | — | oddanie sterowania (`op=7`) — **tylko przy wyjściu**; powtarzalne |
| `--stop-type N` | — | stop budowany od zera (np. 102 na atrapie) |
| `--no-rearm` | off | nie powtarzaj ramek wstępnych przed nowym ruchem |
| `--verbose` | off | pokazuj ramki przychodzące z bazy (poza push stanu) |
| `--arc` / `--no-arc` | `--arc` | łuki: trzymanie np. W+A jedzie **i** skręca |
| `--speed MM_S` | 300 | prędkość jazdy |
| `--turn MRAD_S` | 400 | prędkość obrotu |
| `--rate HZ` | 20 | częstotliwość (poza 1–50 wraca do 20) |
| `--accel MM_S2` | 900 | narastanie/opadanie prędkości (wygładzanie nasze) |
| `--hold-ms MS` | 350 | okno podtrzymania klawisza |
| `--max-seconds S` | 0 (brak) | awaryjny limit czasu jazdy |
| `--dry-run` | off | bez wysyłki |

**Klawisze:** W/S przód/tył, A/D obrót (strzałki równoważne), SPACJA = stop natychmiastowy
i wyczyszczenie zestawu, X/Q/Ctrl-C = wyjście (wysyła `--release-frame`).

**Kolejność ramek w sesji:**

```
start:  --pre-frame…            (jednorazowo)
ruch:   jog (--rate Hz), wygładzany --accel
        przy pierwszym ruchu po pauzie: re-arm (--pre-frame) ponownie
stop:   --stop-frame ×2 (na puszczenie / SPACJĘ)
wyjście: --stop-frame ×2, potem --release-frame ×2
```

**Semantyka `--arc`:** terminal powtarza tylko ostatnio wciśnięty klawisz; tryb łukowy pamięta
osie (`lat_motion`, `lat_turn`) i utrzymuje cały zestaw, dopóki przychodzą powtórzenia
jakiegokolwiek klawisza z zestawu (okno `--hold-ms` od ostatniego powtórzenia). Po ciszy zestaw
jest czyszczony. `--no-arc`: klasyczne „wygasa każdy klawisz osobno".

**HUD** (2 Hz): `vx= +150 mm/s  wz= +200 mrad/s | x= … y= … kat=… faza=7 | JEDZIE/stoi`.

### 10.6 `baza.sh` — nakładka (najprostsze wejście)

| Podkomenda | Działanie |
|---|---|
| `wasd [--speed … --turn … --no-arc …]` | `amr_wasd` z kompletem ramek, domyślnie 150 mm/s, 200 mrad/s |
| `przod [mm/s] [s]` / `tyl` | jazda; domyślnie 100 mm/s przez 1 s |
| `lewo [mrad/s] [s]` / `prawo` | obrót; domyślnie 200 mrad/s przez 1 s |
| `stop` | zera ×2 + `op=7` ×2 (natychmiastowe zatrzymanie) |
| `stan [s]` | podgląd stanu (domyślnie 5 s) |
| `podglad <reszta>` | to samo z `--dry-run` (nic nie wysyła) |

Zmienne środowiskowe: `AMR_IP` (domyślnie `192.168.71.50`), `AMR_PORT` (domyślnie `5002`) —
do testów na atrapie: `AMR_IP=127.0.0.1 AMR_PORT=5003 ./baza.sh przod 100 1`.
Skrypt sam dokłada: `--pre-frame @cap/panel_op6_enable.hex --pre-frame @cap/panel_op111.hex`,
`--from-capture cap/panel_paste.log --type 16 --nth 1`, `--stop-frame @cap/panel_stop_zero.hex`,
`--release-frame @cap/panel_op7_release.hex`.

### 10.7 `amr_selftest` i cele `make`

| Cel | Efekt |
|---|---|
| `make` | budowa wszystkich binarek (C++17, `-O2 -Wall -Wextra`) |
| `make test` | `./amr_selftest` — testy rdzenia (parser, koperta, patche, stan) |
| `make bench` | atrapa bazy na porcie 5003 (`--auto-mission`) |
| `make clean` | usuwa binarki |

### 10.8 Narzędzia w Pythonie/JS

| Plik | Opcje / API | Opis |
|---|---|---|
| `tools/bench_server.py` | `--port` (5003), `--auto-mission` | atrapa bazy: logowanie, sesja, push stanu, jog `op=16`, jawne zera, symulacja ruchu |
| `tools/analyze_capture.py` | `FILE [--dir UP\|DOWN] [--timeline N] [--min-count K]` | grupy ramek po kształcie + serie czasowe wartości |
| `tools/panel_hook.js` | `saveCapture()`, fallback `copy(__amrFrames.join("\n"))` | haczyk do konsoli DevTools; zapis `panel_capture.log` |
| `tools/test_panel_hook.js` | — | test haczyka w node (3 przypadki) |
| `tools/test_wasd_pty.py` | `--keys w=1.5,=1.0,d=1.0 --pause S -- CMD` | sterowanie `amr_wasd` przez pty (pusty klawisz `=1.0` = pauza) |

---

## 11. Format plików danych

### 11.1 Zapis przechwycenia (`amr_proxy --save`, `panel_hook.js`)

Jeden wiersz = jedna ramka; komentarze `#`.

```
+<czas_s> <UP|DOWN> <hex_body>
#        <UP|DOWN> <describeFrame…>            (opcjonalny komentarz — proxy)
```

- `UP` = wysłane przez panel (komendy), `DOWN` = od bazy (odpowiedzi/push).
- `hex_body` = całe body bez prefiksu długości (rozdz. 3.2).
- `#` i wiersze puste są pomijane przez czytniki.
- `panel_hook.js` pisze ten sam format; `saveCapture()` pobiera plik `panel_capture.log`.

### 11.2 Plik pojedynczej ramki (`cap/*.hex`)

Czysty hex (bez `+`, bez `#`), może być w wielu wierszach. Czytany przez `--frame @plik`,
`--pre-frame @plik`, `--stop-frame @plik`, `--release-frame @plik`.

**Uwaga implementacyjna:** przy `--frame @plik` bufor hex jest czyszczony przed czytaniem
(naprawiony błąd: nazwa pliku doklejała się do heksu, gdy bufor nie był pusty).

### 11.3 Pliki w `cap/`

| Plik | Zawartość |
|---|---|
| `panel_paste.log` | zrzut z hali: 119 ramek (jazda panelem + heartbeat) |
| `panel_op6_enable.hex` | `op=6` przejmij sterowanie |
| `panel_op111.hex` | `op=111` (para z `op=6`) |
| `panel_stop_zero.hex` | `op=16` z zerami — chwilowy stop |
| `panel_op7_release.hex` | `op=7` oddaj sterowanie |
| `panel_jog_fwd.hex` | jog z panelu (przód 330) |
| `bench.log` | przykładowy zapis z atrapy |
| `jog_type100.hex` | ramka jogu protokołu atrapy (typ 100) |
| `panel_analiza.txt` | wynik `analyze_capture.py` + potwierdzone znaczenia |

---

## 12. Wymagania operacyjne i bezpieczeństwo

**MUST** (bez tego nie uruchamiać na robocie):

1. Fizyczny STOP / E-stop w zasięgu ręki operatora.
2. Wolna przestrzeń ≥ 1–2 m wokół bazy; brak progów i przeszkód w zasięgu ruchu.
3. `op=7` wysyłany **wyłącznie** na końcu sesji (nigdy między ruchami).
4. Po każdym ruchu jawne zera (`--stop-frame`); nie polegać na deadmanie bazy.
5. Panel Matrix zamknięty podczas sterowania naszym klientem.
6. Pierwsze próby na robocie: ≤ 150 mm/s, ≤ 2 s, po uprzednim `--dry-run`.

**SHOULD:**

7. Heartbeat ≥ 3 s (domyślnie włączony) — jak panel.
8. Re-arm przed każdym nowym ruchem w trybie klawiatury (domyślnie włączony).
9. `--rate` ≤ 20–50 Hz; nie nadawać jogu szybciej niż przyjmuje baza.
10. Trzymać wersję narzędzi zgodną z wersją protokołu (rozdz. 13).

Zachowanie awaryjne narzędzi: Ctrl-C → ramki stopu; utrata połączenia → komunikat i wyjście;
`--max-seconds` → twardy limit czasu jazdy (tylko `amr_wasd`).

---

## 13. Wersje i historia zmian

`amr::kVersion` (w `describeFrame`/`usage()`): **0.3**.

| Wersja | Zakres | Najważniejsze zmiany |
|---|---|---|
| 0.1 | odczyt | transport, koperta, logowanie, push stanu (kod bazowy użytkownika) |
| 0.2 | wysyłka | `encodeEnvelope`, `sendReplay`, `applyPatch`, dekoder ramek, `amr_cmd`/`amr_probe`/`amr_proxy`, atrapa, testy |
| 0.3 | sterowanie panelem | rozpoznanie kanału komend (pole 6, `op=16/6/7/111/32`), heartbeat `type=18`, `--pre-frame`, wielokrotny `--stop-frame`, `--release-frame`, re-arm, `--arc`, `--verbose`, `analyze_capture.py`, `baza.sh` |

---

## 14. Elementy niezweryfikowane i ograniczenia

| Element | Stan | Jak zweryfikować |
|---|---|---|
| `op=111` — znaczenie | nieznane | porównać zachowanie bazy z/bez wysyłki |
| `op=32` — adresowanie celu (pola `9.3/9.4/9.5/9.11`) | nieznane | zrzut z dziennikiem działań (dodatek B w `PRZEWODNIK.md`) |
| Deadman dla naszej sesji (czy stop po urwaniu jogu) | nieznane | uciąć strumień jogu i obserwować |
| Sterowanie równoległe z panelem | nieznane | test na atrapie, potem na robocie z asekuracją |
| Kolejność pól `3.7.12[n]` (sx,sy,ex,ey) | hipoteza | porównać trasę w panelu z ramkami stanu |
| Jednostka `3.7.8` (1 = 10 mm) | wniosek z jednego przypadku | korelacja z przebytym dystansem |
| `op=16` z obiema osiami naraz | `[A]` na atrapie; na robocie do potwierdzenia | `baza.sh wasd` + W+A |
| Obrót: skala mrad/s | `[?]` — oczekiwane `dkat ≈ +0,2 rad` dla 200 mrad/s × 1 s | `baza.sh lewo 200 1` |
| Limity przyspieszenia bazy | nieznane (nasze `--accel` to wygładzanie po naszej stronie) | testy narastających wartości |
| Znaczenie typów żądań 1 / 4 / 17 | nieznane | porównać odpowiedzi z/bez wysyłki |
| Konflikt sesji (panel + my) | nieznane | — |

---

## 15. Szybkie odniesienia

### 15.1 Zadanie → komenda

| Chcę… | Komenda |
|---|---|
| sprawdzić stan | `./amr_state_cli --ip 192.168.71.50 --seconds 5` |
| pojechać prosto 30 cm | `./baza.sh przod 150 2` |
| obrócić w lewo ~11° | `./baza.sh lewo 200 1` |
| prowadzić z klawiatury | `./baza.sh wasd` |
| jechać po łuku | `./baza.sh wasd` + trzymać W+A |
| natychmiast stop | `./baza.sh stop` |
| sprawdzić bez wysyłki | `./baza.sh podglad przod 100 1` |
| rozłożyć ramkę na pola | `./amr_probe --dump-hex <HEX>` |
| znaleźć komendę w zrzucie | `python3 tools/analyze_capture.py cap/panel_paste.log` |
| wybrać ramkę i obejrzeć | `./amr_cmd --from-capture cap/panel_paste.log --type 16 --nth 1 --dry-run` |
| test bez robota | `make bench` + `AMR_IP=127.0.0.1 AMR_PORT=5003 …` |

### 15.2 Ścieżki pól — najważniejsze

| Ścieżka | Znaczenie | Uwaga |
|---|---|---|
| `6.3` | jog: jazda | mm/s, + przód |
| `6.4` | jog: obrót | mrad/s, + lewo |
| `6.2` | opkod | 16 jog, 6/7/111 sterowanie, 32 zadanie |
| `3.2` | faza | 2 spoczynek, 7 jazda, 10 start |
| `3.4.2` / `3.4.3` / `3.4.7` | x / y / kąt | mm, mm, mrad |
| `3.10` | mapa | tekst |

**Pamiętaj o podstawie ścieżki** (`--type` → treść; `--frame`/`--from-capture` → całe body).

---

*Dokumentacja referencyjna zestawu narzędzi do bazy Helios. Metodyka ustaleń i katalog problemów:
`PRZEWODNIK.md`. Kod: `include/amr_matrix.hpp`, `src/`, `tools/`, `baza.sh`.*

---

# 5A. Baza jezdna — procedura pracy, testowania i diagnostyka

Baza Matrix OS jest osobnym systemem niż sterowniki xCore ramion i torsu. Dla nieznanej funkcji nie należy zgadywać ramek na robocie: najpierw obserwuj panel, odtwórz bez zmian komunikat poprawnie uwierzytelniony, a dopiero potem zmieniaj pojedyncze pola. Zapisuj kierunek, czas, sesję oraz czynność wykonaną w panelu.

## Zalecana procedura wdrożenia klienta

1. Potwierdź połączenie z `192.168.71.50:5002` i brak aktywnej sesji panelu. Nie steruj równolegle z panelu i klienta.
2. Ustal transport i framing: WebSocket binarny, prefiks 4-bajtowy długości big-endian, następnie Protobuf. Zapis hex w narzędziach zwykle odnosi się do body bez prefiksu.
3. Zaloguj się, zapisz nadaną sesję i wysyłaj heartbeat `type=18` około co 3 s.
4. Do prowadzenia wymagaj `op=6` oraz `op=111`, następnie wysyłaj cykliczne `op=16`.
5. Zatrzymuj ruchem `op=16` z zerami, nie przez `op=7`. `op=7` kończy grant sterowania i powinien być używany dopiero przy zamykaniu sesji.
6. Testuj najpierw `--dry-run`, potem atrapę na porcie 5003, a dopiero później robot z małą prędkością i krótkim czasem.
7. Po próbie zweryfikuj stan i pozycję bazy; zachowaj log wraz z dziennikiem działań.

## Checklista przed ruchem bazy

- [ ] Wolna przestrzeń wokół robota; operator ma dostęp do fizycznego STOP.
- [ ] Brak osoby w strefie ruchu, przewodów i przeszkód na trasie.
- [ ] Panel Matrix nie steruje jednocześnie tym samym podwoziem.
- [ ] Prędkości początkowe niewielkie (np. 100–150 mm/s) i test krótki.
- [ ] Wysyłana jest cykliczna komenda jog; po zatrzymaniu jawnie wysyłane są zera.
- [ ] `op=7` nie jest wysyłany pomiędzy odcinkami; po oddaniu sterowania klient musi ponownie uzyskać grant.

## Diagnostyka: baza nie reaguje

1. Sprawdź trasę i port: `ping 192.168.71.50`, a następnie dostępność TCP/WS 5002.
2. Sprawdź logowanie i czy klient odebrał niezerową sesję. Jeśli nie, wyklucz zajęcie bazy panelem.
3. Zweryfikuj, czy `op=6` i `op=111` wysłano w bieżącej sesji przed jogiem.
4. Sprawdź ramkę: `kind=2`, kanał komend w polu 6, `op=16`, parametry w `6.3` (mm/s) oraz `6.4` (mrad/s).
5. Po wcześniejszym `op=7` ponów przejęcie sterowania. „Stop” (zerowy jog) i „oddaj sterowanie” są odrębnymi operacjami.
6. Użyj trybu verbose / dekodera drzewa Protobuf. Dla ujemnego `int64` varint może mieć 10 bajtów; używaj dekodera ze znakiem zamiast interpretować wartość jako unsigned.
7. Jeśli nadal nie działa, sprawdź, czy panel sam może sterować. Nie powtarzaj komend w ciemno na robocie.

## Częste pułapki i wnioski operacyjne

| Objaw | Prawdopodobna przyczyna | Co sprawdzić |
|---|---|---|
| Jog nie powoduje ruchu | Brak grantu albo wysłano pole 4 zamiast kanału komend w polu 6 | `op=6` + `op=111`, ścieżkę `6.*`, sesję |
| Klient działa tylko do pierwszego puszczenia klawisza | Wysłano `op=7` po puszczeniu przycisku | Rozdziel jawne zera (stop) od oddania sterowania (`op=7`) |
| „Ogromna” wartość obrotu | Ujemny int64 odczytany jako unsigned | Dekoduj signed varint (`as_i64`) |
| Przejechany dystans mniejszy od prędkość × czas | Rampa rozruchowa bazy, około 0,2 s | Kalibruj dłuższym, kontrolowanym ruchem; uwzględnij rampę |
| Brak odpowiedzi w przechwyceniu | Haczyk założono po zestawieniu WebSocketu i rejestruje tylko nowe połączenia | Po zainstalowaniu haczyka odśwież panel raz i oznacz kierunek ramek |
| Baza przestaje odpowiadać po dłuższym czasie | Brak heartbeat albo wygaśnięta sesja | Heartbeat co około 3 s; odczytaj nową sesję po ponownym logowaniu |
| Dwie osie jogu nie tworzą łuku | Stan klawiszy/osi nie jest utrzymywany w kliencie | Sprawdź cykliczne ramki i zmieniaj jedną oś naraz podczas kalibracji |

## Testowanie bez robota

| Etap | Narzędzie | Cel |
|---|---|---|
| 1 | `--dry-run` / podgląd | Weryfikacja bajtów, pól i parametrów przed transmisją |
| 2 | `tools/bench_server.py` (port 5003) | Test klienta, logowania, sesji, heartbeat, stopu i symulowanego ruchu |
| 3 | Test wejścia/PTY, jeśli dostępny w repozytorium | Sprawdzenie obsługi klawiatury bez fizycznego robota |
| 4 | Robot z prędkością testową | Potwierdzenie zachowania fizycznego w bezpiecznej strefie |

## Elementy protokołu wymagające ostrożności

Znane, ale nie w pełni wyjaśnione pozostają: znaczenie `op=111`, parametry zadania `op=32` (wyznaczanie celu), niektóre pola odpowiedzi, limity przyspieszenia bazy oraz szczegółowe zachowanie grantu/deadmana po utracie połączenia. Nie używaj tych funkcji w produkcji bez własnej weryfikacji.

---

# 5B. Baza jezdna — opis protokołu sieciowego

# Protokół serwera bazy Helios (Matrix OS)
### Co można przechwycić i co można wysłać — opis interfejsu sieciowego

Dokument opisuje **wyłącznie serwer** (bazę jezdną) i jego interfejs sieciowy: co wysyła,
co przyjmuje, jak to zdjąć z sieci i jak zbudować własną komendę. Nie opisuje żadnego
konkretnego klienta ani narzędzia.

Oznaczenia statusu:

| Znaczenie |
|---|
| ✅ potwierdzone bezpośrednio na robocie |
| 🔎 ustalone z obserwacji ruchu panelu |
| ❓ hipoteza / nieustalone |

Stan wiedzy: październik 2026.

---

## 1. Punkt dostępu

| Element | Wartość |
|---|---|
| Adres usługi | `ws://192.168.71.50:5002` (WebSocket, ramki **binarne**) |
| Panel webowy (klient oryginalny) | `http://192.168.71.50/#/homePage` |
| Konto domyślne | login `admin`, hasło `admin` (przesyłane jako MD5 w hex: `21232f297a57a5a743894a0e4a801fc3`) |
| Szyfrowanie | brak (zwykłe `ws://`, bez TLS) — ✅ |
| Inne porty | nie zaobserwowano (5002 obsługuje całą komunikację) |

Baza jest **osobnym systemem** od ramion robota: ma własny serwer, panel i protokół.
Interfejs opisany niżej dotyczy wyłącznie bazy jezdnej.

---

## 2. Warstwa transportowa

### 2.1 Ramkowanie

Każda wiadomość aplikacji, w obie strony, ma postać:

```
[ 4 bajty: długość N, u32 big-endian ][ N bajtów: komunikat protobuf ]
```

Część protobufowa (bez prefiksu) to dalej **„treść"** — w tej postaci najwygodniej zapisywać
przechwycone ramki.

### 2.2 Kodowanie

| Typ drutu | Znaczenie |
|---|---|
| 0 | varint |
| 1 | fixed64 (8 B, little-endian) |
| 2 | długość + bajty (zagnieżdżony komunikat / tekst) |
| 5 | fixed32 (4 B, little-endian) |

Uwagi praktyczne:
- wartości liczbowe są **całkowite** (floatów nie zaobserwowano);
- **liczby ujemne** (np. −266) zapisane jako int64 zajmują 10 bajtów — wyglądają jak ogromna
  liczba (`18446744073709551350`). Zanim uznasz to za błąd, sprawdź interpretację ze znakiem.

---

## 3. Koperta

Każda ramka, niezależnie od treści, ma wspólną „kopertę":

| Pole | Typ | Nazwa | Znaczenie |
|---|---|---|---|
| 1 | varint | `kind` | **0** = żądanie, **1** = odpowiedź, **2** = push/komenda |
| 2 | fixed32 | `seq` | numer sekwencji; klient nadaje własne, serwer odsyła je w odpowiedzi |
| 3 | fixed64 | `session` | identyfikator sesji nadany przez serwer (**0** przed zalogowaniem) |
| 4 | bytes | – | treść żądania/pushu (podpole 1 = typ komunikatu) |
| 5 | bytes | – | treść odpowiedzi |
| 6 | bytes | – | **kanał komend** (podpole 2 = opkod) — patrz rozdz. 6 |

Przykładowy opis ramki (skrót używany w dalszej części dokumentu):
`kind=2 seq=19 sess=0x1a11642c431 op=16 len=27`

Reguła: **`seq` i `session` nadpisuje klient**. Budując własną ramkę z przechwyconej, podmienia się
tylko te dwa pola; reszta zostaje bez zmian.

---

## 4. Uwierzytelnianie i utrzymanie sesji

### 4.1 Logowanie

Pierwsza ramka od klienta: `kind=0`, `seq=1`, `session=0`, treść w polu 4:

```
pole 4 = { 1: 0, 2: { 1: "<login>", 2: "<md5-hasła-hex>", 3: 30 } }
```

| Ścieżka | Znaczenie |
|---|---|
| `4.1` | kod żądania: **0** = logowanie |
| `4.2.1` | login |
| `4.2.2` | MD5 hasła (32 znaki hex) |
| `4.2.3` | `30` — nieustalone ❓ (stałe w panelu) |

Odpowiedź serwera niesie **sesję** (pole 3 koperty). Od tego momentu każda wysyłana ramka musi
ją zawierać — bez sesji serwer nie przyjmuje niczego.

### 4.2 Po zalogowaniu

Obserwowany klient (panel) wysyła kolejno trzy żądania o typach: **1**, **17**, **4** 🔎
(znaczenie każdego nieustalone ❓; prawdopodobnie mapa/konfiguracja/stan).

### 4.3 Podtrzymanie sesji

| Aspekt | Wartość |
|---|---|
| żądanie | `kind=0`, pole 4 = `{ 1: 18 }` |
| częstotliwość | co ~3 s 🔎 |

### 4.4 Znane kody żądań (klient → serwer)

| Typ | Znaczenie | Status |
|---|---|---|
| 0 | logowanie | ✅ |
| 1 | nieustalone (bootstrap) | 🔎 ❓ |
| 4 | nieustalone (bootstrap) | 🔎 ❓ |
| 17 | nieustalone (bootstrap) | 🔎 ❓ |
| 18 | heartbeat | 🔎 |

---

## 5. Co serwer nadaje (do przechwycenia)

### 5.1 Push stanu (typ 2)

Serwer sam, cyklicznie, wysyła stan bazy jako treść w polu 5 (kind=1/2). Trafia też do odpowiedzi
na logowanie. Znane pola:

| Ścieżka | Jednostka | Znaczenie | Status |
|---|---|---|---|
| `3.2` | – | faza: **2** = spoczynek, **7** = jazda, **10** = start | ✅ |
| `3.4.2` | mm | pozycja x | ✅ |
| `3.4.3` | mm | pozycja y | ✅ |
| `3.4.7` | mrad | kąt (yaw) | ✅ |
| `3.7.8` | 10 mm | pozostały dystans do celu (1 jednostka = 1 cm) | 🔎 |
| `3.7.12[n]` | mm | odcinek trasy: (sx, sy, ex, ey) | 🔎 ❓ (kolejność pól) |
| `3.10` | tekst | nazwa mapy (np. `Showroom`) | ✅ |

### 5.2 Odpowiedzi i potwierdzenia

| Ramka | Znaczenie | Status |
|---|---|---|
| `kind=1`, `5.1 = 11` | odpowiedź na zlecenie zadania (rozdz. 6.4), po ~8,5 s | 🔎 |
| `kind=1`, `5.14.1=1`, `5.14.2=0` | dodatkowe pola tej odpowiedzi — nieustalone | ❓ |

---

## 6. Co można wysłać

### 6.1 Kanał komend (pole 6)

Komendy sterujące wysyła się ramkami `kind=2`, z treścią w **polu 6**:

```
pole 6 = { 2: <opkod>, 7: 0, <parametry…> }
```

| Opkod | Nazwa | Parametry | Status |
|---|---|---|---|
| **16** | jog (strumień prędkości) | pole 3 = jazda, pole 4 = obrót | ✅ |
| **6** | przejmij sterowanie | pole 7 = 0 | ✅ (skutek) |
| **111** | nieznane, wysyłane razem z `op=6` | pole 7 = 0 | 🔎 ❓ |
| **7** | oddaj sterowanie | pole 7 = 0 | ✅ (skutek) |
| **32** | zlecenie zadania (jazda do celu) | `9: {3:2, 4:2, 5:03, 11:1}` | 🔎 ❓ (adresowanie) |

### 6.2 Jog (`op=16`)

| Ścieżka | Jednostka | Znaczenie |
|---|---|---|
| `6.3` | mm/s | jazda: **+ przód**, − tył |
| `6.4` | mrad/s | obrót: **+ w lewo**, − w prawo (1000 ≈ 57,3°/s) |
| `6.7` | – | `0` (stałe we wszystkich zaobserwowanych ramkach) |

Kalibracja: wartość **1:1** — komenda `100` daje ~103 mm/s w fazie ustalonej ✅.
Baza ma własną **rampę rozruchu ~0,2 s**, więc krótkie ruchy wypadają nieco krótsze niż
„prędkość × czas".

Obserwowana częstotliwość strumienia: **~5 Hz** (panel) 🔎; wyższe częstotliwości nie były
testowane.

### 6.3 Kolejność ramek (ważne)

```
op=6 + op=111   →  jog (op=16) w pętli  →  jog z zerami  →  op=7
   przejęcie           jazda                zatrzymanie      oddanie
```

| Zasada | Skutek |
|---|---|
| `op=6`+`op=111` **przed** jazdą | bez tego jog jest ignorowany |
| **`op=7` kończy prawo do jazdy** | po `op=7` kolejne jogi nie działają, dopóki nie wyśle się znowu `op=6`+`op=111` |
| serwer nie wysyła własnej ramki „stop" przy przerwaniu strumienia | urwanie jogu samo w sobie wygląda na zatrzymanie (deadman) — 🔎, nie wiadomo, czy dotyczy każdej sesji ❓ |
| brak jawnego stopu | warto wysyłać jog z zerami przed zakończeniem (bezpieczniej niż polegać na deadmanie) |

### 6.4 Zadanie (`op=32`)

```
pole 6 = { 2: 32, 7: 0, 9: { 3: 2, 4: 2, 5: 0x03, 11: 1 } }
```

Po ~8,5 s serwer odpowiada ramką `kind=1`, `5.1 = 11`. Znaczenie pól `9.3/9.4/9.5/9.11`
(wskazanie celu/trasy) — **nieustalone** ❓.

### 6.5 Przykładowe ramki (hex = treść bez prefiksu długości)

| Co | hex |
|---|---|
| jog: przód 330 mm/s | `080215130000001931c44216a10100003209101018ca0220003800` |
| jog: obrót w lewo 266 mrad/s | `0802151f0000001931c44216a1010000320910101800208a023800` |
| jog: obrót w prawo 266 mrad/s | `080215450000001931c44216a101000032111010180020f6fdffffffffffffff013800` |
| jog: zera (zatrzymanie) | `0802150000000019000000000000000032081010180020003800` |
| przejmij sterowanie (`op=6`) | `08021500000000190000000000000000320410063800` |
| `op=111` (para z `op=6`) | `080215000000001900000000000000003204106f3800` |
| oddaj sterowanie (`op=7`) | `08021500000000190000000000000000320410073800` |
| zlecenie zadania (`op=32`) | `0802156b0000001931c44216a1010000320f102038004a09180220022a01035801` |
| odpowiedź na zadanie (`kind=1`) | `0801156b0000001931c44216a10100002a08080b720408011000` |

Uwaga: w ramkach do wysłania pola `seq` (pole 2) i `session` (pole 3) ustawia się na własne
wartości — w powyższych przykładach są zerowe.

---

## 7. Jak przechwycić ruch

Cel: zobaczyć, **co dokładnie** wysyła oryginalny klient (panel), żeby potem powtórzyć to samo.
Trzy podejścia, od najprostszego:

### 7.1 Konsola przeglądarki (bez uprawnień administratora)

1. Otwórz panel i zaloguj się.
2. DevTools (F12) → **Network** → filtr **WS** → wybierz połączenie do portu 5002 →
   zakładka **Messages**: widać wszystkie ramki w obie strony (binarne, w hex).
3. Dodatkowo z konsoli można podpiąć się do gniazda (np. nadpisując `WebSocket.prototype.send`),
   żeby zbierać ramki z etykietami czasu i zapisać je do pliku.

Co daje: komendy wychodzące **oraz** przychodzące, z czasem. Ograniczenia: trzeba wkleić kod
w konsoli i nie przeładowywać strony; nasłuch samych odpowiedzi wymaga świeżego połączenia.

### 7.2 Proxy (pełny, dwukierunkowy zapis)

Wstaw pośrednika między panel a serwer (panel kierujesz na swój komputer, pośrednik przekazuje
dalej). Zapis dostajesz po obu stronach, bez modyfikowania panelu.
Wymaga uprawnień administratora i ostrożności, żeby pośrednik nie złapał własnego ruchu
(np. przekierowanie tylko dla procesu przeglądarki).

### 7.3 Własny klient

Znając kopertę i logowanie, można po prostu **wysyłać własne ramki** i słuchać odpowiedzi.
Najprostsze do rozpoznania protokołu: wysłać przechwyconą komendę **bajt w bajt** (podmieniając
tylko `seq` i `session`), potem stopniowo zmieniać pojedyncze pola.

### 7.4 Co dokładnie zapisywać

- **treść bez prefiksu długości** (4-bajtowy nagłówek długości odrzucamy) — inaczej ramki się nie
  dopasują przy odtwarzaniu;
- kierunek (**wysyłane** vs **odbierane**) i czas;
- **co robiono w danym momencie** (dziennik działań) — bez tego zrzut jest bezużyteczny;
- wartości `seq` i `session` są techniczne — przy analizie można je pominąć.

---

## 8. Bezpieczeństwo i ograniczenia wynikające z protokołu

1. **`op=7` jest nieodwracalne** dla danej chwili — kończy prawo do jogu. Nie wysyłać go między
   ruchami.
2. **Zawsze kończyć jogiem z zerami** — nie polegać na tym, że serwer sam zatrzyma bazę.
3. **Jedna sesja sterująca**: nie wiadomo, jak serwer rozstrzyga sterowanie równoczesne
   (panel + klient) ❓ — bezpiecznie zakładać konflikt.
4. **Rampa rozruchu** bazy (~0,2 s) — krótkie ruchy są krótsze niż wynika to z iloczynu
   prędkości i czasu.
5. `session` i `seq`: `session` nadaje serwer i jest wymagana w każdej ramce; `seq` może się
   powtarzać — nie jest to klucz sesji.
6. Brak TLS: cały ruch (w tym hash hasła) idzie otwartym tekstem w sieci lokalnej.

---

## 9. Czego serwer jeszcze przed nami skrywa

| Element | Status | Jak ustalić |
|---|---|---|
| `op=111` — znaczenie | ❓ | wysłać jog bez tego opkodu i porównać zachowanie |
| `op=32` — adresowanie celu (pola `9.3/9.4/9.5/9.11`) | ❓ | przechwycić zlecenie kilku różnych stacji z opisem, co klikał operator |
| Deadman: czy urwanie jogu zawsze zatrzymuje bazę | 🔎 | przerwać strumień w połowie i obserwować |
| Typy żądań 1 / 4 / 17 | ❓ | porównać odpowiedzi z wysyłką i bez |
| Kolejność pól odcinka trasy (`3.7.12[n]`) | ❓ | porównać trasę widoczną w panelu z ramkami stanu |
| Jednostka pozostałego dystansu (`3.7.8`) | 🔎 | skorelować z faktycznie przejechanym dystansem |
| Limity przyspieszenia i prędkości bazy | ❓ | seria prób z narastającymi wartościami |
| Znaczenie `4.2.3 = 30` w logowaniu | ❓ | próby logowania z inną wartością |

---

*Opis interfejsu sieciowego bazy jezdnej Helios (system Matrix OS). Wszystkie ustalenia dotyczą
wyłącznie warstwy komunikacji: adresacji, koperty, sesji, kanału komend i danych stanu.*

---

# 6. Kamery i strumieniowanie obrazu

# Przewodnik Uruchomienia Kamer i Strumieniowania Obrazu: Rokae Helios (ROS 2 / Orbbec / USB Cam / Web Video Server)

Kompletny podręcznik uruchomienia kamery głębi **Orbbec Gemini 335L** oraz dodatkowych kamer USB w robocie **Rokae Helios**, wraz z instrukcją bezprzewodowego przesyłania obrazu do przeglądarki WWW na komputerze, mapowaniem urządzeń `/dev/video*` oraz rozwiązaniami typowych problemów.

---

## Spis Treści

1. [Architektura Wizyjna Robota Helios](#1-architektura-wizyjna-robota-helios)
2. [Instalacja Wymaganych Pakietów i Konfiguracja Uprawnień](#2-instalacja-wymaganych-pakiet%C3%B3w-i-konfiguracja-uprawnie%C5%84)
   - [2.1 Instalacja pakietów ROS 2](#21-instalacja-pakiet%C3%B3w-ros-2)
   - [2.2 Konfiguracja reguł udev dla Orbbec](#22-konfiguracja-regu%C5%82-udev-dla-orbbec)
   - [2.3 Weryfikacja wykrycia kamery 3D](#23-weryfikacja-wykrycia-kamery-3d)
3. [Uruchomienie Kamery Głównej Orbbec Gemini 335L (Procedura Dwóch Terminali)](#3-uruchomienie-kamery-g%C5%82%C3%B3wnej-orbbec-gemini-335l-procedura-dw%C3%B3ch-terminali)
   - [3.1 Terminal 1: Uruchomienie sterownika kamery 3D](#31-terminal-1-uruchomienie-sterownika-kamery-3d)
   - [3.2 Terminal 2: Uruchomienie Web Video Server](#32-terminal-2-uruchomienie-web-video-server)
   - [3.3 Podgląd obrazu na żywo w przeglądarce](#33-podgl%C4%85d-obrazu-na-%C5%BCywo-w-przegl%C4%85darce)
4. [Błąd cv_bridge przy podglądzie głębi [16UC1] – Wyjaśnienie i Rozwiązanie](#4-b%C5%82%C4%85d-cv_bridge-przy-podgl%C4%85dzie-g%C5%82%C4%99bi-16uc1--wyja%C5%9Bnienie-i-rozwi%C4%85zanie)
   - [4.1 Dlaczego pojawia się ten błąd?](#41-dlaczego-pojawia-si%C4%99-ten-b%C5%82%C4%85d)
   - [4.2 Jak poprawnie odczytywać i oglądać głębię?](#42-jak-poprawnie-odczytywa%C4%87-i-ogl%C4%85da%C4%87-g%C5%82%C4%99bi%C4%99)
5. [Uruchamianie Dodatkowych Kamer USB (Podwozie i Korpus)](#5-uruchamianie-dodatkowych-kamer-usb-podwozie-i-korpus)
   - [5.1 Mapowanie urządzeń w systemie (v4l2-ctl)](#51-mapowanie-urz%C4%85dze%C5%84-w-systemie-v4l2-ctl)
   - [5.2 Zasada numeracji: strumień wideo vs węzeł metadanych](#52-zasada-numeracji-strumie%C5%84-wideo-vs-w%C4%99ze%C5%82-metadanych)
   - [5.3 Obsługiwane formaty pikseli w sterowniku usb_cam](#53-obs%C5%82ugiwane-formaty-pikseli-w-sterowniku-usb_cam)
   - [5.4 Komendy uruchomienia kamer USB](#54-komendy-uruchomienia-kamer-usb)
6. [Ściąga z Komend (Quick Reference)](#6-%C5%9Aci%C4%85ga-z-komend-quick-reference)
7. [Przydatne Linki i Dokumentacja](#7-przydatne-linki-i-dokumentacja)

---

## 1. Architektura Wizyjna Robota Helios

Wszystkie sensory wizyjne w robocie Helios są podłączone bezpośrednio do komputera pokładowego **NVIDIA Jetson** (`192.168.49.97`):

| Urządzenie | Model | Port USB / ID | Rola w systemie |
| :--- | :--- | :--- | :--- |
| **Kamera 3D (Głowa)** | **Orbbec Gemini 335L** | `2bc5:0804` (USB 3.2) | Kolor RGB, mapa głębi (Depth), sensory podczerwieni (IR), IMU 6-DoF |
| **Kamera USB 1** | ARC USB Webcam | `/dev/video8` | Omijanie przeszkód / podwozie |
| **Kamera USB 2** | ARC USB Webcam | `/dev/video10` | Omijanie przeszkód / podwozie |
| **Kamera USB 3** | Sunplus USB Camera | `/dev/video12` | Podgląd otoczenia / korpus |

---

## 2. Instalacja Wymaganych Pakietów i Konfiguracja Uprawnień

Wszystkie poniższe komendy wykonaj na Jetsonie (`std@192.168.49.97`):

### 2.1 Instalacja pakietów ROS 2

Zainstaluj oficjalny sterownik Orbbec, sterownik kamer USB, serwer strumieniowania HTTP oraz narzędzia V4L:

```bash
sudo apt update
sudo apt install -y ros-humble-orbbec-camera \
                    ros-humble-orbbec-description \
                    ros-humble-web-video-server \
                    ros-humble-usb-cam \
                    v4l-utils
```

### 2.2 Konfiguracja reguł udev dla Orbbec

Kamery głębi wymagają uprawnień do surowego portu USB (bez tego sterownik zgłosi brak dostępu):

```bash
sudo cp /opt/ros/humble/share/orbbec_camera/udev/99-obsensor-libusb.rules /etc/udev/rules.d/ 2>/dev/null || \
sudo cp /opt/ros/humble/share/orbbec_camera/scripts/99-obsensor-libusb.rules /etc/udev/rules.d/ 2>/dev/null

sudo udevadm control --reload-rules && sudo udevadm trigger
```

### 2.3 Weryfikacja wykrycia kamery 3D

```zsh
source /opt/ros/humble/setup.zsh
ros2 run orbbec_camera list_devices_node
```

**Oczekiwany wynik:** Wyświetlenie informacji o urządzeniu: `name: Orbbec Gemini 335L`, `pid: 0x0804`, `connection: USB3.2`, wersja firmware'u oraz lista dostępnych presetów (`Default`, `High Accuracy` itp.).

---

## 3. Uruchomienie Kamery Głównej Orbbec Gemini 335L (Procedura Dwóch Terminali)

> ⚠️ **ZASADA DZIAŁANIA:**  
> Do przesyłania obrazu na komputer potrzebujesz **dwóch niezależnych okien terminala PuTTY** działających jednocześnie.  
> **Nie wolno wciskać `Ctrl+C` w pierwszym oknie**, ponieważ wyłącza to fizyczne zasilanie matrycy kamery i serwer wideo nie będzie miał czego nadawać!

---

### 3.1 Terminal 1: Uruchomienie sterownika kamery 3D

W pierwszym oknie PuTTY:

```zsh
source /opt/ros/humble/setup.zsh
ros2 launch orbbec_camera gemini_330_series.launch.py
```

*Po chwili pojawi się komunikat: `Initialize device cost: ... ms`. **Pozostaw to okno otwarte i działające w tle!***

---

### 3.2 Terminal 2: Uruchomienie Web Video Server

Otwórz drugie okno PuTTY do Jetsona (`std@192.168.49.97`):

```zsh
source /opt/ros/humble/setup.zsh
ros2 run web_video_server web_video_server
```

*W terminalu pojawi się: `Waiting For connections on 0.0.0.0:8080`.*

---

### 3.3 Podgląd obrazu na żywo w przeglądarce

Na swoim laptopie (połączonym z siecią robota) otwórz przeglądarkę internetową:

1. **Lista wszystkich tematów kamer:**
   ```text
   http://192.168.49.97:8080/
   ```
2. **Płynny podgląd obrazu RGB (kolor):**
   ```text
   http://192.168.49.97:8080/stream?topic=/camera/color/image_raw
   ```
   *Obraz z głowy robota pojawi się natychmiast na żywo.*

---

## 4. Błąd cv_bridge przy podglądzie głębi [16UC1] – Wyjaśnienie i Rozwiązanie

Gdy w `web_video_server` klikniesz na temat głębi `/camera/depth/image_raw`, w Terminalu 2 pojawia się błąd:

```text
[ERROR] [web_video_server.mjpeg_streamer]: cv_bridge exception: [16UC1] is not a color format. but [bgr8] is. The conversion does not make sense
[INFO] [web_video_server]: Removed Stream: /camera/depth/image_raw
```

### 4.1 Dlaczego pojawia się ten błąd?

1. **Różnica formatów:**
   * Obraz kolorowy (`/camera/color/image_raw`) używa formatu **`RGB8` / `BGR8`** (trzy kanały po 8 bitów na kolor: 0–255). Taki obraz można bez problemu skompresować do klatki JPEG/MJPEG i wyświetlić w przeglądarce.
   * Obraz głębi (`/camera/depth/image_raw`) ma format **`16UC1`** (*16-bit Unsigned, 1 Channel*). Każdy piksel to **pojedyncza 16-bitowa liczba oznaczająca fizyczną odległość od kamery w milimetrach** (od 0 do 65 535 mm).
2. **Brak sensu bezpośredniej konwersji:**
   * Przeglądarka internetowa oraz odtwarzacze wideo (MJPEG) nie obsługują 16-bitowych surowych liczb jako klatek wideo.
   * Moduł `cv_bridge` próbuje przekonwertować tablicę liczb milimetrów na standardowy kolorowy piksel `BGR8`. Zwraca błąd, ponieważ bez zdefiniowania reguły (jaka odległość odpowiada jakiemu kolorowi) bezpośrednia konwersja jest matematycznie niemożliwa.

### 4.2 Jak poprawnie odczytywać i oglądać głębię?

* **Do podglądu wideo na żywo w przeglądarce:** Należy używać strumienia kolorowego:
  ```text
  http://192.168.49.97:8080/stream?topic=/camera/color/image_raw
  ```
* **Statyczny podgląd klatki głębi w przeglądarce (PNG Snapshot):**
  Format PNG w przeciwieństwie do JPEG obsługuje 16-bitową głębię. Wpisz w przeglądarkę:
  ```text
  http://192.168.49.97:8080/snapshot?topic=/camera/depth/image_raw
  ```
* **Pełna wizualizacja 3D (RVIZ2):**
  Surowe dane głębi (`/camera/depth/image_raw`) oraz wygenerowana z nich trójwymiarowa chmura punktów (`/camera/depth/points`) są przeznaczone do bezpośredniej wizualizacji w programie **RViz2** na stacji roboczej lub do algorytmów nawigacji i omijania przeszkód (Costmap 2D/3D).

---

## 5. Uruchamianie Dodatkowych Kamer USB (Podwozie i Korpus)

Oprócz kamery głębi, Helios posiada 3 dodatkowe kamery USB.

### 5.1 Mapowanie urządzeń w systemie (v4l2-ctl)

Aby sprawdzić przypisanie kamer do plików urządzeń w systemie, wpisz:

```zsh
v4l2-ctl --list-devices
```

Wynik w Twoim systemie:
```text
Orbbec Gemini 335L:
        /dev/video0, /dev/video1, /dev/video2, /dev/video3

USB Webcam (ARC):
        /dev/video8, /dev/video9

USB Webcam (ARC):
        /dev/video10, /dev/video11

USB Camera (Sunplus):
        /dev/video12, /dev/video13
```

---

### 5.2 Zasada numeracji: strumień wideo vs węzeł metadanych

W systemie Linux każda kamera USB tworzy **parę urządzeń**:
* **Numer parzysty (`/dev/video8`, `/dev/video10`, `/dev/video12`):**  
  To jest **właściwy strumień wideo** pobierany z sensora kamery. Zawsze wskazuj ten numer!
* **Numer nieparzysty (`/dev/video9`, `/dev/video11`, `/dev/video13`):**  
  To jest węzeł metadanych UVC (parametry ekspozycji, diagnostyka). **Próba otwarcia numeru nieparzystego jako kamery zakończy się błędem!**

---

### 5.3 Obsługiwane formaty pikseli w sterowniku usb_cam

Sterownik `usb_cam` w ROS 2 przyjmuje ściśle zdefiniowane nazwy formatów:

| Format w `usb_cam` | Zastosowanie |
| :--- | :--- |
| **`yuyv`** | Nieskompresowany format koloru YUV 4:2:2 (domyślny i zalecany dla większości kamer USB) |
| **`mjpeg2rgb`** | Sprzętowo kompresowany strumień MJPEG (najmniejsze obciążenie magistrali USB) |
| **`mono8`** | Obraz czarno-biały (8-bitowy). *(Uwaga: format nazywa się `mono8`, a nie `grey`!)* |

---

### 5.4 Komendy uruchomienia kamer USB

Wybierz kamerę, którą chcesz włączyć:

#### Kamera podwozia 1 (`/dev/video8`):
```zsh
source /opt/ros/humble/setup.zsh
ros2 run usb_cam usb_cam_node_exe --ros-args \
  -p video_device:=/dev/video8 \
  -p pixel_format:=yuyv \
  -p image_width:=640 \
  -p image_height:=480
```

#### Kamera podwozia 2 (`/dev/video10`):
```zsh
source /opt/ros/humble/setup.zsh
ros2 run usb_cam usb_cam_node_exe --ros-args \
  -p video_device:=/dev/video10 \
  -p pixel_format:=yuyv \
  -p image_width:=640 \
  -p image_height:=480
```

#### Kamera szerokokątna Sunplus (`/dev/video12`):
```zsh
source /opt/ros/humble/setup.zsh
ros2 run usb_cam usb_cam_node_exe --ros-args \
  -p video_device:=/dev/video12 \
  -p pixel_format:=mjpeg2rgb \
  -p image_width:=640 \
  -p image_height:=480
```

*Obraz z kamery USB natychmiast pojawi się w przeglądarce pod adresem:*
```text
http://192.168.49.97:8080/stream?topic=/image_raw
```

---

## 6. Ściąga z Komend (Quick Reference)

| Czynność | Komenda |
| :--- | :--- |
| **1. Lista fizycznych kamer** | `v4l2-ctl --list-devices` |
| **2. Weryfikacja Orbbec 3D** | `ros2 run orbbec_camera list_devices_node` |
| **3. Start kamery 3D (Terminal 1)** | `ros2 launch orbbec_camera gemini_330_series.launch.py` |
| **4. Start serwera WWW (Terminal 2)** | `ros2 run web_video_server web_video_server` |
| **5. Start kamery USB (np. video8)** | `ros2 run usb_cam usb_cam_node_exe --ros-args -p video_device:=/dev/video8 -p pixel_format:=yuyv` |
| **6. Podgląd w przeglądarce** | `http://192.168.49.97:8080/stream?topic=/camera/color/image_raw` |

---

## 7. Przydatne Linki i Dokumentacja

* **OrbbecSDK ROS 2 Wrapper:** [https://github.com/orbbec/OrbbecSDK_ROS2](https://github.com/orbbec/OrbbecSDK_ROS2)
* **Dokumentacja parametrów Orbbec Gemini:** [https://orbbec.github.io/OrbbecSDK_ROS2/en/source/camera_devices/5_advanced_guide/advanced_guide.html](https://orbbec.github.io/OrbbecSDK_ROS2/en/source/camera_devices/5_advanced_guide/advanced_guide.html)
* **Pakiet ROS 2 usb_cam:** [https://index.ros.org/p/usb_cam/](https://index.ros.org/p/usb_cam/)
* **Pakiet Web Video Server:** [https://index.ros.org/p/web_video_server/](https://index.ros.org/p/web_video_server/)

---

# 7. Mikrofon, STT i integracja ROS 2

# Przewodnik Uruchomienia Mikrofonu i Integracji Speech-to-Text (STT) dla LLM: Rokae Helios (ROS 2)

Kompletna dokumentacja konfiguracji wbudowanej macierzy mikrofonowej robota **Rokae Helios**, integracji ze środowiskiem **ROS 2 Humble** oraz przygotowania kompletnego łańcucha przetwarzania mowy na tekst (**Speech-to-Text / STT**) do sterowania robotem za pomocą modeli językowych (**LLM**).

---

## Spis Treści

1. [Architektura Urządzeń Audio w Heliosie](#1-architektura-urz%C4%85dze%C5%84-audio-w-heliosie)
2. [Identyfikacja i Testy Mikrofonu (ALSA / PulseAudio)](#2-identyfikacja-i-testy-mikrofonu-alsa--pulseaudio)
   - [2.1 Adresowanie sprzętowe ALSA](#21-adresowanie-sprz%C4%89towe-alsa)
   - [2.2 Szybki test rejestracji dźwięku](#22-szybki-test-rejestracji-d%C5%BAwi%C4%99ku)
   - [2.3 Regulacja czułości mikrofonu (alsamixer)](#23-regulacja-czu%C5%82o%C5%9Bci-mikrofonu-alsamixer)
3. [Dwie Ścieżki Integracji z ROS 2](#3-dwie-%C5%9Bcie%C5%BCki-integracji-z-ros-2)
   - [Ścieżka A (Zalecana dla LLM): Węzeł STT publikujący gotowy tekst](#%C5%9Bcie%C5%BCka-a-zalecana-dla-llm-w%C4%99ze%C5%82-stt-publikuj%C4%85cy-gotowy-tekst)
   - [Ścieżka B: Strumieniowanie surowego audio po ROS 2 (audio_capture)](#%C5%9Bcie%C5%BCka-b-strumieniowanie-surowego-audio-po-ros-2-audio_capture)
4. [Co Będzie Potrzebne do Integracji z LLM? (Architektura Potoku)](#4-co-b%C4%99dzie-potrzebne-do-integracji-z-llm-architektura-potoku)
   - [4.1 Warstwa 1: Detekcja głosu (Voice Activity Detection - VAD)](#41-warstwa-1-detekcja-g%C5%82osu-voice-activity-detection---vad)
   - [4.2 Warstwa 2: Model Speech-to-Text (Lokalny na Jetsonie vs Chmura)](#42-warstwa-2-model-speech-to-text-lokalny-na-jetsonie-vs-chmura)
   - [4.3 Warstwa 3: Architektura tematów ROS 2](#43-warstwa-3-architektura-temat%C3%B3w-ros-2)
5. [Instrukcja Konfiguracji Ścieżki A (Lokalny Whisper STT na Jetson AGX Orin)](#5-instrukcja-konfiguracji-%C5%9Bcie%C5%BCki-a-lokalny-whisper-stt-na-jetson-agx-orin)
   - [5.1 Wymagane biblioteki systemowe i pythonowe](#51-wymagane-biblioteki-systemowe-i-pythonowe)
   - [5.2 Gotowy węzeł ROS 2: Mic $\rightarrow$ Whisper $\rightarrow$ ROS Topic](#52-gotowy-w%C4%99ze%C5%82-ros-2-mic-%E2%86%92-whisper-%E2%86%92-ros-topic)
6. [Instrukcja Konfiguracji Ścieżki B (Pakiet ros-humble-audio-capture)](#6-instrukcja-konfiguracji-%C5%9Bcie%C5%BCki-b-pakiet-ros-humble-audio-capture)
7. [Rozwiązywanie Typowych Problemów](#7-rozwi%C4%85zywanie-typowych-problem%C3%B3w)

---

## 1. Architektura Urządzeń Audio w Heliosie

Podczas analizy sprzętowej na komputerze pokładowym NVIDIA Jetson (`192.168.49.97`) zidentyfikowano następujące urządzenia wejściowe:

| Karta ALSA | Nazwa urządzenia | Identyfikator w systemie | Zastosowanie |
| :--- | :--- | :--- | :--- |
| **`card 3`** | **`REF [HK USB REF]`** | `plughw:3,0` / `alsa_input.usb-CF-IC_HK_USB_REF...` | **Wbudowana macierz mikrofonów robota Helios** (podstawowe źródło audio) |
| `card 2` | `Webcam [USB Webcam]` | `plughw:2,0` | Mikrofon wbudowany w kamerę podwozia 1 |
| `card 4` | `Webcam_1 [USB Webcam]` | `plughw:4,0` | Mikrofon wbudowany w kamerę podwozia 2 |

> 💡 **Wniosek:** Do komunikacji z człowiekiem i sterowania LLM należy zawsze używać urządzenia **`card 3` (`plughw:3,0`)**.

---

## 2. Identyfikacja i Testy Mikrofonu (ALSA / PulseAudio)

### 2.1 Adresowanie sprzętowe ALSA

W bibliotece ALSA mikrofon robota adresowany jest jako:
```text
plughw:3,0
```
Użycie prefiksu `plughw` (zamiast `hw`) jest zalecane, ponieważ automatycznie konwertuje próbkowanie i format bitowy, zapobiegając konfliktom sprzętowym.

### 2.2 Szybki test rejestracji dźwięku

Przetestowany i w 100% działający parametr nagrywania:
```zsh
arecord -D plughw:3,0 -f S16_LE -r 16000 -c 1 -d 5 /home/std/test_mic.wav
```
* **Format:** `S16_LE` (16-bit Signed Little-Endian)
* **Częstotliwość:** `16000 Hz` (16 kHz – uniwersalny standard dla modeli AI, np. Whisper)
* **Kanały:** `1` (Mono)
* **Czas:** `5 sekund`

### 2.3 Regulacja czułości mikrofonu (alsamixer)

Jeśli mikrofon nagrywa zbyt cicho lub zbiera zbyt dużo szumów wentylatorów:
1. Wpisz w terminalu:
   ```zsh
   alsamixer -c 3
   ```
2. Klawiszem `F4` przejdź do sekcji **Capture**.
3. Strzałkami w górę/dół ustaw czułość wejściową (optymalnie: **75% – 85%**).
4. Wyjdź wciskając `Esc`.

---

## 3. Dwie Ścieżki Integracji z ROS 2

W zależności od tego, gdzie ma działać model sztucznej inteligencji (LLM), stosuje się jedno z dwóch podejść:

### Ścieżka A (Zalecana dla LLM): Węzeł STT publikujący gotowy tekst
* **Jak działa:** Jetson AGX Orin posiada potężny układ GPU z rdzeniami Tensor. Pobiera dźwięk bezpośrednio z `plughw:3,0`, uruchamia zoptymalizowany model **Faster-Whisper** (lub wysyła zapytanie do API OpenAI/Groq) i publikuje gotowy tekst wprost na temat ROS 2:
  ```text
  /robot/speech_to_text (std_msgs/msg/String)
  ```
* **Zaleta:** Zerowe obciążenie sieci Wi-Fi (przesyłany jest tylko lekki tekst, a nie ciężki strumień audio), brak opóźnień sieciowych, gotowość do bezpośredniego przekazania tekstu do węzła LLM.

### Ścieżka B: Strumieniowanie surowego audio po ROS 2 (audio_capture)
* **Jak działa:** Standardowy pakiet `ros-humble-audio-capture` pobiera dźwięk z ALSA i transmituje pakiety bajtów `audio_common_msgs/msg/AudioData` na temat ROS 2:
  ```text
  /audio/audio
  ```
* **Zastosowanie:** Gdy całe przetwarzanie AI/STT chcesz przenieść na zewnętrzny serwer/laptop, a robot ma działać wyłącznie jako bezprzewodowy mikrofon.

---

## 4. Co Będzie Potrzebne do Integracji z LLM? (Architektura Potoku)

Kompletny system konwersacji człowiek-robot składa się z trzech warstw:

```
[ Mikrofon Heliosa plughw:3,0 ]
              │
              ▼
   1. Detekcja Mowy (VAD - Silero VAD)
      (Wykrywa początek i koniec wypowiedzi użytkownika)
              │
              ▼
   2. Transkrypcja (Whisper STT)
      (Zamienia mowę w języku polskim/angielskim na string tekstowy)
              │
              ▼ Publikacja do ROS 2
   Temat: /robot/user_prompt (std_msgs/String)
              │
              ▼ Subskrypcja przez Węzeł LLM
   3. Mózg Robota: Węzeł LLM (np. Ollama / Llama 3 / Claude / GPT)
      (Analizuje intencję, decyduje o ruchu ramion/bazy i generuje odpowiedź)
```

### 4.1 Warstwa 1: Detekcja głosu (Voice Activity Detection - VAD)
Nie należy transkrybować ciągłej ciszy ani szumu wentylatorów. Biblioteka **Silero VAD** (lub proste odcięcie poziomu RMS) pozwala węzłowi:
* Czekać na moment, gdy człowiek zacznie mówić.
* Zbierać próbki dźwięku tak długo, jak człowiek mówi.
* Gdy nastąpi 0.8s ciszy — zamknąć bufor i wysłać nagranie do transkrypcji.

### 4.2 Warstwa 2: Model Speech-to-Text (Lokalny na Jetsonie vs Chmura)
* **Opcja lokalna (rekomendowana dla Jetson AGX Orin):**  
  Model **Faster-Whisper** (wersja `tiny` lub `base` dla języka polskiego/angielskiego). Dzięki akceleracji CUDA na Orinie transkrypcja 3-sekundowej wypowiedzi trwa około **0.2 sekundy**!
* **Opcja chmurowa (API):**  
  Wysyłanie bufora audio do API (np. Groq Whisper Cloud – odpowiedź w 150 ms lub OpenAI Whisper).

### 4.3 Warstwa 3: Architektura tematów ROS 2

Dla przejrzystości systemu rekomenduje się następujące tematy:
* **`/robot/speech_to_text`** (`std_msgs/msg/String`): Rozpoznany tekst użytkownika.
* **`/robot/speech_status`** (`std_msgs/msg/String`): Stan mikrofonu: `LISTENING`, `RECORDING`, `PROCESSING`.
* **`/robot/llm_response`** (`std_msgs/msg/String`): Odpowiedź wygenerowana przez LLM.

---

## 5. Instrukcja Konfiguracji Ścieżki A (Lokalny Whisper STT na Jetson AGX Orin)

### 5.1 Wymagane biblioteki systemowe i pythonowe

Na Jetsonie (`192.168.49.97`) zainstaluj narzędzia do obsługi audio w Pythonie:

```bash
sudo apt update
sudo apt install -y python3-pyaudio portaudio19-dev python3-pip

# Instalacja lekkiego i superszybkiego silnika Whisper
pip3 install faster-whisper numpy
```

### 5.2 Gotowy węzeł ROS 2: Mic $\rightarrow$ Whisper $\rightarrow$ ROS Topic

Możesz umieścić węzeł transkrypcji mowy w swoim workspace `rokae_ws` lub uruchomić go jako niezależny skrypt Python w ROS 2.

Struktura działania takiego węzła:
1. Otwiera strumień audio z urządzenia `plughw:3,0` (16 kHz, mono).
2. Buforuje próbki po przekroczeniu progu głośności (VAD).
3. Po zakończeniu wypowiedzi przekazuje bufor do `faster_whisper.WhisperModel("base")`.
4. Publikuje rozpoznany tekst do tematu `/robot/speech_to_text`.
5. Węzeł LLM (subskrybujący ten temat) otrzymuje czysty tekst pytania/polecenia od użytkownika.

---

## 6. Instrukcja Konfiguracji Ścieżki B (Pakiet ros-humble-audio-capture)

Jeśli wolisz transmitować surowe audio z mikrofonu przez standardowe pakiety ROS 2:

### 6.1 Instalacja pakietów
```bash
sudo apt update
sudo apt install -y ros-humble-audio-capture ros-humble-audio-common-msgs
```

### 6.2 Uruchomienie węzła audio_capture na karcie robota
Uruchom węzeł wskazując urządzenie ALSA `hw:3,0`:
```zsh
source /opt/ros/humble/setup.zsh
ros2 run audio_capture audio_capture_node --ros-args \
  -p device:="plughw:3,0" \
  -p channels:=1 \
  -p sample_rate:=16000 \
  -p format:=wave
```

### 6.3 Weryfikacja publikacji strumienia
W drugim terminalu sprawdź, czy pakiety audio płyną:
```zsh
ros2 topic hz /audio/audio
ros2 topic echo /audio/audio --once
```

---

## 7. Rozwiązywanie Typowych Problemów

### Problem: Zmiana numeru karty po restarcie robota (`card 3` staje się np. `card 2`)
* **Przyczyna:** Linux przypisuje numery kart USB w kolejności ich wykrycia przy starcie jądra.
* **Rozwiązanie:** W konfiguracji zamiast numeru indeksu `hw:3,0` używaj identyfikatora po nazwie karty ALSA:
  ```text
  plughw:REF,0
  ```
  ALSA automatycznie znajdzie kartę `REF` niezależnie od tego, pod który numer indeksu została podpięta po restarcie!

### Problem: Model Whisper źle rozpoznaje polskie słowa lub ucina końcówki
* **Rozwiązanie:** 
  1. Upewnij się, że parametr języka w Whisperze jest ustawiony na `language="pl"` (zapobiega to próbom tłumaczenia na angielski w locie).
  2. Sprawdź poziom wzmocnienia mikrofonu w `alsamixer -c 3`.

---

## 8. Podsumowanie Kroków dla Zespołu

1. **Mikrofon sprzętowy:** Podłączony jako `plughw:REF,0` (lub `plughw:3,0`).
2. **Format próbek:** Zawsze 16 000 Hz, 16-bit, Mono (`S16_LE`).
3. **Integracja z LLM:** Mikrofon zbiera mowę $\rightarrow$ model STT publikuje tekst do tematu `/robot/speech_to_text` $\rightarrow$ LLM odbiera polecenie i generuje akcję robota.

---

# 8. Głośniki

# Helios: sterowanie głośnikami (komputer pokładowy)

**Sprzęt:** Jetson AGX Orin, Ubuntu. Głośniki to karta USB **card 3: `HK USB REF`** (nazwa ALSA: `REF`). Karty 0 (HDMI) i 1 (APE) to wyjścia wewnętrzne Jetsona, nie głośniki. Jest już ustawiony na deafultowy

## Co działa

| Cel | Komenda |
|---|---|
| Test głośników | `speaker-test -D plughw:3,0 -c 2 -t wav` |
| Odtwarzanie WAV | `aplay -D plughw:3,0 plik.wav` |
| Głośność | `amixer -c 3 sset 'PCM' 70%` |
| Lista suwaków karty | `amixer -c 3 scontrols` (widać `PCM` i `Mic`) |

- Numer karty może się zmienić po restarcie, więc pewniejszy zapis to `plughw:CARD=REF,DEV=0`.
- Zakres `PCM` to 0-999. W logu: 70% = 699 (+2,72 dB), 10% = 100 (+0,39 dB).
- `plughw` sam konwertuje format, więc plik 44,1 kHz stereo 16-bit odtwarza się bez problemu (karta pracuje na 48 kHz).

## Co nie działa

- **`aplay plik.wav` i `speaker-test -c 2 -t wav` bez `-D`** nie grają na głośnikach. Urządzenie domyślne nie wskazuje na kartę USB. Komendy nie zgłaszają błędu, po prostu nie ma dźwięku.
- **`pactl set-sink-volume @DEFAULT_SINK@ ...`** nie zmienia głośności głośników. Prawdopodobnie domyślny sink nie jest kartą USB (wniosek z zachowania, niesprawdzony).

## Komunikaty z logów, które są niegroźne

- `Transfer failed: Bad address` pojawia się po przerwaniu `speaker-test` przez Ctrl+C. To nie jest błąd transferu pliku.
- `aplay: write error: Interrupted system call` pojawia się po przerwaniu `aplay` przez Ctrl+C.

## Uwaga do komend

Poprawna nazwa to `pactl list sinks short` (w logu była literówka `skins`).

## Kopiowanie pliku WAV na robota

```bash
# na robocie
whoami
hostname -I

# na swoim komputerze
scp plik.wav uzytkownik@ADRES_IP:/home/uzytkownik/
```

## Do sprawdzenia

Żeby zwykłe `aplay plik.wav` grało na głośnikach, trzeba ustawić kartę USB jako domyślną:

- przez `~/.asoundrc`:
  ```
  pcm.!default { type plug slave.pcm "hw:CARD=REF,DEV=0" }
  ctl.!default { type hw card REF }
  ```
- albo przez PulseAudio: `pactl list sinks short`, a potem `pactl set-default-sink <nazwa_sinka>`.

---

# 9. Złącze narzędziowe M8

# Dokumentacja Techniczna: Złącze Narzędziowe M8 Flanszy (Rokae AR5 / Helios)

## 1. Identyfikacja i Przeznaczenie

Niniejszy dokument opisuje interfejs elektryczny i programistyczny złącza narzędziowego na flanszy nadgarstka (7. oś) ramienia **Rokae AR5 (AR5-5_0.7R-W4C4A2)** zainstalowanego w robocie mobilnym **Rokae Helios**.

* **Typ złącza fizycznego:** M8, 8-pinowe, kodowanie A (A-coded), męskie (na ramieniu robota).
* **Identyfikator płytki w SDK Rokae:** `board = 1` (*Tool Board* / Płytka narzędziowa).
* **Zastosowanie:** Podłączanie chwytaków elektrycznych, elektrozaworów pneumatycznych, przyssawek próżniowych oraz czujników detekcji detalu.

---

## 2. Rozpiska Wyprowadzeń (Pinout) i Parametry Elektryczne

| Nr Pinu | Sygnał | Kierunek | Logika | Napięcie znamionowe | Prąd maksymalny | Opis |
| :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **1** | **+24V DC** | Wyjście zasilania | Zasilanie | +24V DC ±10% | 1.0 A (pik 1.5 A) | Stałe zasilanie narzędzia po załączeniu serwonapędów |
| **2** | **GND (0V)** | Masa | Odniesienie | 0V | — | Wspólna masa zasilania i sygnałów |
| **3** | **DI1** | Wejście | PNP (Active High) | 0V / +24V DC | ~5 mA | Cyfrowe wejście sygnałowe 1 (czujnik detalu / krańcówka) |
| **4** | **DI2** | Wejście | PNP (Active High) | 0V / +24V DC | ~5 mA | Cyfrowe wejście sygnałowe 2 (potwierdzenie otwarcia szczęk) |
| **5** | **DO1** | Wyjście | PNP (Source) | 0V / +24V DC | 500 mA | Cyfrowe wyjście sterujące 1 (np. zaciśnij chwytak) |
| **6** | **DO2** | Wyjście | PNP (Source) | 0V / +24V DC | 500 mA | Cyfrowe wyjście sterujące 2 (np. otwórz chwytak) |
| **7** | **RS485_A** | Komunikacja | Różnicowa | TIA/EIA-485-A | — | Linia danych D+ (magistrala szeregowa) |
| **8** | **RS485_B** | Komunikacja | Różnicowa | TIA/EIA-485-A | — | Linia danych D- (magistrala szeregowa) |

---

## 3. Co MOŻNA Konfigurować i Sterować

Z poziomu oprogramowania (C++ SDK Rokae oraz ROS 2) w modelu AR5 kontrolowane są:

### A. Stan Wyjść Cyfrowych DO1 i DO2
* Możliwość niezależnego załączania stanu wysokiego (+24V) i niskiego (0V) na pinach 5 i 6.
* Zastosowanie: Załączanie elektrozaworów, przekaźników, wyzwalanie cyklu pracy prostych chwytaków elektrycznych.

### B. Odczyt Wejść Cyfrowych DI1 i DI2
* Możliwość odczytu aktualnego stanu logicznego na pinach 3 i 4.
* Zastosowanie: Weryfikacja sygnałów z kontaktronów, czujników indukcyjnych lub optycznych potwierdzających obecność chwyconego detalu.

---

## 4. Czego NIE MOŻNA Zmieniać i Dlaczego (Ograniczenia Sprzętowe AR5)

W odróżnieniu od innych ramion przemysłowych i innych serii firmy Rokae, model **AR5** posiada sztywną konstrukcję toru zasilania i sygnałów:

### 1. Brak programowej zmiany napięcia zasilania (24V vs 12V)
* **Dlaczego:** W ramionach serii **CR** oraz **SR** na nadgarstku montowany jest opcjonalny moduł `xPanel`, w którym zintegrowano przetwornicę DC-DC sterowaną rejestrem (metoda SDK `setxPanelVout`). W ramieniu **AR5** zasilanie 24V na pinie 1 pochodzi bezpośrednio z wewnętrznej magistrali zasilania robota.
* **Skutek programowy:** Wywołanie metody `setxPanelVout()` lub `setxPanelRS485()` w ramieniu AR5 zawsze zwróci błąd (`General failure` / brak obsługi urządzenia).

### 2. Brak programowej zmiany polaryzacji (PNP vs NPN)
* **Dlaczego:** Flansza AR5 jest sprzętowo zaprojektowana wyłącznie w topologii **PNP (Source)**:
  * Wyjścia **DO** przy stanie `true` łączą pin wyjściowy z szyną **+24V**. Odbiornik musi być wpięty między wyjście DO a masę GND (pin 2).
  * Wejścia **DI** interpretują napięcie z zakresu 15–24V jako stan wysoki `true`, a brak napięcia (lub masę) jako stan niski `false`.
* **Skutek:** Podłączenie czujników typu NPN (zwierających do masy) wymaga zewnętrznego konwertera poziomów logicznych lub przekaźnika.

### 3. Sztywny limit prądowy zasilania (1.0 A)
* **Dlaczego:** Przewody zasilające wewnątrz ramienia przechodzą przez miniaturowe pierścienie ślizgowe i złącza elastyczne 7 przegubów.
* **Skutek:** Nie można zasilać ze złącza M8 chwytaków dużej mocy, wrzecion szlifierskich ani silnych elektromagnesów pobierających > 1.0 A ciągłego prądu. W takich przypadkach przewód zasilający należy poprowadzić zewnętrznie.

---

## 5. Zestawienie Metod C++ (Rokae SDK)

Aby skorzystać z metod w kodzie C++, wymagany jest obiekt robota typu `rokae::xMateErProRobot` (nagłówek `#include "rokae/robot.h"`).

### 1. `robot.setDO(board, port, state, ec)`
Ustawia stan wyjścia cyfrowego.
* **Parametry:**
  * `unsigned int board`: Numer płytki. Dla flanszy narzędziowej **zawsze `1`**.
  * `unsigned int port`: Indeks wyjścia. Indeksowane od 0:
    * `0` = wyjście fizyczne **DO1** (Pin 5)
    * `1` = wyjście fizyczne **DO2** (Pin 6)
  * `bool state`: `true` = podaj +24V (HIGH), `false` = odetnij napięcie / 0V (LOW).
  * `std::error_code &ec`: Zmienna na kod błędu wykonania.
* **Przykład użycia:**
  ```cpp
  std::error_code ec;
  // Włączenie wyjścia DO1 (podanie 24V na pin 5)
  robot.setDO(1, 0, true, ec);
  if (ec) {
      std::cerr << "Błąd setDO: " << ec.message() << std::endl;
  }
  ```

### 2. `robot.getDO(board, port, ec)`
Odczytuje aktualnie ustawiony stan wyjścia cyfrowego.
* **Parametry:**
  * `board`: `1` (Flansza)
  * `port`: `0` (DO1) lub `1` (DO2)
  * `ec`: Referencja do `std::error_code`
* **Wartość zwracana:** `bool` (`true` = HIGH, `false` = LOW).
* **Przykład użycia:**
  ```cpp
  bool stan_do1 = robot.getDO(1, 0, ec);
  ```

### 3. `robot.getDI(board, port, ec)`
Odczytuje fizyczny stan napięcia na wejściu cyfrowym.
* **Parametry:**
  * `board`: `1` (Flansza)
  * `port`: Indeksowane od 0:
    * `0` = wejście fizyczne **DI1** (Pin 3)
    * `1` = wejście fizyczne **DI2** (Pin 4)
  * `ec`: Referencja do `std::error_code`
* **Wartość zwracana:** `bool` (`true` = na wejściu jest +24V, `false` = wejście nieaktywne).
* **Przykład użycia:**
  ```cpp
  bool obiekt_chwycony = robot.getDI(1, 0, ec);
  if (obiekt_chwycony) {
      std::cout << "Czujnik DI1 wykrył detal w chwytaku.\n";
  }
  ```

---

## 6. Sterowanie przez ROS 2 (Serwisy `rokae_driver7`)

W środowisku ROS 2 dostęp do złącza flanszy jest realizowany przez serwisy publikowane przez węzeł sterownika:

### A. Przełączanie wyjścia DO przez ROS 2
* **Nazwa serwisu:** `/rokae_driver7/set_do`
* **Typ wiadomości:** `rokae_msgs/srv/SetDO`
* **Pola:**
  * `uint32 board`: `1` (Tool Board)
  * `uint32 port`: `0` (dla DO1) lub `1` (dla DO2)
  * `bool state`: `true` (24V) / `false` (0V)
* **Wywołanie z terminala:**
  ```bash
  # Załączenie wyjścia DO1:
  ros2 service call /rokae_driver7/set_do rokae_msgs/srv/SetDO "{board: 1, port: 0, state: true}"

  # Wyłączenie wyjścia DO1:
  ros2 service call /rokae_driver7/set_do rokae_msgs/srv/SetDO "{board: 1, port: 0, state: false}"
  ```

### B. Odczyt stanu wejścia DI przez ROS 2
* **Nazwa serwisu:** `/rokae_driver7/get_di`
* **Typ wiadomości:** `rokae_msgs/srv/GetDI`
* **Pola zapytania:** `board: 1`, `port: 0`
* **Wywołanie z terminala:**
  ```bash
  ros2 service call /rokae_driver7/get_di rokae_msgs/srv/GetDI "{board: 1, port: 0}"
  ```

---

## 7. Zalecenia Bezpieczeństwa przy Podłączaniu Chwytaka

1. **Kolejność załączania zasilania:**  
   Prace monterskie i wpinanie wtyku M8 należy wykonywać **wyłącznie przy wyłączonym zasilaniu kontrolera robota**.
2. **Ochrona przed przepięciami (Diody Flyback):**  
   W przypadku podłączania obciążeń indukcyjnych (np. cewki elektrozaworów pneumatycznych) bezpośrednio do pinów DO1/DO2, należy upewnić się, że zawór posiada wbudowaną diodę gaszącą przepięcia (flyback diode). Brak diody może uszkodzić tranzystor wyjściowy płytki flanszy.
3. **Prowadzenie masy (GND):**  
   Masa czujników i masa chwytaka muszą być podłączone do pinu 2 (GND). Nie wolno zamykać obwodu sygnałowego przez metalową konstrukcję mechaniczną ramienia.

---

# 10. Źródła i dalsza dokumentacja

Dokumentację producenta traktuj jako uzupełnienie, a zachowanie konkretnego modelu i wersji kontrolera weryfikuj na stanowisku przed wdrożeniem.

https://docs.rokae.com/en/docs/xCore/控制系统基础操作/

https://docs.rokae.com/en/docs/xCore/故障排查/

https://docs.rokae.com/en/docs/xCore/安全功能/

https://docs.rokae.com/en/docs/SDK/sdk_assist/?_highlight=robot&_highlight=assist#general-apis

https://docs.rokae.com/en/docs/SDK/sdk_assist/?_highlight=movec#arc-trajectory-movec

https://docs.rokae.com/en/docs/Plugin/

## Uwaga o stanie wiedzy

Opis protokołu Matrix OS zawiera poziomy pewności `[R]` (potwierdzone na robocie), `[P]` (wywnioskowane z ruchu panelu), `[A]` (sprawdzone wyłącznie na atrapie) i `[?]` (nieustalone). Nie implementuj zachowań oznaczonych jako niezweryfikowane bez osobnego testu. Kody błędów i zachowanie xCore mogą zależeć od wersji RobotAssist/firmware.

## Zalecana kolejność pracy nowego programisty

1. Przeczytaj mapę systemu, uruchomienie oraz ostrzeżenia bezpieczeństwa.
2. Potwierdź wersje SDK, ROS 2, RobotAssist i firmware w rejestrze projektu.
3. Przetestuj SSH i sieć przed uruchomieniem jakiegokolwiek ruchu.
4. Zacznij od diagnostyki bez ruchu, a następnie testu pojedynczego modułu z małą prędkością.
5. Dla bazy AMR używaj najpierw atrapy `bench_server.py`; na robocie stosuj przejęcie sterowania, jawny stop i kontrolowane oddanie sterowania.
6. Zapisuj datę, wersję oprogramowania, konfigurację, logi i rezultat prób. Nie traktuj niezweryfikowanych obserwacji jako specyfikacji.
