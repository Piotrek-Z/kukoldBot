# Przewodnik Uruchomienia Kamer i Strumieniowania Obrazu: Rokae Helios (ROS 2 / Orbbec / Web Video Server)

Kompletny podręcznik uruchomienia kamery głębi **Orbbec Gemini 335L** oraz dodatkowych kamer USB w robocie **Rokae Helios**, wraz z instrukcją bezprzewodowego przesyłania obrazu na komputer z systemem Windows (przeglądarka WWW, OpenCV, Python).

---

## Spis Treści

1. [Architektura Wizyjna Robota Helios](#1-architektura-wizyjna-robota-helios)
2. [Instalacja i Konfiguracja Sterowników Orbbec w ROS 2](#2-instalacja-i-konfiguracja-sterownik%C3%B3w-orbbec-w-ros-2)
   - [2.1 Instalacja pakietu OrbbecSDK ROS 2](#21-instalacja-pakietu-orbbecsdk-ros-2)
   - [2.2 Konfiguracja uprawnień USB (reguły udev)](#22-konfiguracja-uprawnie%C5%84-usb-regu%C5%82y-udev)
   - [2.3 Test wykrycia kamery](#23-test-wykrycia-kamery)
3. [Uruchomienie Kamery Głównej (Gemini 335L)](#3-uruchomienie-kamery-g%C5%82%C3%B3wnej-gemini-335l)
   - [3.1 Polecenie startowe](#31-polecenie-startowe)
   - [3.2 Dostępne tematy ROS 2 (Topics)](#32-dost%C4%99pne-tematy-ros-2-topics)
4. [Strumieniowanie Obrazu przez Sieć (Web Video Server)](#4-strumieniowanie-obrazu-przez-sie%C4%87-web-video-server)
   - [4.1 Dlaczego Web Video Server?](#41-dlaczego-web-video-server)
   - [4.2 Instalacja i uruchomienie serwera](#42-instalacja-i-uruchomienie-serwera)
   - [4.3 Podgląd w przeglądarce na laptopie](#43-podgl%C4%85d-w-przegl%C4%85darce-na-laptopie)
5. [Odbiór i Przetwarzanie Obrazu we Własnej Aplikacji (Python + OpenCV)](#5-odbi%C3%B3r-i-przetwarzanie-obrazu-we-w%C5%82asnej-aplikacji-python--opencv)
6. [Obsługa Pozostałych Kamer USB (Podwozie / Otoczenie)](#6-obs%C5%82uga-pozosta%C5%82ych-kamer-usb-podwozie--otoczenie)
7. [Rozwiązywanie Problemów (Troubleshooting)](#7-rozwi%C4%85zywanie-problem%C3%B3w-troubleshooting)
8. [Przydatne Linki i Źródła](#8-przydatne-linki-i-%C5%BAr%C3%B3d%C5%82a)

---

## 1. Architektura Wizyjna Robota Helios

Wszystkie sensory wizyjne w robocie Helios są wpięte bezpośrednio do komputera pokładowego **NVIDIA Jetson** (`10.111.169.242`):

| Urządzenie | Model | ID USB (`lsusb`) | Przeznaczenie |
| :--- | :--- | :--- | :--- |
| **Kamera 3D (Głowa)** | **Orbbec Gemini 335L** | `2bc5:0804` | Stereowizja aktywna, mapa głębi (Depth), chmura punktów 3D, IMU 6-DoF |
| **Kamera szerokokątna** | Sunplus USB Camera | `1bcf:0b15` | Podgląd otoczenia / korpus |
| **Kamery pomocnicze (x2)** | ARC International Webcam | `05a3:2b01` | Omijanie przeszkód w podwoziu / inspekcja |

---

## 2. Instalacja i Konfiguracja Sterowników Orbbec w ROS 2

Zaloguj się przez PuTTY na Jetsona (`std@10.111.169.242`) i wykonaj poniższe kroki.

### 2.1 Instalacja pakietu OrbbecSDK ROS 2

Dla dystrybucji ROS 2 **Humble** dostępne są oficjalne pakiety binarne:

```bash
sudo apt update
sudo apt install -y ros-humble-orbbec-camera ros-humble-orbbec-description
```

### 2.2 Konfiguracja uprawnień USB (reguły udev)

Kamery głębi wymagają specjalnych reguł w systemie Linux, aby użytkownik bez praw roota mógł odczytywać strumienie binarne i kalibracyjne z magistrali USB:

```bash
# Skopiowanie reguł dostępu USB do katalogu systemowego
sudo cp /opt/ros/humble/share/orbbec_camera/udev/99-obsensor-libusb.rules /etc/udev/rules.d/ 2>/dev/null || \
sudo cp /opt/ros/humble/share/orbbec_camera/scripts/99-obsensor-libusb.rules /etc/udev/rules.d/ 2>/dev/null

# Przeładowanie reguł udev
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### 2.3 Test wykrycia kamery

Sprawdź, czy sterownik poprawnie komunikuje się z kamerą przez USB:

```zsh
source /opt/ros/humble/setup.zsh
ros2 run orbbec_camera list_devices_node
```

**Oczekiwany wynik:** Na ekranie wyświetli się nazwa kamery `Gemini 335L`, jej numer seryjny oraz wersja oprogramowania układowego (firmware).

---

## 3. Uruchomienie Kamery Głównej (Gemini 335L)

### 3.1 Polecenie startowe

Zgodnie z oficjalną dokumentacją Orbbec, dla modeli serii 330/335L dedykowany jest plik startowy **`gemini_330_series.launch.py`**:

```zsh
source /opt/ros/humble/setup.zsh
ros2 launch orbbec_camera gemini_330_series.launch.py
```

#### Przydatne parametry opcjonalne:
Możesz dostosować rozdzielczość i liczbę klatek, aby nie obciążać łącza Wi-Fi:
```zsh
ros2 launch orbbec_camera gemini_330_series.launch.py \
  color_width:=640 \
  color_height:=480 \
  color_fps:=30 \
  enable_point_cloud:=true
```

### 3.2 Dostępne tematy ROS 2 (Topics)

Gdy węzeł kamery działa, w nowym oknie terminala możesz podejrzeć publikowane tematy:

```zsh
ros2 topic list | grep camera
```

Najważniejsze strumienie:
* **`/camera/color/image_raw`** – surowy obraz kolorowy RGB (`sensor_msgs/msg/Image`)
* **`/camera/depth/image_raw`** – mapa odległości / głębia w milimetrach (`sensor_msgs/msg/Image`, format 16-bit)
* **`/camera/depth/points`** – pełna trójwymiarowa chmura punktów (`sensor_msgs/msg/PointCloud2`)
* **`/camera/gyro/sample`** & **`/camera/accel/sample`** – odczyty z wbudowanego akcelerometru i żyroskopu

---

## 4. Strumieniowanie Obrazu przez Sieć (Web Video Server)

### 4.1 Dlaczego Web Video Server?

Przesyłanie surowych wiadomości obrazu (`sensor_msgs/Image`) z robota na laptopa przez standardowy protokół DDS zużywa ponad **150–200 Mbps** pasma Wi-Fi, co powoduje zrywanie połączenia i gigantyczne opóźnienia.

**Rozwiązanie:** Pakiet **`web_video_server`** kompresuje klatki w locie do formatu **MJPEG** i serwuje je przez protokół HTTP. Zużycie łącza spada do **2–5 Mbps**, a obraz na laptopie można otworzyć w zwykłej przeglądarce internetowej bez instalowania żadnego oprogramowania na Windowsie.

### 4.2 Instalacja i uruchomienie serwera na Jetsonie

1. W oknie terminala PuTTY zainstaluj pakiet:
   ```zsh
   sudo apt update
   sudo apt install -y ros-humble-web-video-server
   ```

2. Uruchom serwer wideo:
   ```zsh
   source /opt/ros/humble/setup.zsh
   ros2 run web_video_server web_video_server
   ```
   *(Serwer wystartuje i zacznie nasłuchiwać na porcie **`8080`**).*

### 4.3 Podgląd w przeglądarce na laptopie

Upewnij się, że Twój laptop jest połączony z tą samą siecią Wi-Fi co robot, a następnie:

1. Otwórz przeglądarkę internetową (**Chrome**, **Edge** lub **Firefox**).
2. Wejdź na stronę główną serwera wideo:
   ```text
   http://10.111.169.242:8080/
   ```
   *(Wyświetli się lista wszystkich dostępnych kamer i tematów w systemie).*
3. **Bezpośrednie linki do strumieni:**
   * **Obraz z kamery RGB (kolor):**
     ```text
     http://10.111.169.242:8080/stream?topic=/camera/color/image_raw
     ```
   * **Wizualizacja głębi (Depth):**
     ```text
     http://10.111.169.242:8080/stream?topic=/camera/depth/image_raw
     ```

---

## 5. Odbiór i Przetwarzanie Obrazu we Własnej Aplikacji (Python + OpenCV)

Jeśli chcesz napisać własną aplikację na laptopie (np. detekcję obiektów, śledzenie twarzy czy prosty podgląd w oknie Windowsa), nie potrzebujesz bibliotek ROS na komputerze. Wystarczy zwykły **OpenCV**:

### Skrypt Python na laptopie (`view_stream.py`):

```python
import cv2

# Adres strumienia z robota Jetson:
STREAM_URL = 'http://10.111.169.242:8080/stream?topic=/camera/color/image_raw'

print(f'Łączenie ze strumieniem: {STREAM_URL}...')
cap = cv2.VideoCapture(STREAM_URL)

if not cap.isOpened():
  print('BŁĄD: Nie można otworzyć strumienia wideo z robota!')
  exit()

print('Połączono! Naciśnij ESC lub Q w oknie obrazu, aby zamknąć.')

while True:
  ret, frame = cap.read()
  if not ret:
    print('Utracono połączenie ze strumieniem.')
    break

  # Tutaj możesz dodać własną obróbkę obrazu (np. detekcję AI, rysowanie prostokątów)
  cv2.putText(
      frame,
      'Rokae Helios - Live Stream',
      (10, 30),
      cv2.FONT_HERSHEY_SIMPLEX,
      0.8,
      (0, 255, 0),
      2,
  )

  # Wyświetlenie okna z obrazem na Windowsie
  cv2.imshow('Rokae Vision', frame)

  # Zamknięcie po naciśnięciu klawisza ESC (27) lub 'q'
  key = cv2.waitKey(1)
  if key == 27 or key == ord('q'):
    break

cap.release()
cv2.destroyAllWindows()
```

---

## 6. Obsługa Pozostałych Kamer USB (Podwozie / Otoczenie)

Robot Helios posiada dodatkowe 3 kamery USB podłączone do podwozia i korpusu. Aby uruchomić obraz z dowolnej z nich:

1. **Zainstaluj standardowy sterownik kamer USB w ROS 2:**
   ```zsh
   sudo apt install -y ros-humble-usb-cam
   ```
2. **Uruchomienie wybranej kamery (np. `/dev/video0`):**
   ```zsh
   ros2 run usb_cam usb_cam_node_exe --ros-args \
     -p video_device:=/dev/video0 \
     -p image_width:=640 \
     -p image_height:=480
   ```
3. Obraz automatycznie pojawi się w `web_video_server` pod adresem:
   ```text
   http://10.111.169.242:8080/stream?topic=/image_raw
   ```

---

## 7. Rozwiązywanie Problemów (Troubleshooting)

### Problem 1: `Failed to initialize device / Component not found`
* **Przyczyna:** Brak uprawnień do surowego portu USB w Linuxie.
* **Rozwiązanie:** Ponownie wgraj reguły udev z punktu 2.2 i przeładuj je (`sudo udevadm control --reload-rules && sudo udevadm trigger`), a następnie wypnij i wepnij kabel kamery lub zrestartuj Jetsona.

### Problem 2: Obraz tnie lub ma duże opóźnienie przez Wi-Fi
* **Rozwiązanie:** W wywołaniu `web_video_server` lub w URL dodaj parametr jakości kompresji JPEG (od 1 do 100) i zmniejszenia rozdzielczości:
  ```text
  http://10.111.169.242:8080/stream?topic=/camera/color/image_raw&quality=50&width=640&height=480
  ```

### Problem 3: Strona `10.111.169.242:8080` nie ładuje się w przeglądarce
* **Rozwiązanie:** Upewnij się, że laptop ma połączenie z Jetsonem (`ping 10.111.169.242` w wierszu poleceń Windowsa) oraz że węzeł `web_video_server` jest stale uruchomiony w terminalu.

---

## 8. Przydatne Linki i Źródła

* **Oficjalne repozytorium Orbbec ROS 2 Wrapper:**  
  [https://github.com/orbbec/OrbbecSDK_ROS2](https://github.com/orbbec/OrbbecSDK_ROS2)
* **Dokumentacja zaawansowana Orbbec (Parametry i kalibracja):**  
  [https://orbbec.github.io/OrbbecSDK_ROS2/en/source/camera_devices/5_advanced_guide/advanced_guide.html](https://orbbec.github.io/OrbbecSDK_ROS2/en/source/camera_devices/5_advanced_guide/advanced_guide.html)
* **Dokumentacja pakietu Web Video Server:**  
  [https://index.ros.org/p/web_video_server/](https://index.ros.org/p/web_video_server/)
* **Karta produktu Rokae Helios (Sensory i kamery):**  
  [https://www.rokae.com/en/product/show/596/Wheeled-Dual-Arm-Robot-Helios.html](https://www.rokae.com/en/product/show/596/Wheeled-Dual-Arm-Robot-Helios.html)
