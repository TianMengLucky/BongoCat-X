<div align="center">

2.2.3: menú radial opcional con clic derecho, ventana de ajustes más grande con maximizar/restaurar, restablecimiento de cada color y mejoras en las unidades y el redondeo de estadísticas.
  <a href="https://bongocat.pet" target="_blank">
    <img src="../resources/assets/bongocat.png" alt="BongoCat" width="249">
  </a>
  <h1><a href="https://bongocat.pet" target="_blank">BongoCat</a></h1>
</div>

<p align="center">💘 C/C++ × Rust × SDL3, mézclalo todo, ¡a tocar! ¡Bong~ Bongo Cat!!!</p>
<p align="center">
  <a href="README.en-US.md">English</a> • <a href="../README.md">简体中文</a> • <a href="README.zh-Hant.md">繁體中文</a> • <a href="README.fr-FR.md">Français</a> • <a href="README.de-DE.md">Deutsch</a> • <a href="README.ko-KR.md">한국어</a> • <a href="README.pt-BR.md">Português</a> • <a href="README.ru-RU.md">Русский</a> • <strong>Español</strong> • <a href="README.id-ID.md">Bahasa Indonesia</a>
</p>
> [!NOTE]
> Este repositorio (**BongoCat-X**) es un fork de [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat). No está afiliado a Live2D Inc. ni incluye Cubism SDK o Core. Deben respetarse las licencias de Live2D.

<p align="center">
  <a href="../LICENSE"><img src="https://img.shields.io/badge/AGPL--3.0-1fa669?style=flat&logo=gnu&logoColor=white"></a>
  <a href="https://github.com/TianMengLucky/BongoCat-X"><img src="https://img.shields.io/badge/C-A8B9CC?style=flat&logo=c&logoColor=white"></a>
</p>

<div align="center"><video src="https://github.com/user-attachments/assets/75719230-9e49-4124-ae5a-8e35592c5d49" autoplay loop style="border-radius: 8px; max-width: 800px;"></video></div>

> [!TIP]
> El modelo usado en la demo proviene de [宇痕冫](https://space.bilibili.com/348616056).
>
> 🎁 ¿Buscas modelos **gratuitos**? Trabajamos con creadores de modelos talentosos para ofrecerte una gran variedad de modelos gratuitos, ¡mientras seguimos explorando experiencias de escritorio aún más divertidas! Visita nuestro sitio web oficial: [bongocat.pet](https://bongocat.pet/models)

<p align="center">
  <a href="https://bongocat.pet/models">
    <img height="1080" src="https://github.com/user-attachments/assets/dedd83ca-742a-4f8c-a64a-69e659fca564" />
  </a>
</p>

## 💬 Comunidad de QQ

Escanea el código QR con QQ o busca el grupo **211957388** para unirte a la comunidad de **BongoCat-X**.

<p align="center"><img src="../resources/assets/qq-group.jpg" alt="BongoCat-X QQ · 211957388" width="320"></p>

## SDK de Live2D y colores

La importación y descarga del SDK están en Ajustes → Plugins; la página de modelos administra modelos. En Windows, activa el plugin Live2D, elige Descargar SDK de Live2D y acepta ambos acuerdos oficiales tras leerlos para instalar el Core compatible. Se muestran el tamaño descargado, el total y el porcentaje; puedes reintentar desde el diálogo. En otras plataformas importa un Core compatible manualmente. Los colores de apariencia y fondo sólido comparten una paleta emergente que se abre al pulsar la muestra de color.

## 📥 Descarga

- GitHub Releases ([v2.2.3](https://github.com/TianMengLucky/BongoCat-X/releases/tag/v2.2.3))

  Descarga la última versión desde [GitHub Releases](https://github.com/TianMengLucky/BongoCat-X/releases/latest).

  > [!IMPORTANT]
  > Las versiones oficiales son **builds runtime-Core**: incluyen soporte de renderizado Live2D, pero **no** incluyen la biblioteca en tiempo de ejecución Live2D Cubism Core (licencia propietaria, nunca se distribuye con los paquetes). **No** son builds de diagnóstico sin Live2D. Si no se encuentra el Core, la aplicación vuelve al backend de diagnóstico y muestra un aviso en la ventana de ajustes.

  **Activar el renderizado Live2D (elige una opción):**

  1. **Importar desde la aplicación (recomendado)**: abre *Ajustes → Plugins*, pulsa *Importar Live2D Core* y selecciona un archivo `Live2DCubismCore.dll` o el zip oficial del SDK de Cubism. Surte efecto de inmediato, sin reiniciar.
  2. **Soltar en la carpeta live2d**: descarga **Cubism SDK for Native** desde la [página oficial de descargas](https://www.live2d.com/en/sdk/download/native/) (acepta la licencia de Live2D) y coloca el zip o el `Live2DCubismCore.dll` extraído en la carpeta `live2d` junto a la aplicación o en el directorio de datos; tras reiniciar se detecta automáticamente.

  Para los pasos completos de importación del SDK al compilar desde el código fuente, consulta la sección «Live2D / Cubism SDK» más abajo.

## 🛠️ Compilar desde el código fuente

BongoCat requiere CMake 3.24 o posterior, compiladores C11 y C++17, archivos de desarrollo de OpenGL y la cadena de herramientas de Rust (cargo, disponible mediante rustup). Corrosion compila los módulos Rust. La configuración descarga SDL3, stb, miniaudio, Nuklear y Corrosion por defecto; la primera compilación necesita acceso a la red.

Ejecuta los siguientes comandos en la raíz del proyecto (el directorio que contiene `CMakeLists.txt`).

### 📋 Requisitos por plataforma

- **Windows:** Visual Studio 2022 (con la carga de trabajo «Desarrollo para escritorio con C++») y CMake. Usa el generador MSVC; MinGW puede compilar el backend de diagnóstico, pero no es compatible con el SDK de Cubism.
- **macOS:** Xcode Command Line Tools, CMake y Ninja. Si la arquitectura de destino difiere de la predeterminada del host, especifícala mediante `CMAKE_OSX_ARCHITECTURES`.
- **Linux (Debian/Ubuntu):** GCC o Clang, Ninja y los encabezados de OpenGL/X11:

  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build \
    libgl1-mesa-dev libx11-dev libxi-dev libxfixes-dev libxcb-screensaver0-dev libfontconfig1-dev fonts-noto-cjk
  ```

### 🔧 Configuración y compilación

En Linux y macOS, usa un generador de configuración única como Ninja:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --parallel
```

En Windows, ejecuta los comandos desde el Símbolo del sistema para desarrolladores de Visual Studio 2022 (o cualquier shell donde MSVC esté disponible):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --config Release --parallel
```

El ejecutable se encuentra en `build/BongoCat` en Linux, en `build/BongoCat.app/Contents/MacOS/BongoCat` en macOS y en `build/Release/BongoCat.exe` para las compilaciones de Visual Studio en Windows.

### 🧪 Pruebas

Los objetivos de prueba están desactivados por defecto para agilizar la compilación habitual. Actívalos y compílalos cuando sea necesario:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Para un generador de múltiples configuraciones como Visual Studio, especifica explícitamente la configuración de compilación:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

### 🎭 Live2D / SDK de Cubism (Opcional: también compila sin él)

El SDK propietario de Cubism sigue siendo opcional. Sin él se compilan la aplicación y el plugin Inox2D. Un plugin Live2D compatible y Cubism Core proporcionado por el usuario permiten activar Live2D sin recompilar la aplicación. Con el SDK se genera el plugin C++ independiente.

1. Abra la [página de descarga del SDK de Cubism](https://www.live2d.com/en/sdk/download/native/), acepte el Live2D Proprietary Software License Agreement y descargue **Cubism SDK for Native** (las versiones se compilan y prueban con el SDK `5-r.5`).
2. Extraiga el archivo. Si la carpeta extraída se llama `CubismSdkForNative-5-r.5`, cámbiele el nombre a `CubismSdkForNative` y colóquela en `vendor/` de modo que el árbol contenga `Core/` y `Framework/`.
3. Los archivos más recientes del SDK ya no incluyen GLEW. Descargue [GLEW 2.2.0](https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip) y extráigalo en `vendor/CubismSdkForNative/Samples/OpenGL/thirdParty/glew` (el directorio que contiene directamente `include/GL/glew.h` y `src/glew.c`).

También puede guardar el SDK en cualquier ubicación y pasar su ruta explícitamente:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_CUBISM_SDK=/path/to/CubismSdkForNative
```

El SDK debe incluir la biblioteca Core, el código fuente de Framework y
el directorio de terceros de OpenGL GLEW en la estructura esperada por
`cmake/Cubism.cmake`. Las compilaciones de Cubism en Windows requieren
Visual Studio 2022. Con el SDK ya en su sitio, configure como de
costumbre para obtener una compilación con renderizado de Live2D;
`BONGO_CAT_REQUIRE_CUBISM=ON` solo hace que la configuración falle de
inmediato con instrucciones de importación cuando falta el SDK (la CI
de releases lo usa). Déjelo en el valor predeterminado `OFF` cuando no
lo necesite.

### ⚙️ Opciones de CMake

| Opción | Valor por defecto | Descripción |
| --- | --- | --- |
| `BONGO_CAT_FETCH_DEPS` | `ON` | Descarga dependencias de terceros en versiones fijadas mediante `FetchContent` de CMake (incluidos Corrosion y las dependencias del crate de Rust). Establécelo en `OFF` solo si SDL3, stb, miniaudio, Nuklear y Corrosion ya están disponibles para CMake. |
| `BONGO_CAT_CUBISM_SDK` | `vendor/CubismSdkForNative` | Ruta del Cubism SDK for Native. |
| `BONGO_CAT_REQUIRE_CUBISM` | `OFF` | Si falta el SDK, la configuración falla. El valor predeterminado es `OFF`: sin el SDK compila el backend de diagnóstico sin renderizado de Live2D; establézcalo en `ON` para exigir el SDK (lo usa la CI de releases). |
| `BONGO_CAT_WARNINGS_AS_ERRORS` | `OFF` | Trata las advertencias del compilador nativo como errores. |

Para una compilación sin conexión, establece `BONGO_CAT_FETCH_DEPS=OFF` y proporciona las configuraciones de paquetes de CMake para SDL3 (incluido `SDL3-static`) ; si stb, Nuklear y miniaudio no se pueden detectar automáticamente, indica también sus directorios de inclusión:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=OFF \
  -DBONGO_CAT_STB_INCLUDE_DIR=/path/to/stb \
  -DBONGO_CAT_NUKLEAR_INCLUDE_DIR=/path/to/nuklear \
  -DBONGO_CAT_MINIAUDIO_INCLUDE_DIR=/path/to/miniaudio
```

## 📌 Estado del proyecto

![Commit activity](https://img.shields.io/github/commit-activity/m/TianMengLucky/BongoCat-X?style=flat)
![Contributors](https://img.shields.io/github/contributors/TianMengLucky/BongoCat-X?style=flat)
![Stars](https://img.shields.io/github/stars/TianMengLucky/BongoCat-X?style=flat)
![Issues](https://img.shields.io/github/issues/TianMengLucky/BongoCat-X?style=flat)
![Release](https://img.shields.io/github/v/release/TianMengLucky/BongoCat-X?style=flat)

![Alt](https://repobeats.axiom.co/api/embed/74334755a589dea40c5d31f8d2bcdc6c2bd39d87.svg "Repobeats analytics image")

## 📜 Licencia

El código fuente y el runtime nativo de BongoCat están bajo la licencia [AGPL-3.0-only](../LICENSE).

El modo de modelo integrado por defecto (`standard`) permanece bajo la licencia MIT. Los recursos de modelo incluidos en `resources/assets/models/standard`, `keyboard` y `gamepad` están cubiertos por la [declaración de licencia MIT](../LICENSE-MIT) independiente. Esta licencia MIT se aplica únicamente a los recursos de los modelos y sus ilustraciones asociadas; no cambia la licencia del código fuente ni del runtime nativo de BongoCat.

## 🧭 Arquitectura técnica

La aplicación C11 utiliza SDL3 para eventos, actualización de modelos y presentación en el hilo principal, y Nuklear para los ajustes. `src/rust/bongo-safe` gestiona el análisis de datos no fiables, las colas y métricas de entrada y el estado compartido de los hilos de plugins. El puente Live2D, el backend de recursos Vulkan y los adaptadores Windows COM/WIC usan C++17.

Los receptores de plataforma envían eventos de teclas y botones a una cola Rust acotada, agrupan el movimiento del puntero por separado y despiertan el hilo principal para actualizar parámetros e interfaz. Una ABI C versionada carga los plugins Live2D/Inox2D con OpenGL, Vulkan o Metal según la plataforma.

Consulta [Plugins de renderizado](model-plugins.md) para compatibilidad y compilación, y [Renderizado nativo](live2d-vulkan-metal.md) para límites y alcance de validación.

## ❓ Preguntas frecuentes

### 🔒 ¿BongoCat registra mis entradas de teclado o ratón?

BongoCat procesa las entradas localmente para animaciones y atajos y no sube datos de entrada ni de uso. Al activarse, el plugin de estadísticas puede guardar totales, tendencias diarias/horarias y tiempo de aplicaciones en primer plano localmente. No guarda texto escrito, orden de teclas, títulos de ventana, URL ni coordenadas. En Estadísticas se puede desactivar la captura, borrar el historial o exportarlo. Consulta [Estadísticas](statistics-plugin.md). Las actualizaciones solicitan metadatos públicos de versiones.

### 🖼️ ¿Cuál es el estado del soporte de Vulkan / Metal?

Ajustes → Aplicación → Backend de renderizado permite cambiar al instante: OpenGL ↔ Vulkan en Windows / Linux x64 y OpenGL ↔ Metal en macOS. Sistema usa OpenGL por defecto. Se conectaron la carga de texturas nativas, el dibujo de Cubism, la lectura de GPU y la recarga del modelo; se conservan el modelo y los movimientos/expresiones seleccionados. Los errores de inicialización o recarga restauran OpenGL. Vulkan/Metal son experimentales: las capas de teclas/efectos/puntero ya admiten todos los motores. Live2D admite resolución dinámica en OpenGL, Vulkan y Metal, con decodificación y escalado en segundo plano y carga nativa a la GPU síncrona. Desactivarla recarga las texturas según la calidad elegida. Los backends nativos almacenan el fondo de mesa en caché, lo incluyen en los límites del marco ajustado y redondean las cuatro esquinas en la GPU sin otra textura de pantalla completa. Las capas usan el renderizado por software de SDL, se almacenan hasta que cambie la entrada y se combinan con el cuadro nativo terminado antes de enviarlo a la GPU; el rendimiento sigue sin medir.

La optimización de renderizado aparece en Vulkan / Metal y está desactivada por defecto. Vulkan ofrece tres opciones; Metal solo grandes asignaciones. Inox2D Metal agrupa los vértices de cada fotograma; Live2D Metal muestra esta opción como no disponible. Sus tres subopciones se guardan por separado; los cambios recargan el modelo. Inox2D Vulkan admite grabación paralela y bloques grandes. Live2D Vulkan admite VMA y grabación paralela con mezcla normal, sin nodos fuera de pantalla ni máscaras de alta precisión. Live2D requiere 128 Drawables para búferes secundarios y 128 instantáneas dibujables para usar hasta cuatro hilos; los demás modelos conservan la ruta serial del SDK. Live2D Vulkan Bindless usa una tabla de hasta 64 texturas de atlas con mezcla normal y sin nodos fuera de pantalla; vuelve a la ruta anterior si faltan capacidades del dispositivo. Inox2D Bindless no está implementado. Los pools grandes pueden usar más memoria; las mejoras de rendimiento aún no se han medido.

Las compilaciones con SDK usan `BONGO_CAT_CUBISM_VULKAN=ON` en Windows / Linux x64 (`glslangValidator` o `glslang`, paquete Linux `glslang-tools`; Vulkan 1.3 al ejecutar) y `BONGO_CAT_CUBISM_METAL=ON` en macOS (herramientas Metal de Xcode). Use `OFF` para desactivar la extensión. Los archivos Metal y `.metallib` se incluyen solo en macOS; Vulkan y `.spv` solo en Windows / Linux x64, con variantes de mezcla. Las compilaciones de diagnóstico sin SDK omiten estos recursos Live2D. GitHub Actions prepara las herramientas y verifica los recursos por plataforma. Se realizaron comprobaciones estáticas; falta validar compilación y GPU. Véase [integración y validación](live2d-vulkan-metal.md).

## 🙏 Agradecimientos especiales
> [!TIP]
> Cada paso de BongoCat está impulsado por el espíritu del código abierto. Agradecemos sinceramente a todos los colaboradores de la comunidad por sus contribuciones desinteresadas (listadas a continuación en orden cronológico). Es vuestro apoyo lo que hace que la compañía en el escritorio sea más libre y genuina.❤️‍🔥


<a href="https://github.com/TianMengLucky/BongoCat-X/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=TianMengLucky/BongoCat-X" />
</a>


---

<div align="center">
Derechos de autor © 2026 - **BongoCat**<br>
By vladelaina<br>
Made with ❤️ & ⌨️
</div>

### Diseño de las opciones

Las preferencias usan tarjetas de grupos plegables, con Ventana, Modelo, Aplicación y Apariencia desplegados por defecto. Activa Diseño compacto en Apariencia para reducir el espacio sin ocultar las descripciones. Se guarda automáticamente.

### Complemento de estadísticas de entrada

El complemento Rust se compila con la aplicación por defecto. Actívalo, desactívalo o impórtalo en la página de complementos. La página de estadísticas muestra totales de teclado y ratón, gráficos diarios y por hora, almacenamiento local y exportación CSV. Consulta [Estadísticas](statistics-plugin.md). El tiempo de la aplicación activa incluye clasificación, totales por hora y un control independiente. La captura, el almacenamiento y la exportación usan un hilo independiente. El complemento registra la entrada de estadísticas al activarse y la elimina al desactivarse o desinstalarse.

Personaliza los nombres de unidades y los factores de movimiento y desplazamiento en Estadísticas; puedes guardarlos o restaurarlos. Apariencia usa una paleta común para los colores principal, secundario, del texto y del fondo de la página.

### Plugins de renderizado dinámicos

Preferencias → Plugins permite instalar, activar y eliminar renderizadores locales. Cambia a otro renderizador antes de modificar un plugin activo. Live2D utiliza un plugin C++; Inochi2D utiliza el plugin Rust Inox2D con OpenGL, Vulkan (Windows/Linux) y Metal (macOS), sin wgpu. La importación identifica primero el contenido del modelo (`.model3.json`, `.inp`, `.inx`). Todo el análisis y la escritura de JSON se realiza en Rust.

Las versiones portátiles de Windows se distribuyen como archivos ZIP. Mantén la carpeta extraída `plugins` junto al ejecutable. Consulta [Plugins de renderizado](model-plugins.md) para los paquetes, las dependencias sin conexión, la ABI, la asignación de parámetros y los límites de compatibilidad.

Cada push a `dev` que cambia código, recursos o configuración de compilación publica una versión preliminar nocturna en GitHub. Se omiten los cambios solo de documentación; no hay ejecución programada. Todas las plataformas usan el commit enviado. Estas versiones no reemplazan la última versión estable.

Las versiones nocturnas reutilizan la [página nightly](https://github.com/TianMengLucky/BongoCat-X/releases/tag/nightly), actualizan la etiqueta y reemplazan archivos con nombres fijos. Si falla una plataforma, conserva sus archivos anteriores.

### Linux Wayland / evdev

La entrada evdev experimental en Linux Wayland se activa con `BONGOCAT_ENABLE_EVDEV=1`. Primero se intenta abrir los dispositivos como usuario normal, en modo de solo lectura. Si faltan permisos, una instalación protegida solicita autenticación mediante `/usr/bin/sudo`, abre los dispositivos y reinicia tras abandonar inmediatamente los privilegios, antes de inicializar SDL, ajustes y modelos. El programa, sudo y sus directorios superiores deben pertenecer a root y no permitir escritura al grupo ni a otros usuarios. Las instalaciones de desarrollo y portátiles necesitan permisos administrados por separado. La ruta de sudo se configura con `BONGO_CAT_SUDO_EXECUTABLE`. Un fallo detiene el inicio; no se cambian permisos permanentes. Solo se vigilan los dispositivos presentes al inicio; conectar nuevos requiere reiniciar. No ejecutar directamente como root ni unirse al grupo `input`. La entrada puede incluir contraseñas y no se pausa al bloquear, cambiar de sesión u ocultar la mascota.

[SECURITY.md](../SECURITY.md#linux-input)

Preferencias → Preferencias → Ventana permite elegir una imagen de fondo personalizada: PNG, JPEG, WebP, BMP o GIF (primer fotograma). La imagen llena la ventana centrada, conservando sus proporciones, y es incompatible con el fondo de color sólido. Se guarda en el directorio de datos de la aplicación, por lo que mover el original no afecta al fondo. Máximo: 64 MB y 16 megapíxeles. Compatible con OpenGL, Vulkan y Metal.

Live2D reutiliza automáticamente los búferes de geometría y estados de renderizado repetidos. Las cargas dinámicas de OpenGL, los lotes y la caché de Vulkan, los búferes compartidos de Metal y las máscaras nativas adaptativas no requieren otro interruptor. El rendimiento y el comportamiento por plataforma están pendientes de medición.

Vulkan administra la memoria automáticamente: Live2D usa bloques VMA de 1 MiB por defecto o de 16 MiB con asignaciones grandes activadas. La presentación por capas de Windows conserva un destino de color. Al ocultar o minimizar, libera las cachés de lectura tras finalizar la GPU y las recrea cuando se necesitan. Calidad y resolución dinámica se configuran por separado.

Ajustes → Mascota → Rango de seguimiento: horizontal/vertical 25–400%, desactivado por defecto; 100% conserva el rango. Las patas Mver dibujadas pueden moverse más; Live2D mantiene sus límites. Mirada y ventana no cambian

### Varias teclas y caracteres legibles en modo espejo

Mver (OpenGL/Vulkan/Metal) recorta por separado la mano, el marco y los caracteres, uniendo las teclas pulsadas con + en un solo bocadillo. El marco y la cola se reflejan con el modelo; los caracteres siguen siendo legibles, también en modo de una tecla. Configura las tres áreas, el espacio alrededor de + y el límite por lado en `hand-layout.json` junto a las imágenes de las manos; reinicia tras editar. Consulta la [guía y el ejemplo](hand-layout.md).

El diálogo de acciones comprueba la referencia `Sound` y si existe el archivo, y muestra un control de volumen junto a los movimientos con audio. Ajusta entre 0 y 100% arrastrando o escribiendo; el doble clic restaura el 100% y el 0% silencia. Los valores se guardan por modelo y movimiento y se aplican al diálogo, atajos, movimientos aleatorios y menú contextual, también durante la reproducción. Otros movimientos que usan el mismo archivo conservan su propio volumen.

### Plugin de objetos

El plugin independiente de objetos se incluye por defecto. Actívalo en Preferencias → Plugins para mostrar Objetos. Admite importación de varias imágenes y arrastrar y soltar, posición, tamaño, rotación, opacidad, capas delanteras/traseras, bloqueo, duplicados y guardado de escenas con OpenGL, Vulkan y Metal. Seguir el área del modelo no fija el objeto a la malla. GIF muestra el primer fotograma; hasta 16 objetos en la mascota principal. Consulta [Plugin de objetos](items-plugin.md). `BONGO_CAT_BUILD_ITEMS=OFF` omite la biblioteca.

Preferencia de memoria de renderizado: Priorizar memoria de vídeo / Priorizar memoria del sistema. Solo Live2D Vulkan. Recarga el renderizado. La memoria de vídeo favorece la velocidad; la del sistema reduce la presión sobre la VRAM, pero puede ser más lenta. Los recursos no compatibles usan una alternativa automática. Otros motores asignan automáticamente; las GPU integradas comparten memoria.

### Sonidos del teclado

Activa Efectos de sonido en Plugins. Haz clic en una tecla para elegir audio, pasa el cursor para ver el nombre y pulsa la tecla física para reproducirlo. Las asignaciones se guardan automáticamente. Sonidos y objetos tienen hilos separados. WAV/MP3/Ogg Vorbis/FLAC en la mascota principal. Consulta [Efectos de sonido](sounds-plugin.md); `BONGO_CAT_BUILD_SOUNDS=OFF` omite la biblioteca.

La tienda Rust ofrece vistas previas, enlaces de autor y botones de descarga/desinstalación. SHA-256 se verifica antes de instalar; se muestra un estado pendiente si el catálogo no está publicado. [Complemento de tienda](store-plugin.md).

La búsqueda automática de actualizaciones está activada por defecto en Preferencias → Aplicación y se ejecuta una vez en segundo plano en cada inicio. El aviso de una nueva versión se conserva hasta abrir Ajustes y ofrece «Actualizar ahora» arriba a la derecha. Todos los avisos de Ajustes se pueden cerrar manualmente.

La página de modelos agrupa los modos del mismo paquete en una tarjeta con selector, incluidos los modos estándar, teclado y mando predeterminados. Puede arrastrar varios ZIP para importarlos en segundo plano. Cerrar Ajustes no interrumpe la importación. Al salir se detienen las importaciones pendientes y se espera a que termine la fuente actual antes de liberar los hilos y recursos temporales.

Los atajos están dentro de Preferencias, con el selector Ajustes / Atajos. La tienda está dentro de Modelo, con Modelos locales / Tienda. Se eliminan sus dos entradas independientes de la barra lateral; cada subpágina conserva su posición de desplazamiento.

Al activar Live2D sin Cubism Core se muestran ambos acuerdos oficiales. En Windows, Aceptar y descargar obtiene el SDK Native R5 por HTTPS oficial, extrae solo el Core dinámico compatible y activa el renderizado local. Cancelar y continuar mantiene el complemento activado sin renderizado. Se puede reintentar o importar manualmente. La aplicación y los paquetes no incluyen SDK ni Core.

Windows con gráficos AMD ofrece Presentación DirectComposition experimental, desactivada por defecto. La composición alfa DXGI se aplica al instante, limita eventos expose a 50 ms y desactiva VSync; si falla, restaura la presentación anterior. Mantiene la lectura de píxeles y no garantiza Independent Flip en juegos.

La ABI de los plugins de renderizado pasa a v2: actualiza o recompila juntos la aplicación y los plugins Live2D/Inox2D; los anteriores son incompatibles. Live2D Vulkan ofrece recursos fuera de pantalla bajo demanda, escalado PNG por filas en Rust y caché BC7 con alternativa RGBA8; consulta los [detalles de renderizado](live2d-vulkan-metal.md) para los límites de plataforma.
