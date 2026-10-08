# Przewodnik: metodyka i kronika problemów
### Studium przypadku: baza jezdna ROKAE Helios (panel Matrix OS, port 5002)

> **To jest przewodnik**, a nie dokumentacja referencyjna. Opisuje *jak dojść* do przechwytywania
> i wysyłania komend (metoda możliwa do zastosowania przy innym sprzęcie) oraz *co nam się po
> drodze wysypało*. **Dokumentacja referencyjna** (protokół, pola, API biblioteki, opcje narzędzi):
> [`DOKUMENTACJA.md`](DOKUMENTACJA.md).

Ten dokument opisuje **jak w ogóle dojść do tego, żeby przechwycić i wysyłać komendy do API
urządzenia** — na naszym konkretnym przykładzie (baza Helios). Druga część to **katalog wszystkich
większych problemów**, na które trafiliśmy, z przyczynami i wnioskami.

> Stan wiedzy: październik 2026. Wszystko poniżej zostało sprawdzone na prawdziwym robocie
> (pierwszy ruch naszym kodem wykonany) i na atrapie. Miejsca niepewne są wyraźnie oznaczone.

---

## Spis treści

0. **Dokumentacja referencyjna:** [`DOKUMENTACJA.md`](DOKUMENTACJA.md) (protokół, API, narzędzia)
1. [Wersja w 10 minut](#1-wersja-w-10-minut)
2. [Kontekst: dlaczego to nie było „po prostu SDK"](#2-kontekst)
3. [Metodyka: przepis „od zera do własnych komend"](#3-metodyka)
4. [Jak to wyglądało u nas (kalendarium)](#4-kalendarium)
5. [Protokół Matrix OS — co wiemy](#5-protokol)
6. [Przechwytywanie — trzy drogi i ich pułapki](#6-przechwytywanie)
7. [Analiza zrzutu](#7-analiza)
8. [Wysyłanie komend](#8-wysylanie)
9. [Katalog problemów (i lekcji)](#9-problemy)
10. [Testowanie bez robota](#10-testowanie)
11. [Narzędzia w repozytorium](#11-narzedzia)
12. [Czego jeszcze nie wiemy](#12-nie-wiemy)
13. [Dodatki](#13-dodatki)

---

## 1. Wersja w 10 minut

- Baza Helios gada **WebSocketem po protobufie** na porcie 5002; panel webowy to jedyny znany klient.
- **Nie ma oficjalnego SDK** do bazy (SDK ROKAE obsługuje tylko ramiona) → trzeba było rozpracować
  protokół samemu.
- Metoda: **przechwycić ruch panelu → odtworzyć go 1:1 naszym kodem → podmieniać pola**.
- Przechwycenie bez roota: **haczyk w konsoli przeglądarki** (`tools/panel_hook.js`).
- Kluczowe odkrycie: komendy ruchu **nie idą kanałem zapytań** (pole 4), tylko **osobnym kanałem
  komend (pole 6)**, z opkodami: `op=6`+`op=111` = przejmij sterowanie, `op=16` = strumień jogu
  (pole 3 = jazda mm/s, pole 4 = obrót mrad/s), `op=7` = oddaj sterowanie.
- Baza potwierdzona eksperymentem: **1 jednostka = 1 mm/s** (faza stała), obrót w mrad/s.
- Efekt: `./baza.sh wasd` — jazda i łuki z klawiatury, plus `przod/tyl/lewo/prawo/stop/stan`.

---

## 2. Kontekst

| Element | Co to jest |
|---|---|
| **ROKAE Helios** | Humanoid: 2 ramiona × 7 DoF + **baza jezdna** (kołowa). ~190 kg |
| **Ramiona** | Obsługiwane przez SDK ROKAE (xCore / Robot Assist, dokumentacja docs.rokae.com) |
| **Baza** | **Inny system**: Standard Robots, system **Matrix OS**, panel webowy `http://192.168.71.50/#/homePage` |
| **Transport bazy** | WebSocket, port **5002**, binarnie, protobuf |

**Wniosek nr 1 (kosztował nas najwięcej czasu):** to, że coś jest jednym robotem, nie znaczy,
że ma jedno API. Dokumentacja ROKAE opisuje ramiona i tryby pracy; o bazie nie mówi nic użytecznego.
Bazę trzeba było rozpracować jak osobne urządzenie sieciowe.

**Skąd wzięliśmy wiedzę o protokole:**
1. Działający kod odczytu stanu (użytkownika) — dał transport, kopertę, logowanie, push stanu.
2. Obserwacja panelu (przechwycenie) — dała resztę: bootstrap, heartbeat, komendy ruchu.
3. Eksperymenty na robocie (małe prędkości) — potwierdziły znaczenie pól i jednostki.

---

## 3. Metodyka

Przepis uniwersalny: **jak dojść od „umiem tylko czytać" do „umiem wysyłać"** dla urządzenia,
które ma własny protokół po sieci.

### Krok 1 — Inwentaryzacja
- Co już działa? (najczęściej: odczyt). Jaki transport, jakie uwierzytelnianie, jakie kodowanie?
- Jaki jest **dobry klient** (panel, aplikacja, oficjalne narzędzie)? To on jest twoją dokumentacją.
- Czy masz dostęp sieciowy do urządzenia i do maszyny, z której pracuje panel?

### Krok 2 — Ustal transport i ramkowanie
- TCP czy WebSocket? Jaki port? Co dokładnie leci w ramce? U nas: **4-bajtowy prefiks długości
  (u32 big-endian) + protobuf**.
- Zapisz sobie to raz w jednym miejscu — połowa późniejszych błędów to zdjęcie/dołożenie tego
  prefiksu w złym miejscu.

### Krok 3 — Rozłóż kopertę
Każdy protokół ma warstwę „koperty" (routing, sesja, numery) i warstwę „treści". U nas:
`1=rodzaj`, `2=numer sekwencji`, `3=sesja`, `4=treść żądania`, `5=treść odpowiedzi/pushu`,
`6=treść komend`. Rozpoznanie tego podziału to moment, w którym przestajesz zgadywać.

### Krok 4 — Opanuj sesję
- Jak wygląda logowanie? (u nas: żądanie z loginem i MD5 hasła w treści).
- Co urządzenie nadaje w odpowiedzi? (u nas: **sesja**, pole 3 koperty — bez niej nic nie działa).
- Czy trzeba coś okresowo wysyłać, żeby sesja nie wygasła? (u nas: **heartbeat co ~3 s**).
- Czy klient po zalogowaniu robi „bootstrap"? (u nas: żądania typów **1, 17, 4**).

### Krok 5 — Naucz się czytać dane, które przychodzą same
To najtańszy sposób weryfikacji, że rozumiesz kodowanie (varint/fixed/int64, powtórzone pola).
U nas: push typu 2 = stan bazy (faza, x, y, kąt, dystans, trasa, mapa). Jeśli umiesz przewidzieć,
jak zmieni się stan po kliknięciu w panelu — rozumiesz format.

### Krok 6 — Przechwyć ruch użytkownika (panel)
To jest sedno: **nie zgaduj komend — podejrzyj je**. Trzy drogi (rozdział 6):
haczyk w konsoli przeglądarki (bez roota), proxy MITM (pełny dwukierunkowy zapis),
własna implementacja klienta (jeśli znasz już protokół).

### Krok 7 — Odtwórz 1:1, potem parametryzuj
1. **Replay bajt w bajt** (z nową sesją i nowym numerem sekwencji). Jeśli robot ruszył —
   masz uprawnienia i poprawny framing. To najbezpieczniejszy możliwy test.
2. Dopiero potem **podmieniaj pola** (u nas: pole 3 = jazda, pole 4 = obrót).
3. **Jednostki ustal pomiarem**, nie założeniem: zmień wartość ×2 i porównaj realny ruch.
4. Na końcu automatyzacja (klawiatura, skrypty).

### Zasady, które się sprawdziły
- **Obserwuj dobrego klienta** — logika „co wysyła, kiedy i w jakiej kolejności" to 80% wiedzy.
  My z panelu wzięliśmy całą kolejność: przejmij → jog → (cisza) → oddaj.
- **Dziennik działań**: notuj, co klikałeś i kiedy. Bez tego zrzut jest bezużyteczny
  (przykład: nie wiemy, co dokładnie kliknąłeś przy `op=32` — i to zostało nierozwiązane).
- **Atrapa zamiast robota**: zanim wyślesz cokolwiek na prawdziwy sprzęt, zbuduj atrapę
  mówiącą tym samym protokołem. U nas: `tools/bench_server.py` — cała logika była testowana
  na niej, łącznie z symulacją jazdy.
- **Nie zgaduj kodów na prawdziwym urządzeniu.** Zgadywanie na atrapie jest darmowe.

---

## 4. Kalendarium

Kolejność, w jakiej faktycznie to szło (dobra ilustracja typowej drogi):

| Etap | Co zrobiliśmy | Wynik |
|---|---|---|
| 1 | Analiza działającego kodu odczytu | transport, koperta, logowanie, push stanu |
| 2 | Proxy MITM (`amr_proxy`) | pełny zapis dwukierunkowy — ale wymaga roota i wpada w pętlę |
| 3 | **Haczyk w konsoli panelu** (`panel_hook.js`) | zrzut komend ruchu panelu, bez roota |
| 4 | Zrzut z hali: 119 ramek z jazdy panelem | komendy ruchu: kanał 6, `op=16`, pole 3=330, pole 4=±266 |
| 5 | Analiza zrzutu (`analyze_capture.py`) | rozpoznane opkody 6/7/111/16/32 + heartbeat 18 |
| 6 | Powtórzenie 1:1 na robocie | **robot ruszył** — sesja ma prawo sterować |
| 7 | Kalibracja (50 i 100 mm/s) | **1 jednostka = 1 mm/s**, rampa rozruchu ~0,2 s |
| 8 | `amr_wasd` na robocie | bug: „działa chwilę i przestaje" → winowajcą było własne `op=7` |
| 9 | Tryb łukowy | W+A nie działało z winy terminala (powtarza tylko ostatni klawisz) |

---

## 5. Protokół
*(skrót; pełny opis pól, typów i reguł → `DOKUMENTACJA.md` §2–§8)*

### 5.1 Transport i ramkowanie

- WebSocket `ws://192.168.71.50:5002`, ramki **binarne**.
- Każda wiadomość: **u32 big-endian (długość treści) + protobuf**. Treść bez prefiksu
  to „body" — i w takiej postaci zapisują je nasze narzędzia (`cap/*.hex`, `panel_capture.log`).

### 5.2 Koperta

| Pole | Typ | Znaczenie |
|---|---|---|
| 1 | varint | rodzaj: **0** = żądanie, **1** = odpowiedź, **2** = push/komenda |
| 2 | fixed32 | numer sekwencji (klient nadaje własne; baza odsyła w odpowiedzi) |
| 3 | fixed64 | **sesja** (nadana przez bazę; bez niej nic nie przejdzie) |
| 4 | bytes | treść żądania / pushu (pole 1 = typ) |
| 5 | bytes | treść odpowiedzi |
| 6 | bytes | **kanał komend** (kind=2): pole 2 = opkod, dalej parametry |

### 5.3 Logowanie i sesja

- Żądanie: pole 4 = `{1: 0, 2: {1: login, 2: MD5-hasła (hex), 3: 30}}`.
  Domyślnie: użytkownik `admin`, MD5 hasła `admin`.
- Odpowiedź niesie **sesję** — wszystkie następne ramki muszą ją mieć w polu 3.
- Po zalogowaniu klient (panel) wysyła żądania typów **1, 17, 4** (prawdopodobnie mapa/stan/konfiguracja).
- **Heartbeat**: `kind=0`, pole 4 = `{1: 18}`, co **~3 s** (nasz klient robi to samo).

### 5.4 Push stanu (typ 2) — to, co baza wysyła sama

| Ścieżka | Znaczenie |
|---|---|
| `3.2` | faza: 2 = spoczynek, 7 = jazda, 10 = start |
| `3.4.2`, `3.4.3` | pozycja x, y [mm] |
| `3.4.7` | kąt [mrad] |
| `3.7.8` | pozostały dystans (1 jedn. = 10 mm = 1 cm) |
| `3.7.12[n]` | odcinki trasy (sx, sy, ex, ey) [mm] |
| `3.10` | mapa (np. `Showroom`) |

### 5.5 Kanał komend (pole 6) — sedno sterowania

Treść: `{2: opkod, 7: 0, ...parametry...}`. Ramki te lecą jako `kind=2`.

| Opkod | Treść | Znaczenie |
|---|---|---|
| **16** | `{2:16, 3: v1, 4: v2, 7:0}` | **jog**: pole 3 = jazda [mm/s, + przód], pole 4 = obrót [mrad/s, + w lewo] |
| 6 | `{2:6, 7:0}` | **przejmij sterowanie** (raz przed jazdą) |
| 111 | `{2:111, 7:0}` | leci w parze z `op=6` (dokładnie ~9 ms po nim); znaczenie nieznane |
| 7 | `{2:7, 7:0}` | **oddaj sterowanie** — po tym baza przestaje słuchać jogu, dopóki nie dostanie znów 6+111 |
| 32 | `{2:32, 7:0, 9:{3:2, 4:2, 5:03, 11:1}}` | zlecenie zadania („jedź do…"); baza odpowiada po ~8,5 s typem 11 |

### 5.6 Przykładowe ramki (hex = body bez prefiksu długości)

```
Jog z panelu, przód 330 mm/s:   080215130000001931c44216a10100003209101018ca0220003800
  kind=2, seq=19, sesja, pole6={2:16, 3:330, 4:0, 7:0}

Nasza ramka stopu (zera):       0802150000000019000000000000000032081010180020003800
  kind=2, seq=0, sesja=0, pole6={2:16, 3:0, 4:0, 7:0}

Przejmij sterowanie:            08021500000000190000000000000000320410063800
Oddaj sterowanie:               08021500000000190000000000000000320410073800
Zadanie (op=32):                0802156b0000001931c44216a1010000320f102038004a09180220022a01035801
Odpowiedź bazy na zadanie:      0801156b0000001931c44216a10100002a08080b720408011000
  kind=1, seq=107, pole5={1:11, 14:{1:1, 2:0}}
```

Zapamiętaj zasadę: **sesja i numer sekwencji są nadpisywane przez nas** przy wysyłce —
z przechwyconej ramki bierzemy tylko treść (pole 6).

---

## 6. Przechwytywanie

### Droga A — haczyk w konsoli przeglądarki (nasza rekomendacja; bez roota)

**Jak:** otwórz panel → F12 → Console → wklej **całą** zawartość `tools/panel_hook.js` → Enter.
Haczyk podmienia `WebSocket.prototype.send` (widzi wszystko, co **wychodzi** z przeglądarki)
i owija konstruktor WebSocket (widzi, co **przychodzi**, ale tylko dla połączeń otwartych
po założeniu haczyka). Potem: klikanie w panelu → `saveCapture()` → plik `panel_capture.log`.

**Zasada działania:** obecne strony łączą się przez `new WebSocket(...)` — patchując prototyp
(metodę `send`), widzimy ruch **także na już otwartym połączeniu**, bez przeładowania strony.

**Pułapki:**
- **Nie przeładowuj strony po wklejeniu** — kod z konsoli znika. (Wersja 1 haczyka tego nie
  przeżyła; wersja 2 łapie już otwarte połączenie właśnie dlatego.)
- **Kierunek DN (odpowiedzi bazy)**: na już otwartym połączeniu się nie złapie — żeby go mieć,
  załaduj haczyk i *wtedy* odśwież stronę (nowe połączenie przejdzie przez owijkę konstruktora).
- Wklejanie wielolinijkowego kodu do konsoli bywa zawodne (przeglądarki zjadają fragment) —
  stąd „cała zawartość pliku", nie „kilka linii".
- Skopiowana nazwa pliku z czatu może zamienić się w link markdown
  (`./[baza.sh](http://baza.sh)`) i wysypać terminal — nazwy plików wpisuj ręcznie.

### Droga B — proxy MITM (pełny, dwukierunkowy zapis)

```bash
# proxy nasłuchuje lokalnie i przekazuje dalej; sam siebie nie łapie, bo reguła działa po uid
sudo ./amr_proxy --listen 127.0.0.1:5002 --remote 192.168.71.50:5002 --save cap/panel.log
sudo iptables -t nat -A OUTPUT -p tcp -d 192.168.71.50 --dport 5002 \
     -m owner --uid-owner $UID -j REDIRECT --to-ports 5002
```

**Pułapki:**
- **Pętla**: jeśli proxy nie wykluczy własnego ruchu, przekieruje sam siebie. Dlatego słucha na
  `127.0.0.1` i reguła działa **tylko dla twojego uid** (przeglądarki), a nie dla procesu proxy.
- Proxy nie zmienia bajtów (1:1); na Ctrl-C wypisuje tabelę `UP kind typ -> liczba`,
  więc nowe typy widać od razu.
- Wymaga Linuksa i `sudo` — dlatego to droga „B", nie „A".

### Droga C — własny klient jako zamiennik panelu
Gdy znasz już kopertę i logowanie, możesz po prostu **wysyłać własne żądania** i obserwować
odpowiedzi. Sami tak zrobiliśmy: `amr_probe`/`amr_cmd` są pełnoprawnymi klientami.
Uwaga: rób to na kopii/konta testowego, jeśli urządzenie ma produkcyjny audyt logowań.

### Czego nie robić
- Nie zaczynaj od brute-force opkodów na prawdziwym urządzeniu.
- Nie używaj SDK, które „nie dotyczy" tego modułu (u nas: całą sesję oszczędziłaby świadomość,
  że baza to Standard Robots, a nie ROKAE).

---

## 7. Analiza zrzutu

Format naszego zapisu: linie `+<czas> UP <hex>` (UP = wysłane przez panel; DOWN = odpowiedzi,
jeśli haczyk je złapał; `#` = komentarz). Hex = body bez prefiksu długości.

**Metoda (tools/analyze_capture.py):**

1. **Grupowanie po kształcie** ramki (zestaw obecnych pól), nie po „typie" — bo kanał komend
   nie ma typu, ma opkod.
2. **Odseparowanie pól technicznych**: pole 2 koperty = numer sekwencji (rośnie o 1) i pole 3 =
   sesja (stała w obrębie połączenia). Oba pomijamy w analizie.
3. **Serie czasowe**: dla każdego pola wypisz przedziały, w których wartość była stała.
   Tak zobaczyliśmy: `pole3=330 przez 2,72 s`, potem `pole4=+266 przez 8,15 s`,
   potem `pole4=−266 przez 6,56 s`.
4. **Korelacja z dziennikiem działań** — i tu jest cała siła: to użytkownik powiedział, że
   „330" to był przód, a `±266` to obroty w lewo/prawo. Zrzut bez komentarza = zgadywanie.
5. **Weryfikacja znaków i jednostek eksperymentem** (małe wartości, pomiar przemieszczenia).

**Przykład użycia:**

```bash
python3 tools/analyze_capture.py cap/panel_paste.log            # grupy + serie
python3 tools/analyze_capture.py cap/panel_paste.log --timeline 2
./amr_probe --dump-hex 080215130000001931c44216a10100003209101018ca0220003800
```

---

## 8. Wysyłanie komend

### 8.1 Dwie filozofie

| Tryb | Kiedy | Kolejność ramek |
|---|---|---|
| **Pojedyncza komenda** (`amr_cmd`, `baza.sh przod 150 2`) | krótki, odmierzany ruch | `op=6`, `op=111` → jog ×N → zera ×2 → `op=7` ×2 |
| **Sesja prowadzenia** (`amr_wasd`) | człowiek przy klawiaturze | `op=6`, `op=111` raz na starcie → jog w kółko (re-arm przed każdym nowym ruchem) → zera po puszczeniu → `op=7` **tylko przy wyjściu** |

**Reguła żelazna:** `op=7` (oddanie sterowania) **kończy** prawo do jazdy. Wysłany w środku sesji
sprawia, że kolejne ramki jogu są ignorowane. Panel wysyła go raz, na końcu — i nasze narzędzia
też. To był najkosztowniejszy błąd całego projektu (patrz problem #1).

### 8.2 Jednostki (potwierdzone eksperymentem)

| Pole | Jednostka | Kierunek |
|---|---|---|
| `6.3` | mm/s | **+ przód**, − tył |
| `6.4` | mrad/s (1000 = 57,3°/s) | **+ lewo**, − prawo |

Pomiar: komenda 50 → 95 mm w 2 s; komenda 100 → 188 mm w 2 s; tempo chwilowe w fazie stałej
równo 50 i 103 mm/s. Różnica w dystansie = **rampa rozruchu bazy** (~0,2 s), więc krótkie ruchy
wypadają odrobinę krótsze niż „prędkość × czas".

Bezpieczne widełki na halę: jazda 100–300 mm/s, obrót 100–300 mrad/s.

### 8.3 Bezpieczeństwo (checklista przed każdym testem)

- [ ] ≥ 1–2 m wolnej przestrzeni, podłoga bez progów i kabli
- [ ] Fizyczny STOP / przycisk awaryjny **w zasięgu ręki**
- [ ] Karta panelu zamknięta (jedno sterowanie naraz — nie wiemy, jak baza rozstrzyga konflikt)
- [ ] Prędkości startowe ≤ 150 mm/s, czasy ≤ 2 s
- [ ] Polecenie najpierw z `podglad`/`--dry-run`
- [ ] Po każdej próbie: spojrzeć na wypisane dx/dy/dkąt (czy zgadza się z zamiarem)

### 8.4 Gdy robot nie reaguje — diagnostyka po kolei

1. Czy jest sesja? (nasze narzędzia krzyczą: `brak sesji po 4 s` → baza zajęta panelem?)
2. Czy wysłaliśmy `op=6`+`op=111` **w tej sesji**? (po `op=7` trzeba je powtórzyć — re-arm)
3. Czy ramka ma poprawną kopertę? (`kind=2`, pole 6, sekwencja; sprawdź `--dry-run`)
4. `--verbose` — co odpowiada baza? (cisza na jog = na 90% brak grantu sterowania)
5. Test kontrolny: zamknij nasz program, weź panel — jeśli panel też nie jedzie, to nie my.
6. Ostatnia deska ratunku: **replay 1:1** przechwyconej ramki (bez podmian) — jeśli to działa,
   problem jest w naszych polach; jeśli nie, w sesji/uprawnieniach.

---

## 9. Katalog problemów

| # | Objaw | Przyczyna | Rozwiązanie / lekcja |
|---|---|---|---|
| 1 | „WASD działa chwilę, potem robot nie reaguje" (na robocie) | `amr_wasd` wysyłał `op=7` (oddaj sterowanie) po każdym puszczeniu klawisza | `op=7` tylko przy wyjściu; osobna opcja `--release-frame`; **lekcja: „stop" ≠ „oddaj sterowanie"** |
| 2 | Wszystko wygląda znajomo, a robot nie rusza | komendy ruchu idą **polem 6**, nie polem 4 (jak zapytania) | rozpoznanie kanału komend; selektor `--type 16` rozumie opkod |
| 3 | „Wielka liczba” `18446744073709551350` w polu obrotu | to **−266** zapisane jako 10-bajtowy varint int64 (uzupełnienie dwójkowe) | nie licz varintów ręcznie — używaj dekodera (`as_i64`); **lekcja: dziwna liczba = sprawdź znak, nie zakładaj błędu** |
| 4 | Panel nie wysyła żadnego „stopu” po puszczeniu przycisku | baza ma **martwego człowieka**: urwany strumień jogu = stop | nasze narzędzia **zawsze** wysyłają jawne zera; nie polegamy na deadmanie, bo nie wiemy, czy sesja zewnętrzna go ma |
| 5 | Trzymanie W+A nie dawało łuku („robi czynność trzymaną krócej") | **terminal powtarza tylko ostatni wciśnięty klawisz**; drugi klawisz wygasał po `--hold-ms` | tryb łukowy `--arc`: pamiętane osie + „zestaw klawiszy żyje, dopóki cokolwiek się powtarza"; czyszczenie po ciszy |
| 6 | Wasd „drga” przy trzymaniu klawisza | długie opóźnienie auto-repeat terminala wobec `--hold-ms` | `--hold-ms 600` albo `xset r rate 250 30` |
| 7 | `Nie umiem zdekodowac ramki komendy` przy `--frame @plik` | bufor hex nie był czyszczony → do heksu doklejał się tekst `@cap/...` | `frame_hex.clear()`; **lekcja: `hexDecode` tolerował śmieci (pomija nie-hex), więc błąd wyszedł dopiero w parsowaniu protobufa** |
| 8 | Zrzut z panelu nie zawierał odpowiedzi bazy | haczyk na **otwartym** połączeniu widzi tylko `send` (UP); DN tylko dla połączeń nowych | po wklejeniu haczyka odśwież stronę (raz), jeśli chcesz widzieć też ruch przychodzący |
| 9 | Haczyk v1 przestawał działać po odświeżeniu / nie łapał otwartego połączenia | nasłuch tylko w konstruktorze `WebSocket` | v2: patch `WebSocket.prototype.send` + owijka konstruktora |
| 10 | Proxy samo się zapętlało / nie łapało ruchu przeglądarki | proxy łapało własne połączenie wychodzące | `--listen 127.0.0.1` + `iptables -m owner --uid-owner $UID` |
| 11 | `./[baza.sh](http://baza.sh): permission denied` | skopiowany z czatu tekst zamienił nazwę pliku w link markdown | wpisuj nazwy ręcznie; `sudo` tu nie pomaga (i jest niepotrzebne); alternatywa: `bash baza.sh` |
| 12 | Pierwsze testy ruchu dawały mniej niż „prędkość × czas" | nieznana rampa rozruchu bazy (~0,2 s) | kalibracja pomiarem; krótkie ruchy planuj z zapasem |
| 13 | „Zgadywane" kody komend nie działały | kody z atrapy ≠ kody realnej bazy | najpierw przechwyć prawdziwe, potem zgaduj — albo wcale |
| 14 | Brak wiedzy, który typ HTTP/SDK ma bazę | baza to **inny system** (Standard Robots Matrix OS) niż ramiona (ROKAE xCore) | traktuj moduły robota jako osobne urządzenia |
| 15 | Sesja „ginęła"/baza przestawała odpowiadać na dłuższą metę | brak **heartbeatu** (typ 18 co ~3 s), tak jak robi panel | dodane do klienta; pierwszy heartbeat dopiero po ~3 s od logowania |
| 16 | Nie wiadomo, co kliknięto przy ramce `op=32` | brak **dziennika działań** przy zrzucie | prowadź notatki z czasami; nie da się odgadnąć intencji z bajtów |
| 17 | Podmiana pola „cicho nie działała” (vx=0) | pomylone ścieżki: `--set` przy `--type` liczy się od treści zadania (`2.1`), przy `--frame` od całego body (`4.2.1`); teraz odpowiednio `6.3`/`6.4` | trzymaj się jednej konwencji i sprawdzaj `--dry-run` z wypisem drzewa |
| 18 | Atrapa wywalała się (`NameError: op`) po dodaniu kanału komend | „na pół” naniesiona poprawka: parsowanie pola 6 w jednym miejscu, brak w drugim | testuj atrapę po każdej zmianie (`python3 -c "import ast;..."`), nie ufaj „już dodałem” |

### Trzy najważniejsze lekcje z tej listy

1. **Rozdzielaj pojęcia.** „Stop”, „puszczenie klawisza” i „oddanie sterowania” to trzy różne
   rzeczy — pomieszanie ich kosztowało nas najwięcej (problem #1).
2. **Weryfikuj empirycznie.** Jednostki, znaki, a nawet to, *czy* urządzenie w ogóle przyjmuje
   nasze ramki — wszystko zostało ustalone pomiarem, nie domysłem.
3. **Zapisuj kontekst.** Zrzut bez „co robiłem w tym momencie” jest bezużyteczny (problem #16);
   dziennik działań to najtańsze narzędzie inżyniera.

---

## 10. Testowanie bez robota

| Poziom | Narzędzie | Co sprawdza |
|---|---|---|
| 1. Podgląd | `--dry-run` / `baza.sh podglad ...` | poprawność ramek i podmian pól, bez wysyłki |
| 2. Atrapa | `tools/bench_server.py` (port 5003) + `AMR_IP=127.0.0.1 AMR_PORT=5003` | cała ścieżka: login, sesja, opkody, ruch, stop, pomiar |
| 3. Klawiatura bez człowieka | `tools/test_wasd_pty.py` | zachowanie `amr_wasd` (drgania, łuki, pauzy) |
| 4. Robot | `baza.sh` z małymi wartościami | fizyka, jednostki, uprawnienia |

Atrapa mówi **tym samym** protokołem co prawdziwa baza (łącznie z kanałem komend op=16,
heartbeatem i symulacją przemieszczenia), więc testy na niej mają sens.

---

## 11. Narzędzia

| Plik | Do czego |
|---|---|
| `baza.sh` | najprostsze wejście: `wasd / przod / tyl / lewo / prawo / stop / stan / podglad` |
| `amr_state_cli` | odczyt stanu bazy (rozszerzona wersja waszego kodu) |
| `amr_probe` | dekoder ramek: `--dump-hex`, `--interactive` |
| `amr_cmd` | wysyłka komend: `--frame`, `--from-capture`, `--set`, `--pre-frame`, `--stop-frame`, `--dry-run` |
| `amr_wasd` | prowadzenie z klawiatury: `--arc`, `--hold-ms`, `--speed/--turn`, re-arm, `--verbose` |
| `amr_proxy` | MITM z zapisem (droga B przechwytywania) |
| `tools/panel_hook.js` | haczyk do konsoli przeglądarki (droga A) |
| `tools/analyze_capture.py` | grupowanie i serie ramek z zapisu |
| `tools/bench_server.py` | atrapa bazy (do ćwiczeń i testów) |
| `tools/test_wasd_pty.py` | test klawiatury bez człowieka |
| `make test` / `amr_selftest` | testy jednostkowe rdzenia |
| `cap/panel_paste.log` + `cap/panel_*.hex` | nasz zrzut z hali i wybrane ramki |

---

## 12. Czego jeszcze nie wiemy

- **`op=32`** (zlecenie zadania): znamy format `{9:{3:2, 4:2, 5:03, 11:1}}` i odpowiedź typ 11,
  ale nie wiemy, jak wskazać cel (stacja/trasa). Potrzebny zrzut **z dziennikiem działań**.
- **`op=111`** — leci w parze z `op=6`; prawdopodobnie „potwierdzenie gotowości”, ale nie wiemy.
- Czy baza daje **naszej sesji** deadmana tak jak panelowi? (Dlatego zawsze wysyłamy jawne zera.)
- Czy można sterować **z panelu i naszym klientem naraz** i jak baza rozstrzyga konflikt?
- Czy istnieje **deadman/keepalive grantu sterowania** (np. trzeba odnawiać `op=6` co N sekund)?
  (Nie zaobserwowano przy sesjach kilkuminutowych.)
- Znaczenie odpowiedzi `{14:{1:1, 2:0}}` przy zadaniu.
- Czy jog przyjmuje **obie osie naraz** (nasze testy na robocie: tak, po dodaniu re-armu
  i trybu łukowego — ale warto potwierdzić na większej liczbie prób).
- Jakie limity **przyspieszenia** ma baza (nasze `--accel` to tylko nasze wygładzanie).

---

## 13. Dodatki

### Dodatek A — jak ręcznie czytać ramkę (mały trening)

Weź `3209101018ca0220003800` (to treść pola 6 koperty, bez nagłówka):
```
32        pole 6, typ „długość” (0x32 = numer 6 << 3 | 2)
09        długość treści = 9 bajtów
  10 10   pole 2, varint = 0x10 = 16      -> opkod 16 (jog)
  18 ca02 pole 3, varint = 0x02ca = 330   -> jazda 330 mm/s
  20 00   pole 4, varint = 0              -> obrót 0
  38 00   pole 7, varint = 0
```
Reguły: pierwszy bajt klucza = `numer_pola << 3 | typ` (0=varint, 1=fixed64, 2=bajty, 5=fixed32);
varint czytasz 7 bitów na bajt, najmłodsze pierwsze; liczby ujemne mają wszystkie wyższe bity
ustawione (dlatego −266 wygląda jak „wielka liczba”).

### Dodatek B — szablon dziennika działań przy zrzucie

```
# Zrzut: cap/panel_<data>.log
# kto: <imię>, gdzie: <hala/stanowisko>, mapa: Showroom
# 12,1 s  - kliknąłem „przód”, trzymałem ~2,7 s
# 21,9 s  - „obrót w lewo”, przytrzymałem ~8 s
# 32,3 s  - „obrót w prawo”, ~6,5 s
# 43,0 s  - zwolniłem sterowanie (przycisk X)
# 45,8 s  - wybrałem stację „3” i zatwierdziłem; odpowiedź przyszła po ~8,5 s
# 54,3 s  - anulowałem zadanie
```
Trzymaj ten plik **razem** ze zrzutem — dopiero razem mają wartość.

### Dodatek C — checklista „nowe urządzenie, od zera”

1. [ ] Znajdź dobrego klienta i sposób jego przechwycenia (proxy lub haczyk).
2. [ ] Ustal transport, port, ramkowanie (prefiks długości?).
3. [ ] Rozłóż kopertę: routing / sesja / treść.
4. [ ] Odtwórz logowanie i utrzymanie sesji (heartbeat).
5. [ ] Zbuduj atrapę mówiącą tym samym protokołem.
6. [ ] Zbierz zrzut z **dziennikiem działań**.
7. [ ] Pogrupuj ramki po kształcie, znajdź serie, skoreluj z działaniami.
8. [ ] Odtwórz wybraną ramkę 1:1 na urządzeniu (najbezpieczniejszy test).
9. [ ] Parametryzuj pola; jednostki ustal pomiarem.
10. [ ] Dopiero potem automatyzacja i bezpieczne limity.

---

*Przewodnik powstał na podstawie sesji pracy z bazą Helios (październik 2026). Wszystkie komendy
i wartości zostały sprawdzone na prawdziwym robocie albo na atrapie; miejsca niepewne są
oznaczone. Kod i narzędzia: katalog `helios_amr/`.*
