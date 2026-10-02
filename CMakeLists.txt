cmake_minimum_required(VERSION 3.16)

project(NaviaBrowser)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Portable: el binario busca sus librerías Qt en ./lib junto a sí mismo
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")

find_package(Qt6 REQUIRED COMPONENTS
    Widgets
    WebEngineWidgets
    Pdf
    PdfWidgets
)

add_executable(NaviaBrowser
    main.cpp
    browser/browserwindow.cpp
    browser/browserwindow.h
    browser/browserview.cpp
    browser/browserview.h
    browser/adblocker.cpp
    browser/adblocker.h
    storage/history.cpp
    storage/history.h
    storage/bookmarks.cpp
    storage/bookmarks.h
    downloads/downloadmanager.cpp
    downloads/downloadmanager.h
    pdf/pdfviewerwidget.cpp
    pdf/pdfviewerwidget.h
    resources.qrc
)

# RPATH (DT_RPATH, estilo viejo) en vez de RUNPATH: tiene prioridad sobre
# LD_LIBRARY_PATH, así el portable no se ve afectado por las Qt del sistema
target_link_options(NaviaBrowser PRIVATE "-Wl,--disable-new-dtags")

target_link_libraries(NaviaBrowser
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::Pdf
    Qt6::PdfWidgets
)

