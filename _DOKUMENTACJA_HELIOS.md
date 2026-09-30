# Dokumentacja Techniczna i Rozwiązywanie Problemów: Rokae Helios (RobotAssist / Sieć / ROS)

Zwięzły przewodnik konfiguracji, procedur bezpieczeństwa oraz rozwiązywania typowych problemów na robocie dwuramiennym **Rokae Helios** (kontroler xCore, tors 4-osiowy `TaiHu`, ramiona 7-osiowe xMate, komputer nadrzędny NVIDIA Jetson).

---

## 1. Architektura Systemu i Sterowanie Wieloma Elementami

Robot składa się z niezależnych kontrolerów połączonych siecią Ethernet:
* **Tors (`TaiHu`):** 4 osie (J1–J4), IP: `192.168.71.254`
* **Prawe ramię:** 7 osi (J1–J7), IP: `192.168.71.50`
* **Lewe ramię:** 7 osi (J1–J7), IP: `192.168.71.51`
* **Komputer pokładowy (Jetson / ROS 2):** IP zewnętrzne Wi-Fi (np. `10.111.169.242`) oraz IP wewnętrzne `192.168.71.1`.

> ⚠️ **Sterowanie wieloma elementami jednocześnie:**  
> Jedna instancja programu RobotAssist może być w danym momencie połączona tylko z jednym kontrolerem (np. samym torsem albo jednym ramieniem).  
> **Aby sterować kilkoma elementami na raz za pomocą RobotAssista, potrzebne jest uruchomienie kilku osobnych instancji programu połączonych z odpowiednimi elementami robota.**  
> Alternatywnie istnieje możliwość jednoczesnego, skoordynowanego sterowania całym robotem poprzez środowisko **ROS (Robot Operating System)**.

---

## 2. Limity Kątowe i Bezpieczeństwo (Soft Limits / Joint Limits)

### Gdzie włączyć Soft Limit?
W RobotAssist limity osi znajdują się w: **`Safety` -> `Joint Limit` -> sekcja `Joint Position(°)`** (w dokumentacji producenta opcja ta nazywa się *软限位 / Soft Limit*).

### Błąd [10051]: Given angles exceed mechanical limits
* **Przyczyna:** Wprowadzono wartości przekraczające fabryczne odbojniki mechaniczne torsu.
* **Wartości graniczne torsu `TaiHu`:**

| Oś | Zakres mechaniczny | Poprawny wpis `Joint Pos` | Poprawny wpis `Reduced Pos` |
| :---: | :---: | :---: | :---: |
| **J1** | `[-68, 3]` | **`-68` do `3`** | **`-67` do `2`** |
| **J2** | `[-175, 49]` | **`-175` do `49`** *(nie wpisywać 141!)* | **`-174` do `48`** |
| **J3** | `[-128, 88]` | **`-128` do `88`** | **`-127` do `87`** |
| **J4** | `[-90, 180]` | **`-90` do `180`** | **`-89` do `179`** |

### Procedura zapisu i zatwierdzenia:
1. W polu **`Safety Login:`** wpisz domyślne hasło: **`safety`** i kliknij **`Unlock`**.
2. Wpisz powyższe wartości w tabeli.
3. Przestaw suwak **`Enable`** w wierszu *Joint Position(°)* na niebieski (włączony).
4. Kliknij **`Confirm`** w prawym dolnym rogu, a w oknie **`Security Check`** potwierdź nową sumę kontrolną (**Safety Checksum**).

---

## 3. Stan Robota i Błędy Napędów (Servo Alarms)

### Błąd [31015]: Axis 1 alarm code (0x3120)
* **Znaczenie kodu `0x3120`:** *Mains / DC-bus Undervoltage* (spadek lub zanik napięcia na szynie zasilania silników serwonapędu).
* **Główna przyczyna:** Przełączenie robota w tryb **Automatic** (sterownik odcina wówczas zasilanie siłowe stycznikiem) lub wciśnięty wyłącznik awaryjny (E-Stop).
* **Dlaczego `Clear Alarm` nie pomaga?** Alarm jest zatrzaśnięty sprzętowo w pamięci napędu osi 1. Czyszczenie bufora w GUI nic nie da, dopóki driver nie dostanie zasilania szyny DC po restarcie.

### Procedura odblokowania robota:
1. Na dolnym pasku upewnij się, że robot jest w trybie **`Manual`** (nie Auto!).
2. Sprawdź, czy żaden fizyczny wyłącznik **E-Stop** nie jest zablokowany.
3. **Zrób restart zasilania (Power Cycle):** Wyłącz zasilanie torsu na **15–20 sekund** (rozładowanie kondensatorów szyny DC) i włącz ponownie.
4. Po podniesieniu systemu wciśnij **3-pozycyjny przycisk zezwalający (Deadman)** na kasecie do pozycji środkowej (lub kliknij ikonę błyskawicy na dolnym pasku), aż załączą się styczniki (*Power On*).

---

## 4. Programowanie i Tworzenie Punktów (RL Editor)

### Prawidłowe tworzenie punktów (Teach / ModPos)
1. Przestaw tors dżojstikiem JOG w żądaną pozycję.
2. Przejdź do: **`Program` -> `Points List` -> `+` (New)**.
3. Wybierz typ danych: **`jointtarget`**.
4. Zaznacz punkt i kliknij **`Teach` / `ModPos`** (przepisze to dokładne, aktualne kąty osi J1–J4 do pamięci punktu).
5. Kliknij **`Save`**.

### Błąd [50512]: Axis quantity mismatch
* **Przyczyna:** Użycie w programie punktów mających strukturę 6 lub 7 osi (np. `MoveAbsJ(j6, ...)`). Tors `TaiHu` ma tylko 4 osie. Używaj wyłącznie punktów zdefiniowanych dla torsu.

### Błąd [61018]: Program not synchronized to the controller
* **Naprawa:** W edytorze kodu kliknij ikonę kompilacji ($\checkmark$), ikonę zapisu/synchronizacji ($\circlearrowright$), a następnie przycisk **`PPtoMain`** (załadowanie wskaźnika do funkcji głównej w sterowniku).

### Błąd [13020]: All RL tasks have been stopped
* **Naprawa:** W trybie Manual ruch trwa tylko wtedy, gdy trzymasz jednocześnie przycisk zezwalający (Deadman) oraz przycisk `Run`. Aby program zapętlał się automatycznie, włącz ikonę pętli $\circlearrowleft$ (*Continuous*) na dolnym pasku.

---

## 5. Diagnostyka Połączenia (Controller Service vs Upgrade Service)

### Stan: Controller Service: Disconnected / Upgrade Service: Connected
* **Znaczenie:** Usługa systemowa Linuxa (`Upgrade Service: 4567`) działa, ale główny proces czasu rzeczywistego (`Controller Service`) uległ awarii lub zawiesił się po błędzie napędu.
* **Rozwiązanie A (Zdalny restart):** W menu przejdź do: **`Option` -> `Software Upgrade` -> `Reboot Robot`**. Odczekaj 45 sekund i kliknij `Connect`.
* **Rozwiązanie B (Fizyczny restart):** Odłącz zasilanie torsu na 20 sekund i włącz ponownie.

---

## 6. Sieć: Połączenie z Robotem i Internetem Jednocześnie

### Problem pętli routingu (`TTL expired in transit`)
Gdy laptop jest w sieci Wi-Fi (`10.111.169.x`) i pinguje ramiona (`192.168.71.x`), pakiety zapętlają się na routerze domowym/biurowym, bo system nie wie, że sieć robota znajduje się za komputerem pokładowym Jetson (`10.111.169.242`).

### Krok 1: Włączenie routingu na Jetsonie (w PuTTY)
Zaloguj się na Jetsona (`std@10.111.169.242`) i wpisz:
```bash
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -t nat -A POSTROUTING -j MASQUERADE
```
*(Aby zapisać to na stałe po restarcie Jetsona: odkomentuj `net.ipv4.ip_forward = 1` w `/etc/sysctl.conf` oraz zainstaluj pakiet `sudo apt install -y iptables-persistent && sudo netfilter-persistent save`).*

### Krok 2: Dodanie trasy w Windowsie (w CMD jako Administrator)
Otwórz Wiersz poleceń jako Administrator i wpisz:
```cmd
route add 192.168.71.0 mask 255.255.255.0 10.111.169.242 -p
```
*(Flaga `-p` zapisuje trasę na stałe, więc nie zniknie po restarcie Windowsa).*

Od tego momentu masz bezpośredni dostęp do ramion, torsu, ROS-a i pełnego internetu w tym samym czasie.

---

## 7. Oficjalna Dokumentacja Rokae

* **Główny portal dokumentacji technicznej:** [https://docs.rokae.com/](https://docs.rokae.com/)
* **Instrukcja systemu xCore:** [https://docs.rokae.com/docs/xCore](https://docs.rokae.com/docs/xCore)
* **Funkcje bezpieczeństwa (Limity, hasło `safety`):** [https://docs.rokae.com/docs/xCore/安全功能](https://docs.rokae.com/docs/xCore/%E5%AE%89%E5%85%A8%E5%8A%9F%E8%83%BD)
* **Obsługa HMI / RobotAssist:** [https://docs.rokae.com/docs/xCore/hmi-简介](https://docs.rokae.com/docs/xCore/hmi-%E7%AE%80%E4%BB%8B)
* **Kody błędów i diagnostyka:** [https://docs.rokae.com/docs/xCore/故障排查](https://docs.rokae.com/docs/xCore/%E6%95%85%E9%9A%9C%E6%8E%92%E6%9F%A5)
* **Centrum pobierania oprogramowania:** [https://docs.rokae.com/docs/DownLoad](https://docs.rokae.com/docs/DownLoad)
* **Karta techniczna robota Helios:** [https://www.rokae.com/en/product/show/596/Wheeled-Dual-Arm-Robot-Helios.html](https://www.rokae.com/en/product/show/596/Wheeled-Dual-Arm-Robot-Helios.html)


8. Jak wyskakuje błąd RCI czyli przez program np C++ limit jakiś osiągniemy to w Robot Assist > Communication > RCI Settings i wyłączasz to
