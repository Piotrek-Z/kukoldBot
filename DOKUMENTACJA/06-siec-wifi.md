> 📄 **Kopia dokumentu.** Oryginał: `helios-wifi.txt` (katalog główny repo, plik `.txt`).
> Treść bez zmian; dodano tytuł i nagłówki sekcji, żeby plik był zwykłym dokumentem Markdown. 2026-10-10.

# Sieć Wi-Fi i dostęp do robota

Notatki o łączeniu z robotem przez Wi-Fi i SSH (komputer pokładowy NVIDIA Jetson).

## 1. Połączenie przez SSH

Uruchamiamy terminal na komputerze z windowsem (win + r -> cmd), potem wpisujemy taką komendę:

```bash
ssh std@192.168.71.51
```
> Terminal poprosi o podanie hasła, standardowe hasło dla użytkownika std to **st**

## 2. Połączenie z siecią Wi-Fi

```bash
sudo nmcli dev wifi connect "Moje_Wifi" password 'HASŁO'
```

> **W HAŚLE MUSZĄ BYĆ POJEDYNCZE CUDZYSŁOWY!!!!**
> Poprawka: Piotrek odkrył, że jak da się w pojedynczym cudzysłowie to hasztagi i inne znaki
> specjalne są tak jakby ok, ale podwójny cudzysłów to tu jest problem ^____^

## 3. Przydatne komendy

Za pomocą tych komend możemy sprawdzić jaki adres ip został przypisany do robota **(potrzebne w następnym kroku!)**

```bash
ip a
nmcli device show
```

## 4. Łączenie robota z komputerem, dostęp do internetu *(niezbędne po działanie modelu AI!)*

Gdy już wiemy jaki adres został przypisany robotowi, możemy się z nim połączyć przez SSH:

```bash
ssh std@111.222.333.444 # <- tu wstawiamy nowy adres!

```
## Adresacja

- `192.168.49.97/24` — adres IP w sieci `JM-TRONIK_5G`

> ⚠️ Do zweryfikowania: pozostałe elementy robota pracują w podsieciach `10.111.169.x`
> (sieć zewnętrzna / Wi-Fi) i `192.168.71.x` (kontrolery) — patrz
> [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §1 i §6
> oraz [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §1.2.
