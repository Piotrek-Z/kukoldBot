# DOKUMENTACJA — robot Rokae Helios

Dokumentacja projektu w jednym miejscu: 16 dokumentów tematycznych + 5 plików pomocniczych.
To **wersja z poprawkami** — oryginalne pliki w katalogu głównym repozytorium i w
`sterowaniePodstawa/` zostały nietknięte (są w historii gita); tutaj pracujemy na kopiach.

*Stan: 10 października 2026 · dokumentacja w fazie porządkowania.*

---

## Od czego zacząć

| Potrzebuję… | Idź do |
|---|---|
| uruchomić robota / cały system | [`URUCHAMIANIE.md`](URUCHAMIANIE.md) — ⏳ szkielet, do uzupełnienia |
| uruchomić konkretny program | [`TUTORIAL-PROGRAMY.md`](TUTORIAL-PROGRAMY.md) |
| pracować w innej sieci / zmienić IP | [`TUTORIAL-ZMIANA-SIECI.md`](TUTORIAL-ZMIANA-SIECI.md) |
| zrozumieć skrót albo termin | [`SLOWNIK.md`](SLOWNIK.md) |
| wiedzieć, co jest do zrobienia | [`ANALIZA-I-LUKI.md`](ANALIZA-I-LUKI.md) |
| włączyć robota (zasilanie) | [`03-robot-zasilanie.md`](03-robot-zasilanie.md) |
| rozwiązać błąd kontrolera / napędu / sieci | [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) |

---

## Spis treści

Pliki ponumerowane w kolejności czytania. Kolumna „Na podstawie" wskazuje oryginał;
„✏️" oznacza plik, w którym naniesiono poprawki (są wymienione w bloku „✏️ Poprawki"
na górze dokumentu).

| # | Plik | Co jest w środku | Na podstawie |
|---:|---|---|---|
| 01 | [`przeglad-projektu.md`](01-przeglad-projektu.md) | Czym jest Helios, z czego się składa, jaki stos technologiczny | `MasterPrompt.md` ✏️ |
| 02 | [`linki-zewnetrzne.md`](02-linki-zewnetrzne.md) | Linki do dokumentacji producenta + linki zebrane z innych dokumentów | `Linki_Dokumentacja` |
| 03 | [`robot-zasilanie.md`](03-robot-zasilanie.md) | Włączanie zasilania robota (oryginalny zapis + lista kontrolna + pytania do uzupełnienia) | `Uruchamianie robota.md` ✏️ |
| 04 | [`robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) | Kompendium konfiguracji i błędów xCore: limity kątowe, alarmy napędów, programowanie RL, diagnostyka usług, routing sieciowy | `DOKUMENTACJA_ROKAE_HELIOS_ROZWIAZYWANIE_PROBLEMOW.md` ✏️ |
| 05 | [`robot-zlacze-narzedziowe.md`](05-robot-zlacze-narzedziowe.md) | Złącze narzędziowe M8: rozpiska pinów, parametry elektryczne, metody C++ SDK i serwisy ROS 2 | `Helios_zlacza.md` |
| 06 | [`siec-wifi.md`](06-siec-wifi.md) | Notatki: Wi-Fi na komputerze pokładowym, SSH, komendy sieciowe | `helios-wifi.txt` |
| 07 | [`sensory-kamery.md`](07-sensory-kamery.md) | Kamera 3D Orbbec Gemini 335L + kamery USB, `web_video_server`, mapowanie `/dev/video*` | `Helios_kamery.md` |
| 08 | [`sensory-mikrofon.md`](08-sensory-mikrofon.md) | Macierz mikrofonów (ALSA), ścieżki integracji z ROS 2, potok STT → LLM | `Helios_mikrofon.md` ✏️ |
| 09 | [`sensory-glosniki.md`](09-sensory-glosniki.md) | Karta dźwiękowa USB: odtwarzanie, głośność, pułapki z domyślnym urządzeniem | `helios-glosniki.md` ✏️ |
| 10 | [`teleoperacja-tracker.md`](10-teleoperacja-tracker.md) | System teleoperacji: aplikacja Windows (Qt/QML/OpenCV), YOLOv8-Pose, warstwa bezpieczeństwa, `rokae_driver7` | `Helios_tracker.md` |
| 11 | [`teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md) | Węzeł ROS 2 do odczytu i sterowania ramionami i torsem przez xCore SDK 0.7.1; katalog 15 problemów | `helios_probe (2).md` |
| 12 | [`teleoperacja-frontend-hud.md`](12-teleoperacja-frontend-hud.md) | Interfejs operatora (HUD) w Qt/QML: układ ekranu, telemetria, przyciski, powiązanie z C++ | **nowy** (z kodu `Main.qml`) |
| 13 | [`programy-cpp.md`](13-programy-cpp.md) | Programy konsolowe C++: panel sterowania ręką, program prezentacyjny, węzeł `helios_probe` | **nowy** (z kodu `.cpp`) |
| 14 | [`baza-jezdna-dokumentacja.md`](14-baza-jezdna-dokumentacja.md) | Referencja: protokół bazy (WebSocket + protobuf, port 5002), API biblioteki, narzędzia CLI | `sterowaniePodstawa/DOKUMENTACJA.md` |
| 15 | [`baza-jezdna-przewodnik.md`](15-baza-jezdna-przewodnik.md) | Metodyka reverse-engineeringu protokołu + kalendarium + katalog problemów | `sterowaniePodstawa/PRZEWODNIK.md` |
| 16 | [`baza-jezdna-protokol.md`](16-baza-jezdna-protokol.md) | Krótszy opis protokołu bazy — **plik 14 jest nadrzędny** | `sterowaniePodstawa/PROTOKOL.md` ✏️ |

### Pliki pomocnicze

| Plik | Co jest w środku |
|---|---|
| [`URUCHAMIANIE.md`](URUCHAMIANIE.md) | Szkielet procedury uruchomienia całego robota — ⏳ do uzupełnienia po testach |
| [`TUTORIAL-PROGRAMY.md`](TUTORIAL-PROGRAMY.md) | Jak budować i uruchamiać każdy program w repo (+ test bez robota) |
| [`TUTORIAL-ZMIANA-SIECI.md`](TUTORIAL-ZMIANA-SIECI.md) | Jak skonfigurować środowisko po zmianie sieci / IP — lista wszystkich miejsc z adresami |
| [`SLOWNIK.md`](SLOWNIK.md) | Wyjaśnienie terminów i skrótów |
| [`ANALIZA-I-LUKI.md`](ANALIZA-I-LUKI.md) | Analiza dokumentacji: co poprawiono, co jest niepełne, co do weryfikacji |

---

## Co zmieniono względem oryginałów

Poprawki są **opisane w każdym pliku z osobna** (blok „✏️ Poprawki" na górze), żeby dało się
je zweryfikować. W skrócie:

| Plik | Co poprawiono |
|---|---|
| `04` | adresy ramion `.50/.51` → `.160/.161`; adres wewnętrzny Jetsona `.1` → `.51`; dopisano adres bazy jezdnej; poprawiono diagram sieci i przykład z pingiem |
| `03` | dodano strukturę, listy kontrolne i wskazówki — **oryginalny zapis zachowany dosłownie** |
| `01` | dopisano strukturę; lista linków przeniesiona do pliku `02` (by nie była zdublowana) |
| `08`, `09` | dopisano uwagę, że `card 3` (`HK USB REF`) jest opisywana w obu dokumentach (mikrofon i głośniki) |
| `16` | dopisano informację, że plik `14` jest nadrzędny (ten plik go częściowo powtarza) |
| `02`, `06` | konwersja pliku bez rozszerzenia / `.txt` na Markdown — treść 1:1 |

Reszta plików (05, 07, 10, 11, 14, 15) to kopie bez zmiany treści.

**Czego celowo nie ruszałem:** wartości, których nie da się potwierdzić (np. `192.168.49.97`
z notatek Wi-Fi, „deafultowy" w pliku 09, nierozstrzygnięte problemy z pliku 11) —
są wypisane w [`ANALIZA-I-LUKI.md`](ANALIZA-I-LUKI.md) jako rzeczy do decyzji.

---

## Konwencja

- nazwy plików: numer + małe litery, bez polskich znaków, wyrazy po myślniku
- każdy plik zaczyna się nagłówkiem `#` i linią informującą, na czym jest oparty
- linki między dokumentami są względne — cały katalog można przenieść bez psucia linków
- datę i stan wiedzy warto wpisywać w nagłówku (jak w plikach 14–16)
- nowy dokument = wpis w tym spisie + (jeśli zmienia sposób uruchomienia) wskazówka w `URUCHAMIANIE.md`

## Czego tu nie ma

- `_DOKUMENTACJA_HELIOS.md` — oznaczona w samej treści jako niepoprawna; zastępuje ją plik `04`
- `helios_probe.md` (bez „(2)") — starsza wersja pliku `11`
- kluczy API (`nemotron_api`, `youtube_api`) — **nie powinny trafić do dokumentacji**
- kodu źródłowego aplikacji do trackingu — jest tylko w archiwach `TrackingRobot*`
