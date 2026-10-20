# Helios – dokumentacja projektu

**Asystent głosowy humanoidalnego robota Rokae** · stan na 20 października 2026

Dokument opisuje całość: co robi system, jak jest zbudowany, jak go uruchomić, skonfigurować, wdrożyć i naprawić, oraz czego jeszcze nie potrafi. Powstał na podstawie faktycznego kodu w workspace (nie z pamięci). Tam, gdzie czegoś nie dało się zweryfikować w plikach, jest to wprost zaznaczone jako **[założenie]**.

## Spis treści

1. [Czym jest Helios](#1-czym-jest-helios)
2. [Stan projektu i co jest do zrobienia](#2-stan-projektu-i-co-jest-do-zrobienia)
3. [Architektura](#3-architektura)
4. [Struktura plików i paczki](#4-struktura-plików-i-paczki)
5. [Core na robocie (C++ / ROS 2)](#5-core-na-robocie-c--ros-2)
6. [Komendy robota (tagi `#...#`)](#6-komendy-robota-tagi-)
7. [Łącze core ↔ GUI (protokół)](#7-łącze-core--gui-protokół)
8. [Aplikacja Qt na laptopie](#8-aplikacja-qt-na-laptopie)
9. [Zarządzanie core'em: skrypt, systemd, tmux, klucze](#9-zarządzanie-coreem-skrypt-systemd-tmux-klucze)
10. [Konfiguracja: wszystkie zmienne środowiskowe](#10-konfiguracja-wszystkie-zmienne-środowiskowe)
11. [Instalacja i wdrożenie od zera](#11-instalacja-i-wdrożenie-od-zera)
12. [Obsługa na co dzień](#12-obsługa-na-co-dzień)
13. [Diagnostyka i naprawa usterek](#13-diagnostyka-i-naprawa-usterek)
14. [Bezpieczeństwo](#14-bezpieczeństwo)
15. [Znane problemy i dług techniczny](#15-znane-problemy-i-dług-techniczny)
16. [Jak rozbudowywać projekt](#16-jak-rozbudowywać-projekt)
17. [Pomysły na targi](#17-pomysły-na-targi)
18. [Historia zmian](#18-historia-zmian)
19. [Słownik](#19-słownik)

---

## 1. Czym jest Helios

Helios to warstwa „rozmowy” dla robota Rokae. Robot **stale nasłuchuje słowa-klucza „Helios”**, nagrywa wypowiedź, rozpoznaje mowę **lokalnie** (whisper.cpp), wysyła tekst do **modelu językowego w chmurze** (wyłącznie OpenRouter), z odpowiedzi wyciąga **komendy** w postaci tagów `#...#` (np. `#wave#`), wykonuje je na robocie i **odpowiada głosem** (Piper, polski głos `pl_PL-darkman-medium`).

Drugą połową projektu jest **aplikacja Qt na laptopie**: czat z robotem, podgląd kamery ze śledzeniem ruchów operatora, nagrywanie stawów robota do CSV oraz zdalny start/stop core'a przez SSH.

```
 Człowiek ──głos──► mikrofon ─► „Helios” ─► nagranie ─► Whisper (lokalnie) ─► tekst
                                                                                │
   głośnik ◄── Piper (TTS) ◄── odpowiedź ◄── LLM (OpenRouter) ◄─────────┘
                                   │
                                   └─► tagi #komenda# ─► ruch robota (Rokae SDK)
```

**Zasady projektu** (wynikają z wymagań zleceniodawcy):

- Narzędzia mają być **darmowe**; sprzęt i internet są dozwolone; całość musi działać płynnie.
- Rozpoznawanie mowy jest **tylko lokalne** (Groq usunięto z STT; model `large-v3`). Groq został usunięty także z modelu językowego (decyzja zleceniodawcy, 2026-10-16): jedynym dostawcą LLM jest OpenRouter, bez modeli zapasowych.
- Słowo-klucza „Helios” **nie zmieniamy**.
- Kod tańca jest **zamrożony** do czasu naprawy sprzętu tułowia (patrz §2).

---

## 2. Stan projektu i co jest do zrobienia

### 2.1 Co działa (zbudowane i opisane w tym dokumencie)

| Obszar | Stan |
|---|---|
| Słowo-klucz + weryfikacja przez STT | działa |
| Rozpoznawanie mowy (whisper.cpp, VAD, prompt, korekta słów) | działa; poprawki błędów z diagnozy wdrożone |
| LLM (tylko OpenRouter, bez modeli zapasowych) | działa |
| TTS (Piper) + muzyczka „myślę…” | działa |
| Komendy: `wave`, `dragOn/Off`, `posZero`, `stats`, `volume`, `youtube` | działają (wymagają sieci do kontrolerów robota) |
| Łącze GUI (TCP + unix, JSON-linie) + keepalive | działa |
| Aplikacja Qt: czat, kamera/śledzenie, nagrywarka stawów, START/STOP core'a | działa |
| Skrypt `helios_core.sh` + `helios.service` (autostart po boocie) | działa |
| Auto-wczytanie `~/rokae_ws/keys.zsh` przez skrypt | **czeka na wgranie i test przez użytkownika** |

### 2.2 Otwarte sprawy

| # | Sprawa | Szczegóły |
|---|---|---|
| 1 | **Stats w czacie jako dymek Heliosa** | Kod gotowy w paczce `helios_stats_chat.zip` (§4.3). **Czeka na wgranie i test** (`/stats` w czacie → dymek Heliosa z raportem). |
| 2 | **Nowy prompt systemowy nie jest jeszcze w kodzie** | Prompt żyje w `main.cpp` jako stała `MASTER_PROMPT` i **nadal jest starą wersją** (m.in. z listą komend z YouTube i bez nazwy firmy). Finalna wersja została dostarczona w czacie, ale nie podmieniono jej w źródle – nie wiadomo było, w którym pliku ma trafić. Odpowiedź: `main.cpp`, stała `MASTER_PROMPT` (linia 432). Szczegóły w §5.7. |
| 3 | **Taniec** | Logika zasilania i ruchu jest w `commands/youtube.cpp` (przedrostek `[Dance]`). Kod **zamrożony**. Blokada: tułów robota zgłasza błąd w Robot Assistant – to naprawa sprzętowa po stronie managera. `posZero` odblokowujemy dopiero **po** działającym tańcu; najpierw włączenie zasilania tułowia. |
| 4 | **Wgranie `helios_core.sh` z obsługą `keys.zsh`** | Kryterium akceptacji: w odpowiedzi na `start` jest `STARTED … keys=TAK`. |
| 5 | **Prompt „targowy”** | Zaparkowany (wstrzymany na życzenie). Nie ruszać bez prośby. |

### 2.3 Decyzje, które obowiązują

- Dostarczamy **pełne pliki**, nie łatki/diffy (chyba że zleceniodawca poprosi inaczej).
- Konfiguracja przez `HELIOS_*`; w terminalu preferowana forma `env HELIOS_...=... ros2 run helios_ai main` zamiast `export`.
- Przycisk ●REC nad czatem **został odrzucony i cofnięty**. Nagrywanie startuje chipem „Nagrywaj” albo komendą w czacie. Nie dodawać z powrotem bez pytania.
- Nagrywarka zapisuje **stawy robota** (ramiona + tułów z `/joint_states`), **nie** kąty człowieka z telemetrii.
- Stats w czacie wyświetlamy jako **dymek Heliosa** (`reply`), nie szary komunikat systemowy.
- **Aplikacja Qt na laptopie działa wyłącznie na Windowsie** (zdecydowane na 100%). Instrukcje dla laptopa są tylko dla Windowsa (PowerShell, `ssh.exe` z „Klienta OpenSSH”); nie opisujemy Linuksa ani macOS-a.
- Przy nowym prompcie: firma jako dosłowny `<nazwa firmy>`; bez sekcji o twórcach; bez reguły głośności na starcie; bez wzmianek o YouTube.

---

## 3. Architektura

### 3.1 Schemat ogólny

```
┌─────────────────────────── LAPTOP (Windows/Linux) ───────────────────────────┐
│  Aplikacja Qt „TrackingRobot”                                                │
│   ├─ HeliosLink  ── TCP :5123, JSON-linie ───────────────┐ (czat, nasłuch)  │
│   ├─ SshCore     ── ssh (BatchMode) ─► helios_core.sh ───┼─┐ (START/STOP/LOG)│
│   ├─ RobotClient ── WebSocket :9090 (rosbridge) ─────────┼─┼─┐ (/joint_states│
│   ├─ VideoReceiver ─ HTTP :8080 (strumień kamery) ───────┼─┼─┼─┐  + komendy)  │
│   └─ JointRecorder ─ CSV w Dokumenty/HeliosNagrania      │ │ │ │              │
└───────────────────────────────────────────────────────────┼─┼─┼─┼──────────────┘
                          sieć 192.168.49.x (laptop ↔ robot)│ │ │ │
┌──────────────────────── ROBOT (Jetson AGX Orin) 192.168.49.97 ┼─┼─┼────────────┐
│  helios_ai/main  (core, ROS 2 pakiet)  ◄──────────────────────┘ │ │ │          │
│   ├─ wątek audio (arecord) ─► detektor „Helios” (ONNX)           │ │ │          │
│   ├─ kolektor wypowiedzi ─► Whisper (whisper-cli / server)       │ │ │          │
│   ├─ klient LLM (curl) ─► OpenRouter  (internet)                 │ │ │          │
│   ├─ Piper ─► aplay (karta REF)                                   │ │ │          │
│   └─ komendy ─► Rokae SDK ─► kontrolery                           │ │ │          │
│  rosbridge :9090, serwer wideo :8080, /joint_states  [założenie: poza tym repo]│
└──────────────────────────────┬────────────────────────────────────────────────┘
                               │ sieć 192.168.71.x (robot ↔ kontrolery)
          lewa ręka 192.168.71.161 · prawa ręka .160 · tułów .254 · Jetson (LOCAL_IP) .51
```

### 3.2 Komponenty

| Komponent | Gdzie działa | Język / technologia | Rola |
|---|---|---|---|
| **core** `helios_ai/main` | robot (Jetson) | C++17, ROS 2 (ament_cmake), libcurl, nlohmann-json, ONNX Runtime, Rokae SDK | pętla rozmowy, komendy, łącze GUI |
| **whisper.cpp** | robot | zewnętrzne (`whisper-cli` lub `whisper-server`) | rozpoznawanie mowy, model `ggml-large-v3.bin` |
| **Piper** | robot | zewnętrzne | synteza mowy (głos `pl_PL-darkman-medium`) |
| **openWakeWord (ONNX)** | robot, w core | ONNX Runtime | detektor „Helios” (`helios.onnx`) |
| **aplikacja Qt** | laptop | C++17, Qt 6.5+ (Quick, QuickControls2, WebSockets, Network), OpenCV | czat, kamera, śledzenie, nagrywanie, START/STOP |
| **`helios_core.sh`** | robot | bash | start/stop/status/log core'a w tmux |
| **`helios.service`** | robot | systemd (usługa użytkownika) | autostart po boocie |
| **rosbridge, serwer wideo, `/joint_states`** | robot | ROS 2 | **[założenie]** działają w innym pakiecie – w workspace ich nie ma |

### 3.3 Sieci i adresy (twardo wpisane w kodzie)

| Adres | Co to | Gdzie wpisany |
|---|---|---|
| `192.168.49.97` | robot (core, rosbridge :9090, wideo :8080, SSH, łącze GUI :5123) | `appconfig.h`, `Main.qml`, `helioslink.cpp`, `sshcore.cpp` (część nadpisywalna `HELIOS_*`) |
| `192.168.71.161` | kontroler lewej ręki | `commands/*.cpp` |
| `192.168.71.160` | kontroler prawej ręki | `commands/*.cpp` |
| `192.168.71.254` | kontroler tułowia | `commands/pos_zero.cpp`, `wave.cpp`, `stats.cpp`, `youtube.cpp` |
| `192.168.71.51` | adres lokalny Jetsona w sieci kontrolerów (`LOCAL_IP`) | `commands/*.cpp` |

Adresy `192.168.71.x` są **zduplikowane w 6 plikach** komend – zmiana sieci wymaga edycji każdego z nich (patrz §15).

### 3.4 Jak płynie jedna rozmowa (od „Helios” do ruchu)

1. **Wątek audio** czyta mikrofon (`arecord`, 16 kHz, mono, ramki 80 ms) i wkłada ramki do kolejki.
2. **Pętla mikrofonu** wyrównuje poziom (AGC), karmi **detektor słowa-klucza** i bufor pierścieniowy (3 s).
3. Detektor wykrywa „Helios” → start nagrywania (z pre-rollem 0,6 s). Jeśli pewność < 0,90, fragment idzie jeszcze do **weryfikacji przez Whispera**; przy ≥ 0,90 reakcja jest natychmiastowa.
4. Odtwarza się **dzwonek**; kolektor zbiera wypowiedź do 1,2 s ciszy (okno na polecenie: 4 s).
5. **Wątek roboczy** rozpoznaje mowę, usuwa słowo-klucz z początku, loguje `Ty: …`, wysyła `heard` do GUI.
6. Tekst trafia do **LLM**; podczas „myślenia” gra muzyczka.
7. `llm_commands::process()` znajduje tagi `#...#`, uruchamia każdy w **osobnym, odłączonym wątku** i zwraca tekst bez tagów.
8. Core wysyła `reply` do GUI i **mówi głosem** (Piper). Przez następne 6 s można mówić bez „Helios” (follow-up).
9. Rozmowa jest zapisywana do `historia/rozmowa_<data>.txt` i `.json`.

---

## 4. Struktura plików i paczki

### 4.1 Źródło prawdy

| Katalog / plik | Rola |
|---|---|
| **`uploads/`** | **kanoniczne źródła core'a** (C++), wraz z `uploads/commands/` i `uploads/qt/` |
| `uploads/qt/` | kanoniczna aplikacja Qt (identyczna z paczką `helios_qt_gui_link.zip`) |
| `helios_core.sh`, `helios.service` | skrypt i usługa dla robota |
| `wdrozenie_gui.md` | instrukcja wdrożenia łącza GUI i zdalnego startu |
| `helios_szybki_start.md` | „na szybko”: uruchamianie, komendy, zmienne, awarie |
| `diagnoza_stt_helios.md`, `research_polski_stt_cpp.md` | analiza jakości rozpoznawania polskiej mowy |
| `diagnoza_stt/` | skrypty A/B testu STT (`ab_stt.py`) i symulacji ucinania (`symulacja_obciecia.py`) + przykładowe dane |
| `test_gui_link.py` | sztuczny klient GUI do testu łącza bez Qt |
| `muzyczka_winda.wav`, `muzyczka_kolejka.wav` | przykładowe pliki muzyczki oczekiwania |

### 4.2 Kopie historyczne (nie edytować)

| Katalog / plik | Co to jest |
|---|---|
| `main.cpp`, `youtube.cpp`, `speech_to_text.*` w katalogu głównym | starsze kopie robocze. `youtube.cpp` i `speech_to_text.*` są identyczne z `uploads/`; **`main.cpp` w katalogu głównym jest starszy** |
| `robot_src/` | scalona kopia z robota (`main.cpp`, `CMakeLists.txt` + oryginały `*.orig.*`); **różni się od `uploads/main.cpp`** (nie ma zmian stats) |
| `uploads_przed_muzyka/`, `uploads_przed_zmiana_groq/`, `uploads_przed_zmiana_watkow/` | migawki sprzed kolejnych zmian (muzyczka, usunięcie Groq z STT, wątki komend) |
| `qt_oryginal_backup/`, `qt_friend_v2/` | oryginał aplikacji Qt i wersja od kolegi (bez czatu/SSH) |
| `qt_chat_layout/` | identyczny z `uploads/qt/` + `podglad_layoutu.html` (makieta układu) |
| `zmiana_*.patch` | trzy łatki (muzyczka, usunięcie Groq z STT + large-v3, wątki komend). Już zastosowane w `uploads/` |
| `image-search/` | zrzuty Robot Assistant użyte jako wzór kolorystyki GUI |

### 4.3 Paczki do wgrania

| Paczka | Zawartość | Uwagi |
|---|---|---|
| `helios_core_gui_link.zip` | `main.cpp`, `CMakeLists.txt`, `gui_link.h`, `gui_link.cpp` | łącze GUI + keepalive. **Jej `main.cpp` jest starszy niż w paczce stats** |
| `helios_stats_chat.zip` | `main.cpp`, `commands/stats.h`, `commands/stats.cpp` | stats w czacie. **Jej `main.cpp` jest najnowszy i zawiera wszystko** (łącze GUI + hook stats) – jest identyczny z `uploads/main.cpp` |
| `helios_qt_gui_link.zip` | cała aplikacja Qt | identyczna z `uploads/qt/` |
| `helios_qt_layout_demo.zip` | „nieaktywna” makieta układu | **demo bez funkcji**: czat martwy, działa tylko przełącznik kamery |

**Kolejność wgrywania na robota** (jeśli robot jest jeszcze „czysty”): najpierw `helios_core_gui_link.zip` (cztery pliki), potem `helios_stats_chat.zip` (nadpisuje `main.cpp` nowszym i dodaje `commands/stats.*`), potem `colcon build`. Jeśli łącze GUI jest już wgrane, wystarczy sama paczka stats.

---

## 5. Core na robocie (C++ / ROS 2)

Pakiet: `helios_ai` (ament_cmake). Plik wykonywalny: `main`. Uruchamianie: `ros2 run helios_ai main`.

### 5.1 Moduły

| Plik | Odpowiada za |
|---|---|
| `main.cpp` (≈2700 linii) | opcje (`Options`), klient LLM (`OpenRouterChat`), maszyna stanów, wątki, AGC (`MicGain`), muzyczka oczekiwania (`WaitingLoop`), tryby diagnostyczne, przejmowanie sesji, obsługa linii z klawiatury/GUI (`handleConsoleLine`) |
| `mic.h/.cpp` | jeden strumień `arecord`, bufor pierścieniowy, zapis/odczyt WAV |
| `wakeword.h/.cpp` | detektor słowa-klucza (potok openWakeWord na ONNX Runtime) |
| `speech_to_text.h/.cpp` | kolektor wypowiedzi (adaptacyjna cisza), silniki STT (server/local), korekta słów, słownik użytkownika |
| `helios_tts.h/.cpp` | synteza: `piper` → `aplay` |
| `llm_commands.h/.cpp` | rejestr komend, wykrywanie tagów, uruchamianie w wątkach |
| `gui_link.h/.cpp` | serwer łącza z GUI (select, TCP + unix, keepalive) |
| `commands/*.cpp` | implementacje komend (rozdział 6) |

### 5.2 Wątki i synchronizacja

| Wątek | Zadanie |
|---|---|
| `audioReader` | czyta ramki z mikrofonu do `FrameQueue` (25 ramek ≈ 2 s zapasu; nadmiar nadpisuje najstarsze). Przy zakończeniu strumienia próbuje wznowić `arecord` |
| pętla główna | AGC, detektor „Helios”, kolektor wypowiedzi, maszyna stanów, statystyki `[Audio]` co 5 s |
| `worker` | weryfikacja słowa-klucza, STT, LLM, komendy, TTS, zapis historii |
| `keyboard` | czyta stdin; `exit`/`quit` kończy program. **Nigdy nie czyta mikrofonu** |
| wątek `GuiLink` | `select()` po gniazdach: akceptacja klienta, odbiór linii, wysyłka kolejki `guiOutbox` |
| wątki komend | po jednym, **odłączone** (`detach`), na każdy tag. Wyjątki są łapane i logowane (`[Błąd komendy …]`) – bez tego jeden wyjątek (np. zerwana sieć do robota) zabiłby cały program |

Wspólny stan to struktura `Shared`. Jest tworzona na **stercie** (`std::shared_ptr`), żeby odłączony wątek (np. `stats`) nie sięgał do zniszczonego stosu przy wyjściu z `main`.

### 5.3 Maszyna stanów

```
WaitingForWake ──„Helios”──► VerifyingWake ──potwierdzone──► Capturing ──cisza 1,2 s──► Busy ─► WaitingForWake
        ▲                          │ odrzucone                      │ timeout 4 s            (STT+LLM+TTS)
        └──────────────────────────┴────────────────────────────────┘
```

Do GUI stan jest tłumaczony na: `listening`, `muted`, `verifying`, `capturing`, `thinking`, `speaking` (wysyłany ~5×/s, ale tylko przy zmianie).

### 5.4 Wejście audio

- Urządzenie: `HELIOS_AUDIO_DEVICE` (domyślnie `plughw:CARD=REF,DEV=0`), 16 kHz, mono, ramka 1280 próbek (80 ms). Sprawdzenie karty: `arecord -l`.
- **AGC (`MicGain`)**: automatyczne wzmocnienie do ×10 (`HELIOS_AGC_MAX`), szybki atak i wolne podnoszenie. Działa przed detektorem i nagrywaniem. Wyłączenie: `HELIOS_AGC=0`. Uwaga: AGC wzmacnia też szum wentylatorów i serw.
- **Statystyki** co 5 s w logu: `[Audio] ramek/s … szczyt … max score … zgubione ramki … wzmocnienie`. Prawidłowo: ≈ 12,5 ramki/s. Poniżej 10 → ostrzeżenie o gubieniu dźwięku.
- Gdy robot mówi albo „myśli”, pętla **nie słucha** (żeby nie łapać własnego głosu).

### 5.5 Słowo-klucz „Helios”

- Potok openWakeWord: ramka 1280 próbek → mel-spektrogram (`melspectrogram.onnx`) → embedding (`embedding_model.onnx`) → klasyfikator własny (`helios.onnx`). Modele domyślnie w `/home/std/helios/models/`.
- Parametry: próg `HELIOS_WAKE_THRESHOLD=0.25`, `HELIOS_WAKE_PATIENCE=2` (ramki ponad progiem), `HELIOS_WAKE_WINDOW=4` (okno reguły: `patience` z ostatnich `window` ramek, nie muszą być kolejne; 0 = kolejne). Te trzy wartości wybrano po teście na robocie: zwykłe „Helios” (nie tylko rozciągnięte „Heeeelios”) jest wykrywane; fałszywe pobudki odsiewa weryfikacja STT, `HELIOS_WAKE_LOG_MIN=0.15` (log „[Wake] score …” dla prawie-trafień; 0 = wyłączony), `HELIOS_WAKE_DEBOUNCE=1.2 s`, wątki ONNX `HELIOS_WAKE_THREADS=2`.
- **Weryfikacja przez STT** (domyślnie włączona): po pobudce nagrywany jest fragment ≈1,6 s i sprawdzany Whisperem. Zaakceptowane są warianty zbliżone do „Helios” (odległość edycyjna ≤ 1, prefiksy `helio…/heliu…/helju…`, rdzenie `elios/elius/eljos`), z listą słów zabronionych. `HELIOS_WAKE_VERIFY=off` wyłącza, `strict` odrzuca pobudkę, gdy STT zawiedzie (domyślnie przy błędzie STT pobudka jest przyjmowana, żeby robot nie był „głuchy” offline).
- **Szybka ścieżka**: pewność detektora ≥ `HELIOS_WAKE_FAST_SCORE=0.90` pomija weryfikację (oszczędza 1–3 s).
- Tryb ręczny: `HELIOS_NO_WAKEWORD=1` – brak detektora, nagrywanie po `/listen`.
- **Nie zmieniać** słowa-klucza (decyzja zleceniodawcy).

### 5.6 Rozpoznawanie mowy (STT)

**Kolektor wypowiedzi** (`UtteranceCollector`): progi **adaptacyjne**, nie stałe. Poziom szumu jest mierzony na bieżąco; mowa = podłoga szumu + 8 dB, koniec = 1,2 s ciszy. Dokładany pre-roll 0,6 s. Normalizacja szczytu do −3 dBFS (max +20 dB). Wypowiedzi krótsze niż 0,25 s są odrzucane, maksymalna długość 15 s. Obcinanie ciszy na końcu zostawia ostatnią mowę + 0,25 s.

**Silniki** (`HELIOS_STT_BACKEND`):

| Wartość | Działanie |
|---|---|
| `local` | `whisper-cli` (large-v3, VAD Silero, beam search `-bs 5 -bo 5`, prompt). Model ładuje się przy każdej wypowiedzi, więc jest wolniej |
| `server` | `whisper-server` z modelem trzymanym w RAM (`HELIOS_STT_SERVER_URL`, np. `http://127.0.0.1:8081/inference`); szybciej |
| `auto` (domyślnie) | `server`, jeśli podano URL, inaczej `local`. Przy awarii serwera zejście na `local` (`HELIOS_STT_FALLBACK_LOCAL`) |

**Jakość polskiego:**

- Model: `/home/std/models/ggml-large-v3.bin` (nie turbo).
- **Prompt dziedzinowy** (lista komend i słownictwa robota) – uwaga: Whisper lubi powtarzać słowa z promptu. Pusty `HELIOS_STT_PROMPT=` wyłącza prompt (do testów A/B).
- **Halucynacje Whispera** („Wszystkie prawa zastrzeżone”, „Napisy stworzone przez społeczność Amara.org”, „Dziękuję za obejrzenie”…): Whisper uczono na filmach z napisami i na ciszy/szumie/niewyraźnej mowie „dopisuje” stopki filmów. Trzy warstwy obrony: (1) **bramka poziomu** w `Recognizer::transcribe` – nagranie, w którym szczyt jest cichszy niż `HELIOS_STT_MIN_PEAK_DB` (domyślnie −42 dBFS), nie idzie do Whispera (normalizacja podbijałaby szum do −3 dB); (2) **flagi whisper-cli** `-mc 0 -nf --suppress-nst` (bez kontekstu, bez ponawiania z wyższą temperaturą, bez tokenów nie-mowy; `HELIOS_STT_ANTYHALUCYNACJE=0` wyłącza; gdy wersja `whisper-cli` ich nie zna, program ponawia bez nich; dodatkowe argumenty: `HELIOS_STT_EXTRA_ARGS`); (3) **filtr fraz** – lista wbudowana + własna (`HELIOS_STT_HALUCYNACJE` lub `~/.helios_halucynacje.txt`, jedna fraza w linii); fraza jest wycinana, a gdy nic nie zostaje, wynik jest pusty. Dodatkowo: to samo słowo 5+ razy z rzędu = pętla halucynacji. Przy braku pliku modelu VAD program ostrzega w logu (bez VAD Whisper halucynuje wyraźnie częściej). Przy weryfikacji słowa-klucza odrzucony wynik (cisza/halucynacja) = pobudka odrzucona, a nie „przyjęta na wszelki wypadek”. Logi: `[STT] wycięto halucynację Whispera (…)`, `[STT] odrzucone: za cicho - szczyt … dBFS`.
- **Korekta słów** (`HELIOS_STT_FIX=1`): wbudowana lista działa tylko w wypowiedziach z komendą (wykrytą po słowie polecenia, np. „włącz”, „drag”, „pozycja”). W zwykłej rozmowie działa wyłącznie **słownik użytkownika**.
- **Słownik użytkownika** (z kalibracji `--kalibracja`): jeden wiersz `usłyszane -> poprawne`, `#` zaczyna komentarz. Szukany kolejno: `HELIOS_STT_SLOWNIK`, `~/.helios_slownik.txt`, `/home/std/helios/slownik.txt`.
- VAD działa tylko, jeśli istnieje plik modelu (`HELIOS_STT_VAD_MODEL`); nieistniejący plik = VAD wyłączony.
- Plik `historia/ostatnia_wypowiedz.wav` zawiera ostatnią wypowiedź po normalizacji – **podstawowe narzędzie do odsłuchu**, co faktycznie usłyszał Whisper.
- Objaw priorytetowy z diagnozy: przekręcone polskie końcówki i głoski miękkie/dźwięczne. Hipotezy i plan testów: `diagnoza_stt_helios.md`.

#### 5.6.1 Słowniczek poprawek (korekta przekręconych słów)

Dwa słowniki, oba w `speech_to_text.cpp`. Obydwa działają po rozpoznaniu, na tekście z Whispera.

| Słownik | Gdzie jest | Kiedy działa |
|---|---|---|
| **wbudowany** | funkcja `commandFix()` – tablica `dictionary` (≈ linie 285–330) | tylko gdy wypowiedź zawiera słowo polecenia (lista `triggers` w `hasCommandTrigger()`, np. „włącz”, „drag”, „pozycja”). W zwykłej rozmowie nie rusza tekstu |
| **użytkownika** (z kalibracji) | plik tekstowy, szukany kolejno: `HELIOS_STT_SLOWNIK` → `~/.helios_slownik.txt` → `/home/std/helios/slownik.txt` | **zawsze** i ma pierwszeństwo przed wbudowanym |

**Ważne: to nie jest dopasowanie „na podobieństwo”.** Słowo jest zamieniane tylko wtedy, gdy **dokładnie** (po zamianie na małe litery) pasuje do wpisu w tabeli. „Prawie takie samo” słowo, którego nie ma w tabeli, przechodzi bez zmian. Jedyne miejsce w core, gdzie jest dopasowanie przybliżone (odległość edycyjna ≤ 1), to **sprawdzanie słowa-klucza „Helios”** w `main.cpp` (`isWakeWordToken`).

**Wbudowane wpisy** (usłyszane → poprawne):

| Grupa | Przykłady |
|---|---|
| włącz | `wlancz`, `włancz`, `włoncz`, `wloncz`, `wlacz`, `włacz`, `wlanczac`, `wlanczam`, `wlanczaj`, `wlanczamy`, `włąncz` → **włącz** |
| wyłącz | `wylancz`, `wyłancz`, `wyłoncz`, `wyloncz`, `wyłacz`, `wylacz`, `wylanczac`, `wylanczam`, `wylanczaj`, `wyłąncz` → **wyłącz**; `wyłonczyc`, `wylanczyc` → **wyłączyć** |
| drag | `drak`, `draka`, `drakę`, `drage`, `dragę`, `dragu`, `draga`, `drakowi` → **drag** |
| statystyki | `statystyk`, `statystka`, `statystke`, `stastyki`, `statystki`, `statyski` → **statystyki** |
| pomachaj | `pomahaj` → **pomachaj** |
| youtube | `jutube`, `jotube`, `yutub`, `youtub`, `jutub`, `jotub` → **youtube** |
| głośność | `głośnosć`, `glosnosc`, `głosc` → **głośność** |
| zdjęcie | `zdjencie`, `zdiencie`, `zdjecię` → **zdjęcie** |
| taniec | `tanic` → **taniec** |

Celowo usunięte z listy: `tańca`, `pomachajmy`, `pomachajcie` – to poprawne polskie formy i zamiana psuła gramatykę zdań typu „posłuchaj tańca”.

**Słownik użytkownika – format pliku** (jedna poprawka na linię, `#` zaczyna komentarz, separator `->` albo `=`):

```
# usłyszane -> poprawne
wlancz -> włącz
drak = drag
statystka -> statystyki      # komentarz po #
```

Przy starcie w logu pojawia się `[STT] wczytano własny słownik poprawek: <ścieżka> (N pozycji)`. **Brak takiej linii = żaden plik słownika nie został znaleziony** (w workspace takiego pliku nie ma; jeśli istnieje, to na robocie).

**Kalibracja** (`ros2 run helios_ai main --kalibracja [plik_ze_zdaniami]`): program czyta kolejne zdania, nagrywa Twój głos, rozpoznaje i porównuje z oczekiwanym tekstem (wyrównanie słów odległością Levenshteina). Z różnic buduje wpisy `usłyszane -> poprawne` i zapisuje do `~/.helios_slownik.txt`. 
> **Uwaga:** każda kalibracja **nadpisuje ten plik w całości**. Ręcznie dopisane wpisy znikną – trzymaj je w osobnym pliku i wskaż przez `HELIOS_STT_SLOWNIK`, albo zrób kopię przed kalibracją.

**Dodanie wpisu bez przebudowy:** dopisz linię do `~/.helios_slownik.txt` i zrestartuj core (słownik czyta się raz, przy pierwszym użyciu). **Wyłączenie wszystkiego:** `HELIOS_STT_FIX=0`. (wyłącza oba słowniki). Każda zastosowana poprawka jest widoczna w logu: `[STT] poprawiłem słowa: wlancz -> włącz`.

### 5.7 Model językowy (LLM) i prompt systemowy

**Dostawca:** wyłącznie OpenRouter, model `HELIOS_LLM_MODEL` (domyślnie `nvidia/nemotron-3-ultra-550b-a55b:free`). Modele zapasowe OpenRouter (`HELIOS_LLM_FALLBACK`) i Groq zostały **usunięte z kodu** (decyzja zleceniodawcy po przypadku „myśli w nieskończoność”).

**Przebieg zapytania:**

1. Zapytanie do modelu głównego.
2. Tylko gdy serwer **odpowiedział** błędem albo pustą treścią (modele „myślące” bywają puste przy wyczerpaniu tokenów): ten sam model jeszcze raz, z wyłączonym `reasoning`.
3. Brak połączenia, przekroczenie czasu albo limit konta (429) kończą zapytanie od razu – robot odpowiada komunikatem `[Błąd LLM: …]`, a nie „myśli” w nieskończoność.

Limit tokenów odpowiedzi: `HELIOS_LLM_MAX_TOKENS=1024` (modele „myślące” przy małym limicie potrafią zwrócić pustą odpowiedź – błąd „brak choices”). Własny dostawca: `HELIOS_LLM_URL` + `HELIOS_LLM_KEY_ENV`.

**Limity czasu zapytań:** połączenie z serwerem max `HELIOS_LLM_CONNECT_TIMEOUT=8` s, odpowiedź max `HELIOS_LLM_TIMEOUT=40` s. (Wcześniej: 180 s na każdą z kilkunastu prób i brak limitu na połączenie, więc przy złej sieci lub „zawieszonym” darmowym modelu robot „myślał” kilkanaście minut.) Log pokazuje każdą próbę: `[LLM] wysyłam zapytanie: … (limit 40 s)...`, `[LLM] odpowiedział: … po 3.2 s` albo `[LLM] nie udało się (…, 8.0 s): …`.

> **Aktualna wersja `main.cpp` (baza użytkownika, `gotowe/main.cpp`):** zapytanie do LLM ma jeden stały limit `CURLOPT_TIMEOUT = 60 s` (było 180 s), bez zmiennych `HELIOS_LLM_*TIMEOUT`. Zmiana w jednej linii (okolice linii 659). Po przekroczeniu limitu w logu pojawia się błąd `połączenie: Timeout was reached`.

**Prompt systemowy** to stała `MASTER_PROMPT` w `main.cpp`. Obecna zawartość definiuje: tożsamość (humanoidalny robot Helios, przyjazny asystent, krótko i po polsku), **listę komend** w formacie `#tag#`, zakres głośności 0–80, przykłady, zasadę „nie wymyślaj nowych komend” i „czysty tekst, bez gwiazdek i emotek”.

> **Do zrobienia (otwarta sprawa nr 2):** podmienić zawartość `MASTER_PROMPT` na finalną wersję. Po podmianie: (a) ustaw dosłownie `<nazwa firmy>` albo wpisz nazwę firmy, (b) model zna tylko komendy z listy w prompcie – jeśli jakiejś tam zabraknie (np. `#youtube:…#`), głos jej nie wywoła. Komendy nadal działają po **ręcznym wpisaniu tagu** w czacie lub w konsoli, niezależnie od tego, czy LLM je zna.

**Uwaga o tagach:** tag w odpowiedzi modelu wykona się **zawsze**, jeśli występuje w rejestrze (§6). Dlatego prompt jest też mechanizmem bezpieczeństwa – lista komend w prompcie to lista tego, co model „wie”, że może zrobić.

### 5.8 Synteza mowy (TTS)

`helios_tts::speak()` składa komendę powłoki: `echo "tekst" | piper --model … --output-raw | aplay -D plughw:CARD=REF,DEV=0 -r 22050 -f S16_LE -t raw -q`. Cudzysłowy w tekście są zamieniane na spacje.

**Ograniczenia** (wartości są wpisane na stałe w `helios_tts.cpp`): `/home/std/piper/piper`, `/home/std/piper_models/pl_PL-darkman-medium.onnx`, karta `REF`. Zmiana `HELIOS_AUDIO_DEVICE` **nie** przekierowuje głosu Heliosa.

### 5.9 Muzyczka oczekiwania

Od końca nasłuchu do początku odpowiedzi głosowej gra zapętlony dźwięk, żeby nie było ciszy. Strażnik `WaitingGuard` gasi go przy każdym wyjściu z bloku „myślę”.

- `HELIOS_WAITING=0` wyłącza.
- `HELIOS_WAITING_STYLE`: `elevator` (8 s miękkiego padu, akordy C–Am–F–G) albo `queue` (cisza z „pipem” co 2 s).
- `HELIOS_WAITING_FILE` (domyślnie `/home/std/rokae_ws/muzyczka.wav`) – własny WAV, ma pierwszeństwo przed stylem; `HELIOS_WAITING_VOLUME=50` (%).
- Przy odtwarzaniu własnego pliku stosowana jest przeskalowana kopia.

### 5.10 Przejmowanie sesji i awarie

- **Jedna instancja:** plik `/tmp/helios_ai.pid`. Nowy start sprawdza, czy stary PID to naprawdę Helios, kończy go i zwalnia mikrofon. Wyłączenie: `HELIOS_PRZEJMIJ=0`.
- **Handler awarii:** przy SIGSEGV/SIGABRT/SIGBUS/SIGFPE/SIGILL w konsoli pojawia się `=== AWARIA: … ===` i zrzut stosu (do wysłania do analizy). Linia `[start] program wystartowal (handler awarii aktywny)` pojawia się **przed** `main()` – jej brak oznacza awarię przy statycznej inicjalizacji (np. w którymś z `commands/*.cpp`).
- Sygnały `SIGINT/SIGTERM` ustawiają flagę zatrzymania (czysty koniec pętli).

### 5.11 Skróty z klawiatury i czatu GUI

Obsługiwane przez `handleConsoleLine` – **bez internetu i bez LLM**:

| Wpis | Efekt |
|---|---|
| `/listen` | nagraj polecenie bez „Helios” |
| `/wave` · `/stats` · `/zero` · `/start` | `#wave#` · `#stats#` · `#posZero:all#` · `#posStart#` |
| `/drag left` · `/drag right` · `/drag both` · `/dragoff` | drag |
| `/gest` | test gestów rąk przez 12 s (§6.6) |
| `/wifi NAZWA HASŁO` · `/wifi "NAZWA Z SPACJĄ" HASŁO` · `/wifi status` · `/wifi ap` | Wi-Fi robota na targach, patrz §17.1 (hasło może mieć spacje i `#`) |
| dowolny tag, np. `#dragOn:left#` | wykonanie wprost (reszta tekstu zostanie wypowiedziana) |
| `exit`, `quit` | koniec programu (**tylko z konsoli**; z GUI idzie do LLM jako zwykły tekst) |
| inny tekst | trafia do LLM |

### 5.12 Tryby diagnostyczne (CLI)

```bash
ros2 run helios_ai main --mic-test              # poziom mikrofonu, 6 s mowy
ros2 run helios_ai main --stt plik.wav          # STT z pliku (16 kHz, mono)
ros2 run helios_ai main --wake-test plik.wav    # detektor słowa-klucza na pliku
ros2 run helios_ai main --llm-test "tekst"      # sam model językowy
ros2 run helios_ai main --kalibracja            # kalibracja słownika poprawek na Twoim głosie
ros2 run helios_ai main --help
```

`--mic-test` ocenia poziom po szczycie, więc werdykt bywa zbyt optymistyczny – oceniaj też odsłuchem `historia/ostatnia_wypowiedz.wav`.

### 5.13 Pliki generowane

| Plik | Zawartość |
|---|---|
| `historia/rozmowa_<RRRR-MM-DD_GG-MM-SS>.txt` / `.json` | rozmowa (zapis po każdej odpowiedzi i przy wyjściu); katalog z `HELIOS_HISTORY_DIR` |
| `historia/ostatnia_wypowiedz.wav` | ostatnia wypowiedź po normalizacji (nadpisywana, także przy weryfikacji słowa-klucza) |
| `/tmp/helios_utterance.wav` | ta sama wypowiedź po AGC, przed normalizacją |
| `/tmp/helios_ai.pid` | PID działającej instancji |
| `~/.helios_core.log` | log core'a uruchomionego przez `helios_core.sh` |

---

## 6. Komendy robota (tagi `#...#`)

### 6.1 Mechanizm

`llm_commands::process(text)` robi to samo dla odpowiedzi LLM, skrótów z klawiatury i tagów wpisanych w czacie:

1. regex `#(\w+):([^#]+)#` → komendy **z argumentem** (rejestr `ARG_COMMAND_REGISTRY`),
2. regex `#(\w+)#` → komendy **bez argumentu** (rejestr `COMMAND_REGISTRY`),
3. każda znaleziona komenda startuje w **odłączonym wątku** z blokiem `try/catch`,
4. tagi są wycinane z tekstu, który wraca do dalszej obróbki (wyświetlenie, TTS).

Tag nieznany daje w konsoli `[Nieznana komenda: …]` i **nie robi nic**. W konsoli widać też `[Uruchamiam komendę: …]`.

### 6.2 Rejestr (stan faktyczny)

| Tag | Funkcja | Plik | Co robi |
|---|---|---|---|
| `#wave#` | `commands::wave` | `wave.cpp` | macha prawą ręką i wraca do pozycji startowej (opis niżej) |
| `#dragOn:left\|right\|both#` | `commands::dragOn` | `drag_on.cpp` | włącza drag wybranej ręki/rąk |
| `#dragOff#` | `commands::dragOff` | `drag_off.cpp` | wyłącza drag obu rąk |
| `#posZero:left\|right\|both\|torso\|all#` | `commands::posZero` | `pos_zero.cpp` | powrót do pozycji zerowej |
| `#stats#` | `commands::stats` | `stats.cpp` | raport stanu modułów (konsola + czat GUI) |
| `#volume:0..80#` | `commands::volume` | `volume.cpp` | głośność głośników USB |
| `#youtube:TYTUŁ AUTOR#` | `commands::youtube` | `youtube.cpp` | wyszukuje i odtwarza utwór; taniec w pozie |

> **Uwaga:** stary `MASTER_PROMPT` wymienia jeszcze `#arms_moves#`, `#wave_big#` i `#full_move#`. **Takich komend nie ma w rejestrze** – model może je wypowiedzieć, ale w konsoli pojawi się `[Nieznana komenda: …]` i robot się nie ruszy. Przy podmianie prompta sprawdź, czy te trzy linie zostały usunięte.

### 6.3 Moduły robota i połączenia

| Moduł | Klasa SDK | Adres | Stawy |
|---|---|---|---|
| lewa ręka | `rokae::xMateErProRobot` | `192.168.71.161` | 7 |
| prawa ręka | `rokae::xMateErProRobot` | `192.168.71.160` | 7 |
| tułów | `rokae::PCB4Robot` | `192.168.71.254` | 4 (+ pole `head[2]` w strukturze `TorsoPose`) |

Ręce łączą się adresem lokalnym `192.168.71.51`. Każda komenda **łączy się z kontrolerami od nowa** i przy błędzie sieci kończy się komunikatem `[Błąd komendy …: network: …]` (nie zabija programu).

### 6.4 Szczegóły komend

**`wave`** – macha **tylko prawą ręką** (lewa ręka i tułów w czasie machania stoją). Prawa ręka jedzie **jednym ciągłym ruchem** w pętli czasu rzeczywistego (pozycja jointów co 1 ms, krzywa 1−cos – ręka nie staje i nie robi przerw między punktami, tylko płynnie zawraca): dojazd z miejsca, w którym stoi, do pozycji A (J1…J7 = −171, −5, −22, 28.92, −5.5, 3.5, 5; J1 = −171° zamiast 171°, bo to ta sama pozycja ręki, a nie obraca J1 o 344° względem pozycji startowej) → pozycja B (−171, −5, −22, 122.52, −5.5, 3.5, 5; zmienia się tylko J4, czyli łokieć, o 93,6°) → z powrotem do A; para B→A powtarza się `HELIOS_WAVE_POWTORZENIA` razy (domyślnie 3, zakres 1–10), a jedno przejście A→B trwa `HELIOS_WAVE_CZAS_RUCHU` sekund (domyślnie 4.0, zakres 0.5–10; mniej = szybciej; przy 4.0 szczytowa prędkość J4 to ok. 37°/s, a przyspieszenie ok. 29°/s²). Dojazd do A i powrót do pozycji startowej mają osobne ograniczenie: `HELIOS_WAVE_DOJAZD_PREDKOSC` (domyślnie 0.8 rad/s) i `HELIOS_WAVE_DOJAZD_MIN_CZAS` (domyślnie 2 s); log `[PRAWA] szczytowe przyspieszenie [deg/s^2]: dojazd …, machanie …, powrót …`. Po ostatnim machnięciu (w A) prawa ręka **w tej samej pętli** wraca płynnie do pozycji startowej i dopiero tam staje; po pętli program sprawdza odchyłkę (log `[PRAWA] pozycja po powrocie: odchyłka od startowej … deg`) i przy odchyłce > 2° dośle zapasowy `MoveJ`. Lewa ręka i tułów dojeżdżają na końcu (`MoveJ`, `PREDKOSC_GLOBALNA = 0.5`). **Pozycja startowa**: lewa ręka (2.22, 87.30, −89.40, 41.27, 93.61, 1.27, 39.44), prawa ręka (−173.06, −79.35, −103.52, 42.80, −84.28, −8.30, −2.92), tułów wszystkie jointy 0° (4 stawy + 2 zewnętrzne). Ręka, która już stoi w danym punkcie (różnica < 0,3° na każdym jointcie), nie jest ruszana. Błąd połączenia z prawą ręką przerywa komendę; brak lewej ręki lub tułowia to tylko ostrzeżenie (`[WAVE] Lewa ręka niedostępna…` / `[WAVE] Tułów niedostępny…`) – prawa ręka i tak macha i wraca. Logi z przedrostkiem `[WAVE]`. Pozycja startowa jest na razie wpisana tylko w `wave.cpp` (stałe `LEWA_START_DEG`, `PRAWA_START_DEG`); inne komendy do niej nie wracają.

**`dragOn`** – domyślnie ramię rusza się **tylko, gdy trzymasz przycisk na końcówce** (bezpieczne ustawienie domyślne). Wariant bez przycisku: `HELIOS_DRAG_BEZ_GUZIKA=1`. Wartości: `left`, `right`, `both`. Logi `[dragOn]`. Gdy kontroler zgłasza `moving` (ruch w toku po przerwanym `wave`/`posZero`), `dragOn` najpierw robi `stop` + `moveReset` i czeka na `idle`; tryb ręczny ustawia tylko, jeśli jeszcze nie jest ręczny (błąd `robot state (moving) unsupported operation` przy `setOperateMode`). Kroki `setMotionControlMode`/`clearServoAlarm` są domyślnie pomijane (`HELIOS_DRAG_PRZYGOTOWANIE=1` je włącza) – przy nich SDK wywalało program.

**`dragOff`** – wyłącza drag na obu rękach. Logi `[dragOff]`.

**`posZero`** – sprowadza wskazane moduły do zera stawów. Parametry: prędkość końcówki 200 mm/s, tryb pracy `manual`, tolerancja dojścia 1°, czas na start ruchu 5 s, maksymalny czas ruchu 60 s, czekanie na zasilanie do 10 s. Przed ruchem w razie potrzeby wyłącza drag (tułów nie ma API draga – kod to wykrywa szablonem). Wartości spoza listy → `[posZero] Dozwolone wartości: left, right, both, torso, all`. `all` = lewa + prawa + tułów; podsumowanie `ok/błąd` dla każdego modułu.
> Zasada zleceniodawcy: **`posZero` odblokowujemy dopiero po działającym tańcu**, a najpierw trzeba włączyć zasilanie tułowia.

**`posStart`** – obie ręce wracają do **pozycji startowej** (tej, w której kończy się `wave`): lewa (2.22, 87.30, −89.40, 41.27, 93.61, 1.27, 39.44), prawa (−173.06, −79.35, −103.52, 42.80, −84.28, −8.30, −2.92). Tułów nie jest ruszany (osobno `#posZero:torso#`). Ruch zwykły (`MoveAbsJ`, tryb NRT), kolejność wywołań jak w `posZero`, obie ręce równolegle; jeśli drag jest włączony, najpierw go wyłącza (awaryjnie przez wyłączenie zasilania). Ręka już w pozycji (odchyłka ≤ 1°) nie jest ruszana; ruch, który stoi w miejscu 8 s, jest przerywany. Tag `#posStart#`, skrót w konsoli i czacie `/start`. `HELIOS_POSSTART_PREDKOSC` (mm/s, domyślnie 100), `HELIOS_POSSTART_TRYB=auto`. Logi `[posStart]`. Pozycja startowa jest wpisana w dwóch miejscach: `wave.cpp` i `pos_start.cpp` – zmieniając jedno, zmień drugie.

**`stats`** – łączy się z lewą, prawą ręką i tułowiem i drukuje raport (~30 linii): dla każdego modułu połączenie, zasilanie, tryb, stan pracy, drag, pozycje stawów w stopniach; na końcu podsumowanie (liczba zasilanych/połączonych modułów, gdzie włączony drag). Raport trafia **na konsolę** i – gdy GUI jest podłączone – **do czatu jako dymek Heliosa** (patrz §6.5).

**`volume`** – walidacja liczby całkowitej 0–80, potem `amixer -c REF sset PCM <N>%`. Błędy: `[Głośność] …`.

**`youtube`** – wymaga zmiennej **`YOUTUBE_API_KEY`** (YouTube Data API v3, wyszukiwanie) oraz programu **`mpv`**. Odtwarza sam dźwięk (`--no-video`, format `251`, karta REF, bufor 30 s). Po starcie utworu robot ustawia ręce w pozie tanecznej, buja tułowiem (3° amplitudy, prędkość 60 mm/s) do końca piosenki, potem wraca do pozycji spoczynkowej. Ruch jest równoległy: ręce idą od razu, tułów przygotowuje się w tle i dołącza. Logi `[YouTube]`, `[Dance]`. **Część taneczna jest zamrożona do naprawy sprzętu tułowia** (patrz §2).

### 6.5 Stats w czacie (jak to działa)

Funkcja `stats()` była wcześniej „tylko konsolowa”. Teraz:

1. `statsReport()` buduje cały raport do `std::string` (to, co dotąd szło do `std::cout`, 1:1),
2. `stats()` wypisuje raport na konsolę **jak dotychczas** i wywołuje hook `g_statsListener`,
3. w `main.cpp` hook jest ustawiony po starcie łącza GUI i – **tylko jeśli GUI jest podłączone** – wysyła `{"type":"reply","text":<raport>,"ref":0}`,
4. aplikacja Qt zamienia `reply` na dymek Heliosa; **po stronie Qt nic nie trzeba było zmieniać**.

Przy komendzie głosowej „pokaż statystyki” w czacie pojawią się **dwa dymki Heliosa pod rząd**: najpierw odpowiedź LLM, potem raport. Hook woła się z wątku komendy, a `guiPush` ma własny mutex.

### 6.6 Gesty rąk podczas mowy (`commands/gesty.cpp`) – NIETESTOWANE na robocie

Gdy Helios mówi, obie ręce losowo jeżdżą między zadanymi punktami, a po końcu mowy płynnie wracają do pozycji startowej. To **nie jest komenda** (nie ma tagu `#...#`): moduł `gesty` jest wołany z `main.cpp` wokół `helios_tts::speak()`: `gesty::start(tekst, odpowiedzZRuchem)` przed mową i `gesty::stop()` po niej (oba nie blokują).

**Kiedy gesty są uruchamiane:**
- odpowiedź ma **min. 60 znaków** (krótkie bez gestów),
- w odpowiedzi modelu **nie ma komendy ruchu** (`#wave#`, `#posStart#`, `#posZero:..#`, `#dragOn:..#`, `#dragOff#`); np. „Macham do Ciebie” + `#wave#` = bez gestów,
- ręka stoi w promieniu **20°** od pozycji startowej (po `/start`, po `wave`); inaczej w logu jest `bez gestów. Uruchom /start.` (np. po `posZero`),
- ręka nie jest w stanie `moving`, poprzedni zestaw gestów już wrócił do startu.
Silniki włączane są przy starcie mowy (trwa kilka sekund; przy krótkiej mowie gest może się nie zdążyć pojawić, wtedy w logu: `mowa już się skończyła, zanim ręka była gotowa`).

**Ruch:** pętla RT jak w `wave` (1 kHz), krzywa 1−cos, w każdym punkcie prędkość 0 i losowa pauza 0,2–1,0 s, potem losowy kolejny punkt (inny niż poprzedni). Czas odcinka dobrany tak, by szczytowe przyspieszenie ≤ **40°/s²** (min. 2 s na odcinek). W symulacji (atrapa SDK): szczytowe przyspieszenie 40°/s², prędkość do 37°/s, zero skoków, koniec dokładnie w pozycji startowej. Po końcu mowy bieżący odcinek jest **dokończony**, potem powrót do startu, więc ręce mogą się ruszać jeszcze 2–6 s po ostatnim słowie. Twardy limit jednego zestawu: 90 s.

**Punkty lewej ręki** (J1..J7, stopnie; 3 pozy ramienia × J7 = 7.14 / 40.51 / −28.43):
| Poza | J1 | J2 | J3 | J4 | J5 | J6 |
|---|---|---|---|---|---|---|
| 1 | −27.37 | 87.09 | −75.45 | 71.64 | 95.73 | −6.77 |
| 2 | −28.45 | 97.08 | −64.61 | 56.81 | 89.66 | 17.87 |
| 3 | −35.60 | 80.57 | −105.79 | 41.62 | 87.16 | 1.49 |

**Punkty prawej ręki** = lustro lewych względem pozycji startowych: `prawa = PRAWA_START + znak × (lewa − LEWA_START)`, znaki J1..J7 = (−1, −1, −1, +1, −1, +1, −1). Uzasadnienie: start prawej zgadza się z odbitym startem lewej w J1..J6 do ok. 13°, jeśli przeliczyć bark na równoważną konfigurację (J1±180°, −J2, J3±180°). **To wniosek z pozycji startowych, nie pomiar.** Wyliczone punkty prawej (J1..J7):
```
poza 1: -143.47 -79.14 -117.47 73.17 -86.40 -16.34  J7 = 29.38 / -3.99 / 64.95
poza 2: -142.39 -89.13 -128.31 58.34 -80.33   8.30  J7 = 29.38 / -3.99 / 64.95
poza 3: -135.24 -72.62  -87.13 43.15 -77.83  -8.08  J7 = 29.38 / -3.99 / 64.95
```
**Pierwszy test:** ustaw ręce w pozycji startowej (`/start`), wpisz w czacie **`/gest`** (gestykulacja przez 12 s bez względu na tekst), stój przy stopie awaryjnym i obserwuj prawą rękę: czy jej ruch jest lustrzany do lewej i nie zbliża się do tułowia. Jeśli prawa jedzie źle, wyłącz ją: `env HELIOS_GESTY=left ros2 run helios_ai main`.

**Przełącznik:** `HELIOS_GESTY` = `both` (domyślnie), `left`, `right`, `off`.

**Prompt a gesty:** gesty nie włączają się, gdy odpowiedź modelu zawiera tag ruchu. Pierwotny prompt kazał dawać `#wave#` przy powitaniu i pożegnaniu, więc model dopisywał go prawie do każdej odpowiedzi i robot tylko machał. W `MASTER_PROMPT` (`gotowe/main.cpp`) na prośbę użytkownika: `#wave#` tylko przy pożegnaniu, zdjęciu lub wyraźnej prośbie; zwykła rozmowa (powitanie, pytania o firmę, żarty) bez tagu ruchu; dodane dwa przykłady odpowiedzi bez tagu. Reszta promptu bez zmian.

**Diagnostyka (po ruchu):** po pętli gesty logują `pętla zakończona po ...`, `wylosowane pozy (nr 1-9, J7): ...`, a potem faktyczną odchyłkę ręki od startu (czekają do 3 s). Jeśli ręka NIE doszła: `UWAGA: pętla się skończyła, ale ręka stoi X deg od pozycji startowej`, `hasMotionError`, `stan: zasilanie=... operationState=... operateMode=...` i jointy z odchyłkami. Przy błędzie startu pętli (`Failed start move timeout`) log dopisuje to samo, a kroki przygotowania logują własne błędy. `posStart` przy `ruch utknął` też loguje `stan: ...`. Obserwacja z testu (hipoteza, nie fakt): lewa ręka zatrzymywała się z odchyłką 37,6–37,82°, a 37,82° to dokładnie J1 pozy 3 (−35,60 − 2,22).

**`/start` po końcu mowy:** gdy gesty się skończą (obie ręce wróciły albo nie weszły w gesty) i mowa się skończyła (`gesty::stop()`), moduł sam woła `commands::posStart()`, czyli to samo co `/start`. Dzięki temu obie ręce na pewno stoją w pozycji startowej (ręka, która była za daleko albo nie doszła, też jest poprawiana). Jeśli ręka jest już w pozycji (≤ 1°), `posStart` pomija ruch. Nie jest wołany, gdy któraś ręka była w ruchu z innej komendy (`wave`, `posZero`...). Log: `[GESTY] koniec mowy - uruchamiam /start (pozycja startowa)` oraz linie `[posStart] ...`. Wymaga `pos_start.cpp|.h` w `commands/`.

**Uwagi i ograniczenia:**
- Stałe `LEWA_START_DEG` / `PRAWA_START_DEG` są zduplikowane w `gesty.cpp`, `wave.cpp` i `pos_start.cpp`: trzymaj je zgodne.
- Gesty działają tylko w ścieżce rozmowy (odpowiedź modelu). Wpis z konsoli z tagiem albo skrót (`/wave`, `/zero`…) gestów nie uruchamia.
- Gdyby polecenie ruchu przyszło dokładnie w czasie, gdy ręce wracają po mowie, może się z nimi zderzyć (oba używają pętli RT); `wave` wtedy najpierw próbuje zatrzymać ruch (§6.4).
- Tułów nie bierze udziału (błąd sprzętowy).
- **Rokae SDK i równoczesne tworzenie robotów:** przy pierwszej wersji obie ręce łączyły się w dwóch wątkach naraz, co kończyło się `spdlog fatal error: logger with name '_rokae_log_console_' already exists` i `SIGABRT` (SDK zakłada logger o stałej nazwie). Tworzenie, łączenie i niszczenie obiektów robota w `gesty.cpp` jest teraz szeregowane mutexem. Przy własnych modułach z kilkoma robotami twórz je po kolei (jak w `wave.cpp`).

---

## 7. Łącze core ↔ GUI (protokół)

**Transport:** TCP `0.0.0.0:5123` (`HELIOS_GUI_PORT`) + gniazdo unix `/tmp/helios.sock` (`HELIOS_GUI_SOCK`, do testów lokalnych). **Format:** JSON-linie (jeden obiekt na linię, zakończony `\n`). Bez zależności zewnętrznych (POSIX + STL). `HELIOS_GUI=0` wyłącza łącze całkowicie.

**GUI → core**

| Wiadomość | Znaczenie |
|---|---|
| `{"type":"chat","text":"…","id":7}` | tekst z czatu; `id` wraca jako `ref` (echo) |
| `{"type":"set_listening","on":false}` | bramka nasłuchu słowa-klucza (przycisk NASŁUCH) |
| `{"type":"ping"}` | odpowiedź `{"type":"pong"}` |

**core → GUI**

| Wiadomość | Znaczenie |
|---|---|
| `{"type":"hello","version":1,"name":"helios-core"}` | powitanie po połączeniu |
| `{"type":"heard","text":"…","source":"voice\|gui\|keyboard","ref":7}` | co usłyszał / dostał core |
| `{"type":"reply","text":"…","ref":7}` | odpowiedź Heliosa (też raport `stats`, `ref=0`) |
| `{"type":"state","state":"listening\|muted\|capturing\|verifying\|thinking\|speaking",…}` | stan; wysyłany tylko przy zmianie |
| `{"type":"log","text":"…"}` | kopia linii z konsoli (bez szumu statystyk) |
| `{"type":"error","code":"…"}` | błąd protokołu (zły JSON, pusty tekst) albo `busy` |

**Zasady działania:**

- **Jeden klient naraz.** Nadmiarowy dostaje `{"type":"error","code":"busy"}` i połączenie jest zamykane.
- **Keepalive:** gniazdo klienta ma `SO_KEEPALIVE` (`TCP_KEEPIDLE` 10 s, `TCP_KEEPINTVL` 5 s, `TCP_KEEPCNT` 3). Martwy klient (zabita aplikacja, zerwane Wi-Fi bez FIN) zwalnia slot po ≈ 25 s. Bez tego „zombie” blokował łącze w nieskończoność – objaw: `[BLAD] busy` w teście i `LINK GUI: BRAK` w aplikacji.
- **Zasada operacyjna:** w danej chwili łączy się **albo aplikacja Qt, albo skrypt testowy** – nigdy oba.
- Kolejka wiadomości do GUI (`guiOutbox`) ma limit 200 linii; nadmiar wypycha najstarsze.
- Wiadomości `chat` z `/komendą` lub tagiem `#…#` wykonują się **bez LLM** (por. §5.11). `exit`/`quit` z GUI idzie do LLM jako zwykły tekst – GUI nie może ubić core'a.
- Czat z GUI: odpowiedź robot **zawsze wypowiada na głos** (ta sama ścieżka co klawiatura). Trybu cichego nie ma.

**Test bez Qt** (na robocie lub z laptopa): `python3 test_gui_link.py [adres]` – zwykły tekst = czat, `/off` i `/on` = nasłuch, `/quit` = wyjście.

---

## 8. Aplikacja Qt na laptopie

Projekt `TrackingRobot` (moduł QML `TrackingRobot`, plik wykonywalny `appTrackingRobot`). Wymagania: **Qt 6.5+** (moduły Quick, QuickControls2, WebSockets, Network), **OpenCV** (domyślna ścieżka w CMake: `C:/msys64/mingw64/lib/cmake/opencv4`, zmiana `-DOpenCV_DIR=…`), CMake ≥ 3.16, C++17. Na Windows potrzebny jest też klient **OpenSSH** (dla SshCore).

### 8.1 Klasy C++ wystawione do QML

| Obiekt w QML | Klasa | Rola |
|---|---|---|
| `heliosLink` | `HeliosLink` | klient łącza core'a (§7); sam łączy i ponawia co 2,5 s; `sendChat()`, `setListening()` |
| `sshCore` | `SshCore` | `startCore()`, `stopCore()`, `refreshStatus()` (co 15 s), `setLogStreaming()`, sterowanie zdalne bazą (`remoteToggle()`, `remoteOn()`, `driveStart/Key/End`, `drivePress/Release`, `driveStop`; przez SSH `baza.sh wasd`), `stopCoreOnExit()` **tylko w starszej wersji, patrz §8.8**; woła systemowe `ssh` z `BatchMode=yes`, `ConnectTimeout=8`, `StrictHostKeyChecking=accept-new` |
| `robotClient` | `RobotClient` | WebSocket do rosbridge (`ws://IP:9090`); publikacja komend, subskrypcja `/joint_states` (max 5 Hz), `emergencyStop()` |
| `videoReceiver` | `VideoReceiver` + `VideoWorker` | wątek roboczy: strumień wideo (OpenCV) → detekcja pozy → filtry → kąty |
| `jointRecorder` | `JointRecorder` | nagrywanie stawów robota do CSV |
| `VideoItem` (`cameraDisplay`) | `VideoItem` | rysowanie klatek w QML |

### 8.2 Układ okna

- **Start:** czat na cały ekran (kamera wyłączona, żeby aplikacja nic nie mieliła w tle).
- **Komenda śledzenia w czacie** (tekst zawiera `śledz`, `track`, `kamer`, `camer`): włącza kamerę – układ przechodzi na **kamerę + panel boczny z czatem po prawej**; startuje strumień i połączenie z robotem. Komenda stopu (`stop`, `przestań`, `koniec`, `zatrzymaj`) wraca do czatu pełnoekranowego. Tekst komendy trafia również do core'a.
- Panel boczny: wiersze statusów **LINK STATUS**, **LINK GUI**, **NASŁUCH**, **NAGRYWANIE**, sekcja **KINEMATYKA OPERATORA** (telemetria kątów człowieka), przyciski **WYCZYŚĆ CZAT**, **OTWÓRZ NAGRANIE**, karta **CORE** (kropka stanu + **START / STOP / LOG**), **POŁĄCZ** i **EMERGENCY STOP**.
- Wygląd: jasny motyw wzorowany na Robot Assistant (białe panele, jasnoszare tło, czerwony akcent Rokae `#e60012`, zielony `#16a34a`, ciemne przyciski `#1c1e21`).
- Okno **LOG** (`LogWindow.qml`) pokazuje log core'a na żywo przez SSH.

### 8.3 Komendy lokalne w czacie (nie idą do core'a)

| Komenda | Efekt |
|---|---|
| `nagrywaj` · `nagrywanie` | start nagrywania stawów, próbka co 1 s |
| `nagrywaj co 0.5` · `nagrywaj co 2 sekundy` | start z interwałem 0,2–60 s |
| `stop nagrywania` · `przestań nagrywać` · `koniec nagrywania` · `nie nagrywaj` | stop (nie rusza śledzenia ani czatu) |

Sprawdzanie „stop nagrywania” odbywa się **przed** zwykłym stopem, żeby nie wyłączało śledzenia. Chip „Nagrywaj” pod czatem to ten sam efekt bez pisania.

**Chipsy (szybkie komendy pod czatem):** `Pozycja zerowa`, `Pomachaj`, `Statystyki`. Kliknięcie wysyła do core'a tekst „Wróć do pozycji zerowej”, „Pomachaj” albo „Statystyki” jak zwykłą wiadomość, czyli **przez LLM** (potrzebny internet i klucz API). Wersja bez LLM to wpisanie w konsoli core'a `/zero`, `/wave` albo `/stats`. Śledzenie i nagrywanie nadal działają po wpisaniu w czacie („zacznij śledzenie”, „nagrywaj”, „stop”, „stop nagrywania”) – tylko bez chipów. Lista chipów jest w `ChatPanel.qml` (`Repeater` pod komentarzem „szybkie komendy”).

### 8.4 Nagrywarka stawów robota

- Źródło: **rzeczywiste stawy robota (ramiona + tułów)** z `/joint_states` (rosbridge, ograniczone do 5 Hz). Nazwy kolumn pochodzą z pierwszej odebranej wiadomości.
- Plik: `Dokumenty/HeliosNagrania/nagranie_RRRRMMDD_GGMMSS.csv` (gdy brak katalogu Dokumenty – katalog tymczasowy). Separator `;` (otwiera się wprost w Excelu), kolumny `timestamp_iso;timestamp_ms;<stawy w radianach…>`.
- Wymaga połączenia z robotem. Jeśli robot nie publikuje `/joint_states`, nagrywanie **czeka na dane** (wiersz statusu `CZEKAM NA DANE…`); sprawdzenie na robocie: `ros2 topic hz /joint_states`.
- Statusy: `● REC n` → `PLIK GOTOWY`. **OTWÓRZ NAGRANIE** otwiera plik w domyślnym programie. Utrata połączenia w trakcie nagrywania daje komunikat w czacie (próbki czekają na dane).
- **Nie** nagrywa kątów człowieka z telemetrii – to osobna funkcja, której świadomie nie używamy do zapisu.

### 8.5 Śledzenie operatora (kamera → robot)

1. `VideoWorker::process()` otwiera strumień (`cv::VideoCapture`) – domyślnie `http://192.168.49.97:8080/stream?topic=/camera/color/image_raw` (strumień z robota); `"0"` oznacza kamerę laptopa.
2. `PoseDetector` wczytuje model ONNX przez OpenCV DNN (CPU). Domyślnie `yolov8n-pose.onnx`; jeśli nazwa pliku zawiera `yolo`, wejście to 640×640, w innym wypadku 256×256 (rozmiar jest też odczytywany z sieci). Wyznacza kąty prawej ręki: bark (yaw) 0–80°, bark (pitch) 0–160°, łokieć 0–130°.
3. Kąty przechodzą przez **filtry One-Euro** (`yaw 0.7/0.004`, `bark 1.0/0.007`, `łokieć 1.2/0.008` – parametry min_cutoff/beta) usuwające drżenie bez lagu.
4. `RobotClient::sendJointAngles()` dławi ruch do **25 Hz** (co 40 ms), obcina wartości do bezpiecznych zakresów (**yaw 0–85°, pitch 0–160°, łokieć 0–130°**), zaokrągla do 0,1° i publikuje `std_msgs/Float64MultiArray` na `/teleop/joint_commands` jako `data[0..2]` (+ dwa zera).
5. Alternatywa: tryb `cartesian_ik` publikuje `geometry_msgs/Point` na `/teleop/cartesian_commands` (przesunięcie nadgarstka).

> Śledzenie przekazuje kąty **tylko prawej ręki**. To, co z tym robi strona robota (subskrybent `/teleop/joint_commands`), **nie jest w tym workspace** – **[założenie]**: odbiera je osobny węzeł ROS 2 pakietu Rokae.

### 8.6 Konfiguracja aplikacji

Wartości domyślne są w `appconfig.h` (IP robota, port rosbridge 9090, tematy, źródło kamery, progi detekcji `conf 0.45` / `kpt 0.50`, filtr, tryb sterowania `joint_angles`). Adres core'a/łącza GUI i SSH można nadpisać zmiennymi `HELIOS_*` (§10).

> **Uwaga:** `AppConfig::load()` (odczyt `config.json`) **istnieje, ale nigdzie nie jest wywoływana** – plik `config.json` obecnie nie ma żadnego wpływu. Dodatkowo adres `192.168.49.97` jest wpisany na stałe w `Main.qml` (POŁĄCZ i start strumienia). Patrz §15.

### 8.7 EMERGENCY STOP w aplikacji – co naprawdę robi

Przycisk wywołuje `RobotClient::emergencyStop()`, który **publikuje zera** (`[0,0,0,0,0]`) na `/teleop/joint_commands`. To **komenda programowa przez ROS, nie awaryjne odcięcie zasilania**. Prawdziwym zatrzymaniem awaryjnym jest **fizyczny E-STOP robota**, który musi być zawsze w zasięgu ręki (§14).


### 8.8 Zamknięcie aplikacji zatrzymuje core

> **Stan wdrożenia:** ta funkcja NIE jest wdrożona w aktualnym projekcie Qt (`TrackingRobotV4`). Jego `sshcore.h` nie deklaruje `stopCoreOnExit()`; core zatrzymuje się wyłącznie przyciskiem STOP. Poniższy opis dotyczy starszej wersji kodu z `uploads/qt/`. Pliki Qt z tej starszej linii **nie nadają się do podmiany** w aktualnym projekcie (brakuje w nich funkcji `drive*` i `remoteOn`).

Przy zamknięciu aplikacji Qt (sygnał `aboutToQuit`) wywoływane jest `SshCore::stopCoreOnExit()`, które przez SSH wykonuje `helios_core.sh stop` – dokładnie to samo, co przycisk **STOP**.

| Aspekt | Zachowanie |
|---|---|
| Wywołanie | synchroniczne: po `aboutToQuit` nie ma już pętli zdarzeń, więc asynchroniczny `QProcess` by się nie wykonał. Okno jest wtedy zamknięte, a proces apki dogasa do 20 s (ConnectTimeout 8 s + sam stop) |
| Przed STOP | zatrzymuje cykliczny status i strumień logu (osobny proces `ssh`) |
| Komenda SSH w locie | jeśli w chwili zamknięcia trwa inna komenda (np. START), apka czeka na nią do 10 s, żeby STOP nie zderzył się ze startującym core'em; potem ją ubija |
| Wynik | do konsoli/`qDebug`: `core zatrzymany: "STOPPED"` albo `STOP nie powiódł się…` / `nie znaleziono ssh…` / `STOP nie odpowiedział w 20 s` |
| Wielokrotne wywołanie | bezpieczne – wykonuje się raz |
| Opt-out | `HELIOS_STOP_ON_EXIT=0` (zmienna po stronie laptopa, bez przebudowy) |

**Konsekwencje, o których trzeba pamiętać:**
- `helios_core.sh stop` zabija też usługę systemd, więc po zamknięciu aplikacji **robot przestaje reagować na „Helios”** aż do kliknięcia START (w aplikacji) albo restartu robota. Jeśli robot ma słuchać także bez laptopa, ustaw `HELIOS_STOP_ON_EXIT=0`.
- Zamknięcie przez ubicie procesu (Menedżer zadań, awaria, utrata zasilania laptopa) **nie wywoła STOP** – `aboutToQuit` się wtedy nie wykonuje.
- Gdy robot jest nieosiągalny, zamknięcie aplikacji może potrwać do ok. 8 s (timeout SSH), a core i tak zostaje uruchomiony.
- Zasada „jeden klient naraz” działa na korzyść: po zamknięciu aplikacji slot łącza GUI zwalnia się razem z zatrzymaniem core'a.

---

## 9. Zarządzanie core'em: skrypt, systemd, tmux, klucze

### 9.1 `helios_core.sh`

Instalacja: `~/bin/helios_core.sh` (+x), uruchamiany **jako użytkownik `std`** (jako root skrypt odmawia: `FAILED (nie jako root!…)`).

| Polecenie | Działanie |
|---|---|
| `start` | sprawdza tmux, `setup.bash`, binarkę; gasi usługę systemd i starą sesję tmux; dobija osierocone procesy; startuje core w sesji tmux `helios` z logiem do `~/.helios_core.log`; czeka na proces i na **port GUI** |
| `stop` | zabija sesję tmux, usługę i resztki procesów; `STOPPED` albo `STOPPED? (coś jeszcze żyje…)` |
| `status` | pierwsze słowo dla GUI: `running tmux pid=…` / `running systemd` / `running pid=… (spoza tmux/systemd!)` / `starting …` / `stopped` |
| `log` | `tail -n 200 -F` logu (dla GUI) |
| `run` | core na pierwszym planie (tylko dla systemd; log do journald) |

**Odpowiedź `start`** (czytana przez GUI):

```
STARTED pid=<PID> tmux:helios log:/home/std/.helios_core.log gui=OK(:5123) keys=TAK
```

| Fragment | Znaczenie |
|---|---|
| `gui=OK(:5123)` | core wstał **i** port łącza GUI przyjmuje połączenia |
| `gui=BRAK (…)` | core stoi, ale nic nie słucha – binarka zbudowana bez haczyków GUI |
| `gui=wylaczone(HELIOS_GUI=0)` | łącze świadomie wyłączone |
| `keys=TAK` / `keys=BRAK` | czy znaleziono plik z kluczami (patrz niżej) |
| `FAILED (…)` | powód w nawiasie (brak tmux, brak workspace, brak binarki, core nie wstał + ogon logu) |

**Zasada:** działa **dokładnie jeden core**. `start` zawsze najpierw gasi usługę i starą sesję, żeby dwa core'y nie walczyły o mikrofon.

### 9.2 Kolejność wczytywania środowiska

1. `~/.helios_env` (ścieżki i nadpisania `HELIOS_*`, np. `HELIOS_WS_SETUP`, `HELIOS_MAIN_BIN`, `HELIOS_KEYS_FILE`, `HELIOS_GUI_PORT`),
2. `~/rokae_ws/keys.zsh` (**klucze API**; domyślna ścieżka `KEYS_FILE`, zmiana `HELIOS_KEYS_FILE`),
3. `setup.bash` workspace'u ROS 2 (celowo `.bash`, bo skrypt odpala core przez `bash -c`, niezależnie od powłoki logowania).

`keys.zsh` ma składnię zsh, ale zwykłe `export KLUCZ=wartość` łyka też bash (to, co się nie da, jest pomijane). Brak pliku = **ostrzeżenie** (`UWAGA: brak … – core wstanie bez kluczy API`) w logu i na stderr oraz `keys=BRAK`; start **kontynuuje** w trybie bez LLM (projektowo; twarde `FAILED` możliwe na życzenie). Sekrety trzymaj w plikach z `chmod 600`.

Klucze, których core używa: `OPENROUTER_API_KEY` (główny), `YOUTUBE_API_KEY` (komenda `youtube`).

### 9.3 Usługa systemd (`helios.service`)

Usługa **użytkownika** (`~/.config/systemd/user/`), `ExecStart=/home/std/bin/helios_core.sh run`, `Restart=on-failure`, `RestartSec=10`, limit restartów 5 w 300 s (żeby nie kręcić się w kółko przy stałym błędzie, np. brak modelu). Start po boocie wymaga `sudo loginctl enable-linger $USER`.

Komendy: `systemctl --user status helios`, `journalctl --user -u helios -f`, `systemctl --user stop helios`.

**Przepływ:** po restarcie robota core wstaje sam z systemd, GUI tylko się łączy. Kliknięcie **START** w GUI zatrzymuje usługę i przejmuje core do tmux (można się podpiąć: `ssh robot`, `tmux attach -t helios`). **STOP** gasi wszystko do następnego boota albo kolejnego START.

---

## 10. Konfiguracja: wszystkie zmienne środowiskowe

Zmienne ustawiasz w tej samej linii co uruchomienie: `env ZMIENNA=wartość ros2 run helios_ai main`. Domyślne wartości pochodzą z kodu.

### 10.1 Audio i słowo-klucz

| Zmienna | Domyślnie | Znaczenie |
|---|---|---|
| `HELIOS_AUDIO_DEVICE` | `plughw:CARD=REF,DEV=0` | mikrofon (nie wpływa na głos Heliosa) |
| `HELIOS_ALSA_PERIOD` / `HELIOS_ALSA_BUFSIZE` | puste (auto) | parametry ALSA; mały bufor powodował „overrun” |
| `HELIOS_AGC` | włączone | `0` wyłącza automatyczne wzmocnienie |
| `HELIOS_AGC_MAX` | `10` | maksymalne wzmocnienie |
| `HELIOS_WAKE_MODEL` / `HELIOS_MEL_MODEL` / `HELIOS_EMBEDDING_MODEL` | `/home/std/helios/models/*.onnx` | modele detektora |
| `HELIOS_WAKE_THRESHOLD` | `0.25` | czułość (niżej = łatwiej, więcej fałszywych pobudek) |
| `HELIOS_WAKE_PATIENCE` | `2` | ile ramek ponad progiem (przy `WINDOW=0` – kolejnych) |
| `HELIOS_WAKE_WINDOW` | `4` | okno w ramkach (80 ms): wystarczy `patience` ramek ponad progiem w ostatnich `window` ramkach; 0 = kolejne ramki (jak dawniej) |
| `HELIOS_WAKE_LOG_MIN` | `0.15` | od jakiego score wypisywać `[Wake] score … - nie przyjęte` (max 2/s); 0 = nie wypisuj |
| `HELIOS_WAKE_DEBOUNCE` | `1.2` | minimalny odstęp detekcji (s) |
| `HELIOS_WAKE_THREADS` | `2` | wątki ONNX (przy ostrzeżeniu o wolnym detektorze ustaw `1`) |
| `HELIOS_WAKE_VERIFY` | włączone | `off` wyłącza, `strict` odrzuca przy błędzie STT |
| `HELIOS_WAKE_FAST_SCORE` | `0.90` | od tej pewności bez weryfikacji (`1.1` wyłącza ścieżkę) |
| `HELIOS_NO_WAKEWORD` | brak | niepusta wartość = tryb ręczny (`/listen`) |
| `HELIOS_CAPTURE_ARM` | `4.0` | sekundy na polecenie po „Helios” |
| `HELIOS_FOLLOW_UP` | `6.0` | sekundy na kolejne zdanie bez słowa-klucza |
| `HELIOS_PREROLL` | `0.6` | audio sprzed startu mowy |
| `HELIOS_CHIME` | włączone | `0` wyłącza dzwonek |

### 10.2 Rozpoznawanie mowy

| Zmienna | Domyślnie | Znaczenie |
|---|---|---|
| `HELIOS_STT_BACKEND` | `auto` | `auto`, `local`, `server` |
| `HELIOS_STT_SERVER_URL` | brak | adres whisper-server |
| `HELIOS_STT_FALLBACK_LOCAL` | włączone | zejście na lokalny przy awarii serwera |
| `HELIOS_STT_WHISPER_CLI` | `/home/std/whisper.cpp/build/bin/whisper-cli` | program |
| `HELIOS_STT_MODEL` | `/home/std/models/ggml-large-v3.bin` | model |
| `HELIOS_STT_VAD_MODEL` | `/home/std/models/ggml-silero-v6.2.0.bin` | VAD; nieistniejący plik = bez VAD |
| `HELIOS_STT_THREADS` | `6` | wątki whisper |
| `HELIOS_STT_PROMPT` | wbudowany | pusty (`HELIOS_STT_PROMPT=`) wyłącza |
| `HELIOS_STT_FIX` | `1` | `0` wyłącza korektę słów w komendach |
| `HELIOS_STT_SLOWNIK` | – | ścieżka słownika użytkownika |
| `HELIOS_STT_SILENCE` | `1.2` | cisza kończąca wypowiedź (s) |
| `HELIOS_STT_PREROLL` | `0.6` | pre-roll kolektora |
| `HELIOS_STT_MIN_SPEECH` / `HELIOS_STT_MAX_UTTERANCE` | `0.25` / `15` | min. i maks. długość (s) |
| `HELIOS_STT_NOISE_MARGIN_DB` | `8` | próg mowy ponad podłogą szumu |
| `HELIOS_STT_WORK_DIR` | `/tmp` | katalog plików roboczych |
| `HELIOS_STT_QUIET` | `0` | `1` wycisza stderr `whisper-cli` |
| `HELIOS_STT_MIN_PEAK_DB` | `-42` | bramka poziomu: cichsze nagranie nie idzie do Whispera (`-120` wyłącza) |
| `HELIOS_STT_ANTYHALUCYNACJE` | `1` | `0` wyłącza flagi `-mc 0 -nf --suppress-nst` (i `temperature_inc=0` dla serwera) |
| `HELIOS_STT_EXTRA_ARGS` | brak | dodatkowe argumenty `whisper-cli`, np. `-nth 0.7` |
| `HELIOS_STT_HALUCYNACJE` | brak | plik z własnymi frazami-halucynacjami (też `~/.helios_halucynacje.txt`) |
| `HELIOS_STT_HALUCYNACJE_FILTR` | `1` | `0` wyłącza filtr fraz |

### 10.3 Model językowy

| Zmienna | Domyślnie | Znaczenie |
|---|---|---|
| `HELIOS_LLM_MODEL` | `nvidia/nemotron-3-ultra-550b-a55b:free` | model główny (OpenRouter) |
| `HELIOS_LLM_URL` / `HELIOS_LLM_KEY_ENV` | puste | własny dostawca (puste = OpenRouter) |
| `HELIOS_LLM_MAX_TOKENS` | `1024` | limit odpowiedzi |
| `HELIOS_LLM_CONNECT_TIMEOUT` | `8` | s na nawiązanie połączenia z serwerem LLM (DNS+TCP+TLS) |
| `HELIOS_LLM_TIMEOUT` | `40` | s na odpowiedź modelu |
| `HELIOS_LLM_REASONING` | (domyślny tryb modelu) | `0` = od pierwszej próby wyłączone „myślenie” modelu (szybsze odpowiedzi modeli reasoning) |
| `OPENROUTER_API_KEY` | – | klucz |

### 10.4 Odgłosy, historia, komendy

| Zmienna | Domyślnie | Znaczenie |
|---|---|---|
| `HELIOS_WAITING` | włączone | `0` wyłącza muzyczkę |
| `HELIOS_WAITING_STYLE` | `elevator` | `elevator` albo `queue` |
| `HELIOS_WAITING_FILE` | `/home/std/rokae_ws/muzyczka.wav` | własny WAV |
| `HELIOS_WAITING_VOLUME` | `50` | % głośności własnego pliku |
| `HELIOS_HISTORY_DIR` | `historia` | katalog rozmów |
| `HELIOS_PRZEJMIJ` | włączone | `0` wyłącza przejmowanie starej sesji |
| `HELIOS_DRAG_BEZ_GUZIKA` | brak | `1` = drag bez trzymania przycisku na końcówce |
| `HELIOS_DRAG_KROKI` | `1` | `0` wyłącza wpisy `[dragOn] … - krok: <funkcja SDK>` (pokazują, na której funkcji SDK program padł) |
| `HELIOS_DRAG_PRZYGOTOWANIE` | brak | `1` = przed dragiem wykonaj `setMotionControlMode(NrtCommand)` i `clearServoAlarm`. Domyślnie POMIJANE – przy nich SDK wywalało program (`terminate called without an active exception`) tuż po „połączono” |
| `HELIOS_WAVE_POWTORZENIA` | `3` | liczba machnięć prawą ręką w `wave` (1–10) |
| `HELIOS_WAVE_CZAS_RUCHU` | `4.0` | czas jednego przejścia A→B w `wave`, w sekundach (0.5–10; mniej = szybciej) |
| `HELIOS_WAVE_A`, `HELIOS_WAVE_B` | z kodu | punkty A i B machania prawą ręką bez przebudowy: 7 liczb w stopniach po przecinku, np. `"-171,-5,-22,28.92,-5.5,3.5,5"` |
| `HELIOS_WAVE_RT_TOLERANCJA` | `60` | tolerancja opóźnień pakietów RT w `wave` (0–100, `setRtNetworkTolerance` wywoływane przed trybem RtCommand); za mała = kontroler zatrzymuje ruch w losowym miejscu, gdy Jetson jest zajęty |
| `HELIOS_WAVE_NRT_PREDKOSC` | `100` | prędkość zapasowego powrotu prawej ręki (MoveAbsJ, tryb NRT), 10–500 |
| `HELIOS_WAVE_NRT_TRYB` | `manual` | tryb pracy zapasowego powrotu NRT; `auto` = automatic (domyślnie jak w `pos_zero.cpp`) |
| `HELIOS_WAVE_DOJAZD_PREDKOSC` | `0.8` | maks. prędkość jointu przy dojeździe do A i powrocie do startu, rad/s (0.2–2.0) |
| `HELIOS_WAVE_DOJAZD_MIN_CZAS` | `2.0` | minimalny czas dojazdu/powrotu w `wave`, s (0.5–10) |
| `YOUTUBE_API_KEY` | – | klucz dla komendy `youtube` |

### 10.5 Łącze GUI, SSH, skrypt

| Zmienna | Domyślnie | Gdzie | Znaczenie |
|---|---|---|---|
| `HELIOS_GUI` | włączone | core | `0` = brak łącza |
| `HELIOS_GUI_PORT` | `5123` | core + Qt | port TCP |
| `HELIOS_GUI_SOCK` | `/tmp/helios.sock` | core | gniazdo unix (puste = wyłączone) |
| `HELIOS_GUI_HOST` | `192.168.49.97` | Qt | adres core'a |
| `HELIOS_SSH_HOST` / `HELIOS_SSH_USER` | `192.168.49.97` / `std` | Qt | cel SSH |
| `HELIOS_SSH_KEY` | domyślne klucze | Qt | `ssh -i` |
| `HELIOS_SSH_SCRIPT` | `/home/std/bin/helios_core.sh` | Qt | skrypt na robocie |
| `HELIOS_SSH_BIN` | auto | Qt | pełna ścieżka do `ssh.exe` |
| `HELIOS_GESTY` | `both` | core | gesty rąk podczas mowy: `both`/`left`/`right`/`off` (§6.6) |
| `HELIOS_WIFI_SCRIPT` | `/home/std/bin/helios_wifi.sh` | core | skrypt wołany przez `/wifi` (§17.1) |
| `HELIOS_AP_SSID` / `HELIOS_AP_IP` / `HELIOS_AP_CON` | `HELIOS-ROBOT` / `192.168.49.97/24` / `helios-ap` | skrypt Wi-Fi | własna sieć robota |
| `HELIOS_WIFI_AP_IFACE` / `HELIOS_WIFI_STA_IFACE` | auto | skrypt Wi-Fi | karta dla AP / dla klienta (domyślnie pierwsza / druga karta Wi-Fi) |
| `HELIOS_WIFI_TIMEOUT` | `30` | skrypt Wi-Fi | s czekania na połączenie |
| `HELIOS_WIFI_JEDNA_KARTA` | `0` | skrypt Wi-Fi | `1` = pozwól zabić AP na jedynej karcie (robot znika z `192.168.49.97`) |
| `HELIOS_STOP_ON_EXIT` | włączone | Qt | `0` = zamknięcie aplikacji **nie** zatrzymuje core'a |
| `HELIOS_WS_SETUP` | `~/rokae_ws/install/setup.bash` | skrypt | workspace ROS 2 |
| `HELIOS_MAIN_BIN` | `~/rokae_ws/install/helios_ai/lib/helios_ai/main` | skrypt | binarka |
| `HELIOS_KEYS_FILE` | `~/rokae_ws/keys.zsh` | skrypt | plik z kluczami |

### 10.6 Ścieżki zależności (domyślne)

| Element | Ścieżka |
|---|---|
| Workspace ROS 2 | `/home/std/rokae_ws` |
| Pakiet | `/home/std/rokae_ws/src/rokae_ros2-main/helios_ai` |
| Rokae SDK | `/home/std/rokae_ws/src/rokae_ros2-main/rokae_hardware/sdk` (`libxCoreSDK.a`, `libxMateModel.a`) |
| whisper-cli | `/home/std/whisper.cpp/build/bin/whisper-cli` |
| Modele STT | `/home/std/models/ggml-large-v3.bin`, `ggml-silero-v6.2.0.bin` |
| Modele słowa-klucza | `/home/std/helios/models/` (`helios.onnx`, `melspectrogram.onnx`, `embedding_model.onnx`) |
| Piper | `/home/std/piper/piper`, `/home/std/piper_models/pl_PL-darkman-medium.onnx` (+ `.json`) |
| ONNX Runtime | `/home/std/onnxruntime` (`-DONNXRUNTIME_ROOT=…`) |

---

## 11. Instalacja i wdrożenie od zera

### 11.1 Wymagania na robocie

ROS 2, `Eigen3`, `orocos_kdl`, Rokae SDK, `libcurl`, `nlohmann-json`, ONNX Runtime (nagłówki + biblioteka), `tmux`, `mpv` (tylko dla `youtube`), `alsa-utils` (`arecord`, `aplay`, `amixer`), zbudowany **whisper.cpp** z modelem `large-v3`, **Piper** z głosem polskim. Kontrolery ramion i tułowia muszą być osiągalne w sieci `192.168.71.x`.

> CMake wspomina skrypt `scripts/pobierz_onnxruntime.sh`, a nagłówek wake-worda `train_wakeword/pobierz_modele.sh` – **żadnego z nich nie ma w workspace**. ONNX Runtime i modele openWakeWord trzeba pobrać ręcznie.

### 11.2 Znalezienie prawdziwego katalogu pakietu

Źródła są pod `~/rokae_ws/src/…/helios_ai`. **Uwaga na pułapkę:** na robocie istnieje zagnieżdżone, śmieciowe drzewo `…/helios_ai/install/helios_ai/share/helios_ai`. Wyszukiwanie musi je pomijać:

```bash
PKGDIR=$(grep -rl "<name>helios_ai</name>" ~/rokae_ws/src --include=package.xml | grep -v "/install/" | head -1 | xargs dirname)
echo "$PKGDIR"                                   # ma być: /home/std/rokae_ws/src/rokae_ros2-main/helios_ai
grep -c handleConsoleLine "$PKGDIR/main.cpp"     # ma dać 3 (to potwierdza właściwy main.cpp)
```

### 11.3 Wgranie kodu core'a i budowa

```bash
cd "$PKGDIR"
cp main.cpp main.cpp.przed_wgraniem
# rozpakuj tu paczki z §4.3 (nadpiszą pliki), potem:
cd ~/rokae_ws
colcon build --packages-select helios_ai
source install/setup.bash
```

**Weryfikacja, że build jest prawdziwy:** build trwający ~15 s i „no-op” (data pliku binarki bez zmian) oznacza, że zbudowano **stare** źródła – prawie zawsze to wynik wgrania do złego katalogu.

### 11.4 Skrypt, usługa, klucze (raz, jako `std`)

```bash
mkdir -p ~/bin ~/.config/systemd/user
cp helios_core.sh ~/bin/ && chmod +x ~/bin/helios_core.sh
cp helios.service ~/.config/systemd/user/
sudo apt install -y tmux
systemctl --user daemon-reload
systemctl --user enable --now helios.service
sudo loginctl enable-linger $USER
chmod 600 ~/rokae_ws/keys.zsh ~/.helios_env      # jeśli istnieją
```

Na laptopie z Windowsem (raz): klucz SSH bez hasła – **automatycznie skryptem `ustaw_ssh.ps1`** (plik jest obok `main.cpp` w paczce do wgrania). W PowerShellu, w katalogu ze skryptem:

```powershell
powershell -ExecutionPolicy Bypass -File .\ustaw_ssh.ps1
```

Skrypt: (1) sprawdza, czy logowanie kluczem już działa – jeśli tak, kończy bez zmian (można go uruchamiać wielokrotnie); (2) tworzy klucz `ed25519` w `%USERPROFILE%\.ssh\id_ed25519` (na pytanie o hasło do klucza naciśnij **Enter dwa razy**, klucz ma być bez hasła); (3) wgrywa klucz na robota – zapyta **jeden raz** o hasło użytkownika `std`; (4) sprawdza to samo logowanie, którego używa aplikacja (`BatchMode=yes`); (5) sprawdza, czy na robocie jest `/home/std/bin/helios_core.sh` z prawem wykonania. Sukces wygląda tak:

```
OK: klucz zainstalowany, logowanie bez hasla dziala.
OK: /home/std/bin/helios_core.sh istnieje i ma prawo wykonania.
Gotowe. Mozesz uruchomic aplikacje Qt.
```

Cel i klucz zmienisz zmiennymi `HELIOS_SSH_HOST` (domyślnie `192.168.49.97`), `HELIOS_SSH_USER` (`std`), `HELIOS_SSH_KEY`, a ścieżkę skryptu na robocie przez `HELIOS_SSH_SCRIPT` – tymi samymi, których używa aplikacja Qt. Wymaga programu `ssh` (Windows 10/11: Ustawienia → Aplikacje → Funkcje opcjonalne → „Klient OpenSSH”).

> Dlaczego skrypt, a nie `ssh-copy-id`: na Windowsie `ssh-copy-id` nie istnieje, a ręczne przekierowanie klucza potokiem z PowerShella dokleja znak `\r` na końcu linii i psuje wpis w `authorized_keys` (skutek: dalej `Permission denied (publickey)`). Skrypt wkłada klucz bezpośrednio do polecenia, bez potoku.

Hasło do robota trzeba wpisać ręcznie jeden raz – tego nie da się obejść, bo aplikacja Qt łączy się z `BatchMode=yes` (nigdy nie pyta o hasło).

### 11.5 Testy odbiorcze (po kolei)

| # | Test | Oczekiwany wynik |
|---|---|---|
| 1 | `~/bin/helios_core.sh start` | `STARTED … gui=OK(:5123) keys=TAK` |
| 2 | log startu (`tail -n 50 ~/.helios_core.log`) | `[start] program wystartowal…`, `[GUI] nasluch TCP na 0.0.0.0:5123`, `Nasłuchuję słowa-klucza "Helios"`, linie `[Audio]` co 5 s |
| 3 | `python3 test_gui_link.py` na robocie, wpisz `cześć` | `[USLYSZANO:gui] …` i `[HELIOS] …` |
| 4 | `/off`, potem `/on` | w logu `[GUI] Nasłuch słowa-klucza: WYŁĄCZONY` / `WŁĄCZONY` |
| 5 | powiedz „Helios” → dzwonek → „pomachaj” | robot macha (**wolna przestrzeń, E-STOP w ręce**) |
| 6 | aplikacja Qt (zamknij najpierw skrypt testowy) | `LINK GUI: POŁĄCZONY`, w czacie „Połączono z corem Heliosa.” |
| 7 | `/stats` w czacie Qt | po kilku sekundach dymek Heliosa z raportem |
| 8 | `zacznij śledzenie` → `stop` | kamera + panel boczny → powrót do czatu |
| 9 | `nagrywaj` … `stop nagrywania` → OTWÓRZ NAGRANIE | CSV ze stawami robota otwiera się w Excelu |

---

## 12. Obsługa na co dzień

**Start dnia:** włącz robota → core wstaje sam (systemd). Uruchom aplikację Qt – kropka CORE zielona, `LINK GUI: POŁĄCZONY`. Gdy core stoi: **START** w karcie CORE.

**Rozmowa:** „Helios” → dzwonek → polecenie (4 s) → odpowiedź → przez 6 s kolejne zdanie bez słowa-klucza. Z czatu GUI pisz wprost; skróty `/wave`, `/stats`, `/zero`, `/drag left`, `/dragoff`.

**Nasłuch:** przycisk NASŁUCH wycisza słowo-klucza (robot „głuchnie”), użyteczne w tłumie. Stan widać w wierszu NASŁUCH.

**Koniec dnia:** zamknij aplikację Qt – **core zatrzyma się automatycznie** (§8.8). Ręcznie: **STOP** w karcie CORE. Oba warianty zabijają też usługę – core nie wstanie sam do następnego boota albo START.

**Zasada jednego klienta:** nie uruchamiaj naraz aplikacji Qt i skryptu testowego.

**Bezpieczeństwo przy ruchu:** `wave`, `dragOn`, `posZero` ruszają ramionami. Przed komendą upewnij się, że wokół robota jest wolna przestrzeń.

---

## 13. Diagnostyka i naprawa usterek

### 13.1 Kluczowe linie w logu

| Linia | Znaczenie |
|---|---|
| `[start] program wystartowal (handler awarii aktywny)` | proces wystartował (jej brak = awaria przed `main`) |
| `[GUI] nasluch TCP na 0.0.0.0:5123` | łącze GUI działa |
| `[GUI] UWAGA: nie udalo sie otworzyc…` | port zajęty albo brak uprawnień |
| `[GUI] klient polaczony (tcp)` | aplikacja podłączona |
| `[GUI] czat (id=N): …` | core dostał tekst z czatu |
| `[Audio] ramek/s: … max score: …` | zdrowie toru audio (≈12,5 ramek/s) |
| `[Słowo-klucz] POTWIERDZONY / odrzucony (…)` | wynik weryfikacji pobudki |
| `[Czas] rozpoznawanie: N ms (silnik)` | czas STT i użyty silnik |
| `Ty: …` / `Helios: …` | rozpoznany tekst / odpowiedź |
| `[Uruchamiam komendę: …]` | start komendy robota |
| `[Błąd komendy …: network: …]` | brak sieci do kontrolera |
| `[LLM] brak zmiennej … – pomijam …` | brak klucza API dostawcy |
| `=== AWARIA: … ===` | crash; skopiuj zrzut stosu |

### 13.2 Najczęstsze objawy

| Objaw | Co zrobić |
|---|---|
| `Mikrofon zajęty` | `pgrep -af arecord`, `pkill -f arecord`, start od nowa |
| Stara sesja nadal działa | `pgrep -af helios_ai`, `pkill -f helios_ai` |
| `[Błąd komendy dragOn: network: …]` | kontroler niedostępny: zasilanie, kabel, `ping` na jego adres, `ip -br addr` |
| `[Błąd LLM: brak klucza API…]` | sprawdź `keys.zsh` / `keys=TAK`; zmienna musi być w środowisku core'a |
| `brak pliku modelu` | w `/home/std/whisper.cpp`: `sh ./models/download-ggml-model.sh large-v3`, skopiuj do `/home/std/models/` |
| Robot nie reaguje na „Helios” | w logu `odrzucony` → test `HELIOS_WAKE_VERIFY=off`; bez takiej linii → sprawdź `max score`, mów bliżej, `HELIOS_AGC_MAX=20` |
| Ucięty początek zdania | `HELIOS_PREROLL=1.0 HELIOS_STT_PREROLL=1.0` |
| Dziwne rozpoznawanie | testy: pusty `HELIOS_STT_PROMPT=`, `HELIOS_STT_FIX=0`, `HELIOS_STT_VAD_MODEL=/nieistniejacy/plik`; odsłuch `historia/ostatnia_wypowiedz.wav` |
| `LINK GUI: BRAK` mimo `[GUI] nasluch…` / test pisze `[BLAD] busy` | slot zajęty przez „zombie”: zamknij aplikację, `stop` + `start` core (po wgraniu keepalive zwalnia się sam w ≈25 s) |
| `LINK GUI: BRAK`, brak `[GUI] nasluch` | port zajęty: `ss -tlnp \| grep 5123`, `pkill -f helios_ai`, start. Firewall: `sudo ufw allow 5123/tcp`; test z laptopa: `Test-NetConnection 192.168.49.97 -Port 5123` |
| Karta CORE: UNKNOWN + `SSH: …` | `Permission denied (publickey)` = laptop nie ma klucza znanego robotowi: uruchom `ustaw_ssh.ps1` (§11.4); jeśli skrypt zgłasza, że klucz wgrany, a logowanie nie działa: na robocie `chmod 700 ~/.ssh; chmod 600 ~/.ssh/authorized_keys`, katalog domowy niezapisywalny dla grupy. `Permission denied` przy samym skrypcie = `chmod +x ~/bin/helios_core.sh`. `Connection timed out` = zły adres/robot poza siecią |
| `SSH: Nie znaleziono programu ssh…` | doinstaluj OpenSSH Client (PowerShell admin: `Add-WindowsCapability -Online -Name OpenSSH.Client~~~~0.0.1.0`) albo `HELIOS_SSH_BIN` |
| `START: FAILED (brak tmux…)` | `sudo apt install -y tmux` |
| `START: FAILED (nie jako root!…)` | wykonaj jako `std` (`su - std`) |
| `START: FAILED (brak …setup.bash…)` / `(brak binarki…)` | zły workspace albo niezbudowany core: `colcon build --packages-select helios_ai` lub `HELIOS_WS_SETUP`/`HELIOS_MAIN_BIN` w `~/.helios_env` |
| `START: STARTED … gui=BRAK` | binarka bez łącza GUI: `strings` z binarki ma zawierać `nasluch TCP`; `grep -c gui_link $PKGDIR/main.cpp` > 0; dograj pliki, zbuduj, restart |
| `START: STARTED … keys=BRAK` | brak `keys.zsh` pod `KEYS_FILE` – core działa bez LLM |
| Core nie wstaje po boocie | `loginctl show-user $USER` ma mieć `Linger=yes`; `systemctl --user status helios` |
| Błąd kompilacji Qt o `QTcpSocket` | brak modułu Network w `find_package` / `Qt6::Network` w linkowaniu |
| Błąd kompilacji core'a o `gui_link` | brak `gui_link.cpp` albo stary `CMakeLists.txt` |
| Build trwa 15 s i nic nie zmienia | zła ścieżka źródeł (patrz §11.2) |
| Nagrywanie „czeka na dane” | robot nie publikuje `/joint_states`: `ros2 topic hz /joint_states`; sprawdź POŁĄCZ |
| Stats nie pojawia się w czacie | GUI musi być podłączone w chwili komendy; w logu core'a `[Uruchamiam komendę: stats]`; upewnij się, że wgrano `helios_stats_chat.zip` |

---

## 14. Bezpieczeństwo

1. **Fizyczny E-STOP jest jedynym prawdziwym zatrzymaniem awaryjnym.** Przycisk EMERGENCY STOP w aplikacji tylko publikuje zera na temat ROS (§8.7).
2. **Łącze GUI nie ma uwierzytelniania.** Port `5123` słucha na `0.0.0.0`. **Każdy, kto widzi robota w sieci, może wysłać `{"type":"chat","text":"#dragOn:both#"}` albo `#posZero:all#`** – tagi z czatu wykonują się wprost, bez LLM. Na targach/publicznym Wi-Fi rozważ: osobną sieć dla robota, regułę zapory ograniczającą port 5123 do adresu laptopa (`sudo ufw allow from <IP_laptopa> to any port 5123 proto tcp`), albo `HELIOS_GUI=0`, gdy GUI nie jest potrzebne.
3. **LLM może wywołać dowolny tag z rejestru.** Komendy ruchu (`wave`, `dragOn`, `posZero`, `youtube`) wykonują się bez potwierdzenia. Lista komend w prompcie ogranicza to, co model „wie”, ale nie stanowi bariery technicznej.
4. **Drag** domyślnie wymaga trzymania przycisku na końcówce – nie zmieniaj (`HELIOS_DRAG_BEZ_GUZIKA`) na stałe.
5. **Klucze API** trzymaj w `keys.zsh`/`.helios_env` z `chmod 600`; nie wklejaj ich do logów ani czatu. Log core'a (`~/.helios_core.log`) i historia rozmów (`historia/`) mogą zawierać treść rozmów z ludźmi – uwzględnij to przy wystawach publicznych.
6. **SSH:** aplikacja używa klucza bez hasła i `StrictHostKeyChecking=accept-new`. Klucz prywatny laptopa daje dostęp do robota jako `std` – chroń laptop.
7. Na robocie **nie uruchamiaj core'a jako root** (skrypt tego odmawia).

---

## 15. Znane problemy i dług techniczny

| # | Problem | Skutek / uwagi |
|---|---|---|
| 1 | `MASTER_PROMPT` w kodzie to wciąż stara wersja | patrz §2.2, pozycja 2 |
| 2 | Stary prompt wymienia komendy `arms_moves`, `wave_big`, `full_move`, których **nie ma w rejestrze** | model może je „zawołać”, nic się nie stanie (`[Nieznana komenda]`) |
| 3 | Adresy `192.168.71.x` skopiowane w 6 plikach komend | zmiana sieci = edycja każdego pliku; lepiej wydzielić wspólny nagłówek konfiguracji |
| 4 | `AppConfig::load()` nigdy nie jest wołane | `config.json` nie działa; domyślne w `appconfig.h`; IP także wpisane w `Main.qml` |
| 5 | `helios_tts.cpp`: ścieżki i karta audio na stałe | `HELIOS_AUDIO_DEVICE` nie wpływa na głos; zmiana karty wymaga edycji kodu i przebudowy |
| 6 | `commands/pos_zero.cpp` i `volume.cpp` mają końcówki linii **CRLF** i **nie są w UTF-8** (polskie znaki w komentarzach/logach psują się) | działa, ale logi `[Głośność]` mogą pokazywać krzaki; do konwersji na UTF-8/LF |
| 7 | `package.xml`: opis, licencja i e-mail to `TODO` | kosmetyka; warto uzupełnić |
| 8 | `CMakeLists.txt`: `SDK_DIR` na stałe do `/home/std/...`; odwołania do nieistniejących skryptów pobierających | budowa tylko na tym robocie |
| 9 | Pusty plik `qt/cwel.h` (nazwa) | do usunięcia/zmiany nazwy; nic w nim nie ma |
| 10 | Whisper w trybie `local` ładuje model przy każdej wypowiedzi | wolniejsza odpowiedź; lepszy `whisper-server` (`HELIOS_STT_SERVER_URL`) |
| 11 | Dzwonek używa `HELIOS_AUDIO_DEVICE` do odtwarzania | przy innym mikrofonie może nie zagrać |
| 12 | Początek pierwszego słowa bywa ucinany (cichy głos) | patrz §13.2; rozwiązanie sprzętowe: mikrofon bliżej/lepszy |
| 13 | Jedno GUI naraz; brak uwierzytelniania łącza | patrz §7 i §14 |
| 14 | Odpowiedzi na czat z GUI zawsze idą na głos | brak trybu cichego |
| 15 | Rosbridge, serwer wideo i odbiorca `/teleop/joint_commands` są poza tym workspace | **[założenie]** działają; dokumentacja ich konfiguracji powinna powstać osobno |
| 16 | Modele śledzenia (`yolov8n-pose.onnx`, `mediapipe_pose_3d.onnx`) i modele głosu/słowa-klucza (`*.onnx`, `*.bin`) nie leżą w workspace | trzeba je mieć obok aplikacji / na robocie |
| 17 | Kopie `main.cpp` w katalogu głównym i `robot_src/` są nieaktualne | źródłem prawdy jest `uploads/main.cpp` |
| 18 | Tułów: błąd w Robot Assistant, taniec zamrożony | sprzęt, po stronie managera |
| 19 | Test akceptacyjny `keys=TAK` i `/stats` w czacie nie został jeszcze potwierdzony na robocie | do wykonania przez użytkownika (§11.5) |

---

## 16. Jak rozbudowywać projekt

### 16.1 Dodanie nowej komendy robota

1. Utwórz `commands/moja.h` i `commands/moja.cpp` (przestrzeń nazw `commands`). `CMakeLists.txt` zbiera `commands/*.cpp` przez `GLOB … CONFIGURE_DEPENDS`, więc nowy plik wejdzie do budowy sam.
2. Zarejestruj w `llm_commands.cpp`: `COMMAND_REGISTRY` (bez argumentu) albo `ARG_COMMAND_REGISTRY` (z argumentem) i dodaj `#include`.
3. Dodaj linię do `MASTER_PROMPT` (format `#tag#` lub `#tag:ARG#`) + jeden przykład.
4. Opcjonalnie skrót `/nazwa` w `handleConsoleLine` (`main.cpp`).
5. Pamiętaj: komenda działa w odłączonym wątku – **łap wyjątki** (rejestr to robi, ale loguj sensownie) i **nie dotykaj zasobów, które znikają przy wyjściu z `main`**.
6. Jeśli komenda ma pokazać wynik w czacie GUI – użyj wzorca z `stats` (hook + `guiPush {"type":"reply"}`).

### 16.2 Zmiana promptu

Edytuj `MASTER_PROMPT` w `main.cpp` (linia 432), zbuduj i zrestartuj core. Lista komend w prompcie musi odpowiadać rejestrowi (§6.2).

### 16.3 Poprawa jakości STT

Kolejność działań wg `diagnoza_stt_helios.md`: (1) tor audio i hałas, (2) VAD, (3) AGC, (4) prompt, (5) model, (6) konwersja próbek. Narzędzia: `diagnoza_stt/ab_stt.py` (test A/B na 30–50 nagraniach), `--kalibracja` (słownik poprawek), odsłuch `ostatnia_wypowiedz.wav`.

### 16.4 Zasady pracy z kodem

- Dostarczaj **pełne pliki**; przed wgraniem rób kopię `*.przed_zmiana`.
- Zmiany konfiguracji przez `HELIOS_*`, bez edycji skryptów, jeśli się da.
- Przed testem na robocie sprawdź `PKGDIR` (§11.2) i poprawność buildu.

---

## 17. Pomysły na targi

Zebrane z rozmowy; **to propozycje, nie zaimplementowane funkcje.**

| Pomysł | Koszt | Uwagi |
|---|---|---|
| Pitch firmy w prompcie (3–5 faktów) | zero kodu | największy zysk; dotyczy treści prompta |
| Quiz o firmie, żarty, zagadki | zero kodu | wyłącznie prompt |
| **Attract loop** (robot sam zaprasza, gdy nikt nie podchodzi 2 min) | mały timer w core | duży efekt na stoisku |
| **Kill-switch dla obsługi** („cisza”/„stop wszystko”: wyciszenie mikrofonu + zatrzymanie ruchu) | mały | przy tłumie to bezpiecznik |
| Małe gesty: ukłon, głowa tak/nie, żółwik, wskazanie produktu | po jednym tagu | wymaga sprawdzenia, czy struktura tułowia z polem `head[2]` jest faktycznie sterowalna |
| „Naśladuj mnie” z kamery | do sprawdzenia | śledzenie istnieje w Qt; do potwierdzenia, czy strona robota faktycznie odtwarza ruch |
| Naucz-i-odtwórz (drag + nagrywanie + **odtwarzanie**) | większy | nagrywanie stawów robota już jest; brakuje odtwarzania |
| Auto-głośność wg szumu otoczenia | średni | dane `[Audio]` już spływają |
| **Taniec** | po naprawie sprzętu | priorytet nr 1 po naprawie tułowia; kod zamrożony |
| Rozdawanie gadżetów | duży | wymaga chwytaka – kod nic o chwytakach nie wie |
| Ostrzeżenia o baterii/błędzie robota w czacie; tryb awaryjny bez internetu | średni | |

> Wcześniejsze wzmianki o „grzaniu silników przy `full_move` co 5 minut” oraz o osiach głowy były **założeniami niesprawdzonymi w kodzie ani dokumentacji robota** – nie planuj na ich podstawie.

### 17.1 Wi-Fi na targach: `/wifi` (zaimplementowane, NIETESTOWANE na robocie)

**Pomysł:** na targach robot nie widzi firmowego Wi-Fi. Robot sam rozgłasza własną sieć (AP) z adresem `192.168.49.97`, więc aplikacja Qt działa bez zmian. Laptop łączy się z tą siecią (bez internetu), w czacie wpisuje `/wifi NAZWA HASŁO`, a robot łączy się z hotspotem z internetem (np. z telefonu).

```
laptop ──Wi-Fi──► HELIOS-ROBOT (AP, 192.168.49.97, bez internetu)
                       │  czat: /wifi MojTelefon haslo123
                       ▼
robot ──druga karta Wi-Fi──► hotspot telefonu ──► internet (OpenRouter, YouTube…)
```

**Elementy:**
- `helios_wifi.sh` (robot, `/home/std/bin/`, `chmod +x`): `status`, `connect SSID [HASŁO]`, `ap-setup HASŁO`, `ap-on`, `ap-off`. Używa `nmcli`. Dotyka **wyłącznie kart Wi-Fi** (Ethernet i sieć kontrolerów `192.168.71.x` nietknięte). Hasła nie wypisuje.
- `main.cpp`: komenda `/wifi` w `handleConsoleLine` (przed obsługą `#`, bo hasła mogą mieć `#`). Skrypt jest uruchamiany przez `fork`/`exec` **bez powłoki** (znaki `$`, `"`, `#` w haśle są bezpieczne), w osobnym wątku, z limitem 120 s. Linie `WYNIK: …` wracają do czatu jako dymek Heliosa, reszta jako szare linie postępu.

**Wymagania sprzętowe (ważne):**
1. **Dwie karty Wi-Fi**: jedna trzyma AP, druga łączy się z hotspotem. Jeśli robot ma jedną kartę, dodaj tani adapter USB Wi-Fi (sprawdź wcześniej, czy jego chipset działa na Jetsonie/L4T). `NetworkManager` nie obsługuje AP i klienta jednocześnie na jednej karcie.
2. **Gdy karta jest tylko jedna**, skrypt odmawia połączenia, dopóki jej AP jest aktywny (połączenie z hotspotem wyłączyłoby AP, laptop straciłby robota, a aplikacja Qt ma na stałe wpisany `192.168.49.97`). Wymuszenie: `HELIOS_WIFI_JEDNA_KARTA=1` (skrypt przywraca AP, jeśli połączenie się nie uda).
3. **Alternatywa bez AP:** laptop podłączony do robota kablem Ethernet (adres `192.168.49.97` na porcie Ethernet), a jedyna karta Wi-Fi robota służy tylko jako klient hotspotu. `/wifi NAZWA HASŁO` działa wtedy bez zmian.

**Jednorazowa konfiguracja na robocie (jako `std`):**
```bash
mkdir -p ~/bin && cp helios_wifi.sh ~/bin/ && chmod +x ~/bin/helios_wifi.sh
nmcli dev                                   # ile kart Wi-Fi? (typ wifi)
echo 'std ALL=(root) NOPASSWD: /usr/bin/nmcli' | sudo tee /etc/sudoers.d/helios-wifi && sudo chmod 440 /etc/sudoers.d/helios-wifi
~/bin/helios_wifi.sh ap-setup 'HasloSieciRobota'   # min. 8 znaków; tworzy profil helios-ap
~/bin/helios_wifi.sh status
```
Wpis `sudoers` jest potrzebny, gdy `nmcli` pyta o uprawnienia (sesja SSH/tmux/systemd bez lokalnego logowania). Skrypt sam sprawdza `nmcli general permissions` i wtedy używa `sudo -n`. Profil AP ma **autostart z najniższym priorytetem** (`-100`): gdy w zasięgu jest znana sieć (firmowa), robot łączy się z nią, a gdy jej nie ma (targi), wstaje własna sieć `HELIOS-ROBOT`. Ręcznie: `/wifi ap` albo `~/bin/helios_wifi.sh ap-on`.

Wgraj nowy `main.cpp` (z `gotowe/`) i zbuduj: `cd ~/rokae_ws && colcon build --packages-select helios_ai`. Sprawdź: `grep -c "handleWifiCommand" <pkg>/main.cpp` = 2 (definicja + wywołanie).

**Procedura na stoisku:**
1. Włącz robota (bez firmowego Wi-Fi w zasięgu po kilkudziesięciu sekundach wstaje `HELIOS-ROBOT`; ewentualnie `ssh` kablem i `helios_wifi.sh ap-on`).
2. Laptop: dołącz do `HELIOS-ROBOT` (hasło z `ap-setup`), uruchom aplikację Qt (adres `192.168.49.97` bez zmian).
3. Telefon: włącz hotspot, **2,4 GHz**, ekran odblokowany.
4. W czacie: `/wifi NazwaTelefonu hasloTelefonu` (nazwa ze spacją: `/wifi "Telefon Ola" haslo`).
5. Oczekiwana odpowiedź: `Wi-Fi: połączono z 'NazwaTelefonu', internet działa (adres robota w tej sieci: 172.20.10.x)`. Kontrola: `/wifi status`.

**Komunikaty błędów:** `robot nie widzi tej sieci` (zasięg, ukryta sieć, 5 GHz), `hasło nie pasuje do sieci`, `przekroczono czas 30s` (telefon uśpił hotspot), `portal logowania` (Wi-Fi targowe z portalem: robot nie ma przeglądarki, użyj hotspotu z telefonu), `brak adresu IP` (DHCP hotspotu nie odpowiada), `brak uprawnień` (brak wpisu `sudoers`).

**Ograniczenia i uwagi bezpieczeństwa:**
- Hasło hotspotu wpisane w czacie jest **widoczne w dymku czatu na ekranie laptopa** (publiczność na stoisku!) i przechodzi przez łącze GUI bez szyfrowania (§14). Użyj **tymczasowego hasła** hotspotu. Core nie zapisuje hasła w logu ani w historii rozmów, a w `ps` jest widoczne tylko przez chwilę w argumentach `nmcli`.
- Profil hotspotu (`helios-hotspot`) jest zapamiętany w NetworkManagerze razem z hasłem (plik w `/etc/NetworkManager/system-connections/`, tylko root). Po targach: `nmcli con delete helios-hotspot`.
- AP używa pasma 2,4 GHz (`bg`). Tryb AP wymaga, by karta go obsługiwała: `iw list | grep -A8 "Supported interface modes"` ma zawierać `AP`.
- Sieć kontrolerów ramion (`192.168.71.x`) jest na Ethernecie i skrypt jej nie rusza. Nie podłączaj nigdy portu kontrolerów do hotspotu.
- AP w trybie `shared` ma NAT: laptop podłączony do `HELIOS-ROBOT` **może dostawać internet przez robota** (gdy robot ma hotspot). To efekt uboczny, nie wymóg.
- **Zweryfikowane tylko makietą `nmcli` w sandboxie** (przypadki: 2 karty, 1 karta z odmową, wymuszenie z przywróceniem AP, portal, maskowanie hasła) oraz testem uruchamiania skryptu z C++. Na prawdziwym robocie i karcie Wi-Fi nic nie sprawdzono.

---

## 18. Historia zmian

| Etap | Zmiana |
|---|---|
| Diagnoza STT (9.10.2026) | analiza polskiego rozpoznawania; wykryte i naprawione błędy: prompt/korekta słów/VAD/przycinanie ciszy; raport `diagnoza_stt_helios.md` |
| Łatka „usuń Groq z STT” | Groq usunięty z STT; domyślny model lokalny `large-v3` zamiast turbo (`zmiana_usun_groq_stt_large.patch`); w tamtym czasie Groq został w LLM – usunięty później, patrz §5.7 |
| Łatka „wątki komend” | komendy robota w odłączonych wątkach z `try/catch` (`zmiana_watki_komendy_robota.patch`) – wyjątek sieciowy nie zabija programu |
| Łatka „muzyczka” | muzyczka oczekiwania `elevator`/`queue`/własny WAV (`zmiana_muzyka_oczekiwania.patch`) |
| Dokument „szybki start” | `helios_szybki_start.md` |
| Łącze GUI | `gui_link.*`, rozszerzenie `main.cpp`, aplikacja Qt z czatem, kamerą, nagrywarką; `wdrozenie_gui.md` |
| Zdalny start | `helios_core.sh`, `helios.service`, `SshCore` (START/STOP/LOG) |
| Diagnoza wdrożenia | pułapka `PKGDIR` (zagnieżdżony `install/`); `grep -v /install/` + weryfikacja `handleConsoleLine=3` |
| Keepalive łącza GUI | `SO_KEEPALIVE` 10/5/3 – slot po martwym kliencie zwalnia się w ≈25 s |
| Zaznaczanie tekstu w czacie | w `ChatPanel.qml` dymki to `TextEdit` (readOnly, `selectByMouse`) zamiast `Text`: tekst zaznaczasz myszą (dwuklik = słowo) i kopiujesz Ctrl+C. Zaznaczenie działa w obrębie jednego dymka; całej rozmowy nie da się zaznaczyć jednym ruchem |
| Stop core'a przy zamknięciu apki | `SshCore::stopCoreOnExit()` podpięte pod `aboutToQuit`; opt-out `HELIOS_STOP_ON_EXIT=0`. Przy okazji poprawiony błąd kompilacji w `onLogReady()` (`readStandardOutput` → `readAllStandardOutput`) |
| `keys.zsh` w skrypcie | automatyczne wczytanie kluczy, `keys=TAK/BRAK` w `STARTED`, ostrzeżenie przy braku pliku |
| Stats w czacie | `statsReport()` + hook + `reply` → dymek Heliosa (`helios_stats_chat.zip`) |
| Prompt systemowy | finalna wersja (placeholder `<nazwa firmy>`, bez twórców, bez reguły głośności na starcie, bez YouTube) dostarczona w czacie; **do podmiany w kodzie** |
| Automatyczna konfiguracja SSH | `ustaw_ssh.ps1` (Windows): klucz `ed25519`, wgranie na robota, test `BatchMode`, kontrola `helios_core.sh`; opis w §11.4; decyzja „aplikacja Qt tylko na Windowsie” w §2.3 |
| Limit zapytania LLM | `CURLOPT_TIMEOUT` 180 s → 60 s (`main.cpp`) |
| Wi-Fi na targach | `helios_wifi.sh` + komenda `/wifi` w `main.cpp`; opis i procedura w §17.1 (nietestowane na robocie) |
| Czułość słowa-klucza | domyślnie próg 0.25, patience 2, okno 4 (było 0.40 / 2 / 0); `main.cpp` czyta teraz `HELIOS_WAKE_WINDOW` i ustawia `config.window` |
| Gesty podczas mowy | nowy moduł `commands/gesty.cpp i gesty.h`, hooki w `main.cpp`, skrót `/gest`; opis w §6.6 (nietestowane na robocie, prawa ręka = lustro lewej) |
| Instalator Qt (Windows) | katalog `instalator/` (Inno Setup, `build_installer.ps1`, bramka `helios_ssh_gate.sh`, przewodnik `INSTALATOR.md`); zmiany w `sshcore.cpp` i `videoreceiver.cpp` kompilują się, sam instalator nietestowany |
| Dokumentacja | niniejszy dokument (aktualizacja 20.10.2026) |

---

## 19. Słownik

| Pojęcie | Znaczenie |
|---|---|
| **core** | program `helios_ai/main` na robocie |
| **GUI / aplikacja Qt** | program `TrackingRobot` na laptopie |
| **tag** | komenda w tekście, np. `#wave#`, `#volume:70#` |
| **słowo-klucz (wake word)** | „Helios”, wywołuje nagrywanie |
| **pre-roll** | audio sprzed wykrytego startu mowy, dokładane do nagrania |
| **follow-up** | okno 6 s po odpowiedzi, w którym nie trzeba mówić „Helios” |
| **STT / TTS / LLM** | rozpoznawanie mowy / synteza mowy / model językowy |
| **VAD** | wykrywanie mowy (Silero) |
| **AGC** | automatyczna regulacja wzmocnienia mikrofonu |
| **drag** | tryb ręcznego prowadzenia ramienia robota |
| **posZero** | pozycja zerowa stawów |
| **rosbridge** | most WebSocket do ROS (port 9090) |
| **E-STOP** | fizyczny przycisk zatrzymania awaryjnego robota |
| **REF** | nazwa karty dźwiękowej USB robota (mikrofon i głośniki) |
| **PKGDIR** | katalog źródeł pakietu `helios_ai` na robocie |
| **keepalive** | mechanizm TCP wykrywający martwego klienta |


## Instalator aplikacji Qt (Windows) – NIETESTOWANY na Windowsie

Katalog `instalator/` (przewodnik: `instalator/INSTALATOR.md`). Decyzje: jeden plik `HeliosSetup-<wersja>.exe` (Inno Setup, instalacja per-użytkownik w `%LOCALAPPDATA%\Programs\Helios`, bez administratora); wszystkie DLL-e (Qt, OpenCV, MinGW) obok `appTrackingRobot.exe`, bez linkowania statycznego Qt; core na robocie NIE startuje sam po włączeniu zasilania (handlowiec używa START w aplikacji); bez sprawdzania wersji core'a, bez aktualizacji i bez certyfikatu. `ssh.exe` (Win32-OpenSSH) jest dociągane przy budowaniu i leży w `ssh\` obok `.exe`; klucz `helios_key` leży obok `.exe`. Po stronie robota klucz jest ograniczony bramką `helios_ssh_gate.sh` (`restrict,command=...` w `authorized_keys`): tylko `helios_core.sh start|stop|status|log`. Zmiany w C++ (kompilują się u użytkownika): `sshcore.cpp` (bundlowany ssh i klucz; `instalator/qt/sshcore.cpp` to aktualny plik użytkownika plus 3 fragmenty) i `videoreceiver.cpp` (model ONNX szukany obok `.exe`; zmiana wprowadzona ręcznie w pliku użytkownika).

**Budowanie (Windows, VS Code + CMake Tools + MSYS2 ucrt64):** wariant `Release` (`CMake: Select Variant`, potem `CMake: Delete Cache and Reconfigure` i build; kontrola: `CMAKE_BUILD_TYPE:STRING=Release` w `build\CMakeCache.txt`) → `build_installer.ps1` z `-ExePath`, `-QtBin` (przy Qt z MSYS2: `C:\msys64\ucrt64\bin`; skrypt szuka `windeployqt.exe`, `windeployqt6.exe` i `windeployqt-qt6.exe`), `-QmlDir`, `-ModelDir`, `-SearchDirs` (ucrt64 przed mingw64) → `Output\HeliosSetup-<wersja>.exe` → test na czystym Windowsie (Windows Sandbox). Uwaga: `CMakeLists.txt` wskazuje OpenCV w `C:/msys64/mingw64/...`, a Qt budowane jest zestawem ucrt64; kolejność w `-SearchDirs` musi odpowiadać temu, z czym aplikacja faktycznie działa. **Pułapka:** nie nadpisywać plików Qt użytkownika kopiami z `uploads/qt/` (starsza linia).
