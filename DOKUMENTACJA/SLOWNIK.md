# Słowniczek pojęć

Jedno miejsce z wyjaśnieniem skrótów i terminów, które pojawiają się w dokumentacji.
Przydaje się przy wdrożeniu nowej osoby w zespół.

| Termin | Co to jest | Gdzie opisane |
|---|---|---|
| **xCore** | system sterowania / oprogramowanie kontrolera Rokae | [`04`](04-robot-rozwiazywanie-problemow.md) |
| **RobotAssist** | aplikacja na laptopie do obsługi kontrolera xCore (HMI, programowanie, safety) | [`04`](04-robot-rozwiazywanie-problemow.md) |
| **TaiHu** | model torsu robota — 4 osie (J1–J4) | [`04`](04-robot-rozwiazywanie-problemow.md) §1 |
| **AR5** | model ramienia (np. `AR5-5_0.7R-W4C4A2` — prawe, `…L…` — lewe); 7 osi | [`05`](05-robot-zlacze-narzedziowe.md) |
| **xMateErProRobot** | klasa SDK dla 7-osiowych rąk Heliosa | [`11`](11-teleoperacja-helios-probe.md) |
| **PCB4Robot** | klasa SDK dla 4-osiowego torsu (jedyna 4-osiowa w SDK 0.7.1) | [`11`](11-teleoperacja-helios-probe.md) |
| **Soft Limit / Joint Limit** | programowy zakres ruchu osi; hasło do edycji: `safety` | [`04`](04-robot-rozwiazywanie-problemow.md) §2 |
| **Safety Checksum** | 4-znakowa suma kontrolna bezpieczeństwa, generowana po zapisie limitów | [`04`](04-robot-rozwiazywanie-problemow.md) §2.4 |
| **Deadman** | 3-pozycyjny przycisk zezwalający na kasecie; ruch w trybie Manual wymaga trzymania | [`04`](04-robot-rozwiazywanie-problemow.md) §3 |
| **E-Stop** | wyłącznik awaryjny; w kodzie SDK `recoverState(1)` | [`04`](04-robot-rozwiazywanie-problemow.md), [`13`](13-programy-cpp.md) |
| **Safeguard / Safety Gate** | pętla bezpieczeństwa drzwi; w kodzie SDK `recoverState(2)` | [`13`](13-programy-cpp.md) |
| **0x3120** | kod alarmu serwonapędu: zanik napięcia na szynie DC | [`04`](04-robot-rozwiazywanie-problemow.md) §3.1 |
| **Controller / Upgrade Service** | dwa procesy w kontrolerze: sterowanie czasem rzeczywistym vs usługa systemowa (port 4567) | [`04`](04-robot-rozwiazywanie-problemow.md) §5 |
| **RL Editor / jointtarget / MoveAbsJ** | język programowania kontrolera, punkt w przestrzeni osiowej, ruch do pozycji bezwzględnej | [`04`](04-robot-rozwiazywanie-problemow.md) §4 |
| **PPtoMain** | przycisk ustawiający wskaźnik wykonania na początek programu | [`04`](04-robot-rozwiazywanie-problemow.md) §4.3 |
| **RCI** | zewnętrzny interfejs komunikacyjny; wyłącza się w RobotAssist → Communication → RCI Settings | [`04`](04-robot-rozwiazywanie-problemow.md) §8 |
| **ROS 2 (Humble)** | stos komunikacyjny na komputerze pokładowym (Jetson) | [`07`](07-sensory-kamery.md)–[`13`](13-programy-cpp.md) |
| **rosbridge** | most WebSocket do ROS 2 (port 9090), z niego korzysta aplikacja Windows | [`10`](10-teleoperacja-tracker.md) §2.4 |
| **rokae_driver7** | węzeł ROS 2 sterujący fizycznym ramieniem (7 osi) | [`10`](10-teleoperacja-tracker.md) §4 |
| **teleop_safety_bridge** | węzeł bezpieczeństwa teleoperacji (25 Hz, watchdog 400 ms) | [`10`](10-teleoperacja-tracker.md) §3 |
| **RtMotionController / MoveJ** | kontroler ruchu w trybie czasu rzeczywistego; metoda ruchu z wielkiej litery `M` | [`11`](11-teleoperacja-helios-probe.md), [`13`](13-programy-cpp.md) |
| **local_ip** | adres komputera w podsieci robota — wymagany dla ruchu (UDP, port 1338) | [`11`](11-teleoperacja-helios-probe.md) |
| **Watchdog** | strażnik: brak ramki przez 400 ms → ramię zamarza w miejscu | [`10`](10-teleoperacja-tracker.md) §3.4 |
| **Orbbec Gemini 335L** | kamera 3D w głowie robota (RGB + głębia + IR + IMU) | [`07`](07-sensory-kamery.md) |
| **web_video_server** | serwer strumieniowania obrazu do przeglądarki (port 8080) | [`07`](07-sensory-kamery.md) §3 |
| **`/dev/video*`** | urządzenia kamer w Linuksie; numer parzysty = strumień wideo, nieparzysty = metadane | [`07`](07-sensory-kamery.md) §5.2 |
| **ALSA / `plughw` / `card 3`** | warstwa dźwięku w Linuksie; karta `REF` (`HK USB REF`) to mikrofon i/lub głośniki | [`08`](08-sensory-mikrofon.md), [`09`](09-sensory-glosniki.md) |
| **VAD** | detekcja aktywności głosowej (np. Silero VAD) — „kto kiedy mówi" | [`08`](08-sensory-mikrofon.md) §4.1 |
| **STT / Whisper** | zamiana mowy na tekst; model działa lokalnie na Jetsonie albo w chmurze | [`08`](08-sensory-mikrofon.md) §4.2 |
| **HUD** | interfejs operatora aplikacji do trackingu (Qt/QML) | [`12`](12-teleoperacja-frontend-hud.md) |
| **YOLOv8-Pose / ONNX** | model wykrywający punkty ciała operatora; plik `yolov8n-pose.onnx` | [`10`](10-teleoperacja-tracker.md) §2.2 |
| **EMA + Deadband** | filtrowanie drgań: wykładnicza średnia + martwa strefa 0,8° | [`10`](10-teleoperacja-tracker.md) §2.3 |
| **Matrix OS** | system bazy jezdnej; ma własny panel web i własny protokół | [`14`](14-baza-jezdna-dokumentacja.md) |
| **AMR** | Autonomous Mobile Robot — baza jezdna (podwozie kołowe) | [`14`](14-baza-jezdna-dokumentacja.md) |
| **koperta (envelope)** | wspólna struktura każdej ramki protokołu bazy: rodzaj, `seq`, sesja, treść | [`14`](14-baza-jezdna-dokumentacja.md) §4 |
| **opkod** | kod operacji w kanale komend bazy: 16 jog, 6/111 przejmij, 7 oddaj, 32 zadanie | [`14`](14-baza-jezdna-dokumentacja.md) §6.2 |
| **jog** | strumień komend prędkości wysyłany cyklicznie (`op=16`) | [`14`](14-baza-jezdna-dokumentacja.md) §6.3 |
| **re-arm** | ponowne wysłanie `op=6` + `op=111` przed nowym ruchem | [`14`](14-baza-jezdna-dokumentacja.md) §1.3 |
| **atrapa (`bench_server.py`)** | fałszywa baza do testów bez robota (port 5003) | [`14`](14-baza-jezdna-dokumentacja.md) §10.8 |
| **colcon / workspace** | narzędzie budowania pakietów ROS 2; workspace robota: `~/rokae_ws` | [`11`](11-teleoperacja-helios-probe.md) |
| **MASQUERADE** | reguła iptables (NAT) pozwalająca laptopowi wyjść przez Jetson do internetu | [`04`](04-robot-rozwiazywanie-problemow.md) §6.3 |
