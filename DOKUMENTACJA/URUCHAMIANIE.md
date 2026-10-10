# Uruchamianie robota Rokae Helios — procedura

> **STATUS: ⏳ SZKIELET — do uzupełnienia po testach end-to-end.**
> Ten plik jest zamieszczony wcześniej niż reszta dokumentacji i celowo zawiera same punkty
> do wypełnienia (oznaczone `> TODO:`), żeby mieć jedno miejsce, w którym cała procedura
> startowa zostanie zebrana w całość. **Nie traktuj go jako sprawdzonej procedury** —
> każdy krok prowadzi do dokumentu, w którym szczegóły są już opisane.
>
> Ostatnia aktualizacja: 10 października 2026 · osoba odpowiedzialna: _do uzupełnienia_

---

## Legenda stanu

| Oznaczenie | Znaczenie |
|---|---|
| ✅ | krok sprawdzony i opisany w dokumentacji |
| 🟡 | krok znany, ale opis wymaga dopracowania |
| ⏳ | krok do ustalenia / do napisania po testach |

---

## 0. Wymagania wstępne

> TODO: uzupełnić po pierwszym pełnym przebiegu.

Co musi być pod ręką i sprawdzone **przed** startem:

- [ ] fizyczny dostęp do robota: kaset z przyciskiem zezwalającym (deadman) i wyłącznik awaryjny w zasięgu ręki
- [ ] wolna przestrzeń wokół robota (baza jezdna + ramiona)
- [ ] laptop/PC w sieci robota, zestawione trasy (`route add`) — patrz [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §6
- [ ] hasła: `safety` (moduł bezpieczeństwa xCore), konto `admin`/`admin` (panel bazy)
- [ ] ⏳ TODO: dostęp SSH do Jetsona (`std@10.111.169.242`), klucze/hasła
- [ ] ⏳ TODO: lista rzeczy, które muszą być już uruchomione (RobotAssist? tryb Manual/Auto?)

## 1. Zasilanie robota

1. Podłącz robota do zasilania.
2. Poczekaj na **dwa piknięcia**.
3. Wciśnij **raz** przycisk na plecach robota i poczekaj, aż system się sprawuje.

Szczegóły: [`03-robot-zasilanie.md`](03-robot-zasilanie.md).
Stan napędów i alarmy (np. `0x3120`): [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §3.

- [ ] ✅ zasilanie włączone, robot w trybie **Manual**
- [ ] ✅ żaden wyłącznik awaryjny nie jest zablokowany
- [ ] ✅ silniki mają zasilanie (styczniki załączone — deadman / ikona błyskawicy)

## 2. Sieć

- [ ] ✅ Jetson ma adres w sieci zewnętrznej (`10.111.169.242`) i wewnętrznej (`192.168.71.51`)
- [ ] ✅ ping do elementów robota przechodzi (ramiona `192.168.71.160/161`, tors `192.168.71.254`, baza `192.168.71.50`)
- [ ] 🟡 Wi-Fi na Jetsonie i dostęp SSH — [`06-siec-wifi.md`](06-siec-wifi.md)
- [ ] 🟡 routing laptop ↔ Jetson ↔ kontrolery — [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §6

## 3. Środowisko ROS 2 na Jetsonie

> TODO: uzupełnić — w dokumentacji brak jednego miejsca opisującego przygotowanie
> workspace (`~/rokae_ws`), kolejność `colcon build` i ładowanie środowiska.

- [ ] ⏳ `source /opt/ros/humble/setup.zsh`
- [ ] ⏳ `source ~/rokae_ws/install/setup.zsh` (build: `colcon build --packages-select rokae_msgs rokae_hardware`)
      — patrz [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §5 i [`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md)
- [ ] ⏳ TODO: czy trzeba coś budować przed każdym startem, czy workspace jest zbudowany na stałe

> 🔧 Komendy budowania i uruchamiania poszczególnych programów (węzły, programy konsolowe,
> aplikacja Windows, narzędzia bazy) są zebrane w
> [`TUTORIAL-PROGRAMY.md`](TUTORIAL-PROGRAMY.md).

## 4. Kamery

- [ ] ✅ Terminal 1: sterownik kamery 3D — `ros2 launch orbbec_camera gemini_330_series.launch.py`
- [ ] ✅ Terminal 2: serwer wideo — `ros2 run web_video_server web_video_server` (port 8080)
- [ ] ✅ (opcjonalnie) kamery USB podwozia/korpusu
- [ ] ⚠️ nie zamykać terminalu 1 (`Ctrl+C` wyłącza zasilanie matrycy kamery)

Szczegóły: [`07-sensory-kamery.md`](07-sensory-kamery.md).

## 5. Most WebSocket (rosbridge)

- [ ] ⏳ TODO: potwierdzić komendę i port (`ros2 launch rosbridge_server rosbridge_websocket_launch.xml`, port 9090)
      — patrz [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §5.2

## 6. Sterownik ramion (`rokae_driver7`)

- [ ] ✅ prawe ramię: `ros2 run rokae_hardware rokae_driver7 --ros-args -p robot_ip:=192.168.71.160 -p local_ip:=192.168.71.51`
- [ ] 🟡 lewe ramię: ten sam węzeł z `robot_ip:=192.168.71.161`
- [ ] 🟡 tors: klasy w SDK i odczyt — [`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md)

Szczegóły i pułapki (np. `Robot instance type does not match`):
[`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §4, [`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md).

## 7. Warstwa bezpieczeństwa teleoperacji

- [ ] ✅ `ros2 run rokae_hardware teleop_safety_bridge` (25 Hz, watchdog 400 ms)
- [ ] ⏳ TODO: czy węzeł startuje zawsze, czy tylko przy teleoperacji

Szczegóły: [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §3.

## 8. Szybki start jednym skryptem

- [ ] 🟡 `~/start_teleop.sh` na Jetsonie podnosi kamerę, serwer wideo, rosbridge i węzeł bezpieczeństwa
      — patrz [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §5.1
- [ ] ⏳ TODO: skrypt leży na robocie, nie w repozytorium — warto go tu przekleić lub zrepozytoryzować

## 9. Aplikacja na Windowsie (teleoperacja)

1. Otwórz projekt `TrackingRobot` w Qt Creatorze.
2. `Ctrl + R` (Uruchom).
3. **POŁĄCZ Z ROBOTEM** (status ma zmienić się na zielony `POŁĄCZONY`).
4. Stań przed kamerą w odległości 1,5–2 m i trzymaj grzybek bezpieczeństwa w dłoni.

Interfejs operatora (co widać na ekranie): [`12-teleoperacja-frontend-hud.md`](12-teleoperacja-frontend-hud.md).
Szczegóły: [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §5.3.

## 10. (Opcjonalnie) Baza jezdna

- [ ] ✅ panel Matrix: `http://192.168.71.50/#/homePage` (login `admin` / hasło `admin`)
- [ ] ✅ nasze narzędzia: `./baza.sh wasd`, `./baza.sh przod 150 2`, `./baza.sh stop`
- [ ] ⚠️ panel zamknięty podczas sterowania naszym klientem; `op=7` tylko na końcu sesji

Szczegóły: [`14-baza-jezdna-dokumentacja.md`](14-baza-jezdna-dokumentacja.md) (referencja),
[`15-baza-jezdna-przewodnik.md`](15-baza-jezdna-przewodnik.md) (metodyka i pułapki).

---

## Weryfikacja — „czy na pewno wszystko chodzi"

> TODO: zamienić tę listę na faktyczny przebieg sprawdzony na robocie.

- [ ] obraz z kamery w przeglądarce: `http://10.111.169.242:8080/stream?topic=/camera/color/image_raw`
- [ ] `ros2 topic echo /teleop/joint_commands` — przychodzą komendy z Windowsa
- [ ] `ros2 topic echo /rokae_driver7/joint_states --once` — enkodery ramienia odpowiadają
- [ ] `ros2 service call /rokae_driver7/get_robot_info rokae_msgs/srv/GetRobotInfo`
- [ ] (baza) `./baza.sh stan` — pozycja, faza, mapa

## Zatrzymywanie

> TODO: ustalić kolejność wyłączania (czy najpierw aplikacja Windows, potem węzły na Jetsonie).

- [ ] ⏳ zamknąć aplikację Windows („Rozłącz")
- [ ] ⏳ `Ctrl + C` w terminalu ze skryptem startowym
- [ ] ⏳ wyłączenie zasilania robota — patrz [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §3
- [ ] (baza) zawsze zakończyć jogiem z zerami + `op=7` — [`14-baza-jezdna-dokumentacja.md`](14-baza-jezdna-dokumentacja.md) §12

## Gdy coś nie działa — skrót

| Objaw | Gdzie szukać |
|---|---|
| `Robot instance type does not match` | [`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md) (problem 7) |
| brak połączenia z portem 8080 | [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) (problem 7) |
| alarm napędu `0x3120` / brak zasilania | [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §3 |
| `Controller Service: Disconnected` | [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §5 |
| baza nie reaguje na komendy | [`15-baza-jezdna-przewodnik.md`](15-baza-jezdna-przewodnik.md) §8.4 |

## Co zostało do zrobienia w tym pliku

1. ⏳ Przebieść całą procedurę na robocie i uzupełnić sekcje 0, 3, 5, 7.
2. ⏳ Ustalić kolejność wyłączania.
3. ⏳ Zamienić listy kontrolne na opisany krok po kroku przebieg z oczekiwanym wynikiem.
4. ⏳ Rozważyć wersję skryptową startu (`start_teleop.sh`) w repozytorium.
