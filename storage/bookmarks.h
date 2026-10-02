#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <QString>
#include <vector>

struct Bookmark
{

    QString title;
    QString url;

};

class Bookmarks
{

public:

    void add(QString title, QString url);
    const std::vector<Bookmark>& getList() const;
    void save();
    void load();
    void clear();
    bool importHtml(const QString &filePath);
    bool exportHtml(const QString &filePath) const;

private:

    std::vector<Bookmark> list;

};

#endif
