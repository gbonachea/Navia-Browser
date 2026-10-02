#!/usr/bin/env bash
set -euo pipefail

APP="build/NaviaBrowser"
DEST="dist/Navia"

# Qt 6.11.2 (Qt Installer en /opt/qt) - se usa para resolver dependencias con ldd
QT_PREFIX="/opt/qt/6.11.2/gcc_64"
export LD_LIBRARY_PATH="$QT_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [ ! -f "$APP" ]; then
    echo "ERROR: $APP not found. Run: cmake --build build"
    exit 1
fi

# System libraries that MUST NOT be bundled
EXCLUDE=(
    libc.so libm.so libdl.so libpthread.so librt.so libutil.so
    libstdc++.so libgcc_s.so
    libGL.so libGLX.so libEGL.so libOpenGL.so libGLdispatch.so
    libX11.so libxcb.so libXau.so libXdmcp.so libXext.so
    libXfixes.so libXrender.so libXrandr.so libXcomposite.so
    libXdamage.so libXtst.so libxkbfile.so
    libdrm.so libgbm.so
    libwayland-client.so libwayland-server.so libwayland-egl.so
    libwayland-cursor.so
    libxkbcommon.so
    libsystemd.so libcap.so
    libexpat.so libffi.so libmd.so
    libresolv.so libblkid.so libmount.so libselinux.so
)

should_exclude() {
    local name="$1"
    for pat in "${EXCLUDE[@]}"; do
        if [[ "$name" == "$pat"* ]]; then return 0; fi
    done
    return 1
}

rm -rf "$DEST"
mkdir -p "$DEST/lib" "$DEST/plugins"

echo "=== 1. Copiando ejecutable ==="
cp "$APP" "$DEST/"

echo "=== 2. Copiando página de inicio ==="
mkdir -p "$DEST/page/page"
cp page/page.html "$DEST/page/"
cp page/page/*.png "$DEST/page/page/"
echo "  page.html e iconos copiados"

echo "=== 2b. Copiando licencias ==="
cp LICENSE "$DEST/"
cp BRANDING.md "$DEST/"
cp TRADEMARKS.md "$DEST/"
echo "  Licencias copiadas"

echo "=== 3. Copiando librerías (sin recursión) ==="
# Collect direct deps of the main binary, excluding system libs
declare -A copied
collect() {
    local binary="$1"
    while IFS= read -r line; do
        local path
        path=$(echo "$line" | awk '{print $3}')
        [[ "$path" == /* && "$path" != "" ]] || continue
        local base
        base=$(basename "$path")
        should_exclude "$base" && continue
        [[ -n "${copied[$base]:-}" ]] && continue
        copied["$base"]=1
        cp -L "$path" "$DEST/lib/"
    done < <(ldd "$binary" 2>/dev/null || true)
}

# Collect main binary deps + transitive deps (one level deeper)
collect "$APP"
for f in "$DEST"/lib/*.so*; do
    [ -f "$f" ] && collect "$f"
done

echo "  Copiadas $(ls "$DEST/lib"/*.so* 2>/dev/null | wc -l) librerías"

echo "=== 4. Copiando plugins Qt ==="
QT_PLUGIN_DIR="$QT_PREFIX/plugins"
if [ -d "$QT_PLUGIN_DIR" ]; then
    cp -r "$QT_PLUGIN_DIR"/* "$DEST/plugins/"
    echo "  Plugins copiados"

    # Recolectar dependencias transitivas de los plugins de plataforma (libQt6XcbQpa, etc.)
    for dir in platforms platforminputcontexts xcbglintegrations; do
        for f in "$DEST/plugins/$dir"/*.so*; do
            [ -f "$f" ] && collect "$f"
        done
    done
    # Segunda pasada por si las nuevas libs traen más dependencias
    for f in "$DEST"/lib/*.so*; do
        [ -f "$f" ] && collect "$f"
    done
else
    echo "  WARNING: $QT_PLUGIN_DIR no encontrado"
fi

echo "=== 4b. Copiando librerías XCB cargadas en runtime ==="
# Qt carga estas librerías via dlopen (no aparecen en ldd)
XCB_RUNTIME=(
    libxcb-xinerama.so
    libxcb-icccm.so
    libxcb-image.so
    libxcb-keysyms.so
    libxcb-randr.so
    libxcb-render.so
    libxcb-render-util.so
    libxcb-shape.so
    libxcb-shm.so
    libxcb-sync.so
    libxcb-util.so
    libxcb-xfixes.so
    libxcb-xkb.so
    libxcb-cursor.so
    libxcb-xinput.so
)
for lib in "${XCB_RUNTIME[@]}"; do
    # Buscar cualquier soname que coincida (libxcb-*.so*, libxcb-*.so.N)
    for f in /usr/lib/x86_64-linux-gnu/"$lib"*; do
        [ -f "$f" ] || continue
        base=$(basename "$f")
        should_exclude "$base" && continue
        [[ -n "${copied[$base]:-}" ]] && continue
        copied["$base"]=1
        cp -L "$f" "$DEST/lib/"
    done
done
echo "  XCB runtime copiadas"

echo "=== 5. Copiando WebEngine ==="
for wep_candidate in \
    "$QT_PREFIX/libexec/QtWebEngineProcess" \
    "/usr/lib/qt6/libexec/QtWebEngineProcess" \
    "/usr/lib/x86_64-linux-gnu/qt6/libexec/QtWebEngineProcess" \
    "/usr/libexec/qt6/QtWebEngineProcess"; do
    if [ -f "$wep_candidate" ]; then
        WEP="$wep_candidate"
        break
    fi
done
if [ -n "${WEP:-}" ]; then
    mkdir -p "$DEST/libexec"
    cp -L "$WEP" "$DEST/libexec/"
    collect "$DEST/libexec/QtWebEngineProcess"
    echo "  QtWebEngineProcess copiado"
else
    echo "  WARNING: QtWebEngineProcess no encontrado"
fi

for res in "$QT_PREFIX/resources" "/usr/share/qt6/resources" "/usr/lib/x86_64-linux-gnu/qt6/resources"; do
    if [ -d "$res" ]; then
        mkdir -p "$DEST/resources"
        cp -r "$res"/* "$DEST/resources/" 2>/dev/null || true
        echo "  Resources copiados"
        break
    fi
done

for trans in "$QT_PREFIX/translations" "/usr/share/qt6/translations" "/usr/lib/x86_64-linux-gnu/qt6/translations"; do
    if [ -d "$trans" ]; then
        mkdir -p "$DEST/translations"
        cp -r "$trans"/* "$DEST/translations/" 2>/dev/null || true
        echo "  Translations copiados"
        break
    fi
done

echo "=== 5b. Ajustando RPATH para el modo portable ==="
if command -v patchelf >/dev/null 2>&1; then
    # Cada .so de ./lib resuelve sus dependencias dentro de ./lib (sin LD_LIBRARY_PATH)
    # --force-rpath => DT_RPATH, que tiene prioridad sobre LD_LIBRARY_PATH del sistema
    n=0
    for f in "$DEST"/lib/*.so*; do
        [ -f "$f" ] || continue
        patchelf --force-rpath --set-rpath '$ORIGIN' "$f" 2>/dev/null && n=$((n+1))
    done
    # QtWebEngineProcess vive en ./libexec y busca sus Qt en ./lib
    [ -f "$DEST/libexec/QtWebEngineProcess" ] && \
        patchelf --force-rpath --set-rpath '$ORIGIN/../lib' "$DEST/libexec/QtWebEngineProcess"
    # Los plugins Qt (libqxcb.so, etc.) buscan sus Qt en ./lib
    m=0
    while IFS= read -r f; do
        rel=$(realpath --relative-to="$(dirname "$f")" "$DEST/lib")
        patchelf --force-rpath --set-rpath "\$ORIGIN/$rel" "$f" 2>/dev/null && m=$((m+1))
    done < <(find "$DEST/plugins" -name '*.so' -o -name '*.so.*' | sort -u)
    echo "  RPATH \$ORIGIN aplicado a $n librerías y $m plugins"
else
    echo "  WARNING: patchelf no encontrado; el portable necesitará navia.sh"
fi

echo "=== 5c. Widevine CDM (DRM) ==="
# El CDM de Widevine es propietario y no se distribuye con el proyecto.
# deploy.sh lo copia desde una instalación local de Chrome/Chromium, desde
# un fichero en downloads/, o desde la ruta indicada en WIDEVINE_CDM_PATH.
# Si no se encuentra, Navia se genera sin DRM (en runtime Qt intentará
# localizar el CDM del sistema si el usuario tiene Chrome/Chromium instalado).

DEST_CDM_DIR="$DEST/widevine"

copy_cdm() {
    local src="$1"
    [ -f "$src" ] || return 1
    mkdir -p "$DEST_CDM_DIR"
    cp -L "$src" "$DEST_CDM_DIR/libwidevinecdm.so"
    # La licencia del CDM acompaña al binario cuando existe (WidevineCdm/LICENSE)
    [ -f "$(dirname "$src")/LICENSE" ] && cp -L "$(dirname "$src")/LICENSE" "$DEST_CDM_DIR/LICENSE"
    echo "  CDM copiado desde: $src"
    return 0
}

find_chrome_component_cdm() {
    # Directorio versionado del componente (~/.config/google-chrome/WidevineCdm/<ver>/...)
    find "$HOME/.config/google-chrome/WidevineCdm" -name libwidevinecdm.so 2>/dev/null \
        | head -n1 || true
}

CDM_FOUND=0
if [ -n "${WIDEVINE_CDM_PATH:-}" ] && copy_cdm "$WIDEVINE_CDM_PATH"; then
    CDM_FOUND=1
elif copy_cdm "downloads/libwidevinecdm.so"; then
    CDM_FOUND=1
elif copy_cdm "/opt/google/chrome/libwidevinecdm.so" \
  || copy_cdm "/opt/google/chrome/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so" \
  || copy_cdm "/usr/lib/chromium/libwidevinecdm.so" \
  || copy_cdm "/usr/lib/chromium-browser/libwidevinecdm.so" \
  || copy_cdm "/usr/lib64/chromium/libwidevinecdm.so"; then
    CDM_FOUND=1
else
    chrome_cdm="$(find_chrome_component_cdm)"
    [ -n "$chrome_cdm" ] && copy_cdm "$chrome_cdm" && CDM_FOUND=1
fi

if [ "$CDM_FOUND" -eq 1 ]; then
    echo "  Widevine CDM empaquetado en $DEST_CDM_DIR/"
else
    echo "  WARNING: libwidevinecdm.so no encontrado; el paquete no incluirá DRM."
    echo "  Opciones: colocar el CDM en downloads/libwidevinecdm.so o exportar WIDEVINE_CDM_PATH"
fi

echo "=== 6. Creando wrapper navia.sh ==="
cat > "$DEST/navia.sh" << 'WRAPPER'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR/plugins/platforms"
export QTWEBENGINEPROCESS_PATH="$DIR/libexec/QtWebEngineProcess"
export QTWEBENGINE_RESOURCES_PATH="$DIR/resources"
if [ "$(id -u)" -eq 0 ]; then
    export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
fi
exec "$DIR/NaviaBrowser" "$@"
WRAPPER
chmod +x "$DEST/navia.sh"
echo "  navia.sh creado"

echo "=== 7. Instalando en el sistema ==="
# Copy icon to destination
ICON_SRC="icons/icon.png"
if [ -f "$ICON_SRC" ]; then
    cp "$ICON_SRC" "$DEST/navia.png"
fi

# Generate desktop file with absolute paths
ABSOLUTE_DEST="$(cd "$DEST" && pwd)"
cat > "$DEST/Navia_Browser.desktop" << DESKTOP
[Desktop Entry]
Name=Navia Browser
Version=1.0
Exec=/opt/Navia/navia.sh %U
Comment=Navegador Web Ligero
Icon=navia
Type=Application
Terminal=false
StartupNotify=true
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;application/xml;application/rss+xml;application/rdf+xml;image/gif;image/jpeg;image/png;image/webp;image/svg+xml;image/bmp;image/x-icon;application/pdf;x-scheme-handler/http;x-scheme-handler/https;
DESKTOP

# Install desktop entry
xdg-desktop-menu install --novendor "$DEST/Navia_Browser.desktop" 2>&1 || echo "  (desktop ya instalado)"
xdg-icon-resource install --context apps --size 128 "$DEST/navia.png" navia-nav 2>&1 || echo "  (icono ya instalado)"
xdg-mime default navia.desktop text/html 2>&1 || true
xdg-mime default navia.desktop application/pdf 2>&1 || true
echo "  Navia registrado como aplicación del sistema"
echo "  (Reinicie el gestor de archivos si no aparece en 'Abrir con...')"

echo ""
echo "=== EMPAQUETADO COMPLETO ==="
echo "  Ruta: $DEST"
echo "  Tamaño: $(du -sh "$DEST" | cut -f1)"
echo "  Librerías: $(ls "$DEST/lib"/*.so* 2>/dev/null | wc -l)"
echo "  Plugins: $(find "$DEST/plugins" -name '*.so' 2>/dev/null | wc -l)"
if [ "$CDM_FOUND" -eq 1 ]; then
    echo "  Widevine CDM: incluido (DRM activo)"
else
    echo "  Widevine CDM: NO incluido (sin DRM)"
fi
echo ""
echo "Ejecuta: $DEST/navia.sh"
