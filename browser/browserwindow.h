#ifndef BROWSERWINDOW_H
#define BROWSERWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QLineEdit>
#include <QToolBar>
#include <QPushButton>
#include <QCloseEvent>
#include <QSettings>
#include <QNetworkProxy>
#include <QCompleter>
#include <QStringListModel>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QLabel>

#include "browserview.h"
#include "adblocker.h"
#include "../storage/bookmarks.h"
#include "../downloads/downloadmanager.h"
#include "../storage/history.h"
#include "../pdf/pdfviewerwidget.h"

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    BrowserWindow();
    void restoreWindowGeometry();

public slots:
    void saveSettings();
    void loadUrl(const QString &url);

private slots:

    void newTab();
    void loadPage();
    void updateUrl(const QUrl &url);
    void onTitleChanged(const QString &title);
    void goBack();
    void goForward();
    void reload();
    void goHome();
    void showBookmarks();
    void openTranslator();
    void toggleAdBlocker();
    void showDownloads();
    void showSettings();
    void addBookmark();
    void closeTab(int index);
    
    // Funciones para configuración
    void settingsHomePage();
    void settingsFavorites();
    void settingsHistory();
    void settingsProxy();
    void settingsPrivacy();
    void settingsAbout();
    
    // Funciones para descargas
    void onDownloadRequested(QWebEngineDownloadRequest *download);
    void onDownloadProgress(const QString &url, qint64 received, qint64 total);
    void onDownloadFinished(const QString &url, bool success);
    void openPdf(const QString &filePath);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void applyDarkTheme();
    void loadPageFromUrl(const QString &url);
    void applyProxy(const QString &type, const QString &host, int port, const QString &user="", const QString &pass="");
    void applyPrivacy(bool clearCookies, bool blockTrackers);
    bool isValidUrl(const QString &text);
    void updateCompleterModel();
    void onLinkHovered(const QString &url);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void toggleInspector();
    void viewSource();
    void showNewTabPage(class BrowserView *view);
    void loadLocalFile(const QString &path);
    void setupViewConnections(class BrowserView *view);
    void saveSession();
    void restoreSession();
    static bool isGoogleDomain(const QString &url);
    void applyGoogleUA(bool enable);

    QTabWidget *tabs;
    QLineEdit *urlBar;
    QPushButton *backBtn;
    QPushButton *forwardBtn;
    QPushButton *reloadBtn;
    QPushButton *homeBtn;
    QPushButton *bookmarksBtn;
    QPushButton *translatorBtn;
    QPushButton *adblockBtn;
    QPushButton *downloadsBtn;
    QPushButton *settingsBtn;
    QPushButton *minimizeBtn;
    QPushButton *maximizeBtn;
    QPushButton *closeBtn;
    QToolBar *toolBar;
    QWidget *tbSpacerRight;
    QFrame *tbSep1, *tbSep2, *tbSep3;
    QLabel *linkStatusBar;
    int currentZoom;
    QWebEnginePage *devToolsPage;
    QWebEngineView *devToolsView;
    bool devToolsVisible;
    
    Bookmarks bookmarks;
    History history;
    DownloadManager downloadManager;
    
    QSettings *settings;
    AdBlocker *interceptor;
    QString proxyType, proxyHost, proxyUser, proxyPass;
    int proxyPort;
    bool clearCookiesOnClose, blockTrackers;
    bool adblockEnabled;
    bool restoreOnStart;
    QString savedUserAgent;
    
    // Autocompletado
    QStringListModel *completerModel;
    QCompleter *urlCompleter;
    QNetworkAccessManager *networkManager;
    QTimer *suggestionTimer;
    QString currentSuggestionPrefix;

private slots:
    void setupUrlCompleter();
    void onUrlBarTextChanged(const QString &text);
    void fetchDuckDuckGoSuggestions();
    void processDuckDuckGoSuggestions(QNetworkReply *reply);
    void onUrlCompleterActivated(const QString &text);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

};

#endif