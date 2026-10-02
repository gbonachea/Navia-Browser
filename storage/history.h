#ifndef HISTORY_H
#define HISTORY_H

#include <QString>
#include <vector>

class History
{

public:

    void add(QString url);
    const std::vector<QString>& getList() const;
    void save();
    void load();
    void clear();

private:

    std::vector<QString> entries;

};

#endif