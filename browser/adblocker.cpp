#include "adblocker.h"

#include <QFile>
#include <QTextStream>
#include <QWebEngineUrlRequestInfo>
#include <QDebug>
#include <QSet>

#define TYPE_SCRIPT      1
#define TYPE_IMAGE       2
#define TYPE_STYLESHEET  4
#define TYPE_SUBFRAME    8
#define TYPE_XHR         16
#define TYPE_OBJECT      32
#define TYPE_MEDIA       64
#define TYPE_FONT        128
#define TYPE_OTHER       256
#define TYPE_MAINFRAME   512
#define TYPE_PING        1024
#define TYPE_WEBSOCKET   2048

AdBlocker::AdBlocker(const QString &easylistPath, QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
    , loaded(false)
    , enabled(false)
{
    // Carga diferida: los filtros solo se cargan cuando setEnabled(true) se
    // llama por primera vez (ver setEnabled), no en el constructor.
    this->easylistPath = easylistPath;
}

void AdBlocker::setEnabled(bool on)
{
    enabled = on;
    if (on && !loaded) {
        loadFilters(easylistPath);
        // Reglas propias para anuncios "first-party" de DuckDuckGo: la lista de
        // easylist solo los cubre con reglas cosméticas (##), que este
        // bloqueador de red no aplica todavía.
        const QStringList native = {
            "@@||duckduckgo.com/d.js^",
            "@@||links.duckduckgo.com/d.js^",
            "@@||duckduckgo.com/w.js^",
            "@@||duckduckgo.com/antipixel^",
            "||duckduckgo.com/y.js^",
        };
        for (const QString &r : native)
            addRule(r);
        buildHostIndex();
    }
}

void AdBlocker::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (!enabled || !loaded) return;

    // Nunca bloquear la navegación del frame principal (el usuario la
    // elige explícitamente; evita que reglas genéricas rompan el arranque).
    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame)
        return;

    // Nunca bloquear el challenge de Cloudflare (rompe la verificación de bots)
    QString reqHost = info.requestUrl().host();
    QString firstParty = info.firstPartyUrl().host();
    if (reqHost.endsWith("cloudflare.com") || reqHost.endsWith("cf-assets.com") ||
        firstParty.endsWith("cloudflare.com") || firstParty.endsWith("cf-assets.com")) {
        return;
    }

    QString urlString = info.requestUrl().toString();
    int typeBit = resourceTypeToBit(info.resourceType());

    // Los requisitos first-party los usa solo el propio sitio: nunca deben
    // caer por reglas "sueltas" (Basic/Regex), únicamente por anclas a
    // dominio precisas (p. ej. ||duckduckgo.com/d.js^), para no romper
    // CSS/JS/fuentes legítimos de Google, YouTube, DDG, etc.
    bool sameOrigin = false;
    {
        QString firstHost = info.firstPartyUrl().host();
        if (!firstHost.isEmpty() && !reqHost.isEmpty())
            sameOrigin = (firstHost == reqHost)
                        || reqHost.endsWith('.' + firstHost)
                        || firstHost.endsWith('.' + reqHost);
    }

    for (const auto &filter : filters) {
        if (filter.type != AdBlockFilter::Exception)
            continue;
        if (typeMatches(filter, typeBit) && matchesFilter(filter, info))
            return;
    }

    // Fast path: las reglas ancla a dominio (||dominio...) son la mayoría
    // (~63k). En lugar de barrer toda la lista, se buscan solo las reglas
    // indexadas bajo el host de la petición y sus dominios padre. Si ninguna
    // acierta, el barrido completo de abajo sigue siendo el árbitro final
    // (cubre Basic/Regex y anclas con comodines), así el comportamiento es
    // idéntico al anterior, solo que más rápido.
    {
        QString chost = reqHost;
        while (!chost.isEmpty()) {
            auto it = hostIndex.constFind(chost);
            if (it != hostIndex.constEnd()) {
                for (int idx : it.value()) {
                    const AdBlockFilter &filter = filters.at(idx);
                    if (typeMatches(filter, typeBit) && matchesFilter(filter, info)) {
                        info.block(true);
                        return;
                    }
                }
            }
            int dot = chost.indexOf('.');
            if (dot < 0) break;
            chost = chost.mid(dot + 1);
        }
    }

    for (const auto &filter : filters) {
        if (filter.type == AdBlockFilter::Exception)
            continue;
        if (matchesFilter(filter, info)) {
            if (sameOrigin && filter.type != AdBlockFilter::DomainAnchor)
                continue;
            if (typeMatches(filter, typeBit)) {
                info.block(true);
                return;
            }
        }
    }
}

void AdBlocker::loadFilters(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "AdBlocker: Cannot open easylist file:" << path;
        return;
    }

    QTextStream in(&file);
    filters.reserve(90000);
    int loadedCount = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('!')) {
            continue;
        }

        AdBlockFilter filter;
        if (parseFilterLine(line, filter)) {
            filters.append(filter);
            loadedCount++;
        }
    }

    file.close();
    loaded = true;
    qDebug() << "AdBlocker: Loaded" << loadedCount << "filters from" << path;
}

void AdBlocker::addRule(const QString &ruleText)
{
    AdBlockFilter filter;
    if (parseFilterLine(ruleText, filter))
        filters.append(filter);
}

void AdBlocker::buildHostIndex()
{
    hostIndex.clear();
    for (int i = 0; i < filters.size(); ++i) {
        const AdBlockFilter &f = filters.at(i);
        if (f.type != AdBlockFilter::DomainAnchor)
            continue;
        QString host = f.pattern;
        if (host.startsWith("||"))
            host = host.mid(2);
        int slash = host.indexOf('/');
        int sep = host.indexOf('^');
        int cut = -1;
        if (slash >= 0) cut = slash;
        else if (sep >= 0) cut = sep;
        if (cut >= 0)
            host = host.left(cut);
        // Solo se indexan dominios limpios. Las reglas con comodines
        // ("*.dominio", etc.) no matchean por esta vía, y de todos modos el
        // barrido completo posterior las cubre igual que antes.
        if (host.isEmpty() || host.contains('*') || host.contains('/'))
            continue;
        hostIndex[host].append(i);
    }
}

bool AdBlocker::parseFilterLine(QString line, AdBlockFilter &filter) const
{
    filter.type = AdBlockFilter::Basic;
    filter.thirdParty = false;
    filter.notThirdParty = false;
    filter.positiveTypes = 0;
    filter.negativeTypes = 0;

    // Reglas cosméticas (##, #@#, #?#, #$#): son de ocultamiento de elementos
    // CSS/HTML (reglas "##selector"). Un interceptor de red no puede
    // aplicarlas, así que cargarlas solo gastaba memoria/CPU en ~24.000
    // regex que jamás matcheaban una petición. Omitirlas no cambia el
    // bloqueo real de tráfico.
    if (line.contains("##") || line.contains("#@#") ||
        line.contains("#?#") || line.contains("#$#"))
        return false;

    bool isException = line.startsWith("@@");
    if (isException) {
        filter.type = AdBlockFilter::Exception;
        line = line.mid(2);
    } else if (line.startsWith("||")) {
        filter.type = AdBlockFilter::DomainAnchor;
        line = line.mid(2);
    } else if (line.startsWith('/')) {
        // Una regla regexp de easylist va envuelta en barras: "/re/" o "/re/$opts".
        // Las reglas de ruta ("/ads/targeted|", "/88/tag.min.js") son literales
        // y NO deben compilarse como regex.
        int dollar = line.indexOf('$');
        QString body = (dollar >= 0) ? line.left(dollar) : line;
        if (body.length() >= 3 && body.endsWith('/')) {
            QRegularExpression rx(body.mid(1, body.length() - 2),
                                  QRegularExpression::CaseInsensitiveOption);
            if (rx.isValid() && !rx.match(QString()).hasMatch()) {
                filter.type = AdBlockFilter::Regex;
                filter.regex = rx;
            }
        }
    }

    int dollarPos = line.indexOf('$');
    if (dollarPos >= 0) {
        QString optionsStr = line.mid(dollarPos + 1);
        QStringList options = optionsStr.split(',', Qt::SkipEmptyParts);
        QSet<QString> negativeOpts;

        for (const QString &opt : options) {
            QString o = opt.trimmed().toLower();
            if (o == "third-party") {
                filter.thirdParty = true;
            } else if (o == "~third-party") {
                filter.notThirdParty = true;
            } else if (o.startsWith("domain=")) {
                QString domainStr = o.mid(7);
                QStringList domains = domainStr.split('|', Qt::SkipEmptyParts);
                for (const QString &d : domains) {
                    if (d.startsWith('~')) {
                        filter.notDomains.append(d.mid(1));
                    } else {
                        filter.domains.append(d);
                    }
                }
            } else if (o.startsWith("~")) {
                negativeOpts.insert(o.mid(1));
            }
        }

        filter.positiveTypes = parseResourceTypes(options, negativeOpts);
        for (const QString &neg : negativeOpts) {
            if (neg == "script") filter.negativeTypes |= TYPE_SCRIPT;
            else if (neg == "image") filter.negativeTypes |= TYPE_IMAGE;
            else if (neg == "stylesheet") filter.negativeTypes |= TYPE_STYLESHEET;
            else if (neg == "subdocument") filter.negativeTypes |= TYPE_SUBFRAME;
            else if (neg == "xmlhttprequest") filter.negativeTypes |= TYPE_XHR;
            else if (neg == "object") filter.negativeTypes |= TYPE_OBJECT;
            else if (neg == "media") filter.negativeTypes |= TYPE_MEDIA;
            else if (neg == "font") filter.negativeTypes |= TYPE_FONT;
            else if (neg == "other") filter.negativeTypes |= TYPE_OTHER;
            else if (neg == "main_frame") filter.negativeTypes |= TYPE_MAINFRAME;
            else if (neg == "ping") filter.negativeTypes |= TYPE_PING;
            else if (neg == "websocket") filter.negativeTypes |= TYPE_WEBSOCKET;
        }
        line = line.left(dollarPos);
    }

    filter.pattern = line.trimmed();
    if (filter.pattern.isEmpty())
        return false;

    if (filter.type == AdBlockFilter::Regex)
        return filter.regex.isValid();

    if (filter.type == AdBlockFilter::Basic || filter.type == AdBlockFilter::Exception) {
        if (filter.pattern.startsWith("||"))
            return true; // excepción con ancla de dominio
        QRegularExpression re = buildBasicRegex(filter.pattern);
        if (!re.isValid())
            return false;
        filter.regex = re;
    }

    return true;
}

QRegularExpression AdBlocker::buildBasicRegex(const QString &input) const
{
    QString pattern = input;
    bool endAnchor = false;
    if (pattern.endsWith('|')) {
        endAnchor = true;
        pattern.chop(1);
    }

    bool pathAnchor = pattern.startsWith('/');
    if (pathAnchor)
        pattern = pattern.mid(1);

    QString core;
    for (const QChar &c : pattern) {
        if (c == '*') {
            core += QStringLiteral(".*");
        } else if (c == '^') {
            core += QStringLiteral("(?:[^a-zA-Z0-9\\-_.%]|$)");
        } else if (c.isLetterOrNumber()) {
            core += c;
        } else if (c == '.') {
            core += QStringLiteral("\\.");
        } else {
            core += QRegularExpression::escape(QString(c));
        }
    }

    QString full;
    if (pathAnchor)
        full = QStringLiteral("^\\/") + core;
    else
        full = core;
    if (endAnchor)
        full += QStringLiteral("$");

    return QRegularExpression(full, QRegularExpression::CaseInsensitiveOption);
}

bool AdBlocker::matchesFilter(const AdBlockFilter &filter, const QWebEngineUrlRequestInfo &info) const
{
    QString urlString = info.requestUrl().toString();
    QString firstParty = info.firstPartyUrl().toString();

    if (filter.thirdParty && !isThirdParty(info)) {
        return false;
    }
    if (filter.notThirdParty && isThirdParty(info)) {
        return false;
    }

    if (!filter.domains.isEmpty() || !filter.notDomains.isEmpty()) {
        QString pageHost = info.firstPartyUrl().host();
        bool domainMatch = false;

        if (!filter.domains.isEmpty()) {
            for (const QString &d : filter.domains) {
                if (pageHost == d || pageHost.endsWith('.' + d)) {
                    domainMatch = true;
                    break;
                }
            }
            if (!domainMatch) return false;
        }

        if (!filter.notDomains.isEmpty()) {
            for (const QString &d : filter.notDomains) {
                if (pageHost == d || pageHost.endsWith('.' + d)) {
                    return false;
                }
            }
        }
    }

    switch (filter.type) {
    case AdBlockFilter::Basic:
        return regexMatchesPathOrUrl(filter, urlString);

    case AdBlockFilter::DomainAnchor:
        return matchesDomainAnchor(filter.pattern, urlString);

    case AdBlockFilter::Regex:
        return filter.pattern.startsWith("||")
                ? matchesDomainAnchor(filter.pattern, urlString)
                : filter.regex.match(urlString).hasMatch();

    case AdBlockFilter::Exception:
        if (filter.pattern.startsWith("||"))
            return matchesDomainAnchor(filter.pattern, urlString);
        return regexMatchesPathOrUrl(filter, urlString);
    }

    return false;
}

bool AdBlocker::regexMatchesPathOrUrl(const AdBlockFilter &filter, const QString &urlString) const
{
    QString target = urlString;
    if (filter.pattern.startsWith('/')) {
        // Los patrones anclados a "/ruta" se comparan solo contra la ruta
        // (y query) para no matchear accidentalmente el host.
        int scheme = urlString.indexOf("://");
        if (scheme >= 0) {
            int slash = urlString.indexOf('/', scheme + 3);
            target = (slash >= 0) ? urlString.mid(slash) : QString();
        }
    }
    return filter.regex.match(target).hasMatch();
}

bool AdBlocker::matchesDomainAnchor(const QString &filterPattern, const QString &urlString) const
{
    QString domain = filterPattern;
    if (domain.startsWith("||")) {
        domain = domain.mid(2);
    }

    QString host;
    int schemeEnd = urlString.indexOf("://");
    if (schemeEnd >= 0) {
        int pathStart = urlString.indexOf('/', schemeEnd + 3);
        if (pathStart >= 0) {
            host = urlString.mid(schemeEnd + 3, pathStart - schemeEnd - 3);
        } else {
            host = urlString.mid(schemeEnd + 3);
        }
    } else {
        host = urlString;
        int slashPos = host.indexOf('/');
        if (slashPos >= 0) host = host.left(slashPos);
    }

    int slashPos = domain.indexOf('/');
    int sepPos = domain.indexOf('^');
    QString filterHost;
    QString filterPath;

    if (slashPos >= 0) {
        filterHost = domain.left(slashPos);
        filterPath = domain.mid(slashPos);
        if (filterPath.endsWith('^'))
            filterPath.chop(1);
        if (filterPath.isEmpty())
            filterPath = "/";
    } else if (sepPos >= 0) {
        filterHost = domain.left(sepPos);
        filterPath = "/";
    } else {
        filterHost = domain;
        filterPath = "/";
    }

    if (filterPath.isEmpty()) {
        filterPath = "/";
    }

    bool hostMatch = (host == filterHost) || host.endsWith('.' + filterHost);
    if (!hostMatch) return false;

    if (filterPath == "/") return true;

    QString path = urlString.mid(schemeEnd >= 0 ? urlString.indexOf('/', schemeEnd + 3) : urlString.indexOf('/'));
    return path.startsWith(filterPath, Qt::CaseInsensitive);
}

bool AdBlocker::isThirdParty(const QWebEngineUrlRequestInfo &info) const
{
    QString requestHost = info.requestUrl().host();
    QString firstPartyHost = info.firstPartyUrl().host();
    if (firstPartyHost.isEmpty()) return false;
    if (requestHost == firstPartyHost) return false;
    if (requestHost.endsWith('.' + firstPartyHost)) return false;
    if (firstPartyHost.endsWith('.' + requestHost)) return false;
    return true;
}

int AdBlocker::resourceTypeToBit(QWebEngineUrlRequestInfo::ResourceType type) const
{
    switch (type) {
    case QWebEngineUrlRequestInfo::ResourceTypeScript:    return TYPE_SCRIPT;
    case QWebEngineUrlRequestInfo::ResourceTypeImage:     return TYPE_IMAGE;
    case QWebEngineUrlRequestInfo::ResourceTypeStylesheet: return TYPE_STYLESHEET;
    case QWebEngineUrlRequestInfo::ResourceTypeSubFrame:   return TYPE_SUBFRAME;
    case QWebEngineUrlRequestInfo::ResourceTypeXhr:        return TYPE_XHR;
    case QWebEngineUrlRequestInfo::ResourceTypeObject:     return TYPE_OBJECT;
    case QWebEngineUrlRequestInfo::ResourceTypeMedia:      return TYPE_MEDIA;
    case QWebEngineUrlRequestInfo::ResourceTypeFontResource: return TYPE_FONT;
    case QWebEngineUrlRequestInfo::ResourceTypeMainFrame:  return TYPE_MAINFRAME;
    case QWebEngineUrlRequestInfo::ResourceTypePing:       return TYPE_PING;
    case QWebEngineUrlRequestInfo::ResourceTypeWebSocket:  return TYPE_WEBSOCKET;
    case QWebEngineUrlRequestInfo::ResourceTypeSubResource:
    case QWebEngineUrlRequestInfo::ResourceTypeWorker:
    case QWebEngineUrlRequestInfo::ResourceTypeSharedWorker:
    case QWebEngineUrlRequestInfo::ResourceTypePrefetch:
    case QWebEngineUrlRequestInfo::ResourceTypeFavicon:
    case QWebEngineUrlRequestInfo::ResourceTypeServiceWorker:
    case QWebEngineUrlRequestInfo::ResourceTypeCspReport:
    case QWebEngineUrlRequestInfo::ResourceTypePluginResource:
    case QWebEngineUrlRequestInfo::ResourceTypeNavigationPreloadMainFrame:
    case QWebEngineUrlRequestInfo::ResourceTypeNavigationPreloadSubFrame:
    case QWebEngineUrlRequestInfo::ResourceTypeUnknown:
    default:                                               return TYPE_OTHER;
    }
}

int AdBlocker::parseResourceTypes(const QStringList &options, QSet<QString> &negativeOpts) const
{
    int bits = 0;
    for (const QString &opt : options) {
        QString o = opt.trimmed().toLower();
        if (o == "script") bits |= TYPE_SCRIPT;
        else if (o == "image") bits |= TYPE_IMAGE;
        else if (o == "stylesheet") bits |= TYPE_STYLESHEET;
        else if (o == "subdocument") bits |= TYPE_SUBFRAME;
        else if (o == "xmlhttprequest") bits |= TYPE_XHR;
        else if (o == "object") bits |= TYPE_OBJECT;
        else if (o == "media") bits |= TYPE_MEDIA;
        else if (o == "font") bits |= TYPE_FONT;
        else if (o == "other") bits |= TYPE_OTHER;
        else if (o == "main_frame") bits |= TYPE_MAINFRAME;
        else if (o == "ping") bits |= TYPE_PING;
        else if (o == "websocket") bits |= TYPE_WEBSOCKET;
    }
    return bits;
}

bool AdBlocker::typeMatches(const AdBlockFilter &filter, int typeBit) const
{
    if (filter.positiveTypes != 0) {
        return (filter.positiveTypes & typeBit) != 0;
    }
    if (filter.negativeTypes != 0) {
        return (filter.negativeTypes & typeBit) == 0;
    }
    return true;
}
