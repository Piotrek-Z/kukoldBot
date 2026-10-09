# Helios: szybki start

Helios to asystent głosowy robota Rokae. Słucha słowa-klucza **„Helios”**, nagrywa Twoją wypowiedź, rozpoznaje mowę lokalnie (whisper.cpp, model large-v3), wysyła tekst do modelu językowego (OpenRouter, zapas: Groq), wykonuje komendy z odpowiedzi i odpowiada głosem (Piper).

```
mikrofon → „Helios” → nagranie → rozpoznawanie (Whisper) → LLM → komendy #…# → ruch robota
                                                        ↘ odpowiedź głosem (Piper)
```

## 1. Co musi być na miejscu

Ścieżki są domyślne. Każdą zmienisz zmienną środowiskową (sekcja 7).

| Element | Domyślna ścieżka |
|---|---|
| Workspace ROS 2 | `/home/std/rokae_ws` |
| Pakiet | `/home/std/rokae_ws/src/rokae_ros2-main/helios_ai` |
| whisper-cli | `/home/std/whisper.cpp/build/bin/whisper-cli` |
| Model STT (large-v3) | `/home/std/models/ggml-large-v3.bin` |
| VAD silero (opcjonalny) | `/home/std/models/ggml-silero-v6.2.0.bin` |
| Modele słowa-klucza | `/home/std/helios/models/` (`helios.onnx`, `melspectrogram.onnx`, `embedding_model.onnx`) |
| Piper i głos | `/home/std/piper/piper`, `/home/std/piper_models/pl_PL-darkman-medium.onnx` |
| ONNX Runtime | `/home/std/onnxruntime` |
| Klucze API | `OPENROUTER_API_KEY` (główny), `GROQ_API_KEY` (zapas) |

Do budowania potrzebne są m.in. ROS 2, Eigen3, orocos_kdl, Rokae SDK (`rokae_hardware/sdk`), `libcurl` i `nlohmann-json`. Kontroler robota musi być osiągalny w sieci.

## 2. Budowanie

```bash
cd /home/std/rokae_ws
colcon build --packages-select helios_ai
source install/setup.bash
```

## 3. Uruchamianie

```bash
env OPENROUTER_API_KEY=TWOJ_KLUCZ ros2 run helios_ai main
```

Ta forma działa w bash, zsh i tcsh. Klucz możesz też ustawić w tym samym terminalu (`export` w bash/zsh, `setenv` w tcsh).

Po starcie baner pokaże model LLM, mikrofon, model słowa-klucza i silnik STT. Gdy pojawi się `Nasłuchuję słowa-klucza "Helios"`, możesz mówić. Program tworzy folder `historia/` w katalogu, z którego go uruchomiłeś.

**Szybki test:** poczekaj na linie `[Audio]` (co 5 s), powiedz „Helios”, usłysz dzwonek, a potem powiedz „pomachaj”. Uważaj na ruch robota.

## 4. Jak rozmawiać

1. Powiedz **„Helios”** i poczekaj na dzwonek.
2. W ciągu **4 s** powiedz polecenie, np. „pomachaj”.
3. Przez **6 s** po odpowiedzi możesz mówić bez powtarzania „Helios”.
4. Zakończ: wpisz `exit` i Enter albo naciśnij Ctrl+C.

## 5. Komendy

### Głosowe (po „Helios”)

| Powiedz | Tag | Działanie |
|---|---|---|
| „pomachaj” | `#wave#` | macha ręką |
| „włącz drag lewej ręki” | `#dragOn:left#` | drag lewej ręki (prowadzenie ręką) |
| „włącz drag prawej ręki” | `#dragOn:right#` | drag prawej ręki |
| „włącz drag obu rąk” | `#dragOn:both#` | drag obu rąk |
| „wyłącz drag” | `#dragOff#` | wyłącza drag |
| „wróć do pozycji zerowej” | `#posZero:all#` | pozycja zerowa (dostępne też `left`, `right`, `both`, `torso`) |
| „pokaż statystyki” | `#stats#` | statystyki |
| „odtwórz 18 lat Axotox” | `#youtube:TYTUŁ AUTOR#` | wyszukuje i odtwarza z YouTube (potrzebny internet) |
| „ustaw głośność na 70 procent” | `#volume:70#` | głośność głośników USB, zakres 0–80 |

Model wybiera tag z tej listy i nie powinien wymyślać nowych komend.

### Klawiatura (bez internetu i bez modelu)

| Wpisz | Działanie |
|---|---|
| `/drag left` (lub `right`, `both`) | drag |
| `/dragoff` | wyłącza drag |
| `/wave` | pomachaj |
| `/stats` | statystyki |
| `/zero` | pozycja zerowa (`#posZero:all#`) |
| `/listen` | nagraj polecenie bez słowa „Helios” |
| `#dragOn:left#` (dowolny tag) | wykonuje tag wprost |
| `exit` | zakończ |

> **Bezpieczeństwo:** `wave`, `dragOn` i `posZero` poruszają ramionami robota. Przed komendą sprawdź, czy wokół robota jest wolna przestrzeń, i miej przycisk zatrzymania awaryjnego (E-STOP) w zasięgu ręki.

## 6. Testy i diagnostyka

```bash
ros2 run helios_ai main --mic-test              # poziom mikrofonu, 6 s mowy
ros2 run helios_ai main --stt plik.wav          # rozpoznawanie z pliku (16 kHz, mono)
ros2 run helios_ai main --wake-test plik.wav    # detektor słowa-klucza na pliku
ros2 run helios_ai main --llm-test "tekst"      # sam model językowy
ros2 run helios_ai main --kalibracja            # kalibracja słownika poprawek na Twoim głosie
ros2 run helios_ai main --help                  # pomoc
```

`--mic-test` ocenia poziom po szczycie, więc werdykt bywa zbyt optymistyczny. Oceniaj też odsłuchem pliku `historia/ostatnia_wypowiedz.wav`.

## 7. Zmienne środowiskowe

Ustawiasz je w tej samej linii co uruchomienie: `env ZMIENNA=wartość ros2 run helios_ai main`.

| Zmienna | Domyślnie | Do czego |
|---|---|---|
| `HELIOS_AUDIO_DEVICE` | `plughw:CARD=REF,DEV=0` | mikrofon (nazwę pokaże `arecord -l`) |
| `HELIOS_AGC` | włączone | `0` wyłącza automatyczne wzmocnienie |
| `HELIOS_AGC_MAX` | `10` | maksymalne wzmocnienie AGC |
| `HELIOS_WAKE_THRESHOLD` | `0.40` | czułość słowa-klucza (niżej = łatwiej, ale więcej fałszywych pobudek) |
| `HELIOS_WAKE_VERIFY` | włączone | `off` wyłącza sprawdzanie słowa przez Whispera, `strict` odrzuca przy błędzie STT |
| `HELIOS_CAPTURE_ARM` | `4.0` | sekundy na polecenie po „Helios” |
| `HELIOS_FOLLOW_UP` | `6.0` | sekundy na kolejne zdanie bez słowa-klucza |
| `HELIOS_STT_SILENCE` | `1.2` | cisza (s), która kończy wypowiedź |
| `HELIOS_PREROLL` / `HELIOS_STT_PREROLL` | `0.6` | audio sprzed startu mowy (pomaga przy ucinaniu początku) |
| `HELIOS_STT_BACKEND` | `auto` | `auto`, `local` albo `server` |
| `HELIOS_STT_SERVER_URL` | brak | adres whisper-server (model w pamięci, szybciej) |
| `HELIOS_STT_PROMPT` | wbudowany | podpowiedź dla Whispera; `" "` wyłącza ją |
| `HELIOS_STT_VAD_MODEL` | `ggml-silero-v6.2.0.bin` | VAD; nieistniejący plik wyłącza VAD |
| `HELIOS_LLM_MODEL` | `nvidia/nemotron-3-ultra-550b-a55b:free` | model główny |
| `HELIOS_CHIME` | włączone | `0` wyłącza dzwonek |
| `HELIOS_NO_WAKEWORD` | brak | dowolna niepusta wartość (np. `1`) włącza tryb ręczny, tylko `/listen` |
| `HELIOS_HISTORY_DIR` | `historia` | katalog zapisu rozmów |

## 8. Pliki i logi

- `historia/rozmowa_<data>.txt`: zapis rozmowy. Powstaje po każdej odpowiedzi i przy wyjściu.
- `historia/ostatnia_wypowiedz.wav`: ostatnia wypowiedź po normalizacji. Jest nadpisywana przy każdej wypowiedzi, także przy sprawdzaniu słowa-klucza. Zapisuje się nawet wtedy, gdy rozpoznawanie się nie uda.
- `/tmp/helios_utterance.wav`: ta sama wypowiedź po AGC, przed normalizacją.
- Konsola: `[Audio]` co 5 s (szczyt, `max score` detektora, wzmocnienie AGC), `[Słowo-klucz]` (potwierdzony albo odrzucony), `[Błąd komendy …]`.

## 9. Gdy coś nie działa

| Objaw | Co zrobić |
|---|---|
| `Mikrofon zajęty` | `pgrep -af arecord`, potem `pkill -f arecord`, i uruchom ponownie |
| Stara sesja nadal działa | `pgrep -af helios_ai`, potem `pkill -f helios_ai` |
| `[Błąd komendy dragOn: network: …]` | robot nieosiągalny: sprawdź zasilanie kontrolera, kabel, `ping` na jego adres i `ip -br addr` |
| `[Błąd LLM: brak klucza API…]` | ustaw `OPENROUTER_API_KEY` (albo `GROQ_API_KEY`) w tym samym terminalu |
| `brak pliku modelu` | w `/home/std/whisper.cpp`: `sh ./models/download-ggml-model.sh large-v3`, potem skopiuj plik do `/home/std/models/` |
| Robot nie reaguje na „Helios” | w logu `[Słowo-klucz] odrzucony` → test z `HELIOS_WAKE_VERIFY=off`; bez takiej linii → sprawdź `max score` i mów bliżej mikrofonu; można dodać `HELIOS_AGC_MAX=20` |
| Ucięty początek zdania | `HELIOS_PREROLL=1.0 HELIOS_STT_PREROLL=1.0` |
| Dziwne rozpoznawanie | test z `HELIOS_STT_PROMPT=" "` i z `HELIOS_STT_VAD_MODEL=/nieistniejacy/plik` |
| Program się wyłącza (`AWARIA`) | skopiuj zrzut stosu z konsoli i wyślij |

## 10. Znane ograniczenia

- Whisper ładuje model przy każdym rozpoznaniu, więc odpowiedź trwa dłużej. Lepszy wybór to whisper-server (`HELIOS_STT_SERVER_URL`).
- Głos Heliosa zawsze idzie na kartę REF (`helios_tts.cpp`).
- Dzwonek używa `HELIOS_AUDIO_DEVICE` do odtwarzania, więc przy innym mikrofonie może nie zagrać.
- Początek pierwszego słowa bywa ucinany, zwłaszcza przy cichym głosie.
- Komendy ruchu wymagają połączenia z kontrolerem robota. Rozmowa z LLM wymaga internetu. Rozpoznawanie mowy działa lokalnie.
