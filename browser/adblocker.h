#ifndef ADBLOCKER_H
#define ADBLOCKER_H

#include <QWebEngineUrlRequestInterceptor>
#include <QString>
#include <QVector>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>
#include <QSet>

struct AdBlockFilter {
    enum Type {
        Basic,
        DomainAnchor,
        Regex,
        Exception,
    };

    Type type;
    QString pattern;
    QRegularExpression regex;
    bool thirdParty;
    bool notThirdParty;
    QStringList domains;
    QStringList notDomains;
    int positiveTypes;
    int negativeTypes;
};

class AdBlocker : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    explicit AdBlocker(const QString &easylistPath, QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    // Carga diferida: los ~65.000 filtros útiles (se omiten las ~24.000
    // reglas cosméticas del easylist, que un interceptor de red no puede
    // aplicar) solo se cargan en memoria cuando el adblocker se activa por
    // primera vez, no al arrancar el navegador.
    bool isEnabled() const { return enabled; }
    void setEnabled(bool on);

private:
    void loadFilters(const QString &path);
    bool parseFilterLine(QString line, AdBlockFilter &filter) const;
    void addRule(const QString &ruleText);
    // Índice host -> filtros ancla a dominio (||dominio...): evita barrer las
    // ~63.000 reglas de ese tipo en cada petición (ver buildHostIndex).
    void buildHostIndex();
    QRegularExpression buildBasicRegex(const QString &pattern) const;
    bool matchesFilter(const AdBlockFilter &filter, const QWebEngineUrlRequestInfo &info) const;
    bool regexMatchesPathOrUrl(const AdBlockFilter &filter, const QString &urlString) const;
    bool isThirdParty(const QWebEngineUrlRequestInfo &info) const;
    int resourceTypeToBit(QWebEngineUrlRequestInfo::ResourceType type) const;
    bool matchesDomainAnchor(const QString &filterPattern, const QString &urlString) const;
    int parseResourceTypes(const QStringList &options, QSet<QString> &negativeOpts) const;
    bool typeMatches(const AdBlockFilter &filter, int typeBit) const;

    QVector<AdBlockFilter> filters;
    QHash<QString, QVector<int>> hostIndex;
    QString easylistPath;
    bool loaded;
    bool enabled;
};

#endif
