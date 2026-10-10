# Analiza dokumentacji — co jest, co poprawiono, czego brakuje

Analiza dokumentacji w katalogu [`DOKUMENTACJA/`](.).

## O etykietach źródeł

Każde twierdzenie w tym pliku jest oznaczone:

| Etykieta | Znaczenie |
|---|---|
| **[OFICJALNA]** | wynika z dokumentacji producenta (linki w [`02-linki-zewnetrzne.md`](02-linki-zewnetrzne.md)) |
| **[ZESPÓŁ]** | zapisane w dokumentacji wewnętrznej (pliki 04, 07–11, 14–16) |
| **[KOD]** | wyczytane z kodu w repozytorium (`.cpp`, `.qml`) |
| **[OPINIA]** | moja ocena albo rekomendacja — nie fakt |

> ⚠️ **Ograniczenie, o którym trzeba wiedzieć:** z tego środowiska **nie mam dostępu do
> `docs.rokae.com`** (mogę sięgnąć tylko do `github.com`, `registry.npmjs.org` i `pypi.org`).
> Dlatego oficjalnej dokumentacji **nie cytuję i nie przytaczam jej treści** — podaję wyłącznie
> linki zapisane w repozytorium. Wszystko poniżej pochodzi z dokumentacji wewnętrznej zespołu
> albo z kodu. Jeśli chcesz, żebym coś zweryfikował w oficjalnej dokumentacji — wklej fragment
> albo zezwól na inny dostęp.

---

## 1. Co jest w tym katalogu

21 plików: 16 dokumentów tematycznych (numerowanych) + 5 plików pomocniczych
(`README`, `URUCHAMIANIE`, `TUTORIAL-PROGRAMY`, `TUTORIAL-ZMIANA-SIECI`, `SLOWNIK`).

| Źródło | Pliki |
|---|---|
| kopie bez zmiany treści | 05, 07, 10, 11, 14, 15 |
| kopie z poprawkami | 01, 03, 04, 08, 09, 16 (zmiany wymienione w bloku „✏️ Poprawki" w każdym pliku) |
| konwersje `.txt` / pliku bez rozszerzenia | 02, 06 (treść 1:1) |
| nowe, napisane od zera | 12, 13, `URUCHAMIANIE.md`, `TUTORIAL-PROGRAMY.md`, `TUTORIAL-ZMIANA-SIECI.md`, `SLOWNIK.md`, ten plik |

Oryginały w katalogu głównym i w `sterowaniePodstawa/` **zostały nietknięte**.

---

## 2. Co poprawiono w tej wersji

To zmiany, które w poprzedniej wersji (same kopie) były tylko wypisane jako „do decyzji".

### 2.1 Adresy IP ramion — rozstrzygnięte ✏️ (plik 04)

| Dokument | podaje |
|---|---|
| `04` **przed poprawką** | prawe `.50`, lewe `.51` |
| `10`, `11`, `13` (z kodu) | prawe `.160`, lewe `.161`, **baza** `.50` |
| `14`, `15`, `16` | baza jezdna `.50:5002` |

**[OPINIA]** Cztery źródła na pięć wskazywało `.160`/`.161`, a `.50` to baza jezdna —
więc tak zapisano w pliku `04` (§1, diagram §6.1, przykład z pingiem §6.2).
Dopisano też adres bazy (`.50`) i wewnętrzny adres Jetsona (`.51`), których w tym rozdziale
nie było — bez nich te adresy były niejednoznaczne.

### 2.2 Adres wewnętrzny Jetsona — rozstrzygnięty ✏️ (plik 04)

`.51` (potwierdzone w `10`, `11` i w kodzie `prezentacja.cpp`) zamiast `.1` z diagramu w `04`.

### 2.3 Zdublowana lista linków ✏️ (plik 01)

Lista linków z `MasterPrompt.md` przeniesiona do pliku `02`; w pliku `01` zostało odesłanie.
Treść oryginalnego opisu projektu zachowana.

### 2.4 Karta dźwiękowa `card 3` ✏️ (pliki 08 i 09)

**[OPINIA]** Ta sama karta (`HK USB REF`, ALSA `REF`) bywała opisana jako mikrofon (08)
i jako głośniki (09). Najprawdopodobniej jedno urządzenie USB z mikrofonem i głośnikiem —
dopisano wzajemne odesłania i **sposób sprawdzenia** (`amixer -c 3 scontrols`),
bez twierdzenia, że to fakt.

### 2.5 Dwa opisy protokołu bazy ✏️ (plik 16)

Dopisano nagłówek: plik `14` jest nadrzędny, `16` go częściowo powtarza i nie zawiera
etykiet pewności. **[OPINIA]** Rozstrzygnięcie „który jest główny" jest ważniejsze niż scalanie —
scalanie można zrobić później, bez ryzyka pomyłki.

### 2.6 Dokument „włączanie zasilania" ✏️ (plik 03)

Oryginał to był jeden wykrzyknik. Zachowano go dosłownie (sekcja „oryginalny zapis"),
a wokół dodano listy kontrolne, wskazówki „gdy robot nie wstaje" i **listę pytań,
na które nikt nie ma odpowiedzi** — bez zmyślania faktów.

### 2.7 Nowe dokumenty

- **`12-teleoperacja-frontend-hud.md`** [KOD] — interfejs operatora opisany z `Main.qml`
  i ze zrzutu ekranu. Wskazano m.in., że adres robota jest **zaszyty w QML**.
- **`13-programy-cpp.md`** [KOD] — opis trzech programów konsolowych: menu, klasy SDK,
  dwa różne sposoby wysyłania ruchu (RT controller dla rąk, `moveAppend`/`moveStart` dla torsu).
- **`TUTORIAL-PROGRAMY.md`** — jak budować i uruchamiać każdy program, w tym test bez robota.
- **`TUTORIAL-ZMIANA-SIECI.md`** — procedura zmiany sieci z **tabelą wszystkich miejsc,
  w których jest adres** (12 pozycji) i konsekwencjami zapomnienia o każdym z nich.
- **`SLOWNIK.md`** — ~45 terminów w jednym miejscu (przydatne przy wdrożeniu nowej osoby).

---

## 3. Pokrycie tematyczne

| Temat | Dokument | Ocena |
|---|---|---|
| Włączanie zasilania | `03` | 🟡 krótkie, ale ma listę pytań do uzupełnienia |
| Konfiguracja i błędy xCore | `04` | ✅ najsolidniejszy dokument |
| Złącze narzędziowe M8 | `05` | ✅ kompletne |
| Sieć / Wi-Fi / SSH | `06` | 🟡 notatki |
| Kamery | `07` | ✅ kompletne |
| Mikrofon | `08` | ✅ kompletne |
| Głośniki | `09` | ✅ krótkie, konkretne |
| Teleoperacja (architektura) | `10` | ✅ kompletne |
| Węzeł `helios_probe` / SDK | `11` | ✅ kompletne, 15 problemów |
| Frontend / HUD | `12` | 🟡 nowy — z kodu QML; brak opisów klas C++ |
| Programy konsolowe C++ | `13` | 🟡 nowy — z kodu; brak buildów i wyników testów |
| Baza jezdna | `14`, `15`, `16` | ✅ najlepiej udokumentowany podsystem |
| Uruchomienie całości | `URUCHAMIANIE.md` | ⏳ szkielet |
| Budowanie czegokolwiek | `TUTORIAL-PROGRAMY.md` | 🟡 szablony; ❓ brak gotowych buildów |
| Zmiana sieci / IP | `TUTORIAL-ZMIANA-SIECI.md` | ✅ nowe — wcześniej nie było nigdzie |
| Potok głosowy end-to-end | `08` §4 | 🟡 architektura opisana, implementacji nie ma |

---

## 4. Co jest sprawdzone na robocie, a co jest hipotezą

**Potwierdzone [ZESPÓŁ]:** odczyt obu rąk (`.160`, `.161`) i torsu (`.254`) przez SDK 0.7.1;
klasy `xMateErProRobot` (ręce) i `PCB4Robot` (tors); ruch ramieniem; dla bazy — logowanie,
push stanu, jog i kalibracja 1 jednostka = 1 mm/s; kamery, mikrofon, głośniki.

**Hipotezy / niezweryfikowane [ZESPÓŁ]:** znaczenie `op=111`; adresowanie celu w `op=32`;
czy urwanie jogu zawsze zatrzymuje bazę (deadman); czy konfiguracje MoveIt
`rokae_xMateAR5L/R_moveit_config` pasują do rąk Heliosa; odczyt głowy (`helios_head_read`);
ruch torsem przez `PCB4Robot` + kontroler RT.

**[OPINIA]** To rozróżnienie warto zachać w dokumentacji tak, jest — nowa osoba od razu wie,
czemu nie może ufać.

---

## 5. Sprzeczności, które zostały

| # | Co | Stan |
|---:|---|---|
| 1 | `192.168.49.97` w notatkach Wi-Fi (`06`) — nie pasuje do żadnej znanej podsieci | ❓ do sprawdzenia; wartość przepisana 1:1, oznaczona jako niepewna |
| 2 | „deafultowy" zamiast „domyślny" (`09`) | ❓ literówka autora — celowo nie poprawione, bo to notatka |
| 3 | problem 15 w pliku `11` (`jointPosition` vs `jointPos`) — „w toku" | ❓ nikt nie Zamknął |
| 4 | `helios_probe` w repo ma tylko odczyt ramienia, a opis w `11` wymienia 5 binarek | ❓ wyjaśnione w `13`, ale warto zweryfikować |
| 5 | prędkości w różnych programach: 20% (`connect_test`), 40% (`prezentacja`), 25% (teleoperacja) | ❓ brak jednej polityki — do ustalenia |
| 6 | która wersja archiwum `TrackingRobot*` jest aktualna | ❌ nieoznaczone |

---

## 6. Luki — czego nadal brakuje

1. **Gotowych buildów** dla `connect_test-pl.cpp` i `prezentacja.cpp` — w `TUTORIAL-PROGRAMY.md`
   są szablony oparte na linkowaniu opisanym w pliku `11`, ale nikt ich nie sprawdził.
2. **Źródeł aplikacji do trackingu** — w repo tylko `Main.qml`/`Main2.qml` i archiwa.
   Klas `RobotClient`, `VideoReceiver`, `PoseDetector`, `MotionFilter` nikt nie opisuje.
3. **Oznaczenia aktualnej wersji** archiwów w `modelDoTrackingu/` i w `programy/`
   (plus literówka w nazwie `taktyczyTest.zip`).
4. **Procedury wyłączania** robota — sekcja „Zatrzymywanie" w `URUCHAMIANIE.md` jest w całości TODO.
5. **Potoku głosowego** — architektura jest, implementacji i opisu uruchomienia nie ma.
6. **Etykiet w pliku `16`** — brakuje etykiet pewności, które ma plik `14`.

---

## 7. Higiena repozytorium (poza dokumentacją)

1. **Klucze API w repo** — `nemotron_api` i `youtube_api` w postaci jawnej, w historii gita.
   **[OPINIA]** Najpilniejsza sprawa w repo: unieważnić i wygenerować nowe, pliki przenieść
   poza repo (`.env` + `.gitignore`). Do dokumentacji **celowo** ich nie kopiowałem.
2. **Binarne w głównej gałęzi** — ~100 MB w `modelDoTrackingu/`, 12 MB `TrackingRobot.zip`
   w katalogu głównym, archiwa w `programy/` i `sterowaniePodstawa/`. **[OPINIA]** Git LFS
   albo trzymanie samego kodu.
3. **Brak `.gitignore`** — stąd klucze i binaria w historii.

---

## 8. Moja opinia i rekomendacje

**[OPINIA]**, w kolejności ważności:

1. **Zaakceptuj ten katalog jako źródło prawdy** i usuń stare pliki z katalogu głównego
   oraz z `sterowaniePodstawa/` — trzymanie dwóch wersji jest gorsze niż stan wyjściowy,
   bo zaczynają się rozjeżdżać (tak się już stało z adresami IP).
2. **Unieważnij klucze API.**
3. **Uzupełnij `URUCHAMIANIE.md`** przy najbliższym pełnym starcie — wypełnione sekcje 0, 3, 5, 7
   i „Zatrzymywanie" zamienią szkielet w procedurę.
4. **Dopisz buildi** dla dwóch programów C++ (albo przenieś je do workspace `~/rokae_ws`
   jako pakiet `colcon`, gdzie jest już opisana procedura).
5. **Przestań trzymać adresy na sztywno w kodzie** — Dziś adres w `Main.qml` i w `prezentacja.cpp`
   oznaczają, że zmiana sieci = edycja kodu + przebudowanie. Wystarczy pole w interfejsie,
   parametry wiersza poleceń i jeden plik konfiguracyjny (szczegóły w `TUTORIAL-ZMIANA-SIECI.md` §5).
6. **Oznacz aktualną wersję** archiwów — jedna linia w `README` wystarczy.
7. **Uzupełnij plik `03`** odpowiedziami na pytania z listy „Do uzupełnienia" —
   to najsłabsze miejsce w dokumentacji, a dotyczy krytycznego kroku.

---

## 9. Lista rzeczy do sprawdzenia na robocie

- [ ] adres `192.168.49.97` z notatek Wi-Fi (`06`),
- [ ] czy `card 3` to jedno urządzenie z mikrofonem i głośnikiem (`08`, `09`),
- [ ] czy `helios_head_read` odczytuje głowę (problem 15 w pliku `11`),
- [ ] ruch torsem przez `PCB4Robot` + kontroler RT,
- [ ] co dokładnie robi przycisk E-STOP w HUD (`12`),
- [ ] czy szablony kompilacji z `TUTORIAL-PROGRAMY.md` działają,
- [ ] pełne przejście procedury z `URUCHAMIANIE.md`,
- [ ] zachowanie bazy przy zerwanym strumieniu jogu,
- [ ] która wersja `TrackingRobot*` jest aktualna.
