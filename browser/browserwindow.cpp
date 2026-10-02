#include "browserwindow.h"

#include <QWebEngineView>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QWebEnginePage>
#include <QWebEngineCertificateError>
#include <QToolBar>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QObject>
#include <QSizeGrip>
#include <QPushButton>
#include <QWidget>
#include <QVBoxLayout>
#include <QDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QIcon>
#include <QSpacerItem>
#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QFontMetrics>
#include <QSettings>
#include <QPalette>
#include <QApplication>
#include <QWebEngineCookieStore>
#include <QCheckBox>
#include <QUrl>
#include <QRegularExpression>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMenu>
#include <QTimer>
#include <QMimeData>
#include <QDateTime>
#include <QScreen>

namespace {

class DragHelper : public QObject {
    QPoint lastPos;
    QWidget *target;
public:
    DragHelper(QWidget *titleBar, QWidget *targetWidget)
        : QObject(titleBar), target(targetWidget)
    {
        titleBar->installEventFilter(this);
    }

    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                lastPos = me->globalPosition().toPoint() - target->pos();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                target->move(me->globalPosition().toPoint() - lastPos);
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

QWidget* makeTitleBar(QDialog *dialog, const QString &title) {
    QWidget *bar = new QWidget();
    bar->setFixedHeight(34);
    bar->setStyleSheet("background-color: #2d3842;");

    QHBoxLayout *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(12, 0, 8, 0);

    QLabel *lbl = new QLabel(title);
    lbl->setStyleSheet("color: #dcdcdc; font-size: 13px; font-weight: bold; background: transparent;");

    QPushButton *closeBtn = new QPushButton();
    closeBtn->setFixedSize(14, 14);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { background-color: #cc0000; border: none; border-radius: 7px; }"
        "QPushButton:hover { background-color: #ff4444; }"
        "QPushButton:pressed { background-color: #880000; }"
    );
    QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::reject);

    lay->addWidget(lbl);
    lay->addStretch();
    lay->addWidget(closeBtn);

    new DragHelper(bar, dialog);
    return bar;
}

QPoint popupPosition(QPushButton *btn, int popupW, int popupH) {
    QPoint btnPos = btn->mapToGlobal(QPoint(0, 0));
    int px = btnPos.x() + btn->width() / 2 - popupW / 2;
    int py = btnPos.y() + btn->height();

    QScreen *screen = QGuiApplication::screenAt(btnPos);
    if (screen) {
        QRect sr = screen->availableGeometry();
        if (px + popupW > sr.right())  px = sr.right() - popupW;
        if (px < sr.left())            px = sr.left();
        if (py + popupH > sr.bottom()) py = btnPos.y() - popupH;
        if (py < sr.top())             py = sr.top();
    }
    return {px, py};
}

} // anonymous namespace

BrowserWindow::BrowserWindow()
{

    QWidget *centralWidget = new QWidget();
    QVBoxLayout *centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 6, 0, 0);
    centralLayout->setSpacing(0);

    tabs = new QTabWidget();
    tabs->setTabsClosable(true);
    tabs->setStyleSheet(
        "QTabBar::close-button { image: url(:/icons/icons/close.png); }"
        "QTabBar::tab { border-top-left-radius: 8px; border-top-right-radius: 8px; }"
    );

    linkStatusBar = new QLabel(this);
    linkStatusBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    linkStatusBar->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(45, 56, 66, 200);"
        "  color: #8ab4f8;"
        "  font-size: 12px;"
        "  padding: 3px 10px;"
        "  border: 1px solid #5a6b7a;"
        "  border-radius: 8px;"
        "}"
    );
    linkStatusBar->setVisible(false);

    centralLayout->addWidget(tabs, 1);

    setCentralWidget(centralWidget);
    setMouseTracking(true);
    centralWidget->setMouseTracking(true);
    tabs->setMouseTracking(true);
    qApp->installEventFilter(this);
    setWindowTitle("Navia");
    setWindowIcon(QIcon(":/icons/icons/icon.png"));
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setMinimumSize(800, 500);



    // La geometría se restaurará después de show() en main.cpp

    settings = new QSettings("Navia", "Navia Browser");
    proxyType = settings->value("proxy/type", "Ninguno").toString();
    proxyHost = settings->value("proxy/host", "").toString();
    proxyPort = settings->value("proxy/port", 8080).toInt();
    proxyUser = settings->value("proxy/user", "").toString();
    proxyPass = settings->value("proxy/pass", "").toString();
    clearCookiesOnClose = settings->value("privacy/clearCookies", false).toBool();
    blockTrackers = settings->value("privacy/blockTrackers", true).toBool();
    restoreOnStart = settings->value("session/restoreOnStart", false).toBool();
    QString easylistPath = ":/easylist/easylist.txt";
    if (!QFileInfo::exists(easylistPath))
        easylistPath = "easylist/easylist.txt";
    interceptor = new AdBlocker(easylistPath);
    int adblockState = settings->value("privacy/adblock", 1).toInt();
    adblockEnabled = (adblockState != 0);
    interceptor->setEnabled(adblockEnabled);
    if (!adblockEnabled)
        QWebEngineProfile::defaultProfile()->setUrlRequestInterceptor(nullptr);
    devToolsPage = nullptr;
    devToolsView = nullptr;
    devToolsVisible = false;
    applyProxy(proxyType, proxyHost, proxyPort, proxyUser, proxyPass);
    applyPrivacy(clearCookiesOnClose, blockTrackers);

    // Guardar el User-Agent Chrome normal para el switch con Google
    savedUserAgent = QWebEngineProfile::defaultProfile()->httpUserAgent();
    
    // Inicializar componentes de autocompletado
    completerModel = nullptr;
    urlCompleter = nullptr;
    networkManager = nullptr;
    suggestionTimer = nullptr;

    QTimer *cursorTimer = new QTimer(this);
    cursorTimer->setInterval(100);
    connect(cursorTimer, &QTimer::timeout, this, [this]() {
        QPoint global = QCursor::pos();
        QPoint local = mapFromGlobal(global);
        QWidget *under = QApplication::widgetAt(global);
        if (!under || under->window() != this) return;
        for (QWidget *w = under; w; w = w->parentWidget()) {
            if (qobject_cast<QDialog*>(w) || qobject_cast<QMenu*>(w)) return;
        }
        if (isMaximized()) {
            if (cursor().shape() != Qt::ArrowCursor) setCursor(Qt::ArrowCursor);
            return;
        }
        int w = width(), h = height();
        const int m = 12;
        const int topM = 3;
        int e = 0;
        if (local.x() <= m) e |= 1;
        if (local.x() >= w - m) e |= 2;
        if (local.y() <= topM) {
            bool overBtn = false;
            for (QWidget *w = QApplication::widgetAt(global); w; w = w->parentWidget()) {
                if (qobject_cast<QPushButton*>(w)) { overBtn = true; break; }
            }
            if (!overBtn) e |= 4;
        }
        if (local.y() >= h - m) e |= 8;
        if (e) {
            if ((e & 1) && (e & 8)) setCursor(Qt::SizeBDiagCursor);
            else if ((e & 2) && (e & 8)) setCursor(Qt::SizeFDiagCursor);
            else if ((e & 1) && (e & 4)) setCursor(Qt::SizeBDiagCursor);
            else if ((e & 2) && (e & 4)) setCursor(Qt::SizeFDiagCursor);
            else if (e & 1) setCursor(Qt::SizeHorCursor);
            else if (e & 2) setCursor(Qt::SizeHorCursor);
            else if (e & 4 || e & 8) setCursor(Qt::SizeVerCursor);
        } else if (cursor().shape() != Qt::ArrowCursor && !(QApplication::mouseButtons() & Qt::LeftButton)) {
            setCursor(Qt::ArrowCursor);
        }
    });
    cursorTimer->start();

    toolBar = addToolBar("Navigation");
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(32, 32));
    toolBar->setStyleSheet("QToolBar { spacing: 0px; padding: 0px; border: none; background: transparent; }");
    toolBar->setContentsMargins(0, 0, 0, 0);

    QWidget *tbContainer = new QWidget();
    QHBoxLayout *tbLayout = new QHBoxLayout(tbContainer);
    tbLayout->setContentsMargins(0, 10, 10, 0);
    tbLayout->setSpacing(0);

    backBtn = new QPushButton();
    backBtn->setIcon(QIcon(":/icons/icons/back.png"));
    backBtn->setIconSize(QSize(20, 20));
    backBtn->setToolTip("Atrás");
    
    forwardBtn = new QPushButton();
    forwardBtn->setIcon(QIcon(":/icons/icons/forward.png"));
    forwardBtn->setIconSize(QSize(20, 20));
    forwardBtn->setToolTip("Adelante");
    
    reloadBtn = new QPushButton();
    reloadBtn->setIcon(QIcon(":/icons/icons/reload.png"));
    reloadBtn->setIconSize(QSize(20, 20));
    reloadBtn->setToolTip("Recargar");
    
    homeBtn = new QPushButton();
    homeBtn->setIcon(QIcon(":/icons/icons/home.png"));
    homeBtn->setIconSize(QSize(20, 20));
    homeBtn->setToolTip("Página de Inicio");
    

    
    bookmarksBtn = new QPushButton();
    bookmarksBtn->setIcon(QIcon(":/icons/icons/star.png"));
    bookmarksBtn->setIconSize(QSize(20, 20));
    bookmarksBtn->setToolTip("Marcadores");
    
    translatorBtn = new QPushButton();
    translatorBtn->setIcon(QIcon(":/icons/icons/traductor.png"));
    translatorBtn->setIconSize(QSize(20, 20));
    translatorBtn->setToolTip("Traductor");

    currentZoom = 100;

    adblockBtn = new QPushButton();
    adblockBtn->setIcon(QIcon(":/icons/icons/seguridad.png"));
    adblockBtn->setIconSize(QSize(20, 20));
    adblockBtn->setToolTip("AdBlocker: Activado");
    adblockBtn->setCheckable(true);
    adblockBtn->setChecked(adblockEnabled);
    adblockBtn->setToolTip(adblockEnabled ? "AdBlocker: Activado" : "AdBlocker: Desactivado");
    
    downloadsBtn = new QPushButton();
    downloadsBtn->setIcon(QIcon(":/icons/icons/download.png"));
    downloadsBtn->setIconSize(QSize(20, 20));
    downloadsBtn->setFixedSize(40, 38);
    downloadsBtn->setToolTip("Descargas");
    downloadsBtn->setStyleSheet(
        "QPushButton { border: 2px solid transparent; border-radius: 6px; background: transparent; }"
        "QPushButton:hover { border-color: #1E88E5; background: rgba(30,136,229,0.1); }"
    );
    
    settingsBtn = new QPushButton();
    settingsBtn->setIcon(QIcon(":/icons/icons/menu.png"));
    settingsBtn->setIconSize(QSize(20, 20));
    settingsBtn->setToolTip("Configuración");

    minimizeBtn = new QPushButton();
    minimizeBtn->setToolTip("Minimizar");
    minimizeBtn->setFixedSize(17, 17);
    minimizeBtn->setStyleSheet(
        "QPushButton { background-color: #3a4955; border: none; border-radius: 8px; }"
        "QPushButton:hover { background-color: #5a6b7a; }"
        "QPushButton:pressed { background-color: #2d3842; }");

    maximizeBtn = new QPushButton();
    maximizeBtn->setToolTip("Maximizar");
    maximizeBtn->setFixedSize(17, 17);
    maximizeBtn->setStyleSheet(
        "QPushButton { background-color: #3a4955; border: none; border-radius: 8px; }"
        "QPushButton:hover { background-color: #5a6b7a; }"
        "QPushButton:pressed { background-color: #2d3842; }");

    closeBtn = new QPushButton();
    closeBtn->setToolTip("Cerrar");
    closeBtn->setFixedSize(17, 17);
    closeBtn->setStyleSheet(
        "QPushButton { background-color: #6b2a2a; border: none; border-radius: 8px; }"
        "QPushButton:hover { background-color: #a03030; }"
        "QPushButton:pressed { background-color: #801818; }");

    urlBar = new QLineEdit();
    urlBar->setPlaceholderText("Escribe una URL...");
    
    // Configurar autocompletado para la barra de direcciones
    setupUrlCompleter();

    // Agregar widgets a la barra de herramientas
    tbLayout->addWidget(backBtn);
    tbLayout->addWidget(forwardBtn);
    tbLayout->addWidget(homeBtn);
    tbLayout->addWidget(urlBar, 1);
    tbLayout->addWidget(reloadBtn);
    tbLayout->addWidget(bookmarksBtn);
    tbLayout->addWidget(translatorBtn);
    tbLayout->addWidget(adblockBtn);
    tbLayout->addWidget(downloadsBtn);
    tbLayout->addWidget(settingsBtn);
    tbSep1 = new QFrame(); tbSep1->setFixedWidth(4); tbLayout->addWidget(tbSep1);
    tbLayout->addWidget(minimizeBtn);
    tbSep2 = new QFrame(); tbSep2->setFixedWidth(6); tbLayout->addWidget(tbSep2);
    tbLayout->addWidget(maximizeBtn);
    tbSep3 = new QFrame(); tbSep3->setFixedWidth(6); tbLayout->addWidget(tbSep3);
    tbLayout->addWidget(closeBtn);
    tbSpacerRight = new QWidget();
    tbSpacerRight->setFixedWidth(0);
    tbLayout->addWidget(tbSpacerRight);

    toolBar->addWidget(tbContainer);

    // Conectar señales
    connect(backBtn, &QPushButton::clicked, this, &BrowserWindow::goBack);
    connect(forwardBtn, &QPushButton::clicked, this, &BrowserWindow::goForward);
    connect(reloadBtn, &QPushButton::clicked, this, &BrowserWindow::reload);
    connect(homeBtn, &QPushButton::clicked, this, &BrowserWindow::goHome);
    connect(bookmarksBtn, &QPushButton::clicked, this, &BrowserWindow::addBookmark);
    connect(translatorBtn, &QPushButton::clicked, this, &BrowserWindow::openTranslator);
    connect(adblockBtn, &QPushButton::clicked, this, &BrowserWindow::toggleAdBlocker);
    connect(downloadsBtn, &QPushButton::clicked, this, &BrowserWindow::showDownloads);
    connect(settingsBtn, &QPushButton::clicked, this, &BrowserWindow::showSettings);
    connect(minimizeBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maximizeBtn, &QPushButton::clicked, [this]() { if (isMaximized()) showNormal(); else showMaximized(); });
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(urlBar, &QLineEdit::returnPressed, this, &BrowserWindow::loadPage);
    connect(tabs, &QTabWidget::tabCloseRequested, this, &BrowserWindow::closeTab);
    
    // Conectar señales de descargas
    connect(&downloadManager, &DownloadManager::downloadProgress,
            this, &BrowserWindow::onDownloadProgress);
    connect(&downloadManager, &DownloadManager::downloadFinished,
            this, &BrowserWindow::onDownloadFinished);
    connect(&downloadManager, &DownloadManager::pdfDownloaded,
            this, &BrowserWindow::openPdf);

    // Configurar la barra de pestañas
    tabs->tabBar()->setExpanding(false);
    tabs->tabBar()->setUsesScrollButtons(false);
    tabs->tabBar()->setDrawBase(false);

    newTab();

    // Pestaña "+" al final de la barra de pestañas
    {
        QWidget *empty = new QWidget();
        tabs->addTab(empty, "+");
        int pi = tabs->count() - 1;
        tabs->tabBar()->setTabButton(pi, QTabBar::RightSide, nullptr);
    }

    connect(tabs->tabBar(), &QTabBar::tabBarClicked, this, [this](int index) {
        if (index == tabs->count() - 1) {
            static bool inClick = false;
            if (!inClick) {
                inClick = true;
                newTab();
                inClick = false;
            }
        }
    });

    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index < 0 || index == tabs->count() - 1) return;
        BrowserView *view = qobject_cast<BrowserView*>(tabs->widget(index));
        if (!view) { urlBar->clear(); return; }
        QUrl url = view->url();
        if (url.scheme() == "navia") {
            urlBar->clear();
        } else {
            urlBar->setText(url.toString());
        }
    });

    // Aplicar tema oscuro
    applyDarkTheme();

    // Restaurar sesión anterior si está habilitado
    restoreSession();

    // Cargar datos persistentes
    bookmarks.load();
    history.load();
    
    // Configurar autocompletado inicial
    setupUrlCompleter();

}

void BrowserWindow::restoreWindowGeometry()
{
    // Restaurar geometría guardada o usar default
    QSettings settings("Navia", "Navia Browser");
    QByteArray geom = settings.value("geometry").toByteArray();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
        qDebug() << "Geometría restaurada desde settings";
    } else {
        setGeometry(100, 100, 1200, 800);
        qDebug() << "Usando geometría por defecto";
    }
}

void BrowserWindow::newTab()
{
    static bool creatingTab = false;
    if (creatingTab) return;
    creatingTab = true;

    BrowserView *view = new BrowserView();
    view->setMouseTracking(true);

    // Insertar antes de la última pestaña (que es la pestaña "+")
    int newIndex = tabs->count() - 1;
    tabs->insertTab(newIndex, view, "Nueva pestaña");

    if (adblockEnabled)
        view->page()->profile()->setUrlRequestInterceptor(interceptor);

    showNewTabPage(view);

    setupViewConnections(view);

    tabs->setCurrentIndex(newIndex);

    creatingTab = false;
}

void BrowserWindow::setupViewConnections(BrowserView *view)
{
    connect(view, &QWebEngineView::urlChanged, this, &BrowserWindow::updateUrl);
    connect(view, &QWebEngineView::titleChanged, this, &BrowserWindow::onTitleChanged);
    connect(view->page(), &QWebEnginePage::linkHovered, this, &BrowserWindow::onLinkHovered);
    connect(view, &BrowserView::downloadRequested, this, &BrowserWindow::onDownloadRequested);

    connect(view, &QWebEngineView::loadStarted, this, [this]() {
        reloadBtn->setIcon(QIcon(":/icons/icons/reloadoff.png"));
    });

    connect(view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        Q_UNUSED(ok);
        reloadBtn->setIcon(QIcon(":/icons/icons/reload.png"));
    });

    connect(view, &BrowserView::newWindowRequested, this, [this](QWebEngineView *newView) {
        BrowserView *bv = qobject_cast<BrowserView*>(newView);
        if (!bv) return;

        bv->page()->profile()->setUrlRequestInterceptor(adblockEnabled ? interceptor : nullptr);

        int idx = tabs->count() - 1;
        tabs->insertTab(idx, bv, "Nueva pestaña");
        tabs->setCurrentIndex(idx);

        setupViewConnections(bv);
    });

    connect(view->page(), &QWebEnginePage::certificateError, this,
        [this](QWebEngineCertificateError error) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Error de certificado SSL");
            msgBox.setText("El certificado SSL de este sitio no es válido.");
            msgBox.setInformativeText(error.description());
            msgBox.setDetailedText("URL: " + error.url().toString() +
                                   "\nTipo: " + QString::number(error.type()) +
                                   "\n¿Continuar de todas formas?");
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::No);
            msgBox.setIcon(QMessageBox::Warning);
            if (msgBox.exec() == QMessageBox::Yes)
                error.acceptCertificate();
            else
                error.rejectCertificate();
        });
}

void BrowserWindow::showNewTabPage(BrowserView *view)
{
    static const QString cached = []() -> QString {
        QStringList candidates = {
            ":/newtab/page.html",
            QCoreApplication::applicationDirPath() + "/page/page.html",
            QCoreApplication::applicationDirPath() + "/../page/page.html",
            "page/page.html",
        };
        QString raw;
        for (const QString &p : candidates) {
            QFile f(p);
            if (f.open(QIODevice::ReadOnly)) {
                raw = f.readAll();
                break;
            }
        }
        if (raw.isEmpty())
            return "<html><body style='background:#020208;color:#fff;display:flex;align-items:center;justify-content:center;height:100vh;margin:0'><h1>Navia</h1></body></html>";

        // Collect image replacements first
        QRegularExpression re("src=\"\\./page/([^\"]+)\"");
        struct Rep { qsizetype pos, len; QString text; };
        QVector<Rep> reps;
        auto it = re.globalMatch(raw);
        while (it.hasNext()) {
            auto m = it.next();
            QString name = m.captured(1);
            QStringList imgPaths = {
                ":/newtab/page/" + name,
                QCoreApplication::applicationDirPath() + "/page/page/" + name,
                QCoreApplication::applicationDirPath() + "/../page/page/" + name,
                "page/page/" + name,
            };
            QByteArray b64;
            for (const QString &ip : imgPaths) {
                QFile imgF(ip);
                if (imgF.open(QIODevice::ReadOnly)) {
                    b64 = imgF.readAll().toBase64();
                    break;
                }
            }
            QString dataUrl = b64.isEmpty() ? "" : "data:image/png;base64," + QString::fromLatin1(b64);
            reps.append({m.capturedStart(), m.capturedLength(), "src=\"" + dataUrl + "\""});
        }
        // Apply from last to first to keep positions valid
        for (int i = reps.size() - 1; i >= 0; --i)
            raw.replace(reps[i].pos, reps[i].len, reps[i].text);
        return raw;
    }();

    view->page()->setHtml(cached, QUrl("navia://newtab"));
}

bool BrowserWindow::isValidUrl(const QString &text)
{
    QString t = text.trimmed();

    if (t.contains("://")) {
        return true;
    }

    if (t.startsWith("data:") || t.startsWith("about:") || t.startsWith("blob:") || t.startsWith("chrome:")) {
        return true;
    }

    if (t.startsWith("localhost") || t.startsWith("127.") || t.startsWith("192.168.") || t.startsWith("10.")) {
        return true;
    }

    if (t.startsWith('/') || t.startsWith("~")) {
        return true;
    }

    if (t.contains('.')) {
        QStringList parts = t.split(" ");
        if (!parts.isEmpty()) {
            QString firstPart = parts[0];
            if (firstPart.contains(".") && firstPart.length() > 2) {
                QStringList domainParts = firstPart.split(".");
                if (domainParts.size() >= 2) {
                    QString extension = domainParts.last();
                    if (extension.length() >= 1 && extension.length() <= 6 && extension.contains(QRegularExpression("^[a-zA-Z]+$"))) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void BrowserWindow::loadPage()
{

    BrowserView *view = (BrowserView*)tabs->currentWidget();
    
    if (view) {
        QString urlText = urlBar->text().trimmed();
        
        if (isValidUrl(urlText)) {
            if (urlText.startsWith('/') || urlText.startsWith("~")) {
                QFileInfo fi(urlText);
                urlText = "file://" + fi.absoluteFilePath();
            } else if (!urlText.startsWith("http://") && !urlText.startsWith("https://") && !urlText.startsWith("file://")) {
                urlText = "https://" + urlText;
            }
            view->load(QUrl(urlText));
        } else {
            QString searchUrl = "https://duckduckgo.com/?q=" + QUrl::toPercentEncoding(urlText);
            view->load(QUrl(searchUrl));
        }
    }

}

void BrowserWindow::loadUrl(const QString &url)
{
    if (!url.isEmpty())
        loadPageFromUrl(url);
}

void BrowserWindow::loadPageFromUrl(const QString &urlText)
{
    BrowserView *view = (BrowserView*)tabs->currentWidget();
    
    if (view) {
        QString processedUrl = urlText.trimmed();
        
        if (isValidUrl(processedUrl)) {
            if (processedUrl.startsWith('/') || processedUrl.startsWith("~")) {
                QFileInfo fi(processedUrl);
                processedUrl = "file://" + fi.absoluteFilePath();
            } else if (!processedUrl.startsWith("http://") && !processedUrl.startsWith("https://") && !processedUrl.startsWith("file://")) {
                processedUrl = "https://" + processedUrl;
            }

            // Google bloquea navegadores QtWebEngine — enviar UA Firefox para Google
            applyGoogleUA(isGoogleDomain(processedUrl));

            view->load(QUrl(processedUrl));
        } else {
            applyGoogleUA(false);
            QString searchUrl = "https://duckduckgo.com/?q=" + QUrl::toPercentEncoding(processedUrl);
            view->load(QUrl(searchUrl));
        }
    }
}

void BrowserWindow::loadLocalFile(const QString &path)
{
    BrowserView *view = qobject_cast<BrowserView*>(tabs->currentWidget());
    if (!view) return;

    QFileInfo fi(path);
    QString ext = fi.suffix().toLower();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QByteArray data = f.readAll();
    f.close();

    QString mime;
    if (ext == "pdf") mime = "application/pdf";
    else if (ext == "png") mime = "image/png";
    else if (ext == "jpg" || ext == "jpeg") mime = "image/jpeg";
    else if (ext == "gif") mime = "image/gif";
    else if (ext == "svg") mime = "image/svg+xml";
    else if (ext == "webp") mime = "image/webp";
    else if (ext == "bmp") mime = "image/bmp";
    else if (ext == "ico") mime = "image/x-icon";
    else if (ext == "html" || ext == "htm") mime = "text/html;charset=utf-8";
    else if (ext == "xml") mime = "application/xml";
    else if (ext == "txt" || ext == "text" || ext == "log" || ext == "md") mime = "text/plain;charset=utf-8";
    else if (ext == "json") mime = "application/json";
    else if (ext == "css") mime = "text/css";
    else if (ext == "js") mime = "application/javascript";
    else mime = "application/octet-stream";

    view->page()->setContent(data, mime.toUtf8(), QUrl::fromLocalFile(fi.absoluteFilePath()));
}

void BrowserWindow::openPdf(const QString &filePath)
{
    PdfViewerWidget *pdfWidget = new PdfViewerWidget(filePath);
    int newIndex = tabs->count() - 1;
    QString tabName = QFileInfo(filePath).fileName();
    if (tabName.length() > 30)
        tabName = tabName.left(27) + "...";
    tabs->insertTab(newIndex, pdfWidget, tabName);
    tabs->setCurrentIndex(newIndex);
}

void BrowserWindow::updateUrl(const QUrl &url)
{
    if (url.scheme() == "navia") {
        urlBar->clear();
        return;
    }
    urlBar->setText(url.toString());
    history.add(url.toString());

    // Cambiar UA según el dominio (Google bloquea QtWebEngine)
    applyGoogleUA(isGoogleDomain(url.toString()));

    QString scheme = url.scheme();
    QString borderColor, focusColor;
    if (scheme == "https") {
        borderColor = "#1a3a6b";
        focusColor  = "#2a5a9b";
    } else if (scheme == "http") {
        borderColor = "#4a9ad9";
        focusColor  = "#6abaf9";
    } else {
        borderColor = "#5a6b7a";
        focusColor  = "#6496c8";
    }
    urlBar->setStyleSheet(
        "QLineEdit {"
        "  background-color: #2d3842;"
        "  color: #dcdcdc;"
        "  border: 2px solid " + borderColor + ";"
        "  padding: 8px;"
        "  border-radius: 8px;"
        "  font-size: 14px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: " + focusColor + ";"
        "}"
    );
}

void BrowserWindow::onTitleChanged(const QString &title)
{
    int currentIndex = tabs->currentIndex();
    if (currentIndex < 0 || currentIndex >= tabs->count()) {
        return;
    }
    
    QString tabName;
    
    if (title.isEmpty()) {
        tabName = "Nueva pestaña";
    } else {
        if (title.length() > 5) {
            tabName = title.left(5) + "...";
        } else {
            tabName = title;
        }
    }
    
    tabs->setTabText(currentIndex, tabName);
}

void BrowserWindow::goBack()
{

    BrowserView *view = (BrowserView*)tabs->currentWidget();
    
    if (view) {
        view->back();
    }

}

void BrowserWindow::goForward()
{

    BrowserView *view = (BrowserView*)tabs->currentWidget();
    
    if (view) {
        view->forward();
    }

}

void BrowserWindow::reload()
{

    BrowserView *view = (BrowserView*)tabs->currentWidget();
    
    if (view) {
        view->reload();
    }

}

void BrowserWindow::goHome()
{
    BrowserView *view = qobject_cast<BrowserView*>(tabs->currentWidget());
    if (view)
        showNewTabPage(view);
}

void BrowserWindow::showBookmarks()
{

    QDialog dialog(this);
    dialog.setWindowTitle("Marcadores");
    dialog.setGeometry(200, 200, 500, 400);
    dialog.setWindowFlags(dialog.windowFlags() | Qt::FramelessWindowHint);
    dialog.setStyleSheet("QDialog { background-color: #19232d; border-radius: 8px 8px 0px 0px; }"
                         "QLabel { color: #dcdcdc; font-size: 13px; }"
                         "QPushButton { background-color: #2d3842; color: #dcdcdc; "
                         "  border: 2px solid #5a6b7a; padding: 6px 12px; border-radius: 6px; font-size: 12px; }"
                         "QPushButton:hover { background-color: #3a4955; }");

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(makeTitleBar(&dialog, "Marcadores"));

    QWidget *body = new QWidget();
    QVBoxLayout *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(12, 12, 12, 12);
    bodyLayout->setSpacing(8);

    QListWidget *bookmarksList = new QListWidget();
    
    for (const auto &bookmark : bookmarks.getList()) {
        QListWidgetItem *item = new QListWidgetItem(bookmark.title + " - " + bookmark.url);
        bookmarksList->addItem(item);
    }

    bodyLayout->addWidget(bookmarksList, 1);

    connect(bookmarksList, &QListWidget::itemDoubleClicked, this, [this, &dialog](QListWidgetItem *item) {
        QString url = item->text().section(" - ", 1, -1);
        if (!url.isEmpty()) {
            dialog.accept();
            loadPageFromUrl(url);
        }
    });

    QHBoxLayout *btnLayout = new QHBoxLayout();

    QPushButton *addBtn = new QPushButton("Agregar marcador");
    connect(addBtn, &QPushButton::clicked, this, &BrowserWindow::addBookmark);
    btnLayout->addWidget(addBtn);

    QPushButton *importBtn = new QPushButton("Importar HTML");
    connect(importBtn, &QPushButton::clicked, [this, &dialog]() {
        QString path = QFileDialog::getOpenFileName(&dialog, "Importar marcadores", QString(),
            "Archivos HTML (*.html *.htm);;Todos los archivos (*)");
        if (path.isEmpty()) return;
        if (bookmarks.importHtml(path)) {
            bookmarks.save();
            QMessageBox::information(&dialog, "Éxito", "Marcadores importados correctamente");
            dialog.accept();
            showBookmarks();
        } else {
            QMessageBox::warning(&dialog, "Error", "No se encontraron marcadores en el archivo");
        }
    });
    btnLayout->addWidget(importBtn);

    QPushButton *exportBtn = new QPushButton("Exportar HTML");
    connect(exportBtn, &QPushButton::clicked, [this, &dialog]() {
        QString path = QFileDialog::getSaveFileName(&dialog, "Exportar marcadores", "marcadores.html",
            "Archivos HTML (*.html *.htm);;Todos los archivos (*)");
        if (path.isEmpty()) return;
        if (bookmarks.exportHtml(path)) {
            QMessageBox::information(&dialog, "Éxito", "Marcadores exportados correctamente");
        } else {
            QMessageBox::warning(&dialog, "Error", "No se pudo exportar el archivo");
        }
    });
    btnLayout->addWidget(exportBtn);

    bodyLayout->addLayout(btnLayout);
    layout->addWidget(body, 1);

    dialog.exec();

}

void BrowserWindow::addBookmark()
{

    BrowserView *view = (BrowserView*)tabs->currentWidget();
    
    if (view) {
        QString url = view->url().toString();
        QString title = tabs->tabText(tabs->currentIndex());
        if (title.isEmpty() || title == "Nueva pestaña") {
            title = url;
        }
        bookmarks.add(title, url);
        bookmarks.save();

        QDialog msg(this);
        msg.setWindowTitle("Guardado");
        msg.setFixedSize(320, 180);
        msg.setWindowFlags(msg.windowFlags() | Qt::FramelessWindowHint);
        msg.setStyleSheet("QDialog { background-color: #19232d; border-radius: 8px 8px 0px 0px; }"
                          "QLabel { color: #dcdcdc; font-size: 14px; }"
                          "QPushButton { background-color: #2d3842; color: #dcdcdc; "
                          "  border: 2px solid #5a6b7a; padding: 8px 28px; border-radius: 6px; font-size: 14px; }"
                          "QPushButton:hover { background-color: #3a4955; }");
        QVBoxLayout *msgL = new QVBoxLayout(&msg);
        msgL->setContentsMargins(0, 0, 0, 0);
        msgL->setSpacing(0);
        msgL->addWidget(makeTitleBar(&msg, "Favorito"));
        QWidget *mb = new QWidget();
        QVBoxLayout *mbl = new QVBoxLayout(mb);
        mbl->setContentsMargins(20, 20, 20, 20);
        mbl->setSpacing(10);
        mbl->addWidget(new QLabel("Sitio guardado como favorito:"), 0, Qt::AlignCenter);
        mbl->addWidget(new QLabel("<b>" + title + "</b>"), 0, Qt::AlignCenter);
        QPushButton *ok = new QPushButton("Aceptar");
        connect(ok, &QPushButton::clicked, &msg, &QDialog::accept);
        mbl->addWidget(ok, 0, Qt::AlignCenter);
        msgL->addWidget(mb, 1);
        msg.exec();
    }

}

void BrowserWindow::showDownloads()
{
    QWidget *popup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setFixedSize(480, 340);
    popup->setStyleSheet(
        "QWidget#downloadsPopup { background-color: #19232d; border: 1px solid #3a4955; border-radius: 8px; }"
        "QLabel { color: #dcdcdc; font-size: 13px; }"
        "QPushButton { background-color: #2d3842; color: #dcdcdc; "
        "  border: 2px solid #5a6b7a; padding: 6px 12px; border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background-color: #3a4955; }"
    );
    popup->setObjectName("downloadsPopup");
    popup->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    layout->addWidget(new QLabel("Descargas activas:"));

    QListWidget *downloadsList = new QListWidget();
    downloadsList->setSelectionMode(QAbstractItemView::SingleSelection);
    
    auto updateList = [downloadsList, this]() {
        int selectedRow = downloadsList->currentRow();
        downloadsList->clear();
        for (const auto &download : downloadManager.getDownloads()) {
            QString progressText;
            if (download.status == "Downloading") {
                if (download.totalBytes > 0) {
                    double progress = (double)download.receivedBytes / download.totalBytes * 100.0;
                    progressText = QString(" (%1%)").arg(progress, 0, 'f', 1);
                } else {
                    progressText = " (Descargando...)";
                }
            } else {
                progressText = QString(" (%1)").arg(download.status);
            }
            
            QString displayText = download.filename + progressText;
            QListWidgetItem *item = new QListWidgetItem(displayText);
            
            if (download.status == "Completed") {
                item->setBackground(Qt::green);
            } else if (download.status == "Failed") {
                item->setBackground(Qt::red);
            } else if (download.status == "Downloading") {
                item->setBackground(Qt::yellow);
            } else if (download.status == "Paused") {
                item->setBackground(Qt::cyan);
            } else if (download.status == "Cancelled") {
                item->setBackground(Qt::gray);
            }
            
            downloadsList->addItem(item);
        }
        if (selectedRow >= 0 && selectedRow < downloadsList->count()) {
            downloadsList->setCurrentRow(selectedRow);
        }
    };

    updateList();
    
    QTimer *timer = new QTimer(popup);
    connect(timer, &QTimer::timeout, updateList);
    timer->start(500);
    
    layout->addWidget(downloadsList, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    QPushButton *pauseBtn = new QPushButton("Pausar");
    connect(pauseBtn, &QPushButton::clicked, [this, downloadsList, updateList, popup]() {
        int currentRow = downloadsList->currentRow();
        if (currentRow < 0) {
            QMessageBox::warning(this, "Selección requerida", "Por favor, selecciona una descarga para pausar.");
            return;
        }
        downloadManager.pauseDownload(currentRow);
        updateList();
    });
    buttonLayout->addWidget(pauseBtn);

    QPushButton *resumeBtn = new QPushButton("Continuar");
    connect(resumeBtn, &QPushButton::clicked, [this, downloadsList, updateList, popup]() {
        int currentRow = downloadsList->currentRow();
        if (currentRow < 0) {
            QMessageBox::warning(this, "Selección requerida", "Por favor, selecciona una descarga para continuar.");
            return;
        }
        downloadManager.resumeDownload(currentRow);
        updateList();
    });
    buttonLayout->addWidget(resumeBtn);

    QPushButton *cancelBtn = new QPushButton("Cancelar");
    connect(cancelBtn, &QPushButton::clicked, [this, downloadsList, updateList, popup]() {
        int currentRow = downloadsList->currentRow();
        if (currentRow < 0) {
            QMessageBox::warning(this, "Selección requerida", "Por favor, selecciona una descarga para cancelar.");
            return;
        }
        downloadManager.cancelDownload(currentRow);
        updateList();
    });
    buttonLayout->addWidget(cancelBtn);

    QPushButton *clearBtn = new QPushButton("Limpiar");
    connect(clearBtn, &QPushButton::clicked, [this, downloadsList, updateList]() {
        downloadManager.clearCompleted();
        updateList();
    });
    buttonLayout->addWidget(clearBtn);

    QPushButton *closeBtn = new QPushButton("Cerrar");
    connect(closeBtn, &QPushButton::clicked, popup, &QWidget::close);
    buttonLayout->addWidget(closeBtn);

    layout->addLayout(buttonLayout);

    popup->move(popupPosition(downloadsBtn, 480, 340));
    popup->show();
}

void BrowserWindow::showSettings()
{
    QWidget *popup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setFixedSize(520, 420);
    popup->setStyleSheet(
        "QWidget#settingsPopup { background-color: #19232d; border: 1px solid #3a4955; border-radius: 8px; }"
        "QLabel { color: #dcdcdc; font-size: 13px; }"
        "QPushButton { background-color: #2d3842; color: #dcdcdc; "
        "  border: 2px solid #5a6b7a; padding: 6px 12px; border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background-color: #3a4955; }"
    );
    popup->setObjectName("settingsPopup");
    popup->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *mainLayout = new QVBoxLayout(popup);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    QTabWidget *tabWidget = new QTabWidget();

    // Pestaña 1: Página de inicio
    QWidget *homePageTab = new QWidget();
    QVBoxLayout *homeLayout = new QVBoxLayout(homePageTab);
    homeLayout->addWidget(new QLabel("Página de inicio:"));
    QLineEdit *homePageEdit = new QLineEdit();
    homePageEdit->setText("https://duckduckgo.com");
    homeLayout->addWidget(homePageEdit);
    QPushButton *saveHomeBtn = new QPushButton("Guardar");
    connect(saveHomeBtn, &QPushButton::clicked, [this, homePageEdit]() {
        QMessageBox::information(this, "Éxito", "Página de inicio guardada: " + homePageEdit->text());
    });
    homeLayout->addWidget(saveHomeBtn);
    homeLayout->addStretch();
    tabWidget->addTab(homePageTab, "Página de Inicio");

    // Pestaña 2: Favoritos
    QWidget *favoritesTab = new QWidget();
    QVBoxLayout *favoritesLayout = new QVBoxLayout(favoritesTab);
    QListWidget *favoritesList = new QListWidget();
    favoritesList->setStyleSheet(
        "QListWidget { background-color: #2d3842; color: #dcdcdc;"
        "  border: 1px solid #5a6b7a; border-radius: 6px; padding: 4px; }"
        "QListWidget::item { padding: 12px 8px; border-radius: 4px; margin: 3px 0px;"
        "  border-bottom: 1px solid #3a4955; }"
        "QListWidget::item:selected { background-color: #6496c8; color: #19232d; }"
        "QListWidget::item:hover { background-color: #3a4955; }"
    );
    auto refreshFavorites = [favoritesList, this]() {
        favoritesList->clear();
        for (const auto &bookmark : bookmarks.getList()) {
            favoritesList->addItem(bookmark.title + " - " + bookmark.url);
        }
    };
    refreshFavorites();
    favoritesLayout->addWidget(new QLabel("Tus Favoritos:"));
    favoritesLayout->addWidget(favoritesList, 1);

    connect(favoritesList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        QString url = item->text().section(" - ", 1, -1);
        if (!url.isEmpty()) {
            loadPageFromUrl(url);
        }
    });

    QHBoxLayout *favBtnLayout = new QHBoxLayout();

    QPushButton *addFavBtn = new QPushButton("Agregar actual");
    connect(addFavBtn, &QPushButton::clicked, [this, refreshFavorites]() {
        BrowserView *view = (BrowserView*)tabs->currentWidget();
        if (view) {
            QString url = view->url().toString();
            QString title = tabs->tabText(tabs->currentIndex());
            if (title.isEmpty() || title == "Nueva pestaña") title = url;
            bookmarks.add(title, url);
            bookmarks.save();
            refreshFavorites();
        }
    });
    favBtnLayout->addWidget(addFavBtn);

    QPushButton *removeFavBtn = new QPushButton("Eliminar");
    connect(removeFavBtn, &QPushButton::clicked, [this, favoritesList, refreshFavorites]() {
        int row = favoritesList->currentRow();
        if (row < 0) {
            QMessageBox::warning(this, "Selección", "Selecciona un favorito para eliminar");
            return;
        }
        auto list = bookmarks.getList();
        Bookmarks temp;
        for (int i = 0; i < static_cast<int>(list.size()); ++i) {
            if (i != row) temp.add(list[i].title, list[i].url);
        }
        bookmarks = temp;
        bookmarks.save();
        refreshFavorites();
    });
    favBtnLayout->addWidget(removeFavBtn);

    QPushButton *importFavBtn = new QPushButton("Importar HTML");
    connect(importFavBtn, &QPushButton::clicked, [this, refreshFavorites]() {
        QString path = QFileDialog::getOpenFileName(this, "Importar marcadores", QString(),
            "Archivos HTML (*.html *.htm);;Todos los archivos (*)");
        if (path.isEmpty()) return;
        if (bookmarks.importHtml(path)) {
            bookmarks.save();
            QMessageBox::information(this, "Éxito", "Marcadores importados correctamente");
            refreshFavorites();
        } else {
            QMessageBox::warning(this, "Error", "No se encontraron marcadores en el archivo");
        }
    });
    favBtnLayout->addWidget(importFavBtn);

    QPushButton *exportFavBtn = new QPushButton("Exportar HTML");
    connect(exportFavBtn, &QPushButton::clicked, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Exportar marcadores", "marcadores.html",
            "Archivos HTML (*.html *.htm);;Todos los archivos (*)");
        if (path.isEmpty()) return;
        if (bookmarks.exportHtml(path)) {
            QMessageBox::information(this, "Éxito", "Marcadores exportados correctamente");
        } else {
            QMessageBox::warning(this, "Error", "No se pudo exportar el archivo");
        }
    });
    favBtnLayout->addWidget(exportFavBtn);

    favoritesLayout->addLayout(favBtnLayout);
    tabWidget->addTab(favoritesTab, "Favoritos");

    // Pestaña 3: Histórico
    QWidget *historyTab = new QWidget();
    QVBoxLayout *historyLayout = new QVBoxLayout(historyTab);
    historyLayout->addWidget(new QLabel("Histórico de navegación:"));
    QListWidget *historyList = new QListWidget();
    historyList->setStyleSheet(
        "QListWidget { background-color: #2d3842; color: #dcdcdc;"
        "  border: 1px solid #5a6b7a; border-radius: 6px; padding: 4px; }"
        "QListWidget::item { padding: 12px 8px; border-radius: 4px; margin: 3px 0px;"
        "  border-bottom: 1px solid #3a4955; }"
        "QListWidget::item:selected { background-color: #6496c8; color: #19232d; }"
        "QListWidget::item:hover { background-color: #3a4955; }"
    );
    for (const auto &url : history.getList()) {
        QListWidgetItem *item = new QListWidgetItem(url);
        historyList->addItem(item);
    }
    connect(historyList, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        loadPageFromUrl(item->text());
    });
    historyLayout->addWidget(historyList);
    QPushButton *clearHistoryBtn = new QPushButton("Limpiar Histórico");
    connect(clearHistoryBtn, &QPushButton::clicked, [this, historyList]() {
        int ret = QMessageBox::question(this, "Confirmación", "¿Estás seguro de que deseas limpiar el histórico?", QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            history.clear();
            historyList->clear();
            QMessageBox::information(this, "Éxito", "Histórico limpiado");
        }
    });
    historyLayout->addWidget(clearHistoryBtn);
    tabWidget->addTab(historyTab, "Histórico");

    // Pestaña 4: Proxy
    QWidget *proxyTab = new QWidget();
    QVBoxLayout *proxyLayout = new QVBoxLayout(proxyTab);
    proxyLayout->addWidget(new QLabel("Tipo de Proxy:"));
    QComboBox *proxyTypeCombo = new QComboBox();
    proxyTypeCombo->addItems({"Ninguno", "HTTP", "SOCKS5"});
    proxyLayout->addWidget(proxyTypeCombo);
    proxyLayout->addWidget(new QLabel("Servidor:"));
    QLineEdit *proxyHostEdit = new QLineEdit();
    proxyLayout->addWidget(proxyHostEdit);
    proxyLayout->addWidget(new QLabel("Puerto:"));
    QSpinBox *proxyPortSpin = new QSpinBox();
    proxyPortSpin->setMinimum(1);
    proxyPortSpin->setMaximum(65535);
    proxyLayout->addWidget(proxyPortSpin);
    proxyLayout->addWidget(new QLabel("Usuario:"));
    QLineEdit *proxyUserEdit = new QLineEdit();
    proxyUserEdit->setPlaceholderText("(opcional)");
    proxyLayout->addWidget(proxyUserEdit);
    proxyLayout->addWidget(new QLabel("Contraseña:"));
    QLineEdit *proxyPassEdit = new QLineEdit();
    proxyPassEdit->setPlaceholderText("(opcional)");
    proxyPassEdit->setEchoMode(QLineEdit::Password);
    proxyLayout->addWidget(proxyPassEdit);
    proxyTypeCombo->setCurrentText(proxyType);
    proxyHostEdit->setText(proxyHost);
    proxyPortSpin->setValue(proxyPort);
    proxyUserEdit->setText(proxyUser);
    proxyPassEdit->setText(proxyPass);
    QPushButton *saveProxyBtn = new QPushButton("Guardar");
    connect(saveProxyBtn, &QPushButton::clicked, [this, proxyTypeCombo, proxyHostEdit, proxyPortSpin, proxyUserEdit, proxyPassEdit]() {
        QString type = proxyTypeCombo->currentText();
        QString host = proxyHostEdit->text();
        int port = proxyPortSpin->value();
        QString user = proxyUserEdit->text();
        QString pass = proxyPassEdit->text();
        settings->setValue("proxy/type", type);
        settings->setValue("proxy/host", host);
        settings->setValue("proxy/port", port);
        settings->setValue("proxy/user", user);
        settings->setValue("proxy/pass", pass);
        proxyType = type;
        proxyHost = host;
        proxyPort = port;
        proxyUser = user;
        proxyPass = pass;
        applyProxy(type, host, port, user, pass);
        QMessageBox::information(this, "Éxito", "Configuración de proxy guardada");
    });
    proxyLayout->addWidget(saveProxyBtn);
    proxyLayout->addStretch();
    tabWidget->addTab(proxyTab, "Proxy");

    // Pestaña 5: Privacidad
    QWidget *privacyTab = new QWidget();
    QVBoxLayout *privacyLayout = new QVBoxLayout(privacyTab);
    privacyLayout->addWidget(new QLabel("Opciones de Privacidad:"));
    QCheckBox *clearCookiesCheck = new QCheckBox("Borrar cookies al cerrar");
    privacyLayout->addWidget(clearCookiesCheck);
    QCheckBox *blockTrackersCheck = new QCheckBox("Bloquear rastreadores");
    privacyLayout->addWidget(blockTrackersCheck);
    QCheckBox *restoreSessionCheck = new QCheckBox("Continuar donde quedaste al iniciar");
    privacyLayout->addWidget(restoreSessionCheck);
    clearCookiesCheck->setChecked(clearCookiesOnClose);
    blockTrackersCheck->setChecked(blockTrackers);
    restoreSessionCheck->setChecked(restoreOnStart);
    QPushButton *clearDataBtn = new QPushButton("Limpiar Datos");
    connect(clearDataBtn, &QPushButton::clicked, [this]() {
        QWebEngineProfile::defaultProfile()->cookieStore()->deleteAllCookies();
        QWebEngineProfile::defaultProfile()->clearHttpCache();
        history.clear();
        bookmarks.clear();
        downloadManager.clear();
        QMessageBox::information(this, "Éxito", "Datos limpiados");
    });
    privacyLayout->addWidget(clearDataBtn);
    QPushButton *savePrivacyBtn = new QPushButton("Guardar");
    connect(savePrivacyBtn, &QPushButton::clicked, [this, clearCookiesCheck, blockTrackersCheck, restoreSessionCheck]() {
        bool clear = clearCookiesCheck->isChecked();
        bool block = blockTrackersCheck->isChecked();
        bool restore = restoreSessionCheck->isChecked();
        settings->setValue("privacy/clearCookies", clear);
        settings->setValue("privacy/blockTrackers", block);
        settings->setValue("session/restoreOnStart", restore);
        clearCookiesOnClose = clear;
        blockTrackers = block;
        restoreOnStart = restore;
        applyPrivacy(clear, block);
        QMessageBox::information(this, "Éxito", "Configuración de privacidad guardada");
    });
    privacyLayout->addWidget(savePrivacyBtn);
    privacyLayout->addStretch();
    tabWidget->addTab(privacyTab, "Privacidad");

    // Pestaña 6: Acerca de
    QWidget *aboutTab = new QWidget();
    QVBoxLayout *aboutLayout = new QVBoxLayout(aboutTab);
    QLabel *logoLabel = new QLabel();
    QPixmap pixmap(":/icons/icons/about.png");
    if (pixmap.isNull()) {
        logoLabel->setText("Imagen about.png no encontrada");
    } else {
        logoLabel->setPixmap(pixmap.scaled(100, 100, Qt::KeepAspectRatio));
        logoLabel->setAlignment(Qt::AlignCenter);
    }
    aboutLayout->addWidget(logoLabel);
    aboutLayout->addWidget(new QLabel("Navia Browser v3.3"));
    aboutLayout->addWidget(new QLabel(""));
    aboutLayout->addWidget(new QLabel("Un navegador web moderno y ligero basado en Qt6"));
    aboutLayout->addWidget(new QLabel("Versión de Qt: " + QString(QT_VERSION_STR)));
    aboutLayout->addWidget(new QLabel(""));
    aboutLayout->addWidget(new QLabel("Desarollado por:"));
    aboutLayout->addWidget(new QLabel("• B&R Corp"));
    aboutLayout->addWidget(new QLabel("• ART Ripoll"));
    aboutLayout->addWidget(new QLabel(""));
    aboutLayout->addStretch();
    tabWidget->addTab(aboutTab, "Acerca de");

    mainLayout->addWidget(tabWidget, 1);

    QPushButton *closeBtn = new QPushButton("Cerrar");
    connect(closeBtn, &QPushButton::clicked, popup, &QWidget::close);
    mainLayout->addWidget(closeBtn);

    popup->move(popupPosition(settingsBtn, 520, 420));
    popup->show();
}

void BrowserWindow::closeTab(int index)
{

    // No cerrar la pestaña "+"
    if (index == tabs->count() - 1) return;

    if (tabs->count() <= 2) {
        QMessageBox::warning(this, "Advertencia", "No puedes cerrar la última pestaña");
        return;
    }

    if (index > 0) {
        tabs->setCurrentIndex(index - 1);
    } else {
        tabs->setCurrentIndex(index + 1);
    }

    QWidget *widget = tabs->widget(index);
    tabs->removeTab(index);
    delete widget;

}

void BrowserWindow::onLinkHovered(const QString &url)
{
    if (url.isEmpty()) {
        linkStatusBar->setVisible(false);
        return;
    }

    linkStatusBar->setText(url);

    const int padX = 22; // padding 10px por lado + bordes
    QFontMetrics fm(linkStatusBar->font());
    int contentW = fm.horizontalAdvance(url) + padX;
    int maxW = width() - 20;

    if (contentW > maxW) {
        linkStatusBar->setText(fm.elidedText(url, Qt::ElideMiddle, maxW - padX));
        contentW = maxW;
    }

    linkStatusBar->adjustSize();
    linkStatusBar->setMinimumWidth(contentW);
    linkStatusBar->setMaximumWidth(maxW);
    linkStatusBar->setFixedWidth(contentW);
    linkStatusBar->move(10, height() - linkStatusBar->height() - 10);
    linkStatusBar->setVisible(true);
    linkStatusBar->raise();
}

void BrowserWindow::zoomIn()
{
    BrowserView *view = qobject_cast<BrowserView*>(tabs->currentWidget());
    if (!view) return;
    if (currentZoom < 200) {
        currentZoom += 10;
        view->setZoomFactor(currentZoom / 100.0);
    }
}

void BrowserWindow::zoomOut()
{
    BrowserView *view = qobject_cast<BrowserView*>(tabs->currentWidget());
    if (!view) return;
    if (currentZoom > 30) {
        currentZoom -= 10;
        view->setZoomFactor(currentZoom / 100.0);
    }
}

void BrowserWindow::resetZoom()
{
    BrowserView *view = qobject_cast<BrowserView*>(tabs->currentWidget());
    if (!view) return;
    currentZoom = 100;
    view->setZoomFactor(1.0);
}

void BrowserWindow::toggleInspector()
{
    BrowserView *view = qobject_cast<BrowserView*>(tabs->currentWidget());
    if (!view) return;

    if (devToolsVisible) {
        if (devToolsView) {
            int idx = tabs->indexOf(devToolsView);
            if (idx >= 0) tabs->removeTab(idx);
        }
        devToolsVisible = false;
        return;
    }

    if (!devToolsPage) {
        devToolsPage = new QWebEnginePage(this);
        devToolsView = new QWebEngineView(this);
        devToolsView->setPage(devToolsPage);
    }
    view->page()->setDevToolsPage(devToolsPage);
    tabs->addTab(devToolsView, "Inspector");
    devToolsVisible = true;
    tabs->setCurrentWidget(devToolsView);
}

void BrowserWindow::viewSource()
{
    BrowserView *view = qobject_cast<BrowserView*>(tabs->currentWidget());
    if (!view) return;

    view->load(QUrl("view-source:" + view->url().toString()));
}

void BrowserWindow::toggleAdBlocker()
{
    QWidget *popup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setFixedSize(260, 200);
    popup->setStyleSheet(
        "QWidget#adblockPopup { background-color: #19232d; border: 1px solid #3a4955; border-radius: 8px; }"
        "QLabel { color: #dcdcdc; font-size: 14px; }"
        "QPushButton { background-color: #2d3842; color: #dcdcdc; "
        "  border: 2px solid #5a6b7a; padding: 8px 16px; border-radius: 6px; font-size: 14px; }"
        "QPushButton:hover { background-color: #3a4955; }"
    );
    popup->setObjectName("adblockPopup");

    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    QLabel *iconLabel = new QLabel();
    iconLabel->setPixmap(QIcon(":/icons/icons/seguridad.png").pixmap(48, 48));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    QString estado = adblockEnabled ? "Activado" : "Desactivado";
    QLabel *statusLabel = new QLabel(QString("Estado: <b style='color:%1'>%2</b>")
        .arg(adblockEnabled ? "#00cc44" : "#cc0000", estado));
    statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(statusLabel);

    QPushButton *toggleBtn = new QPushButton(adblockEnabled ? "Desactivar" : "Activar");
    layout->addWidget(toggleBtn);

    QPushButton *closeBtn = new QPushButton("Cerrar");
    layout->addWidget(closeBtn);

    connect(toggleBtn, &QPushButton::clicked, [this, popup]() {
        adblockEnabled = !adblockEnabled;
        settings->setValue("privacy/adblock", adblockEnabled ? 1 : 0);

        QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
        if (adblockEnabled) {
            interceptor->setEnabled(true);
            profile->setUrlRequestInterceptor(interceptor);
            adblockBtn->setChecked(true);
            adblockBtn->setToolTip("AdBlocker: Activado");
        } else {
            interceptor->setEnabled(false);
            profile->setUrlRequestInterceptor(nullptr);
            adblockBtn->setChecked(false);
            adblockBtn->setToolTip("AdBlocker: Desactivado");
        }
        popup->close();
    });

    connect(closeBtn, &QPushButton::clicked, popup, &QWidget::close);

    popup->move(popupPosition(adblockBtn, 260, 200));
    popup->show();
}

void BrowserWindow::settingsHomePage()
{
    // Esta función está integrada en showSettings()
}

void BrowserWindow::settingsFavorites()
{
    // Esta función está integrada en showSettings()
}

void BrowserWindow::settingsHistory()
{
    // Esta función está integrada en showSettings()
}

void BrowserWindow::settingsAbout()
{
    // Esta función está integrada en showSettings()
}

void BrowserWindow::settingsProxy()
{
    // Esta función está integrada en showSettings()
}

void BrowserWindow::settingsPrivacy()
{
    // Esta función está integrada en showSettings()
}

void BrowserWindow::onDownloadRequested(QWebEngineDownloadRequest *download)
{
    QString mime = download->mimeType();
    QString url = download->url().toString();

    if (url.startsWith("data:")) return;

    // PDFs are handled by QtPdf (native viewer), not WebEngine
    if (mime == "application/pdf" || url.endsWith(".pdf", Qt::CaseInsensitive)) {
        downloadManager.handlePdf(download);
        return;
    }

    auto canView = [&]() {
        return mime.startsWith("image/") ||
            mime.startsWith("text/") ||
            mime == "application/xml" ||
            mime == "image/svg+xml" ||
            url.endsWith(".xml", Qt::CaseInsensitive) ||
            url.endsWith(".txt", Qt::CaseInsensitive) ||
            url.endsWith(".svg", Qt::CaseInsensitive);
    };

    if (!canView()) {
        downloadManager.handle(download, this);
        return;
    }

    if (download->url().isLocalFile()) {
        download->cancel();
        loadLocalFile(download->url().toLocalFile());
        return;
    }

    static qint64 lastNav = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastNav < 2000) return;
    lastNav = now;
    newTab();
    loadPageFromUrl(url);
}

void BrowserWindow::onDownloadProgress(const QString &url, qint64 received, qint64 total)
{
    const auto &downloads = downloadManager.getDownloads();
    double acc = 0.0;
    int active = 0;
    for (const auto &d : downloads) {
        if (d.status == "Downloading" && d.totalBytes > 0) {
            acc += (double)d.receivedBytes / d.totalBytes;
            active++;
        }
    }
    int pct = active > 0 ? static_cast<int>(acc / active * 100) : 0;
    double stop = qBound(0.01, pct / 100.0, 1.0);

    downloadsBtn->setStyleSheet(QString(
        "QPushButton { border: 2px solid transparent; border-radius: 6px; "
        "background: qlineargradient(x1:0, y1:1, x2:0, y2:0, "
        "stop:0 #1E88E5, stop:%1 #1E88E5, stop:%1 transparent, stop:1 transparent); }"
        "QPushButton:hover { border-color: #1E88E5; background: qlineargradient(x1:0, y1:1, x2:0, y2:0, "
        "stop:0 #1E88E5, stop:%1 #1E88E5, stop:%1 rgba(30,136,229,0.1), stop:1 rgba(30,136,229,0.1)); }"
    ).arg(stop, 0, 'f', 3));
}

void BrowserWindow::onDownloadFinished(const QString &url, bool success)
{
    const auto &downloads = downloadManager.getDownloads();
    bool anyDownloading = false;
    for (const auto &d : downloads) {
        if (d.status == "Downloading") {
            anyDownloading = true;
            break;
        }
    }
    if (!anyDownloading) {
        downloadsBtn->setStyleSheet(
            "QPushButton { border: 2px solid transparent; border-radius: 6px; background: transparent; }"
            "QPushButton:hover { border-color: #1E88E5; background: rgba(30,136,229,0.1); }"
        );
    }
}

void BrowserWindow::applyDarkTheme()
{
    QPalette darkPalette;

    // Colores base para tema oscuro con tonos azules
    QColor darkBlue(25, 35, 45);        // Fondo principal muy oscuro azul
    QColor mediumBlue(45, 55, 65);      // Fondo secundario
    QColor lightBlue(70, 85, 100);      // Para botones y elementos activos
    QColor accentBlue(100, 150, 200);   // Azul acento para highlights
    QColor textColor(220, 220, 220);    // Texto claro
    QColor disabledText(150, 150, 150); // Texto deshabilitado

    // Configurar la paleta
    darkPalette.setColor(QPalette::Window, darkBlue);
    darkPalette.setColor(QPalette::WindowText, textColor);
    darkPalette.setColor(QPalette::Base, mediumBlue);
    darkPalette.setColor(QPalette::AlternateBase, darkBlue);
    darkPalette.setColor(QPalette::ToolTipBase, mediumBlue);
    darkPalette.setColor(QPalette::ToolTipText, textColor);
    darkPalette.setColor(QPalette::Text, textColor);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    darkPalette.setColor(QPalette::Button, lightBlue);
    darkPalette.setColor(QPalette::ButtonText, textColor);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    darkPalette.setColor(QPalette::BrightText, accentBlue);
    darkPalette.setColor(QPalette::Link, accentBlue);
    darkPalette.setColor(QPalette::Highlight, accentBlue);
    darkPalette.setColor(QPalette::HighlightedText, darkBlue);

    // Aplicar la paleta a la aplicación
    qApp->setPalette(darkPalette);

     // Estilos adicionales para una apariencia más plana y moderna sin divisiones visibles
     qApp->setStyleSheet(
         "QMainWindow { background-color: #19232d; border-radius: 10px; }"
          "QToolBar { background-color: transparent; border: none; spacing: 0px; padding: 0px; }"
          "QPushButton { "
          "    background-color: transparent; "
          "    color: #dcdcdc; "
          "    border: none; "
          "    padding: 4px 8px; "
          "    border-radius: 6px; "
          "    font-weight: bold; "
          "    font-size: 13px; "
          "}"
         "QPushButton:hover { "
         "    background-color: rgba(70, 90, 107, 0.3); "
         "}"
         "QPushButton:pressed { "
         "    background-color: rgba(58, 73, 85, 0.5); "
         "}"
         "QLineEdit { "
         "    background-color: #2d3842; "
         "    color: #dcdcdc; "
         "    border: 2px solid #5a6b7a; "
         "    padding: 8px; "
         "    border-radius: 8px; "
         "    font-size: 14px; "
         "}"
         "QLineEdit:focus { "
         "    border-color: #6496c8; "
         "}"
          "QTabWidget::pane { "
          "    border: 2px solid #5a6b7a; "
          "    background-color: #19232d; "
          "    border-radius: 10px; "
          "}"
          "QTabBar { "
          "    font-size: 11px; "
          "}"
          "QTabBar::tab { "
          "    background-color: #2d3842; "
          "    color: #dcdcdc; "
          "    padding: 3px 12px; "
          "    border: none; "
          "    margin-right: 0px; "
          "    font-weight: bold; "
          "    font-size: 11px; "
          "    min-height: 0px; "
          "}"
          "QTabBar::tab:selected { "
          "    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #465a6b, stop:1 #3a4955); "
          "    color: #ffffff; "
          "}"
          "QTabBar::tab:hover { "
          "    background-color: #3a4955; "
          "}"
         "QListWidget { "
         "    background-color: #2d3842; "
         "    color: #dcdcdc; "
         "    border: 1px solid #5a6b7a; "
         "    border-radius: 8px; "
         "    padding: 5px; "
         "}"
         "QListWidget::item { "
         "    padding: 8px; "
         "    border-radius: 4px; "
         "    margin: 1px; "
         "}"
         "QListWidget::item:selected { "
         "    background-color: #6496c8; "
         "    color: #19232d; "
         "}"
         "QListWidget::item:hover { "
         "    background-color: #3a4955; "
         "}"
         "QDialog { "
         "    background-color: #19232d; "
         "    border: 2px solid #5a6b7a; "
         "    border-radius: 12px; "
         "}"
         "QLabel { "
         "    color: #dcdcdc; "
         "    font-size: 14px; "
         "}"
         "QComboBox, QSpinBox { "
         "    background-color: #2d3842; "
         "    color: #dcdcdc; "
         "    border: 1px solid #5a6b7a; "
         "    border-radius: 6px; "
         "    padding: 4px; "
         "}"
          "QComboBox::drop-down, QSpinBox::up-button, QSpinBox::down-button { "
          "    border: none; "
          "    background-color: #465a6b; "
          "    border-radius: 3px; "
          "}"
          "QScrollBar:vertical { "
          "    background: rgba(0,0,0,0.08); "
          "    width: 8px; "
          "    margin: 0; "
          "    border-radius: 4px; "
          "}"
          "QScrollBar::handle:vertical { "
          "    background: rgba(100, 150, 200, 0.35); "
          "    border-radius: 4px; "
          "    min-height: 40px; "
          "    margin: 2px; "
          "}"
          "QScrollBar::handle:vertical:hover { "
          "    background: rgba(100, 150, 200, 0.6); "
          "}"
          "QScrollBar::handle:vertical:pressed { "
          "    background: rgba(100, 150, 200, 0.8); "
          "}"
          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
          "    height: 0; "
          "}"
          "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
          "    background: none; "
          "}"
          "QScrollBar:horizontal { "
          "    background: rgba(0,0,0,0.08); "
          "    height: 8px; "
          "    margin: 0; "
          "    border-radius: 4px; "
          "}"
          "QScrollBar::handle:horizontal { "
          "    background: rgba(100, 150, 200, 0.35); "
          "    border-radius: 4px; "
          "    min-width: 40px; "
          "    margin: 2px; "
          "}"
          "QScrollBar::handle:horizontal:hover { "
          "    background: rgba(100, 150, 200, 0.6); "
          "}"
          "QScrollBar::handle:horizontal:pressed { "
          "    background: rgba(100, 150, 200, 0.8); "
          "}"
          "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
          "    width: 0; "
          "}"
          "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { "
          "    background: none; "
          "}"
      );
}

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    // Guardar geometría de la ventana
    QSettings settings("Navia", "Navia Browser");
    settings.setValue("geometry", saveGeometry());
    settings.sync();  // Forzar guardar
    qDebug() << "Geometría guardada";

    if (clearCookiesOnClose) {
        QWebEngineProfile::defaultProfile()->cookieStore()->deleteAllCookies();
    }

    // Guardar pestañas abiertas para restaurar en el próximo inicio
    saveSession();

    // Guardar datos persistentes
    bookmarks.save();
    history.save();

    QMainWindow::closeEvent(event);
}

void BrowserWindow::saveSettings()
{
    settings->setValue("geometry", saveGeometry());
    settings->sync();
    qDebug() << "Settings guardados por señal";
}

void BrowserWindow::applyProxy(const QString &type, const QString &host, int port, const QString &user, const QString &pass)
{
    if (type == "HTTP") {
        QNetworkProxy proxy(QNetworkProxy::HttpProxy, host, port);
        if (!user.isEmpty()) proxy.setUser(user);
        if (!pass.isEmpty()) proxy.setPassword(pass);
        QNetworkProxy::setApplicationProxy(proxy);
    } else if (type == "SOCKS5") {
        QNetworkProxy proxy(QNetworkProxy::Socks5Proxy, host, port);
        if (!user.isEmpty()) proxy.setUser(user);
        if (!pass.isEmpty()) proxy.setPassword(pass);
        QNetworkProxy::setApplicationProxy(proxy);
    } else {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    }
}

void BrowserWindow::applyPrivacy(bool clearCookies, bool block)
{
    clearCookiesOnClose = clearCookies;
    blockTrackers = block;
    // Para blockTrackers, se aplicará en newTab
}

void BrowserWindow::setupUrlCompleter()
{
    // Crear modelo vacío inicialmente - solo mostraremos sugerencias de DuckDuckGo
    completerModel = new QStringListModel(QStringList(), this);
    urlCompleter = new QCompleter(completerModel, this);
    urlCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    urlCompleter->setFilterMode(Qt::MatchContains);
    urlCompleter->setCompletionMode(QCompleter::PopupCompletion);
    urlBar->setCompleter(urlCompleter);
    
    // Estilizar el popup para que sea más visible y menos comprimido
    urlCompleter->popup()->setStyleSheet(
        "QListView {"
        "    background-color: #2d3842;"
        "    color: #dcdcdc;"
        "    border: 1px solid #5a6b7a;"
        "    border-radius: 6px;"
        "    selection-background-color: #6496c8;"
        "    selection-color: #19232d;"
        "    font-size: 16px;"
        "    padding: 8px;"
        "    outline: 0px;"
        "}"
        "QListView::item {"
        "    min-height: 32px;"
        "    padding: 6px 12px;"
        "    border-radius: 3px;"
        "    margin: 2px;"
        "}"
        "QListView::item:selected {"
        "    background-color: #6496c8;"
        "    color: #19232d;"
        "}"
        "QListView::item:hover {"
        "    background-color: #3a4955;"
        "}"
    );
    
    // Conectar la señal de activación para cargar la sugerencia seleccionada
    connect(urlCompleter, static_cast<void (QCompleter::*)(const QString&)>(&QCompleter::activated),
            this, &BrowserWindow::onUrlCompleterActivated);
    
    // Conectar señal de texto cambiado para sugerencias de DuckDuckGo
    connect(urlBar, &QLineEdit::textChanged, this, &BrowserWindow::onUrlBarTextChanged);
    
    // Configurar manager de red para sugerencias
    networkManager = new QNetworkAccessManager(this);
    suggestionTimer = new QTimer(this);
    suggestionTimer->setSingleShot(true);
    suggestionTimer->setInterval(300); // 300ms delay
    connect(suggestionTimer, &QTimer::timeout, this, &BrowserWindow::fetchDuckDuckGoSuggestions);
    connect(networkManager, &QNetworkAccessManager::finished, this, &BrowserWindow::processDuckDuckGoSuggestions);
}

void BrowserWindow::onUrlBarTextChanged(const QString &text)
{
    // Resetear el temporizador cada vez que cambie el texto
    suggestionTimer->stop();
    
    // Guardar el prefijo actual para las sugerencias
    currentSuggestionPrefix = text;
    
    // Si el texto está vacío o muy corto, no mostrar sugerencias
    if (text.length() < 2) {
        completerModel->setStringList(QStringList());
        return;
    }
    
    // Siempre obtener sugerencias de DuckDuckGo (sin importar si parece URL o no)
    // El usuario quiere solo sugerencias de DuckDuckGo
    suggestionTimer->start();
}

void BrowserWindow::fetchDuckDuckGoSuggestions()
{
    // Construir la URL para las sugerencias de DuckDuckGo
    QString encodedQuery = QUrl::toPercentEncoding(currentSuggestionPrefix);
    QString suggestionUrl = QString("https://duckduckgo.com/ac/?q=%1&format=json").arg(encodedQuery);
    
    qDebug() << "Fetching suggestions from:" << suggestionUrl;
    
    QNetworkRequest request{QUrl(suggestionUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    
    networkManager->get(request);
}

void BrowserWindow::processDuckDuckGoSuggestions(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        // En caso de error, mostrar lista vacía
        qDebug() << "Error fetching suggestions:" << reply->errorString();
        completerModel->setStringList(QStringList());
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    reply->deleteLater();
    
    qDebug() << "Received data from DuckDuckGo:" << data;
    
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isNull() || !jsonDoc.isArray()) {
        qDebug() << "Invalid JSON or not an array";
        completerModel->setStringList(QStringList());
        return;
    }
    
    QJsonArray suggestionsArray = jsonDoc.array();
    QStringList suggestions;
    
    // Añadir solo sugerencias de DuckDuckGo (sin historial ni favoritos)
    for (const QJsonValue &value : suggestionsArray) {
        if (value.isObject()) {
            QJsonObject suggestionObj = value.toObject();
            if (suggestionObj.contains("phrase") && suggestionObj["phrase"].isString()) {
                QString suggestion = suggestionObj["phrase"].toString();
                suggestions.append(suggestion);
            }
        }
    }
    
    qDebug() << "Processed suggestions:" << suggestions;
    
    // Ordenar alfabéticamente
    suggestions.sort();
    
    // Actualizar el modelo del completador con solo sugerencias de DuckDuckGo
    completerModel->setStringList(suggestions);
}

void BrowserWindow::onUrlCompleterActivated(const QString &text)
{
    // Cuando el usuario hace clic en una sugerencia, cargar esa búsqueda
    qDebug() << "Completer activated with text:" << text;
    
    // Si es una URL válida, cargarla directamente
    if (isValidUrl(text)) {
        loadPageFromUrl(text);
    } else {
        // Si no es una URL, hacer una búsqueda en DuckDuckGo
        QString searchUrl = QString("https://duckduckgo.com/?q=%1").arg(QUrl::toPercentEncoding(text));
        loadPageFromUrl(searchUrl);
    }
    
    // Actualizar la barra de direcciones con el texto seleccionado
    urlBar->setText(text);
}

void BrowserWindow::updateCompleterModel()
{
    // Actualizar solo con historial y favoritos (sin DuckDuckGo)
    QStringList suggestions;
    
    // Añadir historial
    for (const QString &url : history.getList()) {
        suggestions.append(url);
    }
    
    // Añadir favoritos
    for (const Bookmark &bookmark : bookmarks.getList()) {
        suggestions.append(bookmark.url);
        suggestions.append(bookmark.title);
    }
    
    // Eliminar duplicados y ordenar
    suggestions.removeDuplicates();
    suggestions.sort();
    
    completerModel->setStringList(suggestions);
}

void BrowserWindow::openTranslator()
{
    BrowserView *currentView = (BrowserView*)tabs->currentWidget();

    if (!currentView || currentView->url().toString().isEmpty() || currentView->url().toString() == "about:blank") {
        newTab();
        loadPageFromUrl("https://translate.google.com");
        return;
    }

    QWidget *popup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setFixedSize(400, 200);
    popup->setStyleSheet(
        "QWidget { background-color: #19232d; border: 1px solid #3a4955; border-radius: 8px; }"
        "QLabel { color: #dcdcdc; font-size: 13px; }"
        "QPushButton { background-color: #2d3842; color: #dcdcdc; "
        "  border: 2px solid #5a6b7a; padding: 6px 12px; border-radius: 6px; font-size: 12px; }"
        "QPushButton:hover { background-color: #3a4955; }"
        "QComboBox { background-color: #2d3842; color: #dcdcdc; "
        "  border: 1px solid #5a6b7a; border-radius: 4px; padding: 4px; }"
    );

    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    layout->addWidget(new QLabel("Selecciona el idioma de destino:"));

    QComboBox *languageCombo = new QComboBox();
    languageCombo->addItem("Español", "es");
    languageCombo->addItem("Inglés", "en");
    languageCombo->addItem("Francés", "fr");
    languageCombo->addItem("Alemán", "de");
    languageCombo->addItem("Italiano", "it");
    languageCombo->addItem("Portugués", "pt");
    languageCombo->addItem("Ruso", "ru");
    languageCombo->addItem("Chino (Simplificado)", "zh-CN");
    languageCombo->addItem("Japonés", "ja");
    languageCombo->addItem("Coreano", "ko");

    layout->addWidget(languageCombo);

    QPushButton *translateBtn = new QPushButton("Traducir");
    connect(translateBtn, &QPushButton::clicked, [this, popup, languageCombo, currentView]() {
        QString currentUrl = currentView->url().toString();
        QString language = languageCombo->currentData().toString();
        QString translatorUrl = "https://translate.google.com/translate?u=" + currentUrl + "&hl=" + language + "&sl=auto&tl=" + language;

        newTab();
        loadPageFromUrl(translatorUrl);
        popup->close();
    });
    layout->addWidget(translateBtn);

    QPushButton *cancelBtn = new QPushButton("Cancelar");
    connect(cancelBtn, &QPushButton::clicked, popup, &QWidget::close);
    layout->addWidget(cancelBtn);

    popup->move(popupPosition(translatorBtn, 400, 200));
    popup->show();
}

bool BrowserWindow::eventFilter(QObject *obj, QEvent *event)
{
    static bool handlingDrop = false;

    // Manejar drops de archivos en la barra de pestañas
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
        if (obj == tabs->tabBar()) {
            QDragEnterEvent *de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData()->hasUrls()) {
                de->acceptProposedAction();
                return true;
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }

    if (event->type() == QEvent::Drop) {
        if (handlingDrop) return true;
        if (obj == tabs->tabBar()) {
            QDropEvent *de = static_cast<QDropEvent*>(event);
            if (de->mimeData()->hasUrls()) {
                handlingDrop = true;
                QPoint pos = de->position().toPoint();
                int idx = tabs->tabBar()->tabAt(pos);
                if (idx >= 0) {
                    if (idx == tabs->count() - 1)
                        newTab();
                    else
                        tabs->setCurrentIndex(idx);
                    for (const QUrl &u : de->mimeData()->urls()) {
                        if (u.isLocalFile()) {
                            QString localPath = u.toLocalFile();
                            if (localPath.endsWith(".pdf", Qt::CaseInsensitive)) {
                                QString dir = "/tmp/navia-pdfs/";
                                QDir().mkpath(dir);
                                QString dest = dir + QFileInfo(localPath).fileName();
                                QFile::remove(dest);
                                if (QFile::copy(localPath, dest))
                                    openPdf(dest);
                            } else {
                                loadLocalFile(localPath);
                            }
                            break;
                        }
                    }
                }
                handlingDrop = false;
                de->acceptProposedAction();
                return true;
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }

    if (event->type() != QEvent::MouseButtonPress &&
        event->type() != QEvent::MouseMove &&
        event->type() != QEvent::MouseButtonRelease)
        return QMainWindow::eventFilter(obj, event);

    QMouseEvent *me = static_cast<QMouseEvent*>(event);
    QWidget *target = qobject_cast<QWidget*>(obj);
    if (!target) return QMainWindow::eventFilter(obj, event);

    // Ignorar eventos en diálogos y menús
    for (QWidget *w = target; w; w = w->parentWidget()) {
        if (qobject_cast<QDialog*>(w) || qobject_cast<QMenu*>(w))
            return QMainWindow::eventFilter(obj, event);
    }

    static QPoint dragPos;
    static int resizeEdge = 0;
    static bool dragging = false;

    QPoint global = me->globalPosition().toPoint();
    QPoint local = mapFromGlobal(global);
    int w = width(), h = height();
    const int m = 12;
    const int topM = 3;
    bool maxd = isMaximized();

    if (event->type() == QEvent::MouseButtonRelease) {
        dragging = false;
        resizeEdge = 0;
        return QMainWindow::eventFilter(obj, event);
    }

    if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
        int e = 0;
        if (!maxd) {
            if (local.x() <= m) e |= 1;
            if (local.x() >= w - m && !toolBar->geometry().contains(local)) e |= 2;
            if (local.y() >= h - m) e |= 8;
            if (local.y() <= topM) {
                bool overBtn = false;
                for (QWidget *w = QApplication::widgetAt(global); w; w = w->parentWidget()) {
                    if (qobject_cast<QPushButton*>(w)) { overBtn = true; break; }
                }
                if (!overBtn) e |= 4;
            }
        }
        if (e) {
            resizeEdge = e;
            dragging = true;
            return true;
        }
        // Arrastrar desde toolbar
        if (toolBar->geometry().contains(local)) {
            QWidget *under = QApplication::widgetAt(global);
            if (!qobject_cast<QPushButton*>(under) && !qobject_cast<QLineEdit*>(under)) {
                if (under == tbSpacerRight || under == tbSep1 || under == tbSep2 || under == tbSep3)
                    return QMainWindow::eventFilter(obj, event);
                if (isMaximized()) showNormal();
                dragPos = global - pos();
                dragging = true;
                return true;
            }
        }
    }

    if (event->type() == QEvent::MouseMove) {
        if (me->buttons() == Qt::NoButton && !maxd) {
            int e = 0;
            if (local.x() <= m) e |= 1;
            if (local.x() >= w - m && !toolBar->geometry().contains(local)) e |= 2;
            if (local.y() <= topM) {
                bool overBtn = false;
                for (QWidget *w = QApplication::widgetAt(global); w; w = w->parentWidget()) {
                    if (qobject_cast<QPushButton*>(w)) { overBtn = true; break; }
                }
                if (!overBtn) e |= 4;
            }
            if (local.y() >= h - m) e |= 8;
            if (e) {
                if ((e & 1) && (e & 8)) setCursor(Qt::SizeBDiagCursor);
                else if ((e & 2) && (e & 8)) setCursor(Qt::SizeFDiagCursor);
                else if ((e & 1) && (e & 4)) setCursor(Qt::SizeBDiagCursor);
                else if ((e & 2) && (e & 4)) setCursor(Qt::SizeFDiagCursor);
                else if (e & 1) setCursor(Qt::SizeHorCursor);
                else if (e & 2) setCursor(Qt::SizeHorCursor);
                else if (e & 4 || e & 8) setCursor(Qt::SizeVerCursor);
            } else if (cursor().shape() != Qt::ArrowCursor) {
                setCursor(Qt::ArrowCursor);
            }
        }

        if (me->buttons() & Qt::LeftButton && dragging) {
            if (resizeEdge) {
                QRect r = geometry();
                if (resizeEdge & 1) r.setLeft(global.x());
                if (resizeEdge & 2) r.setRight(global.x());
                if (resizeEdge & 4) r.setTop(global.y());
                if (resizeEdge & 8) r.setBottom(global.y());
                if (r.width() < minimumWidth())
                    resizeEdge & 1 ? r.setLeft(r.right() - minimumWidth()) : r.setWidth(minimumWidth());
                if (r.height() < minimumHeight())
                    resizeEdge & 4 ? r.setTop(r.bottom() - minimumHeight()) : r.setHeight(minimumHeight());
                setGeometry(r);
            } else {
                move(global - dragPos);
            }
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void BrowserWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn(); return;
        case Qt::Key_Minus:
            zoomOut(); return;
        case Qt::Key_0:
            resetZoom(); return;
        case Qt::Key_U:
            viewSource(); return;
        case Qt::Key_I:
            if (event->modifiers() & Qt::ShiftModifier) {
                toggleInspector(); return;
            }
            break;
        }
    }
    if (event->key() == Qt::Key_F12) {
        toggleInspector(); return;
    }
    QMainWindow::keyPressEvent(event);
}

bool BrowserWindow::isGoogleDomain(const QString &url)
{
    static QRegularExpression re(
        "(accounts|mail|drive|docs|sheets|slides|calendar|meet|photos|play|news|"
        "scholar|books|ads|console|cloud|fonts|mybusiness|keep|earth|translate|"
        "workspace|groups|forms|classroom|blogspot|youtube|google)\\.com"
        "|google\\.com|google\\.com\\.br|google\\.[a-z]{2,3}$|googleapis\\.com",
        QRegularExpression::CaseInsensitiveOption);
    return re.match(url).hasMatch();
}

void BrowserWindow::applyGoogleUA(bool enable)
{
    if (enable) {
        QWebEngineProfile::defaultProfile()->setHttpUserAgent(
            "Mozilla/5.0 (X11; Linux x86_64; rv:131.0) Gecko/20100101 Firefox/131.0"
        );
    } else {
        QWebEngineProfile::defaultProfile()->setHttpUserAgent(savedUserAgent);
    }
}

void BrowserWindow::saveSession()
{
    if (!restoreOnStart) return;

    QJsonArray tabsArray;
    int activeIndex = 0;
    for (int i = 0; i < tabs->count(); i++) {
        // Saltar la pestaña "+"
        if (i == tabs->count() - 1) continue;

        BrowserView *view = qobject_cast<BrowserView*>(tabs->widget(i));
        if (!view) continue;

        QUrl url = view->url();
        if (url.isEmpty() || url.scheme() == "navia") continue;

        QJsonObject tabObj;
        tabObj["url"] = url.toString();
        tabObj["title"] = tabs->tabText(i);
        tabsArray.append(tabObj);

        if (i == tabs->currentIndex()) {
            activeIndex = tabsArray.size() - 1;
        }
    }

    QJsonObject session;
    session["tabs"] = tabsArray;
    session["active"] = activeIndex;
    settings->setValue("session/tabs", QJsonDocument(session).toJson(QJsonDocument::Compact));
}

void BrowserWindow::restoreSession()
{
    if (!restoreOnStart) return;

    QByteArray data = settings->value("session/tabs").toByteArray();
    if (data.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject session = doc.object();
    QJsonArray tabsArray = session["tabs"].toArray();

    if (tabsArray.isEmpty()) return;

    // Cerrar la pestaña "Nueva pestaña" por defecto
    while (tabs->count() > 1) {
        tabs->removeTab(0);
        QWidget *w = tabs->widget(0);
        tabs->removeTab(0);
        if (w) w->deleteLater();
    }

    // Restaurar cada pestaña guardada
    for (int i = 0; i < tabsArray.size(); i++) {
        QJsonObject tabObj = tabsArray[i].toObject();
        QString url = tabObj["url"].toString();
        QString title = tabObj["title"].toString();

        BrowserView *view = new BrowserView();
        view->setMouseTracking(true);
        int newIndex = tabs->count() - 1;
        tabs->insertTab(newIndex, view, title.isEmpty() ? url : title);
        view->page()->profile()->setUrlRequestInterceptor(adblockEnabled ? interceptor : nullptr);
        setupViewConnections(view);
        applyGoogleUA(isGoogleDomain(url));
        view->load(QUrl(url));
    }

    // Restaurar pestaña activa
    int activeIdx = session["active"].toInt(0);
    if (activeIdx >= 0 && activeIdx < tabs->count() - 1) {
        tabs->setCurrentIndex(activeIdx);
    }
}