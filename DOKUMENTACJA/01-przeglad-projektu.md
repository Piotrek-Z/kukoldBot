> 📄 **Na podstawie:** `MasterPrompt.md` · wersja z poprawek — zmiany wymienione w bloku „✏️ Poprawki" · 10 października 2026

# Przegląd projektu

Zajmuję się robotem humanoidalnym **Helios** od Rokae. Używamy jak na razie **ROS**, **Robot Assist**
i **C++**.

> ✏️ **Poprawki 10.10.2026 (ten plik):**
> 1. Dodano tytuł i struktury — oryginał był jednym akapitem z listą linków.
> 2. Lista linków do dokumentacji producenta **przeniesiona** do [`02-linki-zewnetrzne.md`](02-linki-zewnetrzne.md),
>    żeby te same adresy nie występowały w dwóch miejscach. Treść oryginalnego opisu projektu
>    została zachowana bez zmian.

## Z czego składa się robot

| Element | Ile | Uwaga |
|---|---|---|
| Ramiona | 2 × 7 osi | manipulatory xMate, klasy SDK `xMateErProRobot` |
| Tors | 4 osie | model `TaiHu`, klasa SDK `PCB4Robot` |
| Głowa | 2 osie (pan/tilt) | w SDK jako osie zewnętrzne |
| Baza jezdna | podwozie kołowe | **osobny system** (Matrix OS), własny protokół |
| Komputer pokładowy | NVIDIA Jetson | Ubuntu + ROS 2, adres wewnętrzny `192.168.71.51` |

Szczegóły architektury i adresy: [`04-robot-rozwiazywanie-problemow.md`](04-robot-rozwiazywanie-problemow.md) §1,
[`10-teleoperacja-tracker.md`](10-teleoperacja-tracker.md) §1.2,
[`11-teleoperacja-helios-probe.md`](11-teleoperacja-helios-probe.md).

## Stos technologiczny

| Warstwa | Technologia | Gdzie opisane |
|---|---|---|
| Sterownik robota | xCore / RobotAssist | [`04`](04-robot-rozwiazywanie-problemow.md) |
| Oprogramowanie na robocie | ROS 2 (Humble), C++ | [`07`](07-sensory-kamery.md)–[`13`](13-programy-cpp.md) |
| Aplikacja operatora | C++ / Qt 6 / QML / OpenCV (Windows) | [`10`](10-teleoperacja-tracker.md), [`12`](12-teleoperacja-frontend-hud.md) |
| Baza jezdna | WebSocket + protobuf, narzędzia C++ | [`14`](14-baza-jezdna-dokumentacja.md)–[`16`](16-baza-jezdna-protokol.md) |

## Dokumentacja producenta

Linki do oficjalnej dokumentacji Rokae są w [`02-linki-zewnetrzne.md`](02-linki-zewnetrzne.md).
