> 📄 **Na podstawie:** `sterowaniePodstawa/DOKUMENTACJA.md` · treść bez zmian (dopasowano nazwę pliku i linki wewnętrzne) · 10 października 2026
> Ścieżki `cap/`, `tools/`, `baza.sh` są względem katalogu `sterowaniePodstawa/`.

# Dokumentacja — Helios AMR (baza jezdna, system Matrix OS)
### Referencja protokołu, biblioteki C++ i narzędzi

**Wersja:** 0.3 · **Data:** 2026-10-08 · **Zakres:** baza jezdna (ramiona: SDK ROKAE, poza zakresem)

Dokument **referencyjny**: opisuje interfejsy, pola i kontrakty. Można go czytać wyrywkowo.
Metodyka dochodzenia do tych ustaleń i katalog problemów: [`15-baza-jezdna-przewodnik.md`](15-baza-jezdna-przewodnik.md).

---

## 1. Konwencje dokumentu

### 1.1 Etykiety pewności

| Etykieta | Znaczenie |
|---|---|
| `[R]` | potwierdzone na prawdziwym robocie |
| `[P]` | ustalone z przechwyconego ruchu panelu Matrix |
| `[A]` | sprawdzone tylko na atrapie (`tools/bench_server.py`) |
| `[?]` | hipoteza lub brak weryfikacji — nie zakładać w kodzie produkcyjnym |

### 1.2 Konwencje zapisu

- **hex** — zawsze treść ramki („body"), **bez** 4-bajtowego prefiksu długości.
- **Ścieżka pola** — kropki od początku treści: `6.3` = pole 6 → podpole 3. Numeracja pól
  protobufa (1-based), zgodna z protobufem.
- **Jednostki SI**: mm, mm/s, mrad, mrad/s (1000 mrad/s = 57,3°/s), ms, Hz.
- **„treść" (ang. content)** — podkomunikat wewnątrz pola koperty (4, 5 lub 6).

### 1.3 Słownik

| Termin | Znaczenie |
|---|---|
| **koperta (envelope)** | zewnętrzny komunikat każdej ramki: rodzaj, seq, sesja, treść (rozdz. 4) |
| **body** | pełna treść ramki bez prefiksu długości; to zapisują narzędzia |
| **kanał komend** | pole 6 koperty — tu jadą komendy ruchu (`kind=2`), patrz rozdz. 6 |
| **opkod** | kod operacji w kanale komend (pole `6.2`): 6, 7, 16, 32, 111 |
| **grant sterowania** | prawo do wysyłania jogu; nadawane przez `op=6`+`op=111`, odbierane przez `op=7` |
| **re-arm** | ponowne wysłanie `op=6`+`op=111` przed nowym ruchem |
| **jog** | strumień komend prędkości (`op=16`) wysyłany cyklicznie |

---

## 2. Architektura i adresacja

```
[panel www]──┐
             ├──ws://192.168.71.50:5002──>[ baza Helios: Matrix OS ]
[amr_*]──────┘                                   ▲
   ▲                                             │
   └── opcjonalnie przez amr_proxy (MITM, 127.0.0.1:5002)
[atrapa bench_server.py  :5003]  ← do testów bez robota (inny port, ten sam protokół)
```

| Element | Rola | Adres domyślny |
|---|---|---|
| Baza Helios | urządzenie sterowane | `192.168.71.50:5002` (WebSocket) |
| Panel Matrix | oryginalny klient (web) | `http://192.168.71.50/#/homePage` |
| `amr_*` | nasi klienci (C++) | łączą się jak panel |
| `amr_proxy` | podsłuch dwukierunkowy | nasłuch `0.0.0.0:5002`, przekazanie na bazę |
| `bench_server.py` | atrapa bazy (testy) | `0.0.0.0:5003` |

### 2.1 Domyślne poświadczenia

| Parametr | Wartość domyślna |
|---|---|
| host / port | `192.168.71.50` / `5002` |
| użytkownik | `admin` |
| hasło | MD5 w hex: `21232f297a57a5a743894a0e4a801fc3` (MD5 z `admin`) |
| timeout połączenia | 3000 ms |
| heartbeat | 3000 ms (0 = wyłączony) |

Zmiana hasła: `echo -n 'haslo' | md5sum` → `--pass-md5`.

---

## 3. Warstwa sieciowa

### 3.1 Transport

| Cecha | Wartość | Status |
|---|---|---|
| protokół | WebSocket (`ws://`), ramki **binarne** | `[R]` |
| port | 5002 | `[R]` |
| kompresja / TLS | brak (nie zaobserwowano) | `[P]` |
| utrzymanie sesji | heartbeat (rozdz. 5.3) | `[P]` |

### 3.2 Ramkowanie

Każda wiadomość aplikacji = **protobuf poprzedzony 4-bajtową długością (u32, big-endian)**.

```
[ 4 B: długość N ][ N B: treść protobuf („body") ]
```

Narzędzia zapisują i przyjmują wyłącznie część „body" (bez prefiksu). Złożenie/rozłożenie
prefiksu robi warstwa transportowa (`amr::matrixFrame`, `WsClient`).

### 3.3 Kodowanie protobuf

| Typ drutu (wire) | Znaczenie |
|---|---|
| 0 | varint |
| 1 | fixed64 (8 B, little-endian) |
| 2 | długość + bajty (zagnieżdżony komunikat, tekst, bajty) |
| 5 | fixed32 (4 B, little-endian) |

Zasady istotne w tym protokole:
- varint: 7 bitów na bajt, najmłodsza grupa pierwsza;
- **liczby ujemne (int64)** mają wszystkie wyższe bity ustawione, więc np. `−266` zajmuje
  10 bajtów i „gołym okiem" wygląda jak liczba ~1,8·10¹⁹. Do interpretacji służy `as_i64` /
  `intField`; nie odczytywać varintów ręcznie.
- Wartości w tym protokole są **całkowite** (bez float); ułamków nie zaobserwowano.

---

## 4. Koperta (envelope)

Struktura każdej ramki w obie strony:

| Pole | Typ (wire) | Nazwa | Znaczenie |
|---|---|---|---|
| 1 | varint | `kind` | **0** = żądanie, **1** = odpowiedź, **2** = push/komenda |
| 2 | fixed32 | `seq` | numer sekwencji; klient nadaje własne, baza odsyła w odpowiedzi |
| 3 | fixed64 | `session` | identyfikator sesji nadany przez bazę; **0** przed zalogowaniem |
| 4 | bytes | `req` | treść żądania/pushu; podpole 1 = typ (rozdz. 5.4) |
| 5 | bytes | `resp` | treść odpowiedzi/pushu |
| 6 | bytes | `cmd` | **kanał komend** (rozdz. 6); podpole 2 = opkod |

Uwagi:
- Pole 6 pojawia się tylko w ramkach `kind=2` wysyłanych przez panel. Ramki `kind=0` (zapytania)
  i `kind=1` (odpowiedzi) używają pól 4/5.
- Wysyłając z przechwyconej ramki podmieniamy **wyłącznie** pola 2 i 3 (nasze `seq` i `session`);
  resztę zostawiamy bajt w bajt — to robi `MatrixClient::sendReplay`.
- `describeFrame()` zwraca skrót w postaci `kind=2 seq=19 sess=0x1a11... op=16 len=27`.

### 4.1 Identyfikacja ramki (`FrameInfo`)

| Pole | Znaczenie |
|---|---|
| `kind`, `seq`, `session` | jak wyżej |
| `type` | typ z treści żądania/pushu (`req.1` lub `resp.1`) |
| `op` | opkod z kanału komend (`cmd.2`) |
| `has_type`, `has_op` | czy dane pole wystąpiło (typ/opkod może nie istnieć) |

Selektor `--type N` w narzędziach dopasowuje ramkę **po `type` albo po `op`** — dlatego
`--type 16` działa zarówno dla jogu z kanału komend, jak i dla zadań z pola 4.

---

## 5. Sesja i uwierzytelnianie

### 5.1 Logowanie

Żądanie (`kind=0`, `seq=1`, `session=0`):

```
pole 4 = { 1: 0, 2: { 1: "<login>", 2: "<md5-hasła-hex>", 3: 30 } }
```

| Ścieżka | Znaczenie |
|---|---|
| `4.1` | kod żądania: **0** = logowanie |
| `4.2.1` | login |
| `4.2.2` | MD5 hasła (32 znaki hex, małe litery) |
| `4.2.3` | 30 — interpretacja nieznana (stałe w panelu) `[P]` `[?]` |

Odpowiedź bazy niesie **sesję** (pole 3 koperty) oraz payload stanu (pole 5). Od tego momentu
każda wysyłana ramka musi zawierać tę sesję.

### 5.2 Bootstrap po zalogowaniu

Do dokładnie tego samego (kolejność i kody) co panel `[P]`:

| Kolejność | Typ żądania | Uwaga |
|---|---|---|
| 1 | `1` | znaczenie nieustalone `[?]` |
| 2 | `17` | znaczenie nieustalone `[?]` (prawdopodobnie mapa/konfiguracja) |
| 3 | `4` | znaczenie nieustalone `[?]` |

Wysyłane raz, po pierwszej odpowiedzi (seq 1). Można wyłączyć: `Options::bootstrap_requests=false`.

### 5.3 Heartbeat

| Aspekt | Wartość |
|---|---|
| żądanie | `kind=0`, pole 4 = `{1: 18}` |
| częstotliwość panelu | co ~3 s (zaobserwowane na całym zrzucie) `[P]` |
| nasza konfiguracja | `Options::heartbeat_ms` (domyślnie 3000; 0 = nie wysyłaj) |
| reguła | wysyłany dopiero po ~3 s **bezczynności** (nie zaraz po logowaniu) |

### 5.4 Znane kody żądań

| Typ | Kierunek | Znaczenie | Status |
|---|---|---|---|
| 0 | klient → baza | logowanie | `[R]` |
| 1 | klient → baza | bootstrap (1. z trzech) | `[P]` [?] |
| 4 | klient → baza | bootstrap (3. z trzech) | `[P]` [?] |
| 17 | klient → baza | bootstrap (2. z trzech) | `[P]` [?] |
| 18 | klient → baza | heartbeat | `[P]` |
| 2 | baza → klient | push stanu (rozdz. 7) | `[R]` |
| 11 | baza → klient | odpowiedź na zadanie (`op=32`) | `[P]` |

---

## 6. Kanał komend (pole 6)

### 6.1 Struktura

```
kind=2, pole 6 = { 2: <opkod>, 7: 0, <parametry…> }
```

`kind=2` i pole 6 występują razem; parametrami są kolejne pola tego komunikatu.

### 6.2 Opkody

| Opkod | Nazwa | Parametry | Status |
|---|---|---|---|
| **16** | jog (strumień prędkości) | `3` = jazda, `4` = obrót, `7` = 0 | `[R]` |
| **6** | przejmij sterowanie | `7` = 0 | `[R]` (skutek) |
| **111** | (nieznany, para z `op=6`) | `7` = 0 | `[P]` |
| **7** | oddaj sterowanie | `7` = 0 | `[R]` (skutek) |
| **32** | zlecenie zadania | `9` = `{3, 4, 5, 11}`, `7` = 0 | `[P]` (adresowanie `[?]`) |

### 6.3 Pole jogu (`op=16`)

| Ścieżka | Typ | Jednostka | Znaczenie | Zakres bezp. |
|---|---|---|---|---|
| `6.2` | varint | — | opkod = 16 | — |
| `6.3` | varint | mm/s | jazda: **+ przód**, − tył | 100–300 `[R]` skala 1:1 |
| `6.4` | varint | mrad/s | obrót: **+ lewo**, − prawo | 100–300 |
| `6.7` | varint | — | 0 (stałe w każdej zaobserwowanej ramce) | — |

Kalibracja `[R]`: komenda `6.3=100` → ~103 mm/s w fazie stałej (1 jednostka = 1 mm/s);
rampa rozruchu bazy ~0,2 s (krótkie ruchy wypadają odpowiednio krótsze).

### 6.4 Przykładowe ramki (hex)

| Ramka | hex | Rozbiór |
|---|---|---|
| jog z panelu: przód 330 | `080215130000001931c44216a10100003209101018ca0220003800` | `kind=2, seq=19, sess, 6={2:16,3:330,4:0,7:0}` |
| jog: obrót w lewo 266 | `0802151f0000001931c44216a1010000320910101800208a023800` | `6={2:16,3:0,4:266,7:0}` |
| jog: obrót w prawo 266 | `080215450000001931c44216a101000032111010180020f6fdffffffffffffff013800` | `6={2:16,3:0,4:−266,7:0}` (10-bajtowy varint) |
| przejmij sterowanie | `08021500000000190000000000000000320410063800` | `6={2:6,7:0}` |
| (para `op=6`) | `080215000000001900000000000000003204106f3800` | `6={2:111,7:0}` |
| oddaj sterowanie | `08021500000000190000000000000000320410073800` | `6={2:7,7:0}` |
| stop (zera) | `0802150000000019000000000000000032081010180020003800` | `6={2:16,3:0,4:0,7:0}` |
| zadanie | `0802156b0000001931c44216a1010000320f102038004a09180220022a01035801` | `6={2:32,7:0,9:{3:2,4:2,5:03,11:1}}` |

W ramkach zapisanych w plikach (`cap/*.hex`) pola `seq` i `session` są zerowane — podmienia je
`sendReplay`.

### 6.5 Częstotliwości

| Źródło | Częstotliwość jogu | Status |
|---|---|---|
| panel Matrix | ~5 Hz (co ~0,2 s) | `[P]` |
| nasze narzędzia | 20 Hz (domyślnie), maks. 50 Hz (`--rate`, przycinane) | — |

---

## 7. Push stanu (typ 2)

Wysyłany cyklicznie (nie na żądanie) jako `kind=1`/`kind=2` z treścią w polu 5. Trafia też
w odpowiedzi na logowanie.

| Ścieżka | Typ | Jednostka | Znaczenie | Status |
|---|---|---|---|---|
| `3.2` | varint | — | faza zadania: **2** = spoczynek, **7** = jazda, **10** = start | `[R]` |
| `3.4.2` | varint | mm | pozycja x | `[R]` |
| `3.4.3` | varint | mm | pozycja y | `[R]` |
| `3.4.7` | varint | mrad | kąt (yaw) | `[R]` |
| `3.7.8` | varint | 10 mm | pozostały dystans do celu (1 jedn. = 1 cm) | `[P]` |
| `3.7.12[n]` | varint ×4 | mm | odcinek trasy (sx, sy, ex, ey) | `[P]` (kolejność pól `[?]`) |
| `3.10` | tekst | — | nazwa mapy (np. `Showroom`) | `[R]` |

Mapowanie na `amr::State` (biblioteka, rozdz. 9.7):

| Pole `State` | Źródło |
|---|---|
| `x_mm`, `y_mm`, `yaw_rad` | `3.4.2`, `3.4.3`, `3.4.7` (mrad → rad) |
| `dist_raw` | `3.7.8` |
| `phase` | `3.2` |
| `map` | `3.10` |
| `route` | `3.7.12[]` |
| `raw` | kopia całego komunikatu stanu |

Format linii HUD (`stateLine`): `x=   -10.0 mm  y=  -267.0 mm  kat=-1.5400  dystans=    0 cm  faza=2  trasa[0]  mapa=Showroom`.

---

## 8. Zadanie (`op=32`) i odpowiedź (typ 11)

**Zlecenie zadania** `[P]` (jedno zaobserwowane):

```
6 = { 2: 32, 7: 0, 9: { 3: 2, 4: 2, 5: 0x03, 11: 1 } }
```

Znaczenie pól `9.3/9.4/9.5/9.11`: **nieustalone** `[?]`. Prawdopodobnie wskazanie trasy/stacji.

**Odpowiedź** po ~8,5 s `[P]`:

```
kind=1, pole 5 = { 1: 11, 14: { 1: 1, 2: 0 } }
```

| Ścieżka | Znaczenie |
|---|---|
| `5.1` | kod odpowiedzi = 11 |
| `5.14.1`, `5.14.2` | `1`, `0` — interpretacja nieustalona `[?]` |

W panelu: wybór stacji + zatwierdzenie; w zrzucie brak pełnego adresowania celu.
**Do rozgryzienia potrzebny zrzut z dziennikiem działań** (patrz `15-baza-jezdna-przewodnik.md`, dodatek B).

---

## 9. Biblioteka `include/amr_matrix.hpp`

Nagłówek-only (C++17, bez zależności), wszystko w przestrzeni `amr`. `amr::kVersion` = `"0.3"`.

### 9.1 Warstwa surowych komunikatów

| Symbol | Sygnatura | Opis |
|---|---|---|
| `Field` | `{uint32_t num; int wire; uint64_t value; std::string bytes;}` | jedno pole protobufa |
| `readVarint` | `bool readVarint(std::string_view b, size_t& i, uint64_t& out)` | dekoder varint |
| `parseMessage` | `bool parseMessage(std::string_view b, std::vector<Field>& out)` | pełny podział komunikatu |
| `findField` | `const Field* findField(const std::vector<Field>& v, uint32_t num, int wire)` | wyszukanie pola o dokładnym numerze **i** typie drutu; `nullptr`, gdy brak |
| `intField` | `int64_t intField(const std::vector<Field>& v, uint32_t num)` | wartość pola varint jako **int64** (obsługa znaku); brak pola = 0 |
| `varintBytes` | `std::string varintBytes(uint64_t)` | odwrotność `readVarint` |

### 9.2 Drzewo i edycja (do podmian pól)

| Symbol | Sygnatura | Opis |
|---|---|---|
| `Node` | `{num, wire, val, str, kids}` | węzeł drzewa; `kids` = podpola |
| `parseTree` | `bool parseTree(std::string_view, std::vector<Node>&)` | body → drzewo |
| `serializeTree` | `std::string serializeTree(const std::vector<Node>&)` | drzewo → body |
| `encodeNode` | `std::string encodeNode(const Node&)` | jeden węzeł → bajty |
| `varintNode` / `stringNode` | `Node varintNode(num, int64_t)` / `Node stringNode(num, const std::string&)` | konstruktory |
| `applyPatch` | `bool applyPatch(std::vector<Node>&, const std::string& spec, std::string& err)` | patrz 9.2.1 |
| `dumpTree` / `dumpBody` | `std::string dumpBody(std::string_view)` | czytelny wydruk drzewa |
| `flattenTree` | `void flattenTree(...)` | drzewo → lista ścieżek |
| `diffBodies` | `std::vector<std::string> diffBodies(a, b, size_t limit = 32)` | różnice pól dwóch body |

#### 9.2.1 Gramatyka `applyPatch` (i `--set`)

```
spec := ścieżka '=' wartość
ścieżka := element ('.' element)*        element := NUMER [ ':' TYP ]
TYP   := v | i (varint, domyślny) | u | z (zigzag) | s (tekst) | f (float32) | d (float64)
```

- Brakujący element pośredni jest **tworzony** (jako podkomunikat, wire=2).
- Jeśli wskazane pole ma już podpola, podmiana kończy się błędem (nie nadpisujemy poddrzewa).
- Działanie jest **względne wobec przekazanego drzewa** — stąd dwa różne „punkty odniesienia"
  w narzędziach (rozdz. 10.4.1, tabela pułapki).

### 9.3 Identyfikacja i budowa ramek

| Symbol | Sygnatura | Opis |
|---|---|---|
| `frameInfo` | `FrameInfo frameInfo(std::string_view body)` | rozbiór koperty: kind/seq/session/type/op |
| `describeFrame` | `std::string describeFrame(std::string_view)` | jednowierszowy opis ramki |
| `encodeEnvelope` | `std::string encodeEnvelope(kind, seq, session, content_field, content)` | budowa koperty |
| `matrixHeader` / `matrixFrame` | — | nagłówek z sesją / pełna ramka z prefiksem długości |
| `loginFrame` | `std::string loginFrame(user, pass_md5_hex)` | gotowa ramka logowania (z prefiksem) |
| `requestFrame` | `std::string requestFrame(seq, session, type)` | gotowe zapytanie o typie `type` |
| `hexEncode` / `hexDecode` | `std::string hexEncode(...)` / `bool hexDecode(..., std::string&)` | hex ↔ bajty; `hexDecode` **pomija znaki nie-hex** (tolerancyjny), zwraca `false` przy nieparzystej liczbie cyfr |

### 9.4 Stan

| Symbol | Opis |
|---|---|
| `State` | `{x_mm, y_mm, yaw_rad, dist_raw, phase, map, route, raw}` + `operator==` |
| `RouteSegment` | `{sx, sy, ex, ey}` w mm (kolejność pól `[?]`) |
| `decodeState(payload, State&)` | payload pola 5 → `State` (zwraca `false`, gdy to nie stan) |
| `stateFromBody(body, State&)` | to samo, ale dla całego body |
| `stateLine(const State&)` | jednowierszowy HUD |

`decodeState` akceptuje payload tylko gdy `payload.1 == 2` (typ pushu stanu).

### 9.5 Wybór ramki z zapisu

```cpp
bool readCaptureSelect(const std::string& path, const std::string& dir /*"UP"|"DOWN"*/,
                       int type, int nth, std::string& hex_out, std::string& info, std::string& err);
```

Iteruje po zapisie, filtruje po kierunku i dopasowaniu `type` **lub** `op`, bierze `nth`-te
trafienie. Wypisuje `info` = `describeFrame`. Błąd np.:
`nie znalazlem ramki dir=UP type=16 nr 1 w cap/panel_paste.log`.

### 9.6 Klient

#### 9.6.1 `WsClient` (niski poziom)

| Metoda | Opis |
|---|---|
| `bool connect(host, port, origin, timeout_ms, std::string& err)` | połączenie (handshake HTTP Upgrade) |
| `bool sendBinary(const std::string&)` | surowa ramka binarna (już z prefiksem) |
| `Recv receive(std::string& msg, int timeout_ms)` | odbiór (`Recv::Ok/Timeout/Closed`) |
| `int nativeFd() const` | deskryptor do własnego `poll()` |
| `void shutdown()` | zamknięcie |

#### 9.6.2 `MatrixClient` (wysoki poziom)

| Metoda | Sygnatura | Opis |
|---|---|---|
| ctor | `explicit MatrixClient(Options)` | konfiguracja |
| `connect` | `bool connect(std::string& err)` | połączenie + wysłanie logowania |
| `logged_in` | `bool logged_in() const` | czy znamy sesję (`session != 0`) |
| `poll` | `Recv poll(int timeout_ms, const StateCb&)` | czekanie na ramki (tylko stan) |
| `pollAny` | `Recv pollAny(int timeout_ms, const FrameCb&, const StateCb&)` | każda ramka + stan; **wywołuje heartbeat** |
| `sendBody` | `bool sendBody(const std::string& body)` | wysyłka gotowego body (uzupełnia nagłówek) |
| `sendRequest` | `bool sendRequest(uint32_t type, const std::string& extra = {}, std::string* effective = nullptr)` | żądanie `{1:type} + extra` |
| `sendReplay` | `bool sendReplay(const std::string& captured_body, std::string* effective = nullptr)` | odtworzenie ramki: **podmienia tylko `seq` i `session`** |
| `close` | `void close()` | zamknięcie |
| `nativeFd` | `int nativeFd() const` | deskryptor (do `poll` razem ze stdin) |
| `options` | `const Options& options() const` | bieżące opcje |

Zachowania automatyczne:
- po pierwszej odpowiedzi (seq 1) wysyła bootstrap **1, 17, 4** (jeśli `bootstrap_requests`);
- `pollAny` wywołuje `maybeHeartbeat()` — `type=18` po `heartbeat_ms` bezczynności;
- każda odebrana ramka idzie do `on_frame`; payload stanu → `on_state`.

#### 9.6.3 `Options`

| Pole | Domyślnie | Znaczenie |
|---|---|---|
| `host` | `192.168.71.50` | adres bazy |
| `port` | `5002` | port |
| `user` | `admin` | login |
| `pass_md5` | MD5(`admin`) | hash hasła w hex |
| `connect_timeout_ms` | `3000` | timeout połączenia |
| `bootstrap_requests` | `true` | wysyłaj 1/17/4 po logowaniu |
| `heartbeat_ms` | `3000` | okres heartbeatu (0 = wyłącz) |

### 9.7 Minimalny przykład (wysyłka jogu)

```cpp
amr::MatrixClient::Options opt;
opt.host = "192.168.71.50";
amr::MatrixClient client(opt);
std::string err;
if (!client.connect(err)) { /* err */ }
while (!client.logged_in()) client.pollAny(100, nullptr, nullptr);

// op=6 i op=111: przejmij sterowanie (hex z cap/panel_op6_enable.hex, panel_op111.hex)
std::string f_enable, f_111, f_jog, f_zero, f_rel;
amr::hexDecode("08021500000000190000000000000000320410063800", f_enable);
amr::hexDecode("080215000000001900000000000000003204106f3800", f_111);
// jog: op=16, vx=150, wz=0 (budujemy od zera)
std::vector<amr::Node> six{ amr::varintNode(2, 16), amr::varintNode(3, 150), amr::varintNode(4, 0), amr::varintNode(7, 0) };
std::vector<amr::Node> env{ amr::varintNode(1, 2), amr::stringNode(6, amr::serializeTree(six)) };
f_jog = amr::serializeTree(env);
amr::hexDecode("0802150000000019000000000000000032081010180020003800", f_zero);  // zera
amr::hexDecode("08021500000000190000000000000000320410073800", f_rel);          // op=7

client.sendReplay(f_enable);
client.sendReplay(f_111);
for (int i = 0; i < 20; ++i) { client.sendReplay(f_jog); client.pollAny(50, nullptr, nullptr); }
client.sendReplay(f_zero);
client.sendReplay(f_zero);
client.sendReplay(f_rel);   // dopiero na koniec!
client.sendReplay(f_rel);
```

---

## 10. Narzędzia — referencja CLI

Wspólne dla wszystkich binarek: `--ip`, `--port`, `--user`, `--pass-md5`, `--help`/`-h`.
Kody wyjścia: **0** sukces · **1** błąd połączenia · **2** błąd użycia/argumentów/danych.

### 10.1 `amr_state_cli` — odczyt stanu

| Opcja | Typ | Domyślnie | Opis |
|---|---|---|---|
| `--seconds S` | liczba | 0 (do Ctrl-C) | ile sekund nasłuchiwać |
| `--all` | flaga | off | każda ramka stanu (bez odsiewania powtórek) |
| `--full` | flaga | off | pełny wydruk drzewa stanu (`state.raw`) |
| `--segments` | flaga | off | wypisz odcinki trasy |

### 10.2 `amr_probe` — podglądacz i próbnik

| Opcja | Typ | Domyślnie | Opis |
|---|---|---|---|
| `--seconds S` | liczba | 0 (z `--type`: 5) | czas nasłuchu |
| `--all` | flaga | off | każda ramka w całości (wyłącza tryb diff) |
| `--type N` | int | — | po zalogowaniu wyślij żądanie o kodzie N |
| `--set a.b=v` | string | — | pole treści żądania (powtarzalne; typy jak 9.2.1) |
| `--repeat N` | int | 1 | ile razy wysłać |
| `--rate H` | liczba | 10 | częstotliwość wysyłki [Hz] |
| `--hex HEX` | string | — | wyślij przechwyconą ramkę (podmienia seq/sesję) |
| `--interactive` | flaga | off | polecenia ze stdin: `type N [a.b=v …]`, `hex HEX`, `dump HEX`, `q` |
| `--dump-hex HEX` | string | — | dekodowanie **offline** (bez łączenia) i wyjście |
| `--dry-run` | flaga | off | pokaż, ale nie wysyłaj |

### 10.3 `amr_proxy` — podsłuch (MITM)

| Opcja | Domyślnie | Opis |
|---|---|---|
| `--listen HOST:PORT` | `0.0.0.0:5002` | gdzie nasłuchuje |
| `--remote HOST:PORT` | `192.168.71.50:5002` | adres bazy |
| `--save PLIK` | — | zapis wszystkich ramek (format: rozdz. 11) |
| `--down-full` | off | pełne drzewo ramek z bazy (poza stanem) |
| `--no-up-full` | off | nie pokazuj pełnego drzewa ramek z panelu |

Ctrl-C kończy pracę i drukuje podsumowanie typów (`UP kind type -> liczba`).
Uwaga operacyjna: uruchamiać jako root i kierować na niego tylko ruch przeglądarki, inaczej
proxy podsłuchuje sam siebie (patrz `15-baza-jezdna-przewodnik.md`, §6, droga B).

### 10.4 `amr_cmd` — wysyłka komend

| Opcja | Typ | Domyślnie | Opis |
|---|---|---|---|
| `--frame HEX\|@plik` | string | — | gotowa ramka (body) |
| `--type N` | int | — | zadanie budowane od zera: treść `{1:N, --set …}` |
| `--from-capture PLIK` | string | — | wybór ramki z zapisu |
| `--dir UP\|DOWN` | string | `UP` | kierunek w zapisie |
| `--nth K` | int | 1 | które trafienie wybrać |
| `--set a.b=v` | string | — | podmiana pola (powtarzalne) |
| `--pre-frame HEX\|@plik` | string | — | ramka wstępna, wysyłana raz przed komendą (powtarzalne) |
| `--stop-frame HEX\|@plik` | string | — | ramka stopu; **można podać kilka** (każda 2×) |
| `--stop-type M` | int | — | stop budowany od zera: treść `{1:M, --stop-set …}` |
| `--stop-set a.b=v` | string | — | pola ramki stopu budowanej od zera |
| `--seconds S` | liczba | 2 (gdy brak `--repeat`) | czas nadawania w pętli; `0` = jednorazowo |
| `--repeat N` | int | 0 (wyłączone) | dokładnie N ramek (nadrzędne nad `--seconds`) |
| `--rate H` | liczba | 20 | częstotliwość; >50 przycinane do 50 z ostrzeżeniem |
| `--hold S` | liczba | 1.0 | przy pojedynczej ramce: zwłoka przed stopem |
| `--echo-state` | flaga | off | drukuj stan bazy przy każdym odczycie |
| `--dry-run` | flaga | off | nic nie wysyła |
| `--yes` | flaga | off | pomija 3-sekundowe odliczanie |

#### 10.4.1 Punkt odniesienia ścieżek `--set` (częsta pomyłka)

| Źródło komendy | Względem czego liczy się ścieżka | Przykład |
|---|---|---|
| `--type N --set …` | **treść** zadania (`{1:N, …}`), opakowana potem w pole 4 | `--type 100 --set 2.1=250` |
| `--frame` / `--from-capture` | **całego body** przechwyconej ramki | `--set 6.3=150` |
| `--stop-type M --stop-set …` | treści ramki stopu | jak `--type` |

Przy ramkach panelu (kanał 6) zawsze używamy drugiej postaci (`--from-capture` + `--set 6.3=…`).

### 10.5 `amr_wasd` — prowadzenie z klawiatury

Źródło ramki (jedno): `--frame HEX|@plik`, `--from-capture PLIK [--type N] [--nth K]`, `--type N`.
Podmiany pól: `--set a.b=@@vx@@`, `--set a.b=@@wz@@` (**wymagane**; `@@vx@@`/`@@wz@@` to znaczniki
wartości bieżącej).

| Opcja | Domyślnie | Opis |
|---|---|---|
| `--dir UP\|DOWN`, `--nth K` | `UP`, 1 | wybór ramki z zapisu |
| `--pre-frame HEX\|@plik` | — | ramki wstępne (np. `op=6`, `op=111`); powtarzalne |
| `--stop-frame HEX\|@plik` | — | **chwilowy stop** (puszczenie klawisza / SPACJA); bez oddawania sterowania; powtarzalne |
| `--release-frame HEX\|@plik` | — | oddanie sterowania (`op=7`) — **tylko przy wyjściu**; powtarzalne |
| `--stop-type N` | — | stop budowany od zera (np. 102 na atrapie) |
| `--no-rearm` | off | nie powtarzaj ramek wstępnych przed nowym ruchem |
| `--verbose` | off | pokazuj ramki przychodzące z bazy (poza push stanu) |
| `--arc` / `--no-arc` | `--arc` | łuki: trzymanie np. W+A jedzie **i** skręca |
| `--speed MM_S` | 300 | prędkość jazdy |
| `--turn MRAD_S` | 400 | prędkość obrotu |
| `--rate HZ` | 20 | częstotliwość (poza 1–50 wraca do 20) |
| `--accel MM_S2` | 900 | narastanie/opadanie prędkości (wygładzanie nasze) |
| `--hold-ms MS` | 350 | okno podtrzymania klawisza |
| `--max-seconds S` | 0 (brak) | awaryjny limit czasu jazdy |
| `--dry-run` | off | bez wysyłki |

**Klawisze:** W/S przód/tył, A/D obrót (strzałki równoważne), SPACJA = stop natychmiastowy
i wyczyszczenie zestawu, X/Q/Ctrl-C = wyjście (wysyła `--release-frame`).

**Kolejność ramek w sesji:**

```
start:  --pre-frame…            (jednorazowo)
ruch:   jog (--rate Hz), wygładzany --accel
        przy pierwszym ruchu po pauzie: re-arm (--pre-frame) ponownie
stop:   --stop-frame ×2 (na puszczenie / SPACJĘ)
wyjście: --stop-frame ×2, potem --release-frame ×2
```

**Semantyka `--arc`:** terminal powtarza tylko ostatnio wciśnięty klawisz; tryb łukowy pamięta
osie (`lat_motion`, `lat_turn`) i utrzymuje cały zestaw, dopóki przychodzą powtórzenia
jakiegokolwiek klawisza z zestawu (okno `--hold-ms` od ostatniego powtórzenia). Po ciszy zestaw
jest czyszczony. `--no-arc`: klasyczne „wygasa każdy klawisz osobno".

**HUD** (2 Hz): `vx= +150 mm/s  wz= +200 mrad/s | x= … y= … kat=… faza=7 | JEDZIE/stoi`.

### 10.6 `baza.sh` — nakładka (najprostsze wejście)

| Podkomenda | Działanie |
|---|---|
| `wasd [--speed … --turn … --no-arc …]` | `amr_wasd` z kompletem ramek, domyślnie 150 mm/s, 200 mrad/s |
| `przod [mm/s] [s]` / `tyl` | jazda; domyślnie 100 mm/s przez 1 s |
| `lewo [mrad/s] [s]` / `prawo` | obrót; domyślnie 200 mrad/s przez 1 s |
| `stop` | zera ×2 + `op=7` ×2 (natychmiastowe zatrzymanie) |
| `stan [s]` | podgląd stanu (domyślnie 5 s) |
| `podglad <reszta>` | to samo z `--dry-run` (nic nie wysyła) |

Zmienne środowiskowe: `AMR_IP` (domyślnie `192.168.71.50`), `AMR_PORT` (domyślnie `5002`) —
do testów na atrapie: `AMR_IP=127.0.0.1 AMR_PORT=5003 ./baza.sh przod 100 1`.
Skrypt sam dokłada: `--pre-frame @cap/panel_op6_enable.hex --pre-frame @cap/panel_op111.hex`,
`--from-capture cap/panel_paste.log --type 16 --nth 1`, `--stop-frame @cap/panel_stop_zero.hex`,
`--release-frame @cap/panel_op7_release.hex`.

### 10.7 `amr_selftest` i cele `make`

| Cel | Efekt |
|---|---|
| `make` | budowa wszystkich binarek (C++17, `-O2 -Wall -Wextra`) |
| `make test` | `./amr_selftest` — testy rdzenia (parser, koperta, patche, stan) |
| `make bench` | atrapa bazy na porcie 5003 (`--auto-mission`) |
| `make clean` | usuwa binarki |

### 10.8 Narzędzia w Pythonie/JS

| Plik | Opcje / API | Opis |
|---|---|---|
| `tools/bench_server.py` | `--port` (5003), `--auto-mission` | atrapa bazy: logowanie, sesja, push stanu, jog `op=16`, jawne zera, symulacja ruchu |
| `tools/analyze_capture.py` | `FILE [--dir UP\|DOWN] [--timeline N] [--min-count K]` | grupy ramek po kształcie + serie czasowe wartości |
| `tools/panel_hook.js` | `saveCapture()`, fallback `copy(__amrFrames.join("\n"))` | haczyk do konsoli DevTools; zapis `panel_capture.log` |
| `tools/test_panel_hook.js` | — | test haczyka w node (3 przypadki) |
| `tools/test_wasd_pty.py` | `--keys w=1.5,=1.0,d=1.0 --pause S -- CMD` | sterowanie `amr_wasd` przez pty (pusty klawisz `=1.0` = pauza) |

---

## 11. Format plików danych

### 11.1 Zapis przechwycenia (`amr_proxy --save`, `panel_hook.js`)

Jeden wiersz = jedna ramka; komentarze `#`.

```
+<czas_s> <UP|DOWN> <hex_body>
#        <UP|DOWN> <describeFrame…>            (opcjonalny komentarz — proxy)
```

- `UP` = wysłane przez panel (komendy), `DOWN` = od bazy (odpowiedzi/push).
- `hex_body` = całe body bez prefiksu długości (rozdz. 3.2).
- `#` i wiersze puste są pomijane przez czytniki.
- `panel_hook.js` pisze ten sam format; `saveCapture()` pobiera plik `panel_capture.log`.

### 11.2 Plik pojedynczej ramki (`cap/*.hex`)

Czysty hex (bez `+`, bez `#`), może być w wielu wierszach. Czytany przez `--frame @plik`,
`--pre-frame @plik`, `--stop-frame @plik`, `--release-frame @plik`.

**Uwaga implementacyjna:** przy `--frame @plik` bufor hex jest czyszczony przed czytaniem
(naprawiony błąd: nazwa pliku doklejała się do heksu, gdy bufor nie był pusty).

### 11.3 Pliki w `cap/`

| Plik | Zawartość |
|---|---|
| `panel_paste.log` | zrzut z hali: 119 ramek (jazda panelem + heartbeat) |
| `panel_op6_enable.hex` | `op=6` przejmij sterowanie |
| `panel_op111.hex` | `op=111` (para z `op=6`) |
| `panel_stop_zero.hex` | `op=16` z zerami — chwilowy stop |
| `panel_op7_release.hex` | `op=7` oddaj sterowanie |
| `panel_jog_fwd.hex` | jog z panelu (przód 330) |
| `bench.log` | przykładowy zapis z atrapy |
| `jog_type100.hex` | ramka jogu protokołu atrapy (typ 100) |
| `panel_analiza.txt` | wynik `analyze_capture.py` + potwierdzone znaczenia |

---

## 12. Wymagania operacyjne i bezpieczeństwo

**MUST** (bez tego nie uruchamiać na robocie):

1. Fizyczny STOP / E-stop w zasięgu ręki operatora.
2. Wolna przestrzeń ≥ 1–2 m wokół bazy; brak progów i przeszkód w zasięgu ruchu.
3. `op=7` wysyłany **wyłącznie** na końcu sesji (nigdy między ruchami).
4. Po każdym ruchu jawne zera (`--stop-frame`); nie polegać na deadmanie bazy.
5. Panel Matrix zamknięty podczas sterowania naszym klientem.
6. Pierwsze próby na robocie: ≤ 150 mm/s, ≤ 2 s, po uprzednim `--dry-run`.

**SHOULD:**

7. Heartbeat ≥ 3 s (domyślnie włączony) — jak panel.
8. Re-arm przed każdym nowym ruchem w trybie klawiatury (domyślnie włączony).
9. `--rate` ≤ 20–50 Hz; nie nadawać jogu szybciej niż przyjmuje baza.
10. Trzymać wersję narzędzi zgodną z wersją protokołu (rozdz. 13).

Zachowanie awaryjne narzędzi: Ctrl-C → ramki stopu; utrata połączenia → komunikat i wyjście;
`--max-seconds` → twardy limit czasu jazdy (tylko `amr_wasd`).

---

## 13. Wersje i historia zmian

`amr::kVersion` (w `describeFrame`/`usage()`): **0.3**.

| Wersja | Zakres | Najważniejsze zmiany |
|---|---|---|
| 0.1 | odczyt | transport, koperta, logowanie, push stanu (kod bazowy użytkownika) |
| 0.2 | wysyłka | `encodeEnvelope`, `sendReplay`, `applyPatch`, dekoder ramek, `amr_cmd`/`amr_probe`/`amr_proxy`, atrapa, testy |
| 0.3 | sterowanie panelem | rozpoznanie kanału komend (pole 6, `op=16/6/7/111/32`), heartbeat `type=18`, `--pre-frame`, wielokrotny `--stop-frame`, `--release-frame`, re-arm, `--arc`, `--verbose`, `analyze_capture.py`, `baza.sh` |

---

## 14. Elementy niezweryfikowane i ograniczenia

| Element | Stan | Jak zweryfikować |
|---|---|---|
| `op=111` — znaczenie | nieznane | porównać zachowanie bazy z/bez wysyłki |
| `op=32` — adresowanie celu (pola `9.3/9.4/9.5/9.11`) | nieznane | zrzut z dziennikiem działań (dodatek B w `15-baza-jezdna-przewodnik.md`) |
| Deadman dla naszej sesji (czy stop po urwaniu jogu) | nieznane | uciąć strumień jogu i obserwować |
| Sterowanie równoległe z panelem | nieznane | test na atrapie, potem na robocie z asekuracją |
| Kolejność pól `3.7.12[n]` (sx,sy,ex,ey) | hipoteza | porównać trasę w panelu z ramkami stanu |
| Jednostka `3.7.8` (1 = 10 mm) | wniosek z jednego przypadku | korelacja z przebytym dystansem |
| `op=16` z obiema osiami naraz | `[A]` na atrapie; na robocie do potwierdzenia | `baza.sh wasd` + W+A |
| Obrót: skala mrad/s | `[?]` — oczekiwane `dkat ≈ +0,2 rad` dla 200 mrad/s × 1 s | `baza.sh lewo 200 1` |
| Limity przyspieszenia bazy | nieznane (nasze `--accel` to wygładzanie po naszej stronie) | testy narastających wartości |
| Znaczenie typów żądań 1 / 4 / 17 | nieznane | porównać odpowiedzi z/bez wysyłki |
| Konflikt sesji (panel + my) | nieznane | — |

---

## 15. Szybkie odniesienia

### 15.1 Zadanie → komenda

| Chcę… | Komenda |
|---|---|
| sprawdzić stan | `./amr_state_cli --ip 192.168.71.50 --seconds 5` |
| pojechać prosto 30 cm | `./baza.sh przod 150 2` |
| obrócić w lewo ~11° | `./baza.sh lewo 200 1` |
| prowadzić z klawiatury | `./baza.sh wasd` |
| jechać po łuku | `./baza.sh wasd` + trzymać W+A |
| natychmiast stop | `./baza.sh stop` |
| sprawdzić bez wysyłki | `./baza.sh podglad przod 100 1` |
| rozłożyć ramkę na pola | `./amr_probe --dump-hex <HEX>` |
| znaleźć komendę w zrzucie | `python3 tools/analyze_capture.py cap/panel_paste.log` |
| wybrać ramkę i obejrzeć | `./amr_cmd --from-capture cap/panel_paste.log --type 16 --nth 1 --dry-run` |
| test bez robota | `make bench` + `AMR_IP=127.0.0.1 AMR_PORT=5003 …` |

### 15.2 Ścieżki pól — najważniejsze

| Ścieżka | Znaczenie | Uwaga |
|---|---|---|
| `6.3` | jog: jazda | mm/s, + przód |
| `6.4` | jog: obrót | mrad/s, + lewo |
| `6.2` | opkod | 16 jog, 6/7/111 sterowanie, 32 zadanie |
| `3.2` | faza | 2 spoczynek, 7 jazda, 10 start |
| `3.4.2` / `3.4.3` / `3.4.7` | x / y / kąt | mm, mm, mrad |
| `3.10` | mapa | tekst |

**Pamiętaj o podstawie ścieżki** (`--type` → treść; `--frame`/`--from-capture` → całe body).

---

*Dokumentacja referencyjna zestawu narzędzi do bazy Helios. Metodyka ustaleń i katalog problemów:
`15-baza-jezdna-przewodnik.md`. Kod: `include/amr_matrix.hpp`, `src/`, `tools/`, `baza.sh`.*
