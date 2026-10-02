#!/bin/bash

# Script para ejecutar el navegador Navia con todas las dependencias
# Este script instala todas las dependencias necesarias, compila el proyecto y lo ejecuta

set -e  # Salir si hay algún error

echo "=== Instalando dependencias del sistema ==="

# Actualizar el sistema
sudo apt update

# Instalar dependencias básicas de compilación
sudo apt install -y build-essential cmake pkg-config

# Instalar dependencias de Qt6 WebEngine
sudo apt install -y \
    qt6-base-dev \
    qt6-webengine-dev \
    qt6-tools-dev \
    qt6-l10n-tools \
    qt6-translations-l10n

# Instalar dependencias de X11 y gráficos
sudo apt install -y \
    libgl1-mesa-dev \
    libx11-dev \
    libxext-dev \
    libxfixes-dev \
    libxi-dev \
    libxrender-dev \
    libxcb1-dev \
    libxcb-glx0-dev \
    libxcb-icccm4-dev \
    libxcb-image0-dev \
    libxcb-keysyms1-dev \
    libxcb-randr0-dev \
    libxcb-render0-dev \
    libxcb-shape0-dev \
    libxcb-shm0-dev \
    libxcb-sync-dev \
    libxcb-xfixes0-dev \
    libxcb-xinerama0-dev \
    libxcb-xkb-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-dev

# Instalar dependencias de fuentes y audio
sudo apt install -y \
    libfontconfig1-dev \
    libfreetype6-dev \
    libxss1 \
    libasound2-dev \
    libpulse-dev \
    libudev-dev

# Instalar dependencias de DRM y GBM para WebEngine
sudo apt install -y \
    libdrm2 \
    libgbm1 \
    libnss3-dev

# Instalar dependencias de GTK y accesibilidad
sudo apt install -y \
    libgtk-3-dev \
    libatk-bridge2.0-dev \
    libdconf-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxdamage-dev \
    libxrandr-dev \
    libxss-dev \
    libxtst-dev

# Instalar dependencias adicionales que pueden ser necesarias
sudo apt install -y \
    libwebkit2gtk-4.0-dev \
    libsqlite3-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libwebp-dev \
    libicu-dev \
    libxml2-dev \
    libxslt1-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev

echo "=== Dependencias instaladas exitosamente ==="

echo "=== Creando directorio de compilación ==="
mkdir -p build
cd build

echo "=== Configurando el proyecto con CMake ==="
# Qt 6.11.2 instalado vía Qt Installer en /opt/qt
cmake .. -DCMAKE_PREFIX_PATH=/opt/qt/6.11.2/gcc_64

echo "=== Compilando el proyecto ==="
make -j$(nproc)

echo "=== Compilación completada ==="

echo "=== Ejecutando el navegador ==="
echo "El navegador Navia se está ejecutando..."
echo "Presiona Ctrl+C para detenerlo"

# Ejecutar el navegador
./NaviaBrowser