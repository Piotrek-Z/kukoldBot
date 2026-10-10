> 📄 **Na podstawie:** `Uruchamianie robota.md` · wersja z poprawek — zmiany wymienione w bloku „✏️ Poprawki" · 10 października 2026

# Włączanie zasilania robota

Procedura włączania robota Rokae Helios. To najkrótszy dokument w zestawie, a dotyczy kroku,
bez którego nic dalej nie działa — dlatego warto go kiedyś rozbudować (patrz „Do uzupełnienia").

> ✏️ **Poprawki 10.10.2026 (ten plik):**
> 1. Dodano tytuł, strukturę i linki. **Treść oryginalna jest zachowana dosłownie** w sekcji
>    „Procedura (oryginalny zapis)" — nie zmieniano jej ani o literę.
> 2. Dopisano listy kontrolne i wskazówki, co warto doprecyzować — bez zmyślania faktów,
>    których nie ma w oryginale.

## Procedura (oryginalny zapis)

> **Aby włączyć robota trzeba podłączyć go do prądu i poczekać aż piknie dwa razy,
> potem KLIKNĄĆ przycisk na plecach RAZ i poczekać aż się sprawuje.**

## Procedura (do wykonania krok po kroku)

1. Podłącz robota do zasilania.
2. Poczekaj, aż robot **piknie dwa razy**.
3. Wciśnij **raz** przycisk na plecach robota.
4. Poczekaj, aż system się sprawuje (robot jest gotowy do pracy).

## Lista kontrolna

- [ ] robot podłączony do zasilania,
- [ ] dwa piknięcia,
- [ ] przycisk na plecach wciśnięty raz,
- [ ] system się sprawował,
- [ ] robot w trybie **Manual** (wymagany do ruchu ręcznego — patrz [`04`](04-robot-rozwiazywanie-problemow.md) §3.3),
- [ ] żaden wyłącznik awaryjny nie jest zablokowany,
- [ ] silniki mają zasilanie (deadman / ikona zasilania — patrz [`04`](04-robot-rozwiazywanie-problemow.md) §3).

## Gdy robot nie wstaje

| Objaw | Gdzie szukać |
|---|---|
| brak piknięć | [`06-siec-wifi.md`](06-siec-wifi.md) (zasilanie / stan Jetsona) |
| alarm napędu `0x3120` | [`04`](04-robot-rozwiazywanie-problemow.md) §3 |
| `Controller Service: Disconnected` | [`04`](04-robot-rozwiazywanie-problemow.md) §5 |
| ramię nie odblokowuje się po E-Stop | [`13-programy-cpp.md`](13-programy-cpp.md) §1 (opcja 7 „Reset błędów") |

## Do uzupełnienia

To są pytania, na które nikt w zespole nie ma zapisanej odpowiedzi — warto dopisać,
jak tylko ktoś sprawdzi na robocie:

- [ ] co oznaczają dwa piknięcia (kontroler gotowy? sieć gotowa?),
- [ ] ile trwa „sprawowanie się" systemu (jaki jest typowy czas),
- [ ] co robić, gdy robot nie piknie drugi raz,
- [ ] jaki jest poprawny przebieg wyłączania (patrz też [`URUCHAMIANIE.md`](URUCHAMIANIE.md) → „Zatrzymywanie"),
- [ ] czy procedura jest taka sama dla całego robota i dla samego torsu.
