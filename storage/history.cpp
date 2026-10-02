#include "history.h"
#include <QSettings>

void History::add(QString url)
{

    entries.push_back(url);

}

const std::vector<QString>& History::getList() const
{

    return entries;

}

void History::save()
{
    QSettings settings("Navia", "Navia Browser");
    settings.beginWriteArray("history");
    for (int i = 0; i < entries.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("url", entries[i]);
    }
    settings.endArray();
    settings.sync();
}

void History::load()
{
    QSettings settings("Navia", "Navia Browser");
    int size = settings.beginReadArray("history");
    entries.clear();
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString url = settings.value("url").toString();
        entries.push_back(url);
    }
    settings.endArray();
}

void History::clear()
{
    entries.clear();
}