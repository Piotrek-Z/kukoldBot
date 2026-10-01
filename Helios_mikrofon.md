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

Podczas analizy sprzętowej na komputerze pokładowym NVIDIA Jetson (`10.111.169.242`) zidentyfikowano następujące urządzenia wejściowe:

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

Na Jetsonie (`10.111.169.242`) zainstaluj narzędzia do obsługi audio w Pythonie:

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
