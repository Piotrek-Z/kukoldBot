> 📄 **Na podstawie:** `Helios_kamery.md` · treść bez zmian (dopasowano nazwę pliku i linki wewnętrzne) · 10 października 2026

# Przewodnik Uruchomienia Kamer i Strumieniowania Obrazu: Rokae Helios (ROS 2 / Orbbec / USB Cam / Web Video Server)

Kompletny podręcznik uruchomienia kamery głębi **Orbbec Gemini 335L** oraz dodatkowych kamer USB w robocie **Rokae Helios**, wraz z instrukcją bezprzewodowego przesyłania obrazu do przeglądarki WWW na komputerze, mapowaniem urządzeń `/dev/video*` oraz rozwiązaniami typowych problemów.

---

## Spis Treści

1. [Architektura Wizyjna Robota Helios](#1-architektura-wizyjna-robota-helios)
2. [Instalacja Wymaganych Pakietów i Konfiguracja Uprawnień](#2-instalacja-wymaganych-pakiet%C3%B3w-i-konfiguracja-uprawnie%C5%84)
   - [2.1 Instalacja pakietów ROS 2](#21-instalacja-pakiet%C3%B3w-ros-2)
   - [2.2 Konfiguracja reguł udev dla Orbbec](#22-konfiguracja-regu%C5%82-udev-dla-orbbec)
   - [2.3 Weryfikacja wykrycia kamery 3D](#23-weryfikacja-wykrycia-kamery-3d)
3. [Uruchomienie Kamery Głównej Orbbec Gemini 335L (Procedura Dwóch Terminali)](#3-uruchomienie-kamery-g%C5%82%C3%B3wnej-orbbec-gemini-335l-procedura-dw%C3%B3ch-terminali)
   - [3.1 Terminal 1: Uruchomienie sterownika kamery 3D](#31-terminal-1-uruchomienie-sterownika-kamery-3d)
   - [3.2 Terminal 2: Uruchomienie Web Video Server](#32-terminal-2-uruchomienie-web-video-server)
   - [3.3 Podgląd obrazu na żywo w przeglądarce](#33-podgl%C4%85d-obrazu-na-%C5%BCywo-w-przegl%C4%85darce)
4. [Błąd cv_bridge przy podglądzie głębi [16UC1] – Wyjaśnienie i Rozwiązanie](#4-b%C5%82%C4%85d-cv_bridge-przy-podgl%C4%85dzie-g%C5%82%C4%99bi-16uc1--wyja%C5%9Bnienie-i-rozwi%C4%85zanie)
   - [4.1 Dlaczego pojawia się ten błąd?](#41-dlaczego-pojawia-si%C4%99-ten-b%C5%82%C4%85d)
   - [4.2 Jak poprawnie odczytywać i oglądać głębię?](#42-jak-poprawnie-odczytywa%C4%87-i-ogl%C4%85da%C4%87-g%C5%82%C4%99bi%C4%99)
5. [Uruchamianie Dodatkowych Kamer USB (Podwozie i Korpus)](#5-uruchamianie-dodatkowych-kamer-usb-podwozie-i-korpus)
   - [5.1 Mapowanie urządzeń w systemie (v4l2-ctl)](#51-mapowanie-urz%C4%85dze%C5%84-w-systemie-v4l2-ctl)
   - [5.2 Zasada numeracji: strumień wideo vs węzeł metadanych](#52-zasada-numeracji-strumie%C5%84-wideo-vs-w%C4%99ze%C5%82-metadanych)
   - [5.3 Obsługiwane formaty pikseli w sterowniku usb_cam](#53-obs%C5%82ugiwane-formaty-pikseli-w-sterowniku-usb_cam)
   - [5.4 Komendy uruchomienia kamer USB](#54-komendy-uruchomienia-kamer-usb)
6. [Ściąga z Komend (Quick Reference)](#6-%C5%9Aci%C4%85ga-z-komend-quick-reference)
7. [Przydatne Linki i Dokumentacja](#7-przydatne-linki-i-dokumentacja)

---

## 1. Architektura Wizyjna Robota Helios

Wszystkie sensory wizyjne w robocie Helios są podłączone bezpośrednio do komputera pokładowego **NVIDIA Jetson** (`10.111.169.242`):

| Urządzenie | Model | Port USB / ID | Rola w systemie |
| :--- | :--- | :--- | :--- |
| **Kamera 3D (Głowa)** | **Orbbec Gemini 335L** | `2bc5:0804` (USB 3.2) | Kolor RGB, mapa głębi (Depth), sensory podczerwieni (IR), IMU 6-DoF |
| **Kamera USB 1** | ARC USB Webcam | `/dev/video8` | Omijanie przeszkód / podwozie |
| **Kamera USB 2** | ARC USB Webcam | `/dev/video10` | Omijanie przeszkód / podwozie |
| **Kamera USB 3** | Sunplus USB Camera | `/dev/video12` | Podgląd otoczenia / korpus |

---

## 2. Instalacja Wymaganych Pakietów i Konfiguracja Uprawnień

Wszystkie poniższe komendy wykonaj na Jetsonie (`std@10.111.169.242`):

### 2.1 Instalacja pakietów ROS 2

Zainstaluj oficjalny sterownik Orbbec, sterownik kamer USB, serwer strumieniowania HTTP oraz narzędzia V4L:

```bash
sudo apt update
sudo apt install -y ros-humble-orbbec-camera \
                    ros-humble-orbbec-description \
                    ros-humble-web-video-server \
                    ros-humble-usb-cam \
                    v4l-utils
```

### 2.2 Konfiguracja reguł udev dla Orbbec

Kamery głębi wymagają uprawnień do surowego portu USB (bez tego sterownik zgłosi brak dostępu):

```bash
sudo cp /opt/ros/humble/share/orbbec_camera/udev/99-obsensor-libusb.rules /etc/udev/rules.d/ 2>/dev/null || \
sudo cp /opt/ros/humble/share/orbbec_camera/scripts/99-obsensor-libusb.rules /etc/udev/rules.d/ 2>/dev/null

sudo udevadm control --reload-rules && sudo udevadm trigger
```

### 2.3 Weryfikacja wykrycia kamery 3D

```zsh
source /opt/ros/humble/setup.zsh
ros2 run orbbec_camera list_devices_node
```

**Oczekiwany wynik:** Wyświetlenie informacji o urządzeniu: `name: Orbbec Gemini 335L`, `pid: 0x0804`, `connection: USB3.2`, wersja firmware'u oraz lista dostępnych presetów (`Default`, `High Accuracy` itp.).

---

## 3. Uruchomienie Kamery Głównej Orbbec Gemini 335L (Procedura Dwóch Terminali)

> ⚠️ **ZASADA DZIAŁANIA:**  
> Do przesyłania obrazu na komputer potrzebujesz **dwóch niezależnych okien terminala PuTTY** działających jednocześnie.  
> **Nie wolno wciskać `Ctrl+C` w pierwszym oknie**, ponieważ wyłącza to fizyczne zasilanie matrycy kamery i serwer wideo nie będzie miał czego nadawać!

---

### 3.1 Terminal 1: Uruchomienie sterownika kamery 3D

W pierwszym oknie PuTTY:

```zsh
source /opt/ros/humble/setup.zsh
ros2 launch orbbec_camera gemini_330_series.launch.py
```

*Po chwili pojawi się komunikat: `Initialize device cost: ... ms`. **Pozostaw to okno otwarte i działające w tle!***

---

### 3.2 Terminal 2: Uruchomienie Web Video Server

Otwórz drugie okno PuTTY do Jetsona (`std@10.111.169.242`):

```zsh
source /opt/ros/humble/setup.zsh
ros2 run web_video_server web_video_server
```

*W terminalu pojawi się: `Waiting For connections on 0.0.0.0:8080`.*

---

### 3.3 Podgląd obrazu na żywo w przeglądarce

Na swoim laptopie (połączonym z siecią robota) otwórz przeglądarkę internetową:

1. **Lista wszystkich tematów kamer:**
   ```text
   http://10.111.169.242:8080/
   ```
2. **Płynny podgląd obrazu RGB (kolor):**
   ```text
   http://10.111.169.242:8080/stream?topic=/camera/color/image_raw
   ```
   *Obraz z głowy robota pojawi się natychmiast na żywo.*

---

## 4. Błąd cv_bridge przy podglądzie głębi [16UC1] – Wyjaśnienie i Rozwiązanie

Gdy w `web_video_server` klikniesz na temat głębi `/camera/depth/image_raw`, w Terminalu 2 pojawia się błąd:

```text
[ERROR] [web_video_server.mjpeg_streamer]: cv_bridge exception: [16UC1] is not a color format. but [bgr8] is. The conversion does not make sense
[INFO] [web_video_server]: Removed Stream: /camera/depth/image_raw
```

### 4.1 Dlaczego pojawia się ten błąd?

1. **Różnica formatów:**
   * Obraz kolorowy (`/camera/color/image_raw`) używa formatu **`RGB8` / `BGR8`** (trzy kanały po 8 bitów na kolor: 0–255). Taki obraz można bez problemu skompresować do klatki JPEG/MJPEG i wyświetlić w przeglądarce.
   * Obraz głębi (`/camera/depth/image_raw`) ma format **`16UC1`** (*16-bit Unsigned, 1 Channel*). Każdy piksel to **pojedyncza 16-bitowa liczba oznaczająca fizyczną odległość od kamery w milimetrach** (od 0 do 65 535 mm).
2. **Brak sensu bezpośredniej konwersji:**
   * Przeglądarka internetowa oraz odtwarzacze wideo (MJPEG) nie obsługują 16-bitowych surowych liczb jako klatek wideo.
   * Moduł `cv_bridge` próbuje przekonwertować tablicę liczb milimetrów na standardowy kolorowy piksel `BGR8`. Zwraca błąd, ponieważ bez zdefiniowania reguły (jaka odległość odpowiada jakiemu kolorowi) bezpośrednia konwersja jest matematycznie niemożliwa.

### 4.2 Jak poprawnie odczytywać i oglądać głębię?

* **Do podglądu wideo na żywo w przeglądarce:** Należy używać strumienia kolorowego:
  ```text
  http://10.111.169.242:8080/stream?topic=/camera/color/image_raw
  ```
* **Statyczny podgląd klatki głębi w przeglądarce (PNG Snapshot):**
  Format PNG w przeciwieństwie do JPEG obsługuje 16-bitową głębię. Wpisz w przeglądarkę:
  ```text
  http://10.111.169.242:8080/snapshot?topic=/camera/depth/image_raw
  ```
* **Pełna wizualizacja 3D (RVIZ2):**
  Surowe dane głębi (`/camera/depth/image_raw`) oraz wygenerowana z nich trójwymiarowa chmura punktów (`/camera/depth/points`) są przeznaczone do bezpośredniej wizualizacji w programie **RViz2** na stacji roboczej lub do algorytmów nawigacji i omijania przeszkód (Costmap 2D/3D).

---

## 5. Uruchamianie Dodatkowych Kamer USB (Podwozie i Korpus)

Oprócz kamery głębi, Helios posiada 3 dodatkowe kamery USB.

### 5.1 Mapowanie urządzeń w systemie (v4l2-ctl)

Aby sprawdzić przypisanie kamer do plików urządzeń w systemie, wpisz:

```zsh
v4l2-ctl --list-devices
```

Wynik w Twoim systemie:
```text
Orbbec Gemini 335L:
        /dev/video0, /dev/video1, /dev/video2, /dev/video3

USB Webcam (ARC):
        /dev/video8, /dev/video9

USB Webcam (ARC):
        /dev/video10, /dev/video11

USB Camera (Sunplus):
        /dev/video12, /dev/video13
```

---

### 5.2 Zasada numeracji: strumień wideo vs węzeł metadanych

W systemie Linux każda kamera USB tworzy **parę urządzeń**:
* **Numer parzysty (`/dev/video8`, `/dev/video10`, `/dev/video12`):**  
  To jest **właściwy strumień wideo** pobierany z sensora kamery. Zawsze wskazuj ten numer!
* **Numer nieparzysty (`/dev/video9`, `/dev/video11`, `/dev/video13`):**  
  To jest węzeł metadanych UVC (parametry ekspozycji, diagnostyka). **Próba otwarcia numeru nieparzystego jako kamery zakończy się błędem!**

---

### 5.3 Obsługiwane formaty pikseli w sterowniku usb_cam

Sterownik `usb_cam` w ROS 2 przyjmuje ściśle zdefiniowane nazwy formatów:

| Format w `usb_cam` | Zastosowanie |
| :--- | :--- |
| **`yuyv`** | Nieskompresowany format koloru YUV 4:2:2 (domyślny i zalecany dla większości kamer USB) |
| **`mjpeg2rgb`** | Sprzętowo kompresowany strumień MJPEG (najmniejsze obciążenie magistrali USB) |
| **`mono8`** | Obraz czarno-biały (8-bitowy). *(Uwaga: format nazywa się `mono8`, a nie `grey`!)* |

---

### 5.4 Komendy uruchomienia kamer USB

Wybierz kamerę, którą chcesz włączyć:

#### Kamera podwozia 1 (`/dev/video8`):
```zsh
source /opt/ros/humble/setup.zsh
ros2 run usb_cam usb_cam_node_exe --ros-args \
  -p video_device:=/dev/video8 \
  -p pixel_format:=yuyv \
  -p image_width:=640 \
  -p image_height:=480
```

#### Kamera podwozia 2 (`/dev/video10`):
```zsh
source /opt/ros/humble/setup.zsh
ros2 run usb_cam usb_cam_node_exe --ros-args \
  -p video_device:=/dev/video10 \
  -p pixel_format:=yuyv \
  -p image_width:=640 \
  -p image_height:=480
```

#### Kamera szerokokątna Sunplus (`/dev/video12`):
```zsh
source /opt/ros/humble/setup.zsh
ros2 run usb_cam usb_cam_node_exe --ros-args \
  -p video_device:=/dev/video12 \
  -p pixel_format:=mjpeg2rgb \
  -p image_width:=640 \
  -p image_height:=480
```

*Obraz z kamery USB natychmiast pojawi się w przeglądarce pod adresem:*
```text
http://10.111.169.242:8080/stream?topic=/image_raw
```

---

## 6. Ściąga z Komend (Quick Reference)

| Czynność | Komenda |
| :--- | :--- |
| **1. Lista fizycznych kamer** | `v4l2-ctl --list-devices` |
| **2. Weryfikacja Orbbec 3D** | `ros2 run orbbec_camera list_devices_node` |
| **3. Start kamery 3D (Terminal 1)** | `ros2 launch orbbec_camera gemini_330_series.launch.py` |
| **4. Start serwera WWW (Terminal 2)** | `ros2 run web_video_server web_video_server` |
| **5. Start kamery USB (np. video8)** | `ros2 run usb_cam usb_cam_node_exe --ros-args -p video_device:=/dev/video8 -p pixel_format:=yuyv` |
| **6. Podgląd w przeglądarce** | `http://10.111.169.242:8080/stream?topic=/camera/color/image_raw` |

---

## 7. Przydatne Linki i Dokumentacja

* **OrbbecSDK ROS 2 Wrapper:** [https://github.com/orbbec/OrbbecSDK_ROS2](https://github.com/orbbec/OrbbecSDK_ROS2)
* **Dokumentacja parametrów Orbbec Gemini:** [https://orbbec.github.io/OrbbecSDK_ROS2/en/source/camera_devices/5_advanced_guide/advanced_guide.html](https://orbbec.github.io/OrbbecSDK_ROS2/en/source/camera_devices/5_advanced_guide/advanced_guide.html)
* **Pakiet ROS 2 usb_cam:** [https://index.ros.org/p/usb_cam/](https://index.ros.org/p/usb_cam/)
* **Pakiet Web Video Server:** [https://index.ros.org/p/web_video_server/](https://index.ros.org/p/web_video_server/)
