# Helios: sterowanie głośnikami (komputer pokładowy)

**Sprzęt:** Jetson AGX Orin, Ubuntu. Głośniki to karta USB **card 3: `HK USB REF`** (nazwa ALSA: `REF`). Karty 0 (HDMI) i 1 (APE) to wyjścia wewnętrzne Jetsona, nie głośniki.

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
