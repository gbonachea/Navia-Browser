#ifndef BROWSERVIEW_H
#define BROWSERVIEW_H

#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>

class BrowserView : public QWebEngineView
{
    Q_OBJECT

public:
    BrowserView();

signals:
    void downloadRequested(QWebEngineDownloadRequest *download);
    void newWindowRequested(QWebEngineView *view);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onDownloadRequested(QWebEngineDownloadRequest *download);

};

#endif
