#include <QApplication>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <signal.h>
#include "browser/browserwindow.h"

static BrowserWindow *globalWindow = nullptr;

void signalHandler(int signal) {
    if (signal == SIGINT && globalWindow) {
        globalWindow->saveSettings();
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("Navia");
    app.setApplicationName("Navia Browser");
    app.setStyle("Fusion");

    // Versión empaquetada (portable): todo relativo al ejecutable
    QString appDir = QCoreApplication::applicationDirPath();
    QString libDir = appDir + "/lib";
    QString pluginsDir = appDir + "/plugins";
    if (QDir(libDir).exists()) {
        // El subproceso QtWebEngineProcess hereda esta ruta y carga sus Qt
        // desde ./lib sin depender del sistema
        QString old = qEnvironmentVariable("LD_LIBRARY_PATH");
        qputenv("LD_LIBRARY_PATH", (libDir + (old.isEmpty() ? QString() : ":" + old)).toLocal8Bit());
    }
    if (QDir(pluginsDir).exists())
        QCoreApplication::setLibraryPaths({pluginsDir});

    // Auto-configurar QtWebEngine para que el binario no dependa del sistema
    QString weProcess = appDir + "/libexec/QtWebEngineProcess";
    if (QFile::exists(weProcess))
        qputenv("QTWEBENGINEPROCESS_PATH", weProcess.toLocal8Bit());
    QString weResources = appDir + "/resources";
    if (QDir(weResources).exists())
        qputenv("QTWEBENGINE_RESOURCES_PATH", weResources.toLocal8Bit());
    QString weLocales = appDir + "/translations/qtwebengine_locales";
    if (QDir(weLocales).exists())
        qputenv("QTWEBENGINE_LOCALES_PATH", weLocales.toLocal8Bit());

    // Flags de Chromium que añade Navia (se concatenan con los que vengan del
    // entorno, p. ej. --no-sandbox de navia.sh cuando se ejecuta como root).
    QStringList naviaFlags;

    // Límite de procesos renderer: ahorra RAM haciendo que las pestañas
    // compartan proceso (por defecto Chromium abre un proceso por pestaña).
    // En Chromium moderno el switch solo se respeta si el feature
    // "RendererProcessLimit" está activado, por eso se fuerza con
    // --enable-features.
    naviaFlags << "--enable-features=RendererProcessLimit";
    naviaFlags << "--renderer-process-limit=3";

    // Widevine CDM (DRM: Netflix, Prime Video, Disney+, ...). El plugin es
    // propietario y no se distribuye con el proyecto: deploy.sh lo copia desde
    // una instalación local de Chrome/Chromium a ./widevine/libwidevinecdm.so.
    // Si está presente, se lo indicamos a Chromium con --widevine-path.
    QString cdmPath = appDir + "/widevine/libwidevinecdm.so";
    if (QFile::exists(cdmPath))
        naviaFlags << "--widevine-path=" + cdmPath;

    if (!naviaFlags.isEmpty()) {
        QString flags = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS");
        if (!flags.isEmpty())
            flags += " ";
        flags += naviaFlags.join(' ');
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags.toLocal8Bit());
    }

    // User-Agent de Chrome real, acorde al Chromium embebido en QtWebEngine
    // (Qt 6.11.2 -> Chromium 140.0.7339.264, coincide con el Sec-CH-UA real)
    QWebEngineProfile::defaultProfile()->setHttpUserAgent(
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/140.0.7339.264 Safari/537.36"
    );

    // Tope del caché HTTP en disco: Qt usa por defecto DiskHttpCache (los
    // datos no ocupan RAM del proceso principal), pero sin límite el disco
    // crece sin control. 256 MB es suficiente para una sesión normal.
    QWebEngineProfile::defaultProfile()->setHttpCacheMaximumSize(256 * 1024 * 1024);

    // Cookies y datos persistentes (sobreviven al reinicio)
    QString storagePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(storagePath);
    QWebEngineProfile::defaultProfile()->setPersistentStoragePath(storagePath);
    QWebEngineProfile::defaultProfile()->setPersistentCookiesPolicy(
        QWebEngineProfile::ForcePersistentCookies);

    BrowserWindow window;
    globalWindow = &window;
    signal(SIGINT, signalHandler);

    window.show();
    window.restoreWindowGeometry();

    // Open URL passed as command-line argument (e.g. from "Abrir con...")
    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (!arg.startsWith('-')) {
            window.loadUrl(arg);
            break;
        }
    }

    return app.exec();
}
