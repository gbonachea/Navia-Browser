# Navia Browser

Navegador web moderno y ligero basado en **Qt6 WebEngine**. Diseñado con una interfaz oscura, sin bordes del sistema, y con herramientas integradas para privacidad y productividad.

## Características

### Navegación
- Pestañas con botón `+` para nueva pestaña
- Barra de direcciones con validación inteligente (detecta URLs, dominios, y redirige búsquedas a DuckDuckGo)
- Navegación: Atrás, Adelante, Recargar, Inicio
- Página de inicio galáctica (`navia://newtab`) con accesos directos animados, buscador DuckDuckGo, reloj Sci-Fi y fondo de estrellas interactivo
- Apertura de enlaces `target="_blank"` en nueva pestaña
- Arrastrar y soltar archivos (PDFs y locales) sobre la barra de pestañas

### Interfaz
- Ventana sin bordes del sistema (`FramelessWindowHint`) con diseño oscuro personalizado
- Botones de ventana (Minimizar, Maximizar/Restaurar, Cerrar) integrados en la barra de herramientas
- Arrastre de ventana desde la barra de herramientas (excluyendo botones y campos de texto)
- Redimensionamiento por bordes y esquinas (márgenes de 3-12px, tamaño mínimo 800×500)
- Tema oscuro completo (paleta Fusion + stylesheet Qt)
- Barra de estado que muestra URLs al pasar el cursor sobre enlaces

### Marcadores e Historial
- Agregar marcador desde la pestaña actual
- Lista de favoritos con doble clic para navegar
- Importar/Exportar marcadores en formato HTML (Netscape)
- Historial de navegación persistente con doble clic para navegar
- Limpieza de historial

### Privacidad y Seguridad
- **Anti-fingerprinting**: Inyección de JavaScript al inicio del documento que aleatoriza `toDataURL` y añade ruido sub-píxel a `fillText`
- **AdBlocker integrado**: Interceptor de solicitudes basado en filtros EasyList
- Bloqueo de rastreadores
- Gestión de cookies (borrar al cerrar, limpiar datos)
- Limpieza de caché HTTP
- Manejo de errores de certificado SSL

### Descargas
- Gestor de descargas con cola persistente
- Pausar, Reanudar, Cancelar, Limpiar completadas
- Progreso en tiempo real (bytes/porcentaje)
- Enrutamiento automático de PDFs al visor nativo
- Visualización inline de imágenes y texto

### Visor de PDF
- Visor nativo con QtPdf
- Navegación página a página (anterior/siguiente/número)
- Zoom y ajuste al ancho
- Apertura desde descargas o arrastrando archivos

### Traductor
- Integración con Google Translate
- Selección de idioma de destino (10 idiomas)
- Apertura en nueva pestaña

### Herramientas de Desarrollador
- Inspector Web (`Ctrl+Shift+I` / `F12`)
- Ver código fuente (`Ctrl+U`)
- Guardar página como PDF (menú contextual)

### Control de Zoom
- Acercar (`Ctrl++`), Alejar (`Ctrl+-`), Restablecer (`Ctrl+0`)
- Rango: 30% – 200%

### Autocompletado Inteligente
- Sugerencias de DuckDuckGo (mín. 2 caracteres, 300ms de espera)
- Historial y marcadores en el modelo de autocompletado
- Menú desplegable con estilo oscuro

### Proxy
- Soporte para proxy HTTP y SOCKS5
- Configuración de servidor, puerto, usuario y contraseña
- Persistente entre sesiones

### Personalización
- Página de inicio configurable
- Tema oscuro completo con barras de desplazamiento personalizadas
- Ventana redimensionable y arrastrable

## Requisitos del Sistema

- **Sistema operativo**: Linux (x86_64)
- **Qt 6.11** (instalado vía Qt Installer en `/opt/qt/6.11.2/gcc_64`) con los siguientes módulos:
  - Qt6 Widgets
  - Qt6 WebEngine Widgets
  - Qt6 Pdf
  - Qt6 PdfWidgets
- **Compilador**: GCC (C++17)
- **CMake** ≥ 3.16

Dependencias de compilación (Debian/Ubuntu):

```bash
sudo apt install build-essential cmake pkg-config \
    qt6-base-dev qt6-webengine-dev qt6-tools-dev \
    qt6-l10n-tools qt6-translations-l10n \
    libgl1-mesa-dev libx11-dev libxext-dev \
    libxfixes-dev libxi-dev libxrender-dev \
    libxcb1-dev libxcb-glx0-dev libxcb-icccm4-dev \
    libxcb-image0-dev libxcb-keysyms1-dev \
    libxcb-randr0-dev libxcb-render0-dev \
    libxcb-shape0-dev libxcb-shm0-dev \
    libxcb-sync-dev libxcb-xfixes-dev \
    libxcb-xinerama0-dev libxcb-xkb-dev \
    libxkbcommon-dev libxkbcommon-x11-dev \
    libfontconfig1-dev libfreetype6-dev \
    libxss1 libasound2-dev libpulse-dev \
    libudev-dev libdrm2 libgbm1 libnss3-dev \
    libgtk-3-dev libatk-bridge2.0-dev \
    libdconf-dev libxcomposite-dev libxcursor-dev \
    libxdamage-dev libxrandr-dev libxss-dev \
    libxtst-dev libsqlite3-dev libjpeg-dev \
    libpng-dev libtiff-dev libwebp-dev \
    libicu-dev libxml2-dev libxslt1-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

## Compilación

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/opt/qt/6.11.2/gcc_64
make -j$(nproc)
./NaviaBrowser
```

## Empaquetado Portátil

```bash
./deploy.sh
```

Esto genera `dist/Navia/` con ejecutable, librerías, plugins Qt, QtWebEngineProcess, página de inicio, licencias, un wrapper `navia.sh` y, si se encuentra, el CDM de Widevine para reproducir vídeo con DRM.

```bash
./dist/Navia/navia.sh
```

El script también registra Navia como aplicación del sistema (icono, MIME types, asociación de PDF/HTML).

## DRM (Widevine) — Netflix, Prime Video, Disney+

Qt WebEngine no incluye el CDM de Widevine por licencia. Navia lo detecta
automáticamente en tiempo de ejecución: si existe `widevine/libwidevinecdm.so`
junto al ejecutable, lo activa añadiendo `--widevine-path` a los flags de
Chromium (ver `main.cpp`).

`deploy.sh` lo copia automáticamente al paquete desde (por orden de prioridad):

1. `WIDEVINE_CDM_PATH` (variable de entorno)
2. `downloads/libwidevinecdm.so` (puedes dejarlo ahí manualmente)
3. Rutas estándar de Chrome/Chromium del sistema:
   `/opt/google/chrome/...`, `/usr/lib/chromium/...`,
   `/usr/lib/chromium-browser/...`, `~/.config/google-chrome/WidevineCdm/...`

Si no se encuentra, el paquete se genera **sin** DRM: en ese caso Qt intentará
localizar el CDM del sistema en tiempo de ejecución (solo funcionará si el
usuario tiene Chrome/Chromium instalado en una ruta estándar).

Notas:

- El CDM debe extraerse de un **Chrome/Chromium reciente** para ser compatible
  con el Chromium 140.0.7339.264 embebido en Qt 6.11.2.
- En Linux de escritorio Widevine opera en nivel de seguridad **L3** (software,
  sin HDCP): Netflix se limita a ~720p y no hay 4K/HDR. Es una limitación de la
  plataforma, no del navegador.
- El User-Agent ya simula Chrome 140 (`main.cpp`), requisito de estos servicios.
- Para verificar: visita una demo DRM (Bitmovin/Shaka) o prueba en la consola
  `navigator.requestMediaKeySystemAccess('com.widevine.alpha', ...)`.

## Estructura del Proyecto

```
├── CMakeLists.txt          # Configuración de compilación
├── main.cpp                # Punto de entrada
├── browser/
│   ├── browserwindow.h/cpp # Ventana principal, UI, navegación, settings
│   ├── browserview.h/cpp   # Vista web personalizada (context menu, anti-fp, new window)
│   ├── adblocker.h/cpp     # Bloqueador de anuncios (EasyList)
├── storage/
│   ├── bookmarks.h/cpp     # Gestión de marcadores
│   ├── history.h/cpp       # Historial de navegación
├── downloads/
│   ├── downloadmanager.h/cpp # Gestor de descargas
├── pdf/
│   ├── pdfviewerwidget.h/cpp # Visor nativo de PDF
├── page/
│   ├── page.html           # Página de inicio galáctica
│   ├── page/*.png          # Iconos de accesos directos
├── icons/                  # Iconos de la interfaz
├── easylist/
│   ├── easylist.txt        # Filtros de bloqueo
├── deploy.sh               # Script de empaquetado portátil
├── run.sh                  # Script de instalación y compilación
├── resources.qrc           # Recursos Qt
├── Navia_Browser.desktop   # Archivo de entrada de escritorio
├── LICENSE                 # GPLv3
├── BRANDING.md             # Política de marca (assets)
├── TRADEMARKS.md           # Política de marcas registradas
```

## Licencias

- **Código fuente** (`/` → código): **GNU General Public License v3.0** – ver `LICENSE`
- **Assets gráficos** (`/assets`): **Todos los derechos reservados** – ver `BRANDING.md`
- **Marca "Navia"** (`/branding`): **Trademark Policy** – ver `TRADEMARKS.md`

## Atribución

Al referenciar Navia Browser en materiales publicados, incluir:

> Navia Browser — https://github.com/art-ripoll/navia

## Créditos

Desarrollado por **GusBonachea** by **ART Ripoll**, **B&R Corp**.

---

*Navia Browser — Un navegador para el Stellar.*
