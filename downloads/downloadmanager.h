#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QWebEngineDownloadRequest>
#include <QString>
#include <vector>
#include <QObject>
#include <QWidget>

struct DownloadItem {
    QString url;
    QString filename;
    qint64 totalBytes;
    qint64 receivedBytes;
    QString status; // "Downloading", "Completed", "Failed", "Cancelled", "Paused"
    QWebEngineDownloadRequest *request;
};

class DownloadManager : public QObject
{
    Q_OBJECT

public:
    DownloadManager(QObject *parent = nullptr);

    void handle(QWebEngineDownloadRequest *download, QWidget *parent = nullptr);
    void handlePdf(QWebEngineDownloadRequest *download);
    const std::vector<DownloadItem>& getDownloads() const;
    void clearCompleted();
    void clear();
    void pauseDownload(int index);
    void resumeDownload(int index);
    void cancelDownload(int index);

signals:
    void downloadProgress(const QString &url, qint64 received, qint64 total);
    void downloadFinished(const QString &url, bool success);
    void pdfDownloaded(const QString &filePath);

private slots:
    void onDownloadProgress();
    void onTotalBytesChanged();
    void onDownloadStateChanged(QWebEngineDownloadRequest::DownloadState state);

private:
    std::vector<DownloadItem> downloads;
};

#endif