#include "bookmarks.h"
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDateTime>

void Bookmarks::add(QString title, QString url)
{

    list.push_back({title, url});

}

const std::vector<Bookmark>& Bookmarks::getList() const
{

    return list;

}

void Bookmarks::clear()
{
    list.clear();
}

void Bookmarks::save()
{
    QSettings settings("Navia", "Navia Browser");
    settings.beginWriteArray("bookmarks");
    for (int i = 0; i < list.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("title", list[i].title);
        settings.setValue("url", list[i].url);
    }
    settings.endArray();
    settings.sync();
}

void Bookmarks::load()
{
    QSettings settings("Navia", "Navia Browser");
    int size = settings.beginReadArray("bookmarks");
    list.clear();
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Bookmark bookmark;
        bookmark.title = settings.value("title").toString();
        bookmark.url = settings.value("url").toString();
        list.push_back(bookmark);
    }
    settings.endArray();
}

bool Bookmarks::importHtml(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    QString html = in.readAll();
    file.close();

    QRegularExpression re("<A\\s+HREF=\"([^\"]+)\"[^>]*>([^<]+)</A>",
                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(html);

    int count = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString url = match.captured(1).trimmed();
        QString title = match.captured(2).trimmed();
        if (!url.isEmpty()) {
            list.push_back({title, url});
            count++;
        }
    }

    return count > 0;
}

bool Bookmarks::exportHtml(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n";
    out << "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n";
    out << "<TITLE>Marcadores</TITLE>\n";
    out << "<H1>Marcadores</H1>\n";
    out << "<DL><p>\n";

    for (const auto &bm : list) {
        QString title = bm.title.isEmpty() ? bm.url : bm.title;
        title.replace("&", "&amp;").replace("\"", "&quot;").replace("<", "&lt;").replace(">", "&gt;");
        QString url = bm.url;
        url.replace("&", "&amp;").replace("\"", "&quot;");
        out << "    <DT><A HREF=\"" << url << "\" ADD_DATE=\""
            << QDateTime::currentSecsSinceEpoch() << "\">" << title << "</A>\n";
    }

    out << "</DL><p>\n";
    file.close();
    return true;
}
