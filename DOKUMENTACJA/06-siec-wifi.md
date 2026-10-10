> 📄 **Kopia dokumentu.** Oryginał: `helios-wifi.txt` (katalog główny repo, plik `.txt`).
> Treść bez zmian; dodano tytuł i nagłówki sekcji, żeby plik był zwykłym dokumentem Markdown. 2026-10-10.

# Sieć Wi-Fi i dostęp do robota

Notatki o łączeniu z robotem przez Wi-Fi i SSH (komputer pokładowy NVIDIA Jetson).

## Połączenie z siecią Wi-Fi

```bash
sudo nmcli dev wifi connect "Moje_Wifi" password 'HASŁO'
```

> **W HAŚLE MUSZĄ BYĆ POJEDYNCZE CUDZYSŁOWY!!!!**
> Poprawka: Piotrek odkrył, że jak da się w pojedynczym cudzysłowie to hasztagi i inne znaki
> specjalne są tak jakby ok, ale podwójny cudzysłów to tu jest problem ^____^

## Przydatne komendy

```bash
ip a
nmcli device show
```

## Adresacja

- `192.168.49.97/24` — adres IP w sieci `JM-TRONIK_5G`

> ⚠️ Do zweryfikowania: pozostałe elementy robota pracują w podsieciach `10.111.169.x`
> (sieć zewnętrzna / Wi-Fi) i `192.168.71.x` (kontrolery) — patrz
> [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §1 i §6
> oraz [`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §1.2.
