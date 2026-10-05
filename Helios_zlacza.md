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
