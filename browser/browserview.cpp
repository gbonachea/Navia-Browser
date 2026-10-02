#include "browserview.h"
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QFileDialog>
#include <QTimer>

BrowserView::BrowserView()
{
    connect(page()->profile(), &QWebEngineProfile::downloadRequested,
            this, &BrowserView::onDownloadRequested);

    static const QString scrollCSS =
        "::-webkit-scrollbar{width:8px !important;height:8px !important}"
        "::-webkit-scrollbar-track{background:rgba(0,0,0,0.08) !important}"
        "::-webkit-scrollbar-thumb{background:rgba(100,150,200,0.35) !important;border-radius:4px !important}"
        "::-webkit-scrollbar-thumb:hover{background:rgba(100,150,200,0.6) !important}"
        "::-webkit-scrollbar-thumb:active{background:rgba(100,150,200,0.8) !important}"
        "::-webkit-scrollbar-corner{background:transparent !important}"
        "*{scrollbar-width:thin !important;scrollbar-color:rgba(100,150,200,0.35) rgba(0,0,0,0.08) !important}";

    connect(page(), &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok) {
            page()->runJavaScript(
                "(function(){"
                "var el=document.createElement('style');"
                "el.textContent='" + scrollCSS + "';"
                "document.head.appendChild(el);"
                "})()"
            );
        }
    });

    QWebEngineScript fpScript;
    fpScript.setName("antifingerprint");
    fpScript.setWorldId(QWebEngineScript::MainWorld);
    fpScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    fpScript.setSourceCode(
        "(function(){"
        "var host=(location.hostname||'').toLowerCase();"
        "if(/cloudflare/.test(host)) return;"
        "var randomize = function(){"
        "  const orig = HTMLCanvasElement.prototype.toDataURL;"
        "  HTMLCanvasElement.prototype.toDataURL = function(type){"
        "    const r = orig.call(this, type);"
        "    const noise = Array.from({length: 64}, () => Math.floor(Math.random()*16).toString(16)).join('');"
        "    return r.slice(0, -64) + noise;"
        "  };"
        "  const getContext = HTMLCanvasElement.prototype.getContext;"
        "  HTMLCanvasElement.prototype.getContext = function(){"
        "    const ctx = getContext.apply(this, arguments);"
        "    if (ctx && arguments[0] === '2d'){"
        "      const fillText = ctx.fillText;"
        "      ctx.fillText = function(){"
        "        arguments[1] += (Math.random() - 0.5) * 0.01;"
        "        arguments[2] += (Math.random() - 0.5) * 0.01;"
        "        return fillText.apply(this, arguments);"
        "      };"
        "    }"
        "    return ctx;"
        "  };"
        "};"
        "if (document.readyState !== 'loading') randomize();"
        "else document.addEventListener('DOMContentLoaded', randomize);"
        "})()"
    );
    page()->scripts().insert(fpScript);

    QWebEngineScript uaScript;
    uaScript.setName("uaspoof");
    uaScript.setWorldId(QWebEngineScript::MainWorld);
    uaScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    uaScript.setSourceCode(
        "(function(){"
        "try{"
        "Object.defineProperty(Navigator.prototype,'userAgentData',{"
        "  configurable:true,"
        "  get:function(){"
        "    var brands=["
        "      {brand:'Chromium',version:'140'},"
        "      {brand:'Not)A;Brand',version:'24'},"
        "      {brand:'Google Chrome',version:'140'}"
        "    ];"
        "    return {"
        "      brands:brands,"
        "      mobile:false,"
        "      platform:'Linux x86_64',"
        "      getHighEntropyValues:function(){"
        "        return Promise.resolve({"
        "          architecture:'x86',bitness:'64',model:'',platformVersion:'6.11.2',"
        "          uaFullVersion:'140.0.7339.264',fullVersionList:brands"
        "        });"
        "      },"
        "      toJSON:function(){return {brands:brands,mobile:false,platform:'Linux x86_64'};}"
        "    };"
        "  }"
        "});"
        "var mk=function(name,desc,file){"
        "  var p={name:name,description:desc,filename:file,length:1};"
        "  p.item=function(){return null;};p.namedItem=function(){return null;};"
        "  return p;"
        "};"
        "var pdfFile='mhjfbmdgcfjbbpaeojofohoefgiehjai';"
        "var pl=["
        "  mk('PDF Viewer','Portable Document Format','internal-pdf-viewer'),"
        "  mk('Chrome PDF Viewer','Portable Document Format',pdfFile),"
        "  mk('Chromium PDF Viewer','Portable Document Format',pdfFile),"
        "  mk('Microsoft Edge PDF Viewer','Portable Document Format',pdfFile),"
        "  mk('WebKit built-in PDF','Portable Document Format','internal-pdf-viewer')"
        "];"
        "pl.item=function(i){return pl[i]||null;};"
        "pl.namedItem=function(n){for(var i=0;i<pl.length;i++){if(pl[i].name===n)return pl[i];}return null;};"
        "pl.refresh=function(){};"
        "Object.defineProperty(Navigator.prototype,'plugins',{configurable:true,get:function(){return pl;}});"
        "Object.defineProperty(Navigator.prototype,'mimeTypes',{configurable:true,get:function(){return [];}});"
        "try{Object.defineProperty(Navigator.prototype,'webdriver',{configurable:true,get:function(){return undefined;}});}catch(e){}"
        "try{var _c=(window.chrome&&typeof window.chrome==='object')?window.chrome:{};"
        "  if(!('loadTimes'in _c)){_c.loadTimes=function(){return {requestTime:0,startLoadTime:0,endLoadTime:0,fcp:0,wt:0,responseStarted:0,transferSize:0};};}"
        "  if(!('csi'in _c)){_c.csi=function(){return {e:0,s:0,t:0};};}"
        "  if(!('runtime'in _c)){_c.runtime={};}"
        "  if(!('app'in _c)){_c.app={isInstalled:false,installState:'not_installed',runningState:'not_running'};}"
        "  window.chrome=_c;"
        "}catch(e){}"
        "}catch(e){}"
        "})()"
    );
    page()->scripts().insert(uaScript);
}

void BrowserView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    if (!menu) return;

    for (QAction *act : menu->actions()) {
        QString t = act->text();
        if (t == "Open link in new tab") act->setText("Abrir enlace en nueva pestaña");
        else if (t == "Open link in new window") act->setText("Abrir enlace en nueva ventana");
        else if (t == "Open link in new private window") act->setText("Abrir enlace en ventana privada");
        else if (t == "Save link as...") act->setText("Guardar enlace como...");
        else if (t == "Copy link address") act->setText("Copiar dirección de enlace");
        else if (t == "Copy link") act->setText("Copiar enlace");
        else if (t == "Copy image") act->setText("Copiar imagen");
        else if (t == "Copy image address") act->setText("Copiar dirección de imagen");
        else if (t == "Save image as...") act->setText("Guardar imagen como...");
        else if (t == "Open image in new tab") act->setText("Abrir imagen en nueva pestaña");
        else if (t == "Cut") act->setText("Cortar");
        else if (t == "Copy") act->setText("Copiar");
        else if (t == "Paste") act->setText("Pegar");
        else if (t == "Paste as plain text") act->setText("Pegar como texto plano");
        else if (t == "Delete") act->setText("Eliminar");
        else if (t == "Select all") act->setText("Seleccionar todo");
        else if (t == "Undo") act->setText("Deshacer");
        else if (t == "Redo") act->setText("Rehacer");
        else if (t == "Reload") act->setText("Recargar");
        else if (t == "Save page as...") act->setText("Guardar página como...");
        else if (t == "Save page") act->setText("Guardar página");
        else if (t == "View page source") act->setText("Ver código fuente");
        else if (t == "View source") act->setText("Ver código fuente");
        else if (t == "Inspect") act->setText("Inspeccionar");
        else if (t == "Inspect element") act->setText("Inspeccionar elemento");
        else if (t == "Search the web") act->setText("Buscar en la web");
        else if (t == "Search with DuckDuckGo") act->setText("Buscar con DuckDuckGo");
        else if (t == "Search with Google") act->setText("Buscar con Google");
        else if (t == "Add to reading list") act->setText("Añadir a lista de lectura");
        else if (t == "Look up") act->setText("Buscar definición");
        else if (t == "Print...") act->setText("Imprimir...");
        else if (t == "Back") act->setText("Atrás");
        else if (t == "Forward") act->setText("Adelante");
        else if (t == "Save media as...") act->setText("Guardar multimedia como...");
        else if (t == "Copy media address") act->setText("Copiar dirección multimedia");
        else if (t == "Show controls") act->setText("Mostrar controles");
        else if (t == "Hide controls") act->setText("Ocultar controles");
        else if (t == "Loop") act->setText("Repetir");
        else if (t == "Play") act->setText("Reproducir");
        else if (t == "Pause") act->setText("Pausar");
        else if (t == "Mute") act->setText("Silenciar");
        else if (t == "Unmute") act->setText("Activar sonido");
    }

    menu->addSeparator();
    QAction *savePdf = menu->addAction("Guardar como PDF");
    connect(savePdf, &QAction::triggered, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
            QString path = QFileDialog::getSaveFileName(window(), "Guardar como PDF", QString(), "PDF (*.pdf)");
            if (!path.isEmpty())
                page()->printToPdf(path);
        });
    });

    menu->exec(event->globalPos());
    delete menu;
}

QWebEngineView *BrowserView::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type)
    BrowserView *view = new BrowserView();
    emit newWindowRequested(view);
    return view;
}

void BrowserView::onDownloadRequested(QWebEngineDownloadRequest *download)
{
    emit downloadRequested(download);
}
