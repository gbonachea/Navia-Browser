#include "downloadmanager.h"

#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDateTime>
#include <QUrl>

DownloadManager::DownloadManager(QObject *parent) : QObject(parent)
{
}

void DownloadManager::handle(QWebEngineDownloadRequest *download, QWidget *parent)
{
    QString filename = download->downloadFileName();
    if (filename.isEmpty()) {
        filename = "download";
    }

    // Sugerir la ruta de descarga predeterminada
    QString suggestedPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + filename;

    // Mostrar diálogo para elegir dónde guardar
    QString filePath = QFileDialog::getSaveFileName(parent, "Guardar archivo como", suggestedPath, "Todos los archivos (*)");

    if (filePath.isEmpty()) {
        // Usuario canceló
        return;
    }

    // Configurar la ruta y nombre del archivo
    QFileInfo fi(filePath);
    download->setDownloadDirectory(fi.absolutePath());
    download->setDownloadFileName(fi.fileName());

    // Crear item de descarga
    DownloadItem item;
    item.url = download->url().toString();
    item.filename = fi.fileName();
    item.totalBytes = download->totalBytes();
    item.receivedBytes = download->receivedBytes();
    item.status = "Downloading";
    item.request = download;

    downloads.push_back(item);

    // Conectar señales
    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged,
            this, &DownloadManager::onDownloadProgress);
    connect(download, &QWebEngineDownloadRequest::totalBytesChanged,
            this, &DownloadManager::onTotalBytesChanged);
    connect(download, &QWebEngineDownloadRequest::stateChanged,
            this, &DownloadManager::onDownloadStateChanged);

    // Iniciar descarga
    download->accept();

    qDebug() << "Iniciando descarga:" << fi.fileName() << "en" << fi.absolutePath() << "desde" << item.url;
}

void DownloadManager::handlePdf(QWebEngineDownloadRequest *download)
{
    QString dir = "/tmp/navia-pdfs/";
    QDir().mkpath(dir);

    QString filename = download->downloadFileName();
    if (filename.isEmpty() || !filename.endsWith(".pdf", Qt::CaseInsensitive))
        filename = "document_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".pdf";

    QString filePath = dir + filename;

    if (download->url().isLocalFile()) {
        download->cancel();
        QFile::remove(filePath);
        if (QFile::copy(download->url().toLocalFile(), filePath))
            emit pdfDownloaded(filePath);
        return;
    }

    DownloadItem item;
    item.url = download->url().toString();
    item.filename = filename;
    item.totalBytes = download->totalBytes();
    item.receivedBytes = download->receivedBytes();
    item.status = "Downloading";
    item.request = download;
    downloads.push_back(item);

    download->setDownloadDirectory(dir);
    download->setDownloadFileName(filename);

    connect(download, &QWebEngineDownloadRequest::stateChanged, this,
        [this, filePath](QWebEngineDownloadRequest::DownloadState state) {
            if (state == QWebEngineDownloadRequest::DownloadCompleted)
                emit pdfDownloaded(filePath);
            onDownloadStateChanged(state);
        });

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged,
            this, &DownloadManager::onDownloadProgress);
    connect(download, &QWebEngineDownloadRequest::totalBytesChanged,
            this, &DownloadManager::onTotalBytesChanged);

    download->accept();
}

const std::vector<DownloadItem>& DownloadManager::getDownloads() const
{
    return downloads;
}

void DownloadManager::clearCompleted()
{
    downloads.erase(
        std::remove_if(downloads.begin(), downloads.end(),
            [](const DownloadItem &item) {
                return item.status == "Completed" || item.status == "Failed" || item.status == "Cancelled";
            }),
        downloads.end());
}

void DownloadManager::clear()
{
    downloads.clear();
}

void DownloadManager::onDownloadProgress()
{
    QWebEngineDownloadRequest *download = qobject_cast<QWebEngineDownloadRequest*>(sender());
    if (!download) return;

    QString url = download->url().toString();
    qint64 received = download->receivedBytes();

    // Actualizar progreso en la lista
    for (auto &item : downloads) {
        if (item.url == url) {
            item.receivedBytes = received;
            break;
        }
    }

    emit downloadProgress(url, received, download->totalBytes());
}

void DownloadManager::onTotalBytesChanged()
{
    QWebEngineDownloadRequest *download = qobject_cast<QWebEngineDownloadRequest*>(sender());
    if (!download) return;

    QString url = download->url().toString();
    qint64 total = download->totalBytes();

    // Actualizar total en la lista
    for (auto &item : downloads) {
        if (item.url == url) {
            item.totalBytes = total;
            break;
        }
    }
}

void DownloadManager::onDownloadStateChanged(QWebEngineDownloadRequest::DownloadState state)
{
    QWebEngineDownloadRequest *download = qobject_cast<QWebEngineDownloadRequest*>(sender());
    if (!download) return;

    QString url = download->url().toString();
    bool success = (state == QWebEngineDownloadRequest::DownloadCompleted);

    // Actualizar estado en la lista
    for (auto &item : downloads) {
        if (item.url == url) {
            if (state == QWebEngineDownloadRequest::DownloadCompleted) {
                item.status = "Completed";
            } else if (state == QWebEngineDownloadRequest::DownloadInterrupted) {
                item.status = "Failed";
            } else if (state == QWebEngineDownloadRequest::DownloadCancelled) {
                item.status = "Cancelled";
            }
            break;
        }
    }

    if (state == QWebEngineDownloadRequest::DownloadCompleted ||
        state == QWebEngineDownloadRequest::DownloadInterrupted ||
        state == QWebEngineDownloadRequest::DownloadCancelled) {
        emit downloadFinished(url, success);
    }
}

void DownloadManager::pauseDownload(int index)
{
    if (index < 0 || index >= static_cast<int>(downloads.size())) return;
    
    DownloadItem &item = downloads[index];
    if (item.status == "Downloading" && item.request) {
        item.request->pause();
        item.status = "Paused";
        qDebug() << "Descarga pausada:" << item.filename;
    } else {
        qDebug() << "No se puede pausar descarga en estado:" << item.status;
    }
}

void DownloadManager::resumeDownload(int index)
{
    if (index < 0 || index >= static_cast<int>(downloads.size())) return;
    
    DownloadItem &item = downloads[index];
    if (item.status == "Paused" && item.request) {
        item.request->resume();
        item.status = "Downloading";
        qDebug() << "Descarga reanudada:" << item.filename;
    } else {
        qDebug() << "No se puede reanudar descarga en estado:" << item.status;
    }
}

void DownloadManager::cancelDownload(int index)
{
    if (index < 0 || index >= static_cast<int>(downloads.size())) return;
    
    DownloadItem &item = downloads[index];
    if ((item.status == "Downloading" || item.status == "Paused") && item.request) {
        item.request->cancel();
        item.status = "Cancelled";
        qDebug() << "Descarga cancelada:" << item.filename;
    } else {
        qDebug() << "No se puede cancelar descarga en estado:" << item.status;
    }
}