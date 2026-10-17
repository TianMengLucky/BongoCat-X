<div align="center">

2.2.2: Plugin-eigene Seiten und Übersetzungen, optimierter Speicherverbrauch beim Rendern und Laden von Bildern sowie Kauflinks im Modellshop.
  <a href="https://bongocat.pet" target="_blank">
    <img src="../resources/assets/bongocat.png" alt="BongoCat" width="249">
  </a>
  <h1><a href="https://bongocat.pet" target="_blank">BongoCat</a></h1>
</div>

<p align="center">💘 C/C++ × Rust × SDL3, zusammenrühren, drauflos trommeln! Bong~ Bongo Cat!!!</p>
<p align="center">
  <a href="README.en-US.md">English</a> • <a href="../README.md">简体中文</a> • <a href="README.zh-Hant.md">繁體中文</a> • <a href="README.fr-FR.md">Français</a> • <strong>Deutsch</strong> • <a href="README.ko-KR.md">한국어</a> • <a href="README.pt-BR.md">Português</a> • <a href="README.ru-RU.md">Русский</a> • <a href="README.es-ES.md">Español</a> • <a href="README.id-ID.md">Bahasa Indonesia</a>
</p>
> [!NOTE]
> Dieses Repository (**BongoCat-X**) ist ein Fork von [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat). Es ist nicht mit Live2D Inc. verbunden und enthält weder Cubism SDK noch Core. Die Live2D-Lizenzbedingungen gelten.

<p align="center">
  <a href="../LICENSE"><img src="https://img.shields.io/badge/AGPL--3.0-1fa669?style=flat&logo=gnu&logoColor=white"></a>
  <a href="https://github.com/TianMengLucky/BongoCat-X"><img src="https://img.shields.io/badge/C-A8B9CC?style=flat&logo=c&logoColor=white"></a>
</p>

<div align="center"><video src="https://github.com/user-attachments/assets/75719230-9e49-4124-ae5a-8e35592c5d49" autoplay loop style="border-radius: 8px; max-width: 800px;"></video></div>

> [!TIP]
> Das in der Demo verwendete Modell stammt von [宇痕冫](https://space.bilibili.com/348616056).
>
> 🎁 Auf der Suche nach **kostenlosen** Modellen? Wir arbeiten mit talentierten Modellerstellern zusammen, um dir eine große Auswahl an kostenlosen Modellen zu bieten – und erkunden daneben ständig weitere unterhaltsame Desktop-Erlebnisse! Besuche unsere offizielle Website: [bongocat.pet](https://bongocat.pet/models)

<p align="center">
  <a href="https://bongocat.pet/models">
    <img height="1080" src="https://github.com/user-attachments/assets/dedd83ca-742a-4f8c-a64a-69e659fca564" />
  </a>
</p>

## 💬 QQ-Community

Scanne den QR-Code mit QQ oder suche nach Gruppe **211957388**, um der **BongoCat-X**-Community beizutreten.

<p align="center"><img src="../resources/assets/qq-group.jpg" alt="BongoCat-X QQ · 211957388" width="320"></p>

## 📥 Download

- GitHub Releases ([v2.2.2](https://github.com/TianMengLucky/BongoCat-X/releases/tag/v2.2.2))

  Lade die neueste Version von den [GitHub Releases](https://github.com/TianMengLucky/BongoCat-X/releases/latest) herunter.

  > [!IMPORTANT]
  > Offizielle Releases sind **Runtime-Core-Builds**: Die Live2D-Renderunterstützung ist eingebaut, aber die Live2D-Cubism-Core-Laufzeitbibliothek ist **nicht enthalten** (proprietäre Lizenz, wird nie mit den Paketen ausgeliefert). Es sind **keine** Diagnose-Builds ohne Live2D. Wenn kein Core gefunden wird, fällt die App in den Diagnose-Backend zurück und zeigt einen Hinweis im Einstellungsfenster an.

  **Live2D-Rendering aktivieren (eines von beiden):**

  1. **Import in der App (empfohlen)**: Öffne *Einstellungen → Plugins*, klicke auf *Live2D Core importieren* und wähle eine `Live2DCubismCore.dll` oder das offizielle Cubism-SDK-Zip. Wirkt sofort, ohne Neustart.
  2. **Ablegen im live2d-Ordner**: Lade **Cubism SDK for Native** von der [offiziellen Downloadseite](https://www.live2d.com/en/sdk/download/native/) herunter (Live2D-Lizenz zustimmen) und lege das Zip oder die entpackte `Live2DCubismCore.dll` in den `live2d`-Ordner neben der App bzw. im Datenverzeichnis; nach einem Neustart wird sie automatisch erkannt.

  Die vollständigen SDK-Import Schritte beim Bau aus dem Quellcode findest du im Abschnitt „Live2D / Cubism SDK" unten.

## 🛠️ Aus dem Quellcode erstellen

BongoCat benötigt CMake 3.24 oder neuer, C11- und C++17-Compiler, OpenGL-Entwicklungsdateien und die Rust-Toolchain (cargo, über rustup verfügbar). Corrosion baut die Rust-Module. SDL3, stb, miniaudio, Nuklear und Corrosion werden standardmäßig bei der Konfiguration heruntergeladen; der erste Build benötigt Netzwerkzugang.

Führe die folgenden Befehle im Projektstammverzeichnis aus (dem Verzeichnis, das `CMakeLists.txt` enthält).

### 📋 Plattform-Voraussetzungen

- **Windows:** Visual Studio 2022 (mit der Workload „Desktopentwicklung mit C++“) und CMake. Verwende den MSVC-Generator; MinGW kann das Diagnose-Backend bauen, unterstützt jedoch nicht das Cubism SDK.
- **macOS:** Xcode Command Line Tools, CMake und Ninja. Weicht die Zielarchitektur von der Standardarchitektur des Hosts ab, gib sie über `CMAKE_OSX_ARCHITECTURES` an.
- **Linux (Debian/Ubuntu):** GCC oder Clang, Ninja sowie die OpenGL/X11-Header:

  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build \
    libgl1-mesa-dev libx11-dev libxi-dev libxfixes-dev libxcb-screensaver0-dev libfontconfig1-dev fonts-noto-cjk
  ```

### 🔧 Konfiguration und Build

Verwende unter Linux und macOS einen Einzelkonfigurations-Generator wie Ninja:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --parallel
```

Führe die Befehle unter Windows von der Visual Studio 2022 Developer Command Prompt aus (oder einer anderen Shell, in der MSVC verfügbar ist):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --config Release --parallel
```

Die ausführbare Datei befindet sich unter `build/BongoCat` auf Linux, unter `build/BongoCat.app/Contents/MacOS/BongoCat` auf macOS und unter `build/Release/BongoCat.exe` für Visual-Studio-Builds auf Windows.

### 🧪 Tests

Die CTest-Ziele sind standardmäßig aktiviert. Führe nach dem Build Folgendes aus:

```bash
ctest --test-dir build --output-on-failure
```

Gib bei einem Mehrkonfigurations-Generator wie Visual Studio die Build-Konfiguration explizit an:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

### 🎭 Live2D / Cubism SDK (Optional – läuft auch ohne Installation)

Das proprietäre Cubism SDK bleibt optional. Ohne SDK werden Host und Inox2D-Plugin gebaut. Ein kompatibles Live2D-Plugin mit separat bereitgestelltem Cubism Core aktiviert Live2D ohne Neubau des Hosts. Mit SDK entsteht das separate C++-Plugin.

1. Öffnen Sie die [Cubism-SDK-Downloadseite](https://www.live2d.com/en/sdk/download/native/), akzeptieren Sie die Live2D Proprietary Software License Agreement und laden Sie **Cubism SDK for Native** herunter (die Releases werden mit dem SDK `5-r.5` gebaut und getestet).
2. Entpacken Sie das Archiv. Wenn der entpackte Ordner `CubismSdkForNative-5-r.5` heißt, benennen Sie ihn in `CubismSdkForNative` um und legen Sie ihn unter `vendor/` ab, sodass der Baum `Core/` und `Framework/` enthält.
3. Neuere SDK-Archive enthalten GLEW nicht mehr. Laden Sie [GLEW 2.2.0](https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip) herunter und entpacken Sie es nach `vendor/CubismSdkForNative/Samples/OpenGL/thirdParty/glew` (der Ordner, der direkt `include/GL/glew.h` und `src/glew.c` enthält).

Alternativ können Sie das SDK an einem beliebigen Ort ablegen und den Pfad explizit übergeben:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_CUBISM_SDK=/path/to/CubismSdkForNative
```

Das SDK muss seine Core-Bibliothek, die Framework-Quellen und den
OpenGL-GLEW-Drittanbieterbaum in der von `cmake/Cubism.cmake` erwarteten
Struktur enthalten. Cubism-Builds unter Windows erfordern Visual Studio
2022. Sobald das SDK vorliegt, konfigurieren Sie wie gewohnt, um einen
Build mit Live2D-Rendering zu erhalten. `BONGO_CAT_REQUIRE_CUBISM=ON` sorgt
nur dafür, dass die Konfiguration bei fehlendem SDK sofort mit
Importhinweisen fehlschlägt (die Release-CI verwendet es). Belassen Sie es
auf dem Standard `OFF`, wenn Sie das nicht benötigen.

### ⚙️ CMake-Optionen

| Option | Standard | Beschreibung |
| --- | --- | --- |
| `BONGO_CAT_FETCH_DEPS` | `ON` | Lädt Drittanbieter-Abhängigkeiten in festgelegten Versionen über CMake `FetchContent` herunter (einschließlich Corrosion und der Rust-Crate-Abhängigkeiten). Setze es nur auf `OFF`, wenn SDL3, stb, miniaudio, Nuklear und Corrosion bereits für CMake verfügbar sind. |
| `BONGO_CAT_CUBISM_SDK` | `vendor/CubismSdkForNative` | Pfad zum Cubism SDK for Native. |
| `BONGO_CAT_REQUIRE_CUBISM` | `OFF` | Ob ein fehlendes SDK die Konfiguration fehlschlagen lässt. Standard `OFF`: Ein fehlendes SDK baut das Diagnose-Backend ohne Live2D-Rendering; setze auf `ON`, um das SDK vorauszusetzen (die Release-CI verwendet es). |
| `BONGO_CAT_WARNINGS_AS_ERRORS` | `OFF` | Behandelt Warnungen des nativen Compilers als Fehler. |

Setze für Offline-Builds `BONGO_CAT_FETCH_DEPS=OFF` und stelle die CMake-Paketkonfigurationen für SDL3 (einschließlich `SDL3-static`)  bereit; falls stb, Nuklear und miniaudio nicht automatisch gefunden werden können, gib zusätzlich deren Include-Verzeichnisse an:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=OFF \
  -DBONGO_CAT_STB_INCLUDE_DIR=/path/to/stb \
  -DBONGO_CAT_NUKLEAR_INCLUDE_DIR=/path/to/nuklear \
  -DBONGO_CAT_MINIAUDIO_INCLUDE_DIR=/path/to/miniaudio
```

## 📌 Projektstatus

![Commit activity](https://img.shields.io/github/commit-activity/m/TianMengLucky/BongoCat-X?style=flat)
![Contributors](https://img.shields.io/github/contributors/TianMengLucky/BongoCat-X?style=flat)
![Stars](https://img.shields.io/github/stars/TianMengLucky/BongoCat-X?style=flat)
![Issues](https://img.shields.io/github/issues/TianMengLucky/BongoCat-X?style=flat)
![Release](https://img.shields.io/github/v/release/TianMengLucky/BongoCat-X?style=flat)

![Alt](https://repobeats.axiom.co/api/embed/74334755a589dea40c5d31f8d2bcdc6c2bd39d87.svg "Repobeats analytics image")

## 📜 Lizenz

Der Quellcode und die native Laufzeit von BongoCat stehen unter der [AGPL-3.0-only](../LICENSE)-Lizenz.

Das standardmäßig eingebaute Modell (`standard`) bleibt unter der MIT-Lizenz. Die in `resources/assets/models/standard`, `keyboard` und `gamepad` gebündelten Modellressourcen fallen unter die separate [MIT-Lizenz-Deklaration](../LICENSE-MIT). Diese MIT-Lizenz gilt ausschließlich für die Modellressourcen und die zugehörigen Grafiken; sie ändert nichts an der Lizenz des BongoCat-Quellcodes oder der nativen Laufzeit.

## 🧭 Technische Architektur

Der C11-Host verwendet SDL3 für Ereignisse, Modellaktualisierung und Darstellung im Hauptthread sowie Nuklear für die Einstellungen. `src/rust/bongo-safe` übernimmt das Parsen nicht vertrauenswürdiger Daten, Eingabewarteschlangen und Messwerte sowie den gemeinsamen Zustand der Plugin-Arbeitsthreads. Die Live2D-Brücke, das Vulkan-Ressourcenbackend und die Windows-COM/WIC-Adapter verwenden C++17.

Plattform-Listener übergeben Tasten- und Maustastenereignisse an eine begrenzte Rust-Warteschlange, fassen Zeigerbewegungen separat zusammen und wecken den Hauptthread für Modellparameter und UI. Eine versionierte C-ABI lädt Live2D-/Inox2D-Plugins mit OpenGL, Vulkan oder Metal auf unterstützten Plattformen.

Kompatibilität und Build-Details: [Renderer-Plugins](model-plugins.md). Backend-Grenzen und Prüfumfang: [Natives Rendering](live2d-vulkan-metal.md).

## ❓ Häufig gestellte Fragen

### 🔒 Zeichnet BongoCat meine Tastatur- oder Mauseingaben auf?

BongoCat verarbeitet Eingaben lokal für Animationen und Tastenkürzel und lädt keine Eingabe- oder Nutzungsdaten hoch. Das aktivierte Statistik-Plugin kann Summen, Tages-/Stundenverläufe und die Zeit aktiver Anwendungen lokal speichern. Text, Tastenreihenfolge, Fenstertitel, URLs und Koordinaten werden nicht gespeichert. Auf der Statistikseite lassen sich Erfassung deaktivieren und Daten zurücksetzen oder exportieren. Siehe [Statistik-Plugin](statistics-plugin.md). Updateprüfungen fragen öffentliche Versionsmetadaten ab.

### 🖼️ Wie weit ist die Vulkan-/Metal-Unterstützung?

Einstellungen → Anwendung → Render-Backend wechselt sofort: OpenGL ↔ Vulkan unter Windows / Linux x64, OpenGL ↔ Metal unter macOS. System verwendet standardmäßig OpenGL. Native Texturübertragung, Cubism-Zeichnen, GPU-Auslesen und Modellwechsel sind verbunden; Modell und ausgewählte Bewegungen/Ausdrücke bleiben erhalten. Bei Initialisierungs- oder Ladefehlern wird OpenGL wiederhergestellt. Vulkan/Metal sind experimentell: Tasten-/Effekt-/Zeiger-Overlays unterstützen jetzt alle Backends. Live2D unterstützt dynamische Texturauflösung mit OpenGL, Vulkan und Metal; Dekodierung und Skalierung erfolgen im Hintergrund, native GPU-Uploads synchron. Deaktivieren lädt die Texturen gemäß der Qualitätsstufe neu. Native Backends speichern statische Tischhintergründe im Cache und unterstützen GPU-Rundungen an allen vier Ecken ohne zusätzliche Vollbildtextur. Overlays nutzen SDL-Softwarerendering, werden bis zur Eingabeänderung zwischengespeichert und in den fertigen nativen Frame gemischt und zur GPU übertragen; die Leistung ist noch nicht gemessen.

Große Rendering-Optimierung erscheint für Vulkan / Metal und ist standardmäßig aus. Vulkan bietet drei Optionen; Metal nur große Speicherzuweisungen. Inox2D Metal nutzt damit einen gemeinsamen Vertexpuffer pro Frame; Live2D Metal meldet die Option als nicht verfügbar. Die drei Unteroptionen werden einzeln gespeichert; Änderungen laden das Modell neu. Inox2D Vulkan unterstützt parallele Befehlsaufzeichnung und große Speicherblöcke. Live2D Vulkan unterstützt VMA und parallele Aufzeichnung bei normalem Blending ohne Offscreen-Knoten oder hochpräzise Masken. Ab 128 Drawables nutzt Live2D sekundäre Puffer, ab 128 Zeichen-Snapshots bis zu vier Threads; andere Modelle behalten den seriellen SDK-Pfad. Live2D Vulkan Bindless nutzt eine Tabelle mit bis zu 64 Atlastexturen bei normalem Blending ohne Offscreen-Knoten; fehlende Gerätefunktionen oder Kapazität führen zum bisherigen Pfad zurück. Inox2D Bindless ist nicht implementiert. Größere Pools können mehr Speicher benötigen; Leistungsgewinne sind noch nicht gemessen.

SDK-Builds aktivieren `BONGO_CAT_CUBISM_VULKAN=ON` unter Windows / Linux x64 (benötigt `glslangValidator` oder `glslang`, unter Linux `glslang-tools`; Vulkan 1.3 zur Laufzeit) und `BONGO_CAT_CUBISM_METAL=ON` unter macOS (Xcode-Metal-Werkzeuge). Mit `OFF` wird die jeweilige Erweiterung deaktiviert. Metal-Quellen und `.metallib` werden nur unter macOS, Vulkan-Quellen und `.spv` nur unter Windows / Linux x64 eingebunden, einschließlich Blend-Varianten. Diagnose-Builds ohne SDK enthalten diese Live2D-Ressourcen nicht. GitHub Actions bereitet plattformspezifische Werkzeuge vor und prüft die Ressourcen. Statische Prüfungen wurden ausgeführt; Builds und GPU-Verhalten wurden noch nicht geprüft. Siehe [Integrationsstatus und Validierung](live2d-vulkan-metal.md).

## 🙏 Besonderer Dank
> [!TIP]
> Jeder Schritt von BongoCat wird vom Open-Source-Gedanken getragen. Wir danken allen Community-Beitragenden aufrichtig für ihren selbstlosen Beitrag (unten nach Beitragsdatum sortiert aufgeführt). Eure Unterstützung macht die Begleitung auf dem Desktop freier und echten.❤️‍🔥


<a href="https://github.com/TianMengLucky/BongoCat-X/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=TianMengLucky/BongoCat-X" />
</a>


---

<div align="center">
Copyright © 2026 - **BongoCat**<br>
By vladelaina<br>
Made with ❤️ & ⌨️
</div>

### Darstellung der Einstellungen

Die Einstellungen verwenden aufklappbare Gruppenkarten. Fenster, Modell, Anwendung und Darstellung sind standardmäßig geöffnet. Das kompakte Layout unter Darstellung verkleinert Abstände und behält Beschreibungen bei. Die Einstellung wird automatisch gespeichert.

### Eingabestatistik-Plugin

Das Rust-Statistik-Plugin wird standardmäßig mitgebaut. Auf der Plugin-Seite lässt es sich aktivieren, deaktivieren oder importieren. Die Statistik-Seite bietet Tastatur- und Mauswerte, Tages- und Stundenverläufe, lokale Speicherung und CSV-Export. Siehe [Statistik-Plugin](statistics-plugin.md). Die aktive App-Zeit bietet Ranglisten, Stundenwerte und einen eigenen Schalter. Erfassung, Speicherung und Export laufen in einem separaten Thread. Das Plugin registriert den Statistikeintrag beim Aktivieren und entfernt ihn beim Deaktivieren oder Deinstallieren.

Auf der Statistik-Seite lassen sich Einheitennamen sowie Faktoren für Bewegung und Scrollen speichern und zurücksetzen. Die Darstellung verwendet eine gemeinsame Palette für Haupt-, Sekundär-, Text- und Seitenhintergrundfarben.

### Dynamische Renderer-Plugins

Einstellungen → Plugins ermöglicht die lokale Installation, Aktivierung und Entfernung von Renderern. Wechseln Sie vor Änderungen an einem aktiven Plugin zu einem anderen Renderer. Live2D verwendet ein C++-Plugin; Inochi2D verwendet das Rust-Plugin Inox2D mit OpenGL, Vulkan (Windows/Linux) und Metal (macOS), ohne wgpu. Beim Import wird zuerst der Modellinhalt erkannt (`.model3.json`, `.inp`, `.inx`). Alle JSON-Daten werden in Rust gelesen und geschrieben.

Portable Windows-Versionen werden als ZIP-Archive verteilt. Der entpackte Ordner `plugins` muss neben der Anwendung bleiben. [Renderer-Plugins](model-plugins.md) beschreibt Pakete, Offline-Abhängigkeiten, die Plugin-ABI, Parameterzuordnungen und Kompatibilitätsgrenzen.

Jeder Push auf `dev`, der Code, Ressourcen oder Build-Konfigurationen ändert, veröffentlicht eine GitHub-Vorabversion. Reine Dokumentationsänderungen werden übersprungen; es gibt keinen Zeitplan. Alle Plattformen bauen den gepushten Commit. Die neueste stabile Version bleibt unverändert.

Nächtliche Vorabversionen verwenden die [nightly-Release-Seite](https://github.com/TianMengLucky/BongoCat-X/releases/tag/nightly) erneut, aktualisieren den Tag und ersetzen Dateien mit festen Namen. Bei fehlgeschlagenen Plattform-Builds bleiben die bisherigen Dateien erhalten.

### Linux Wayland / evdev

Experimentelle evdev-Eingabe unter Linux Wayland wird mit `BONGOCAT_ENABLE_EVDEV=1` aktiviert. Zunächst werden Geräte als normaler Benutzer schreibgeschützt geöffnet. Bei fehlenden Rechten fordert eine geschützte Installation über `/usr/bin/sudo` eine Authentifizierung an, öffnet die Geräte und startet nach sofortigem Rechteabbau neu, bevor SDL, Einstellungen und Modelle initialisiert werden. Programm, sudo und Elternverzeichnisse müssen root gehören und dürfen für Gruppe und andere Benutzer nicht beschreibbar sein. Entwicklungs- und portable Installationen benötigen separat verwaltete Geräterechte. Der vertrauenswürdige sudo-Pfad ist über `BONGO_CAT_SUDO_EXECUTABLE` konfigurierbar. Fehler stoppen den Start; dauerhafte Rechte bleiben unverändert. Nur beim Start vorhandene Geräte werden überwacht; Hotplug erfordert einen Neustart. Nicht direkt als root starten oder der Gruppe `input` beitreten. Rohe Eingaben können Passwörter enthalten; Bildschirmsperre, Sitzungswechsel und Ausblenden des Tiers pausieren die Überwachung nicht.

[SECURITY.md](../SECURITY.md#linux-input)

Unter Einstellungen → Einstellungen → Fenster bietet Eigenes Hintergrundbild die Auswahl von PNG, JPEG, WebP, BMP oder GIF (erstes Bild). Das Bild füllt das Fenster zentriert und proportional; einfarbiger Hintergrund und Bild schließen sich aus. Importierte Bilder bleiben im App-Datenverzeichnis gespeichert, auch wenn das Original verschoben wird. Maximal 64 MB und 16 Megapixel pro Bild; unterstützt OpenGL, Vulkan und Metal.

Live2D verwendet Geometriepuffer und wiederholte Renderzustände automatisch wieder. OpenGL-Streaming, Vulkan-Upload-Batches und Pipeline-Caches, gemeinsame Metal-Puffer sowie adaptive native Masken benötigen keinen zusätzlichen Schalter. Leistung und Plattformverhalten sind noch zu prüfen.

Vulkan verwaltet Speicher automatisch: Live2D nutzt standardmäßig VMA-Blöcke von 1 MiB, bei aktivierter großer Speicherzuweisung 16 MiB. Die Windows-Layered-Darstellung behält ein Farbziel. Verborgene oder minimierte Fenster geben Rücklesecaches nach GPU-Abschluss frei und erzeugen sie bei Bedarf neu. Texturqualität und dynamische Auflösung bleiben getrennte Einstellungen.

Einstellungen → Haustier → Maus-Folgebereich: horizontal/vertikal 25–400%, standardmäßig aus, 100% unverändert. Gezeichnete Mver-Pfoten bewegen sich weiter; Live2D behält Modellgrenzen. Blick und Fenster bleiben gleich

### Mehrere Tasten und lesbare Spiegelzeichen

Mver (OpenGL/Vulkan/Metal) schneidet Hand, Blasenrahmen und Zeichen getrennt zu und verbindet gedrückte Tasten mit + in einer Blase. Rahmen und Zipfel werden mit dem Modell gespiegelt; die Zeichen bleiben auch im Einzeltastenmodus lesbar. Konfigurieren Sie die drei Bereiche, den Abstand um + und das Limit je Seite in `hand-layout.json` neben den Handbildern; starten Sie nach Änderungen neu. Siehe [Anleitung und Beispiel](hand-layout.md).

Der Aktionsdialog prüft den `Sound`-Verweis und das Vorhandensein der Audiodatei und zeigt nur bei Bewegungen mit Ton einen eigenen Lautstärkeregler. 0–100% sind per Ziehen oder Eingabe einstellbar; Doppelklick stellt 100% wieder her, 0% schaltet stumm. Die Werte werden je Modell und Bewegung gespeichert und gelten für Dialog, Tastenkürzel, Zufallsbewegungen und Kontextmenü, auch während der Wiedergabe. Andere Bewegungen mit derselben Datei behalten ihre eigene Lautstärke.

### Objekt-Plugin

Das unabhängige Objekt-Plugin ist standardmäßig enthalten. Unter Einstellungen → Plugins aktivieren, um die Objektseite einzublenden. Unterstützt Bildimport und Drag-and-drop, mehrere Objekte, Position, Größe, Drehung, Deckkraft, Vorder-/Hintergrundebenen, Sperren, Duplikate und Szenenspeicherung mit OpenGL, Vulkan und Metal. Folgen der Modellansicht ist keine Mesh-Bindung. GIF zeigt nur das erste Bild; bis zu 16 Objekte am primären Haustier. Siehe [Objekt-Plugin](items-plugin.md). `BONGO_CAT_BUILD_ITEMS=OFF` lässt die Bibliothek weg.

Speicherpriorität beim Rendern: Grafikspeicher bevorzugen / Arbeitsspeicher bevorzugen. Nur Live2D Vulkan. Änderungen laden das Rendering neu. Grafikspeicher bevorzugt Geschwindigkeit; Arbeitsspeicher entlastet den VRAM, kann aber langsamer sein. Nicht unterstützte Ressourcen weichen automatisch aus. Andere Backends verteilen automatisch; integrierte GPUs teilen den Speicher.

### Tastatur-Soundeffekte

Soundeffekte unter Plugins aktivieren. Eine Taste anklicken, Audio wählen, den Dateinamen beim Überfahren sehen und mit der physischen Taste abspielen. Zuweisungen werden automatisch gespeichert. Sounds und Objekte haben separate Threads. WAV/MP3/Ogg Vorbis/FLAC am primären Haustier. Siehe [Soundeffekte](sounds-plugin.md); `BONGO_CAT_BUILD_SOUNDS=OFF` lässt die Bibliothek weg.

Das Rust-Shop-Plugin bietet Modellvorschauen, Autorenlinks und Download-/Deinstallationsschaltflächen. SHA-256 wird vor der Installation geprüft. Ein unveröffentlichter Katalog zeigt einen Platzhalter. [Shop-Plugin](store-plugin.md).

Die automatische Updateprüfung ist unter Einstellungen → Anwendung standardmäßig aktiviert und läuft bei jedem Start einmal im Hintergrund. Hinweise auf neue Versionen bleiben bis zum Öffnen der Einstellungen erhalten und bieten oben rechts „Jetzt aktualisieren“. Alle Benachrichtigungen in den Einstellungen lassen sich manuell schließen.

Die Modellseite fasst Modi desselben Pakets in einer Karte mit Modusauswahl zusammen, einschließlich Standard, Tastatur und Gamepad der Standardmodelle. Mehrere Modell-ZIPs können gleichzeitig abgelegt und im Hintergrund entpackt, geprüft und importiert werden. Das Schließen der Einstellungen unterbricht den Import nicht. Beim Beenden werden weitere Importe gestoppt; laufende Quelldateien werden vor der Freigabe von Threads und temporären Ressourcen fertig verarbeitet.

Tastenkombinationen sind jetzt unter Einstellungen über Einstellungen / Tastenkombinationen erreichbar. Der Store liegt unter Modell und wird über Lokale Modelle / Store geöffnet. Die beiden eigenen Seitenleisteneinträge entfallen; jede Unterseite behält ihre Scrollposition.

Beim Aktivieren von Live2D ohne Cubism Core erscheinen beide offiziellen Lizenzverträge. Unter Windows lädt Zustimmen und herunterladen das Native-R5-SDK per offiziellem HTTPS, entpackt nur den passenden dynamischen Core und aktiviert das Rendering lokal. Abbrechen und fortfahren lässt das Plugin ohne Rendering aktiviert. Downloads können erneut versucht werden; manueller Import bleibt möglich. Anwendung und Pakete enthalten weder SDK noch Core.

Windows mit AMD-Grafik bietet eine experimentelle DirectComposition-Darstellung, standardmäßig aus. DXGI-Alpha-Komposition wirkt sofort, begrenzt Expose-Ereignisse auf 50 ms und deaktiviert VSync; bei Fehlern wird die bisherige Darstellung wiederhergestellt. Pixel werden weiterhin ausgelesen; Independent Flip bei Spielen ist nicht garantiert.

Die Renderer-Plugin-ABI ist jetzt v2: Anwendung und Live2D-/Inox2D-Plugins gemeinsam aktualisieren oder neu bauen; ältere Plugins sind inkompatibel. Live2D Vulkan unterstützt bedarfsgerechte Offscreen-Ressourcen, PNG-Skalierung per Rust-Streaming und BC7-Caching mit RGBA8-Rückfall; Plattformgrenzen stehen in den [Rendering-Hinweisen](live2d-vulkan-metal.md).
