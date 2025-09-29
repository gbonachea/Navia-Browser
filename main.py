import os
import gi
gi.require_version("Gtk", "3.0")
gi.require_version("WebKit2", "4.1")
from gi.repository import Gtk, WebKit2, Gio, Gdk, GLib, GdkPixbuf


class DownloadWindow(Gtk.Window):
    def __init__(self, browser):
        super().__init__(title="Descargas")
        self.set_default_size(400, 300)
        self.browser = browser
        self.listbox = Gtk.ListBox()
        self.add(self.listbox)
        self.set_border_width(10)
        self._timer = None
        self.refresh()
        self.show_all()
        self.start_auto_refresh()

    def refresh(self):
        self.listbox.foreach(lambda row: self.listbox.remove(row))
        if not self.browser.active_downloads:
            self.listbox.add(Gtk.Label(label="No hay descargas activas."))
        else:
            for download in self.browser.active_downloads:
                # Descarga manual: dict con claves 'manual', 'filename', 'progress', 'cancel', 'thread'
                if isinstance(download, dict) and download.get('manual'):
                    fname = download.get('filename', 'descarga')
                    row = Gtk.ListBoxRow()
                    hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
                    lbl = Gtk.Label(label=fname, xalign=0)
                    hbox.pack_start(lbl, True, True, 0)
                    progress = Gtk.ProgressBar()
                    prog = download.get('progress', 0)
                    progress.set_fraction(prog)
                    progress.set_text(f"{int(prog*100)}%" if prog > 0 else "Descargando...")
                    hbox.pack_start(progress, False, False, 0)
                    btn_cancel = Gtk.Button(label="Cancelar")
                    def cancelar_manual(btn, d=download):
                        d['cancel'][0] = True
                    btn_cancel.connect("clicked", cancelar_manual)
                    hbox.pack_start(btn_cancel, False, False, 0)
                    row.add(hbox)
                    self.listbox.add(row)
                else:
                    fname = download.get_suggested_filename() or "descarga"
                    row = Gtk.ListBoxRow()
                    hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
                    lbl = Gtk.Label(label=fname, xalign=0)
                    hbox.pack_start(lbl, True, True, 0)
                    progress = Gtk.ProgressBar()
                    prog = 0
                    show_text = "Descargando..."
                    if hasattr(download, 'get_estimated_progress'):
                        try:
                            prog = download.get_estimated_progress()
                            progress.set_fraction(prog)
                            show_text = f"{int(prog*100)}%"
                        except Exception:
                            progress.set_fraction(0)
                    else:
                        progress.set_fraction(0)
                    progress.set_text(show_text)
                    hbox.pack_start(progress, False, False, 0)
                    btn_pause = Gtk.Button(label="Pausar")
                    btn_pause.connect("clicked", lambda _, d=download: d.pause() if hasattr(d, 'pause') and callable(getattr(d, 'pause', None)) else None)
                    hbox.pack_start(btn_pause, False, False, 0)
                    btn_resume = Gtk.Button(label="Reanudar")
                    btn_resume.connect("clicked", lambda _, d=download: d.resume() if hasattr(d, 'resume') and callable(getattr(d, 'resume', None)) else None)
                    hbox.pack_start(btn_resume, False, False, 0)
                    btn_cancel = Gtk.Button(label="Cancelar")
                    btn_cancel.connect("clicked", lambda _, d=download: d.cancel())
                    hbox.pack_start(btn_cancel, False, False, 0)
                    row.add(hbox)
                    self.listbox.add(row)
        self.listbox.show_all()

    def update_progress(self):
        self.refresh()

    def start_auto_refresh(self):
        if self._timer:
            GLib.source_remove(self._timer)
        self._timer = GLib.timeout_add_seconds(1, self._auto_refresh)

    def _auto_refresh(self):
        self.refresh()
        # Si no hay descargas activas, detener el timer
        if not self.browser.active_downloads:
            self._timer = None
            return False
        return True

class BrowserTab(Gtk.Box):
    def __init__(self, browser, url="https://duckduckgo.com"):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.browser = browser

        # Configurar proxy si está definido
        self.webview = WebKit2.WebView()
        # Desactivar el anuncio de reproducción de medios en el sistema
        settings = self.webview.get_settings()
        if hasattr(settings, 'set_enable_media_stream'):  # WebKit2 >= 2.24
            settings.set_enable_media_stream(False)
        if hasattr(settings, 'set_enable_mediasource'):  # WebKit2 >= 2.24
            settings.set_enable_mediasource(False)
        if hasattr(settings, 'set_autoplay'):  # WebKit2 >= 2.28
            settings.set_autoplay(False)
        proxy_uri = browser.data.get("proxy", "").strip()
        if proxy_uri:
            context = self.webview.get_context()
            settings = context.get_settings()
            # Configuración de proxy para WebKit2GTK
            # Solo funciona si se usa GIO y el entorno soporta la variable
            os.environ["http_proxy"] = proxy_uri
            os.environ["https_proxy"] = proxy_uri
        self.webview.load_uri(url)
        self.pack_start(self.webview, True, True, 0)
        self.show_all()

        # Conectar señal de política para gestionar descargas
        self.webview.connect("decide-policy", self.on_decide_policy)

    def on_decide_policy(self, webview, decision, decision_type):
        print(f"[DEBUG] decide-policy llamada. decision_type={decision_type}, is_download={getattr(decision, 'is_download', None)}")
        # Compatibilidad: detectar descargas usando is_download()
        if hasattr(decision, 'is_download') and decision.is_download():
            print(f"[DEBUG] ¡Descarga detectada! decision={decision}")
            download = decision.download()
            download.connect("decide-destination", self.on_decide_destination)
            self.browser.active_downloads.append(download)
            def remove_download(d, *args):
                if d in self.browser.active_downloads:
                    self.browser.active_downloads.remove(d)
                if hasattr(self.browser, 'download_window') and self.browser.download_window:
                    self.browser.download_window.update_progress()
            download.connect("finished", remove_download)
            download.connect("failed", remove_download)
            download.connect("progress-changed", lambda d, *_: self.browser.download_window.update_progress() if hasattr(self.browser, 'download_window') and self.browser.download_window else None)
            if hasattr(self.browser, 'download_window') and self.browser.download_window:
                self.browser.download_window.update_progress()
            return True
        # Forzar descarga si el tipo de decisión es RESPONSE y el MIME es de archivo
        # Esto cubre casos donde is_download es No pero el servidor envía un archivo
        if str(decision_type) == "<enum WEBKIT_POLICY_DECISION_TYPE_RESPONSE of type WebKit2.PolicyDecisionType>":
            # Intentar obtener el tipo MIME
            mime_type = None
            if hasattr(decision, 'get_response'):
                response = decision.get_response()
                if response and hasattr(response, 'get_mime_type'):
                    mime_type = response.get_mime_type()
            print(f"[DEBUG] Forzado: decision_type=RESPONSE, mime_type={mime_type}")
            # Lista de tipos MIME típicos de archivos descargables
            # Detectar cualquier archivo application/* excepto html, json, javascript
            if mime_type and mime_type.startswith("application/") and mime_type not in ["application/json", "application/javascript", "application/xml"]:
                print(f"[DEBUG] ¡Descarga forzada detectada! MIME={mime_type}")
                if hasattr(decision, 'download'):
                    download = decision.download()
                    if download is not None:
                        download.connect("decide-destination", self.on_decide_destination)
                        self.browser.active_downloads.append(download)
                        def remove_download(d, *args):
                            if d in self.browser.active_downloads:
                                self.browser.active_downloads.remove(d)
                            if hasattr(self.browser, 'download_window') and self.browser.download_window:
                                self.browser.download_window.update_progress()
                        download.connect("finished", remove_download)
                        download.connect("failed", remove_download)
                        download.connect("progress-changed", lambda d, *_: self.browser.download_window.update_progress() if hasattr(self.browser, 'download_window') and self.browser.download_window else None)
                        if hasattr(self.browser, 'download_window') and self.browser.download_window:
                            self.browser.download_window.update_progress()
                        return True
                    else:
                        print(f"[DEBUG] No se pudo crear el objeto de descarga para MIME={mime_type}")
                        # Intentar descarga manual
                        # Obtener la URL del archivo
                        url = None
                        if hasattr(decision, 'get_response'):
                            response = decision.get_response()
                            if response and hasattr(response, 'get_uri'):
                                url = response.get_uri()
                        if not url and hasattr(decision, 'get_request'):
                            req = decision.get_request()
                            if req and hasattr(req, 'get_uri'):
                                url = req.get_uri()
                        print(f"[DEBUG] Intentando descarga manual para URL={url}")
                        if url:
                            # Preguntar ubicación al usuario
                            browser = self.browser
                            dialog = Gtk.FileChooserDialog(title="Guardar archivo como...", parent=browser, action=Gtk.FileChooserAction.SAVE)
                            dialog.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_SAVE, Gtk.ResponseType.OK)
                            dialog.set_current_name(os.path.basename(url))
                            response = dialog.run()
                            if response == Gtk.ResponseType.OK:
                                dest = dialog.get_filename()
                                dialog.destroy()
                                # Crear ventana de descargas si no existe
                                if not hasattr(browser, 'download_window') or browser.download_window is None:
                                    browser.download_window = DownloadWindow(browser)
                                    browser.download_window.connect("destroy", lambda w: setattr(browser, 'download_window', None))
                                # Descargar el archivo manualmente con progreso y cancelación
                                import threading
                                cancel_flag = [False]
                                manual_download = {
                                    'manual': True,
                                    'filename': os.path.basename(dest),
                                    'progress': 0,
                                    'cancel': cancel_flag,
                                    'thread': None
                                }
                                browser.active_downloads.append(manual_download)
                                def descargar_manual():
                                    import requests
                                    try:
                                        r = requests.get(url, stream=True, timeout=30)
                                        r.raise_for_status()
                                        total = int(r.headers.get('content-length', 0))
                                        downloaded = 0
                                        with open(dest, "wb") as f:
                                            for chunk in r.iter_content(chunk_size=8192):
                                                if cancel_flag[0]:
                                                    print("Descarga manual cancelada por el usuario.")
                                                    break
                                                if chunk:
                                                    f.write(chunk)
                                                    downloaded += len(chunk)
                                                    prog = downloaded / total if total > 0 else 0
                                                    manual_download['progress'] = prog
                                                    if hasattr(browser, 'download_window') and browser.download_window:
                                                        GLib.idle_add(browser.download_window.update_progress)
                                        if not cancel_flag[0]:
                                            print(f"Descarga manual completada: {dest}")
                                            GLib.idle_add(lambda: browser.mostrar_mensaje(f"Descarga completada: {os.path.basename(dest)}"))
                                        else:
                                            GLib.idle_add(lambda: browser.mostrar_mensaje(f"Descarga cancelada: {os.path.basename(dest)}"))
                                    except Exception as exc:
                                        print(f"Error en descarga manual: {exc}")
                                        GLib.idle_add(lambda exc=exc: browser.mostrar_mensaje(f"Error al descargar: {exc}"))
                                    # Eliminar de la lista al terminar
                                    if manual_download in browser.active_downloads:
                                        browser.active_downloads.remove(manual_download)
                                        if hasattr(browser, 'download_window') and browser.download_window:
                                            GLib.idle_add(browser.download_window.update_progress)
                                t = threading.Thread(target=descargar_manual, daemon=True)
                                manual_download['thread'] = t
                                t.start()
                                if hasattr(browser, 'download_window') and browser.download_window:
                                    GLib.idle_add(browser.download_window.update_progress)
                                return True
                            else:
                                dialog.destroy()
        return False

    def on_download_started(self, webview, download):
        if download not in self.browser.active_downloads:
            self.browser.active_downloads.append(download)
        download.connect("decide-destination", self.on_decide_destination)
        download.connect("progress-changed", lambda d, *_: self.browser.download_window.update_progress() if hasattr(self.browser, 'download_window') and self.browser.download_window else None)
        if hasattr(self.browser, 'download_window') and self.browser.download_window:
            self.browser.download_window.update_progress()

    def on_decide_destination(self, download, suggested_filename):
        browser = self.browser
        mode = browser.data.get("download_mode", "ask")
        default_path = browser.data.get("download_path", os.path.expanduser("~/Descargas"))
        import urllib.parse
        suggested = urllib.parse.unquote(suggested_filename) if suggested_filename else "descarga"
        if mode == "auto":
            dest = os.path.join(default_path, suggested)
            uri = Gio.File.new_for_path(dest).get_uri()
            download.set_destination(uri)
        else:
            dialog = Gtk.FileChooserDialog(
                title="Guardar archivo como...",
                parent=browser,
                action=Gtk.FileChooserAction.SAVE,
                buttons=(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_SAVE, Gtk.ResponseType.OK)
            )
            dialog.set_current_folder(default_path)
            dialog.set_current_name(suggested)
            response = dialog.run()
            if response == Gtk.ResponseType.OK:
                dest = dialog.get_filename()
                uri = Gio.File.new_for_path(dest).get_uri()
                download.set_destination(uri)
            dialog.destroy()

import json
import threading
import requests


CONFIG_FILE = os.path.expanduser("~/.foxgtk_config.json")
DATA_FILE = os.path.expanduser("~/.foxgtk_data.json")

def load_data():
    try:
        with open(DATA_FILE, "r") as f:
            return json.load(f)
    except Exception:
        return {"history": [], "bookmarks": [], "homepage": "https://duckduckgo.com", "proxy": ""}

def save_data(data):
    try:
        with open(DATA_FILE, "w") as f:
            json.dump(data, f)
    except Exception as e:
        print(f"Error guardando datos: {e}")

class Navia(Gtk.Window):

    def __init__(self):
        super().__init__(title="Navia Browser")
        # === Aplicar hoja de estilos CSS ===
        css_provider = Gtk.CssProvider()
        css_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "themes.css")
        if os.path.exists(css_path):
            css_provider.load_from_path(css_path)
            screen = Gdk.Screen.get_default()
            Gtk.StyleContext.add_provider_for_screen(
                screen,
                css_provider,
                Gtk.STYLE_PROVIDER_PRIORITY_USER
            )
        else:
            print(f"No se encontró el archivo de estilos: {css_path}")
        # Leer tamaño guardado
        width, height = self.load_window_size()
        # Establecer tamaño mínimo permitido usando geometry hints correctamente
        geometry = Gdk.Geometry()
        geometry.min_width = 200
        geometry.min_height = 100
        self.set_geometry_hints(None, geometry, Gdk.WindowHints.MIN_SIZE)
        self.resize(width, height)
        icon_path = os.path.join(os.path.dirname(__file__), "icons", "icon.png")
        self.set_icon_from_file(icon_path)
        self.connect("destroy", self.on_destroy)
        self.connect("configure-event", self.on_configure_event)
        self._last_size = (width, height)

        self.header = Gtk.HeaderBar(show_close_button=True)
        self.set_titlebar(self.header)

        self.toolbar = Gtk.Box(spacing=5)


        self.btn_new_tab = self.make_button("icons/new_tab.png", self.create_tab)
        self.btn_home = self.make_button("icons/home.png", self.go_home)
        self.btn_home.get_style_context().add_class("icon-only")
        self.btn_back = self.make_button("icons/back.png", self.go_back)
        self.btn_back.get_style_context().add_class("icon-only")
        self.btn_forward = self.make_button("icons/forward.png", self.go_forward)
        self.btn_forward.get_style_context().add_class("icon-only")
        self.btn_reload = self.make_button("icons/reload.png", self.reload)
        self.btn_reload.get_style_context().add_class("icon-only")

        self.entry = Gtk.Entry()
        self.entry.set_placeholder_text("Buscar o escribir URL")
        self.entry.set_width_chars(100)
        self.entry.connect("activate", self.load_url)
        self.btn_go = self.make_button("icons/go.png", self.load_url)
        self.btn_go.get_style_context().add_class("icon-only")
        self.btn_fav = self.make_button("icons/star.png", self.save_favorite)
        self.btn_fav.get_style_context().add_class("icon-only")
        self.btn_downloads = self.make_button("icons/download.png", self.open_downloads_window)
        self.btn_downloads.get_style_context().add_class("icon-only")
        self.btn_extensions = self.make_button("icons/extension.png", self.show_extensions)
        self.btn_extensions.get_style_context().add_class("icon-only")
        self.btn_menu = self.make_button("icons/menu.png", self.open_menu)
        self.btn_menu.get_style_context().add_class("icon-only")

        self.header.pack_end(self.btn_menu)
        self.header.pack_end(self.btn_extensions)
        self.header.pack_end(self.btn_downloads)
    # Estado de extensiones: {nombre: activo}
        self.extensions_state = {}
        self.load_extensions_state()

        # Agregar botones a la toolbar (sin el de nueva pestaña)
        for btn in [self.btn_home, self.btn_back, self.btn_forward, self.btn_reload]:
            self.toolbar.pack_start(btn, False, False, 0)

        self.toolbar.pack_start(self.entry, True, True, 0)
        self.toolbar.pack_start(self.btn_go, False, False, 0)
        self.toolbar.pack_start(self.btn_fav, False, False, 0)

        self.header.pack_start(self.toolbar)




        # Contenedor principal vertical
        self.main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(self.main_box)

        # Barra de pestañas horizontal tipo Chrome
        self.tabs_scroller = Gtk.ScrolledWindow()
        self.tabs_scroller.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.NEVER)
        self.tabs_scroller.set_shadow_type(Gtk.ShadowType.NONE)
        self.tabs_bar = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        self.tabs_bar.set_size_request(-1, 28)  # Reducir altura de la barra de pestañas
        self.tabs_scroller.set_size_request(-1, 32)  # Altura total de la barra de pestañas
        self.tabs_scroller.add(self.tabs_bar)
        self.main_box.pack_start(self.tabs_scroller, False, False, 0)
    # Botón de nueva pestaña siempre después de las pestañas
        self.btn_new_tab.set_relief(Gtk.ReliefStyle.NONE)
        self.btn_new_tab.set_tooltip_text("Nueva pestaña")
        self.btn_new_tab.set_size_request(28, 28)
        self.tabs_bar.pack_start(self.btn_new_tab, False, False, 0)

        # Contenedor para el contenido de la pestaña activa
        self.tab_content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.main_box.pack_start(self.tab_content, True, True, 0)

        self.tabs = []  # Lista de (tab_widget, tab_button, tab_box)
        self.current_tab_index = -1

        self.data = load_data()

        # Botón de traducción
        self.btn_translate = self.make_button("icons/traductor.png", self.translate_page)
        self.btn_translate.get_style_context().add_class("icon-only")
        self.toolbar.pack_start(self.btn_translate, False, False, 0)

        # Forzar DuckDuckGo como página principal si no está configurada
        if self.data.get("homepage", "") != "https://duckduckgo.com":
            self.data["homepage"] = "https://duckduckgo.com"
            save_data(self.data)

        self.create_tab()
        self.load_active_extensions()
    def load_active_extensions(self):
        self.loaded_extensions = {}
        ext_dir = os.path.join(os.path.dirname(__file__), "extensions")
        if not os.path.isdir(ext_dir):
            return
        for nombre in os.listdir(ext_dir):
            ruta = os.path.join(ext_dir, nombre)
            if not os.path.isdir(ruta):
                continue
            if not self.extensions_state.get(nombre, True):
                print(f"[Extensiones] '{nombre}' está suspendida, no se carga.")
                continue
            manifest_path = os.path.join(ruta, "manifest.json")
            if not os.path.exists(manifest_path):
                continue
            try:
                with open(manifest_path, "r") as f:
                    manifest = json.load(f)
                module_name = manifest.get("module", "plugin.py")
                plugin_path = os.path.join(ruta, module_name)
                if os.path.exists(plugin_path):
                    import importlib.util
                    spec = importlib.util.spec_from_file_location(f"ext_{nombre}", plugin_path)
                    mod = importlib.util.module_from_spec(spec)
                    spec.loader.exec_module(mod)
                    # Llamar setup si existe
                    if hasattr(mod, "setup"):
                        api = type("API", (), {"window": self})()
                        mod.setup(api)
                        print(f"[Extensiones] '{nombre}' cargada correctamente.")
                        self.loaded_extensions[nombre] = mod
                # Cargar scripts JS si existen en manifest
                for script in manifest.get("content_scripts", []):
                    for js_file in script.get("js", []):
                        js_path = os.path.join(ruta, js_file)
                        if os.path.exists(js_path):
                            with open(js_path, "r") as jsf:
                                js_code = jsf.read()
                            webview = getattr(self, "get_current_webview", lambda: None)()
                            if webview:
                                webview.run_javascript(js_code, None, None)
                                print(f"[Extensiones] JS '{js_file}' inyectado en '{nombre}'.")
            except Exception as e:
                print(f"[Extensiones] Error cargando '{nombre}': {e}")

        # Sugerencias como popup
        self.suggest_popup = Gtk.Window(type=Gtk.WindowType.POPUP)
        self.suggest_popup.set_decorated(False)
        self.suggest_popup.set_transient_for(self)
        self.suggest_popup.set_resizable(False)
        self.suggest_popup.set_border_width(0)
        self.suggest_list = Gtk.ListBox()
        self.suggest_list.set_selection_mode(Gtk.SelectionMode.NONE)
        self.suggest_popup.add(self.suggest_list)
        self.suggest_popup.set_visible(False)

        self.entry.connect("changed", self.on_entry_changed)
        self.entry.connect("focus-out-event", self.hide_suggestions)
        self.suggest_list.connect("row-activated", self.on_suggestion_clicked)

        self.active_downloads = []

    def translate_page(self, widget):
        webview = self.get_current_webview()
        if not webview:
            return
        js = '''
            try {
                if (!document.getElementById('google_translate_element')) {
                    var div = document.createElement('div');
                    div.id = 'google_translate_element';
                    div.style.position = 'fixed';
                    div.style.top = '0';
                    div.style.right = '0';
                    div.style.zIndex = '99999';
                    div.style.background = 'white';
                    div.style.border = '1px solid #ccc';
                    div.style.padding = '4px 8px 4px 4px';
                    document.body.appendChild(div);
                    var s = document.createElement('script');
                    s.type = 'text/javascript';
                    s.src = 'https://translate.google.com/translate_a/element.js?cb=googleTranslateElementInit';
                    document.body.appendChild(s);
                    window.googleTranslateElementInit = function() {
                        new window.google.translate.TranslateElement({pageLanguage: 'auto', includedLanguages: 'es,en,fr,de,it,pt,ru,zh-CN,ar,ja,ko', layout: window.google.translate.TranslateElement.InlineLayout.SIMPLE}, 'google_translate_element');
                    }
                }
                true;
            } catch (e) {
                false;
            }
        '''
        def on_js_finished(webview, result):
            try:
                value = webview.run_javascript_finish(result).get_js_value().to_boolean()
                if not value:
                    self.mostrar_mensaje("No se pudo insertar el traductor de Google. La política de la página lo bloquea.")
            except Exception:
                self.mostrar_mensaje("No se pudo insertar el traductor de Google. La política de la página lo bloquea.")
        webview.run_javascript(js, None, on_js_finished)

    def mostrar_mensaje(self, texto):
        dialog = Gtk.MessageDialog(transient_for=self, flags=0, message_type=Gtk.MessageType.WARNING, buttons=Gtk.ButtonsType.OK, text=texto)
        dialog.run()
        dialog.destroy()

    def _translate_page_to(self, lang_code):
        webview = self.get_current_webview()
        if not webview:
            return
        # Extraer texto visible de la página usando JS
        js = '''
            (function() {
                function getTextNodes(node) {
                    var text = [];
                    if (node.nodeType === Node.TEXT_NODE && node.nodeValue.trim()) {
                        text.push(node.nodeValue);
                    } else if (node.nodeType === Node.ELEMENT_NODE && node.tagName !== 'SCRIPT' && node.tagName !== 'STYLE') {
                        for (var i = 0; i < node.childNodes.length; i++) {
                            text = text.concat(getTextNodes(node.childNodes[i]));
                        }
                    }
                    return text;
                }
                return getTextNodes(document.body).join('\n---\n');
            })();
        '''
        def on_text_fetched(webview, result, self=self, lang_code=lang_code):
            try:
                text = webview.run_javascript_finish(result).get_js_value().to_string()
                if not text.strip():
                    return
                # Llamar a la API de LibreTranslate
                threading.Thread(target=self._do_translate, args=(webview, text, lang_code), daemon=True).start()
            except Exception as e:
                print(f"Error extrayendo texto: {e}")
        webview.run_javascript(js, None, on_text_fetched)

    def _do_translate(self, webview, text, lang_code):
        import requests
        import json
        # Separar los textos para evitar límites de la API
        partes = text.split('\n---\n')
        traducciones = []
        for parte in partes:
            if not parte.strip():
                traducciones.append("")
                continue
            try:
                resp = requests.post(
                    "https://libretranslate.de/translate",
                    data={
                        "q": parte,
                        "source": "auto",
                        "target": lang_code,
                        "format": "text"
                    },
                    timeout=10
                )
                if resp.status_code == 200:
                    traducciones.append(resp.json().get("translatedText", ""))
                else:
                    traducciones.append(parte)
            except Exception as e:
                print(f"Error traduciendo: {e}")
                traducciones.append(parte)
        # Volver a unir y reemplazar en la página
        js_replace = '''
            (function(trads) {
                var idx = 0;
                function setTextNodes(node) {
                    if (node.nodeType === Node.TEXT_NODE && node.nodeValue.trim()) {
                        node.nodeValue = trads[idx++] || node.nodeValue;
                    } else if (node.nodeType === Node.ELEMENT_NODE && node.tagName !== 'SCRIPT' && node.tagName !== 'STYLE') {
                        for (var i = 0; i < node.childNodes.length; i++) {
                            setTextNodes(node.childNodes[i]);
                        }
                    }
                }
                setTextNodes(document.body);
            })(%s);
        ''' % json.dumps(traducciones)
        GLib.idle_add(lambda: webview.run_javascript(js_replace, None, None))
        # Forzar DuckDuckGo como página principal si no está configurada
        if self.data.get("homepage", "") != "https://duckduckgo.com":
            self.data["homepage"] = "https://duckduckgo.com"
            save_data(self.data)

        self.create_tab()

        # Sugerencias como popup
        self.suggest_popup = Gtk.Window(type=Gtk.WindowType.POPUP)
        self.suggest_popup.set_decorated(False)
        self.suggest_popup.set_transient_for(self)
        self.suggest_popup.set_resizable(False)
        self.suggest_popup.set_border_width(0)
        self.suggest_list = Gtk.ListBox()
        self.suggest_list.set_selection_mode(Gtk.SelectionMode.NONE)
        self.suggest_popup.add(self.suggest_list)
        self.suggest_popup.set_visible(False)

        self.entry.connect("changed", self.on_entry_changed)
        self.entry.connect("focus-out-event", self.hide_suggestions)
        self.suggest_list.connect("row-activated", self.on_suggestion_clicked)

        self.active_downloads = []

    def open_downloads_window(self, widget):
        if not hasattr(self, 'download_window') or self.download_window is None:
            self.download_window = DownloadWindow(self)
            self.download_window.connect("destroy", lambda w: setattr(self, 'download_window', None))
        else:
            self.download_window.present()

    def make_button(self, icon_path, callback):
        import mimetypes
        btn = Gtk.Button()
        if os.path.exists(icon_path):
            mime, _ = mimetypes.guess_type(icon_path)
            if mime == "image/svg+xml":
                # SVG: usar Gtk.Image y set_pixel_size
                img = Gtk.Image.new_from_file(icon_path)
                img.set_pixel_size(24)
            else:
                # PNG/JPG: cargar y escalar
                pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size(icon_path, 24, 24)
                img = Gtk.Image.new_from_pixbuf(pixbuf)
            btn.set_image(img)
        else:
            btn = Gtk.Button(label="+")
        btn.set_relief(Gtk.ReliefStyle.NONE)
        btn.connect("clicked", callback)
        return btn

    def get_current_webview(self):
        if self.current_tab_index >= 0 and self.current_tab_index < len(self.tabs):
            return self.tabs[self.current_tab_index][0].webview
        return None

    def load_url(self, widget=None):
        url = self.entry.get_text()
        if not url.startswith("http"):
            url = "https://duckduckgo.com/?q=" + url.replace(" ", "+")
        self.get_current_webview().load_uri(url)
        # Guardar en historial si es diferente al último
        if url:
            history = self.data.get("history", [])
            if not history or history[-1] != url:
                history.append(url)
                # Limitar historial a 100 entradas
                if len(history) > 100:
                    history = history[-100:]
                self.data["history"] = history
                save_data(self.data)

    def go_home(self, widget):
        homepage = self.data.get("homepage", "https://duckduckgo.com")
        self.get_current_webview().load_uri(homepage)

    def go_back(self, widget):
        web = self.get_current_webview()
        if web.can_go_back():
            web.go_back()

    def go_forward(self, widget):
        web = self.get_current_webview()
        if web.can_go_forward():
            web.go_forward()

    def reload(self, widget):
        self.get_current_webview().reload()

    def save_favorite(self, widget):
        web = self.get_current_webview()
        uri = web.get_uri()
        if uri:
            bookmarks = self.data.get("bookmarks", [])
            if uri not in bookmarks:
                bookmarks.append(uri)
                self.data["bookmarks"] = bookmarks
                save_data(self.data)
                print(f"Favorito guardado: {uri}")
            else:
                print("La página ya está en marcadores.")

    def open_menu(self, widget):
        menu = Gtk.Menu()
        items = {
            "Historial": self.show_history,
            "Marcadores": self.show_bookmarks,
            "Guardar como PDF": self.save_pdf,
            "Ajustes": self.open_settings,
            "Extensiones": self.show_extensions,
            "Acerca de": self.show_about
        }
        for label, action in items.items():
            item = Gtk.MenuItem(label=label)
            item.connect("activate", action)
            menu.append(item)

        menu.show_all()
        if hasattr(menu, 'popup_at_widget'):
            menu.popup_at_widget(widget, Gdk.Gravity.SOUTH, Gdk.Gravity.NORTH, None)
        else:
            menu.popup(None, None, None, None, 0, Gtk.get_current_event_time())

    def show_extensions(self, widget):
        ext_dir = os.path.join(os.path.dirname(__file__), "extensions")
        extensiones = []
        if os.path.isdir(ext_dir):
            for nombre in os.listdir(ext_dir):
                ruta = os.path.join(ext_dir, nombre)
                if os.path.isdir(ruta):
                    extensiones.append(nombre)

        win = Gtk.Window(title="Gestor de Extensiones")
        win.set_default_size(400, 300)
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_border_width(15)
        label = Gtk.Label(label="Extensiones instaladas:")
        label.set_markup("<b>Extensiones instaladas:</b>")
        vbox.pack_start(label, False, False, 0)

        if extensiones:
            for ext in extensiones:
                hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
                lbl = Gtk.Label(label=ext)
                lbl.set_xalign(0)
                hbox.pack_start(lbl, True, True, 0)
                estado = self.extensions_state.get(ext, True)
                estado_lbl = Gtk.Label(label="Activo" if estado else "Suspendido")
                estado_lbl.set_xalign(0)
                hbox.pack_start(estado_lbl, False, False, 0)
                btn_suspend = Gtk.Button(label="Suspender" if estado else "Activar")
                btn_suspend.connect("clicked", self.toggle_extension, ext)
                hbox.pack_start(btn_suspend, False, False, 0)
                btn_restart = Gtk.Button(label="Reiniciar")
                btn_restart.connect("clicked", self.restart_extension, ext)
                hbox.pack_start(btn_restart, False, False, 0)
                vbox.pack_start(hbox, False, False, 0)
        else:
            vbox.pack_start(Gtk.Label(label="No se encontraron extensiones."), False, False, 0)

        btn_close = Gtk.Button(label="Cerrar")
        btn_close.connect("clicked", lambda w: win.destroy())
        vbox.pack_end(btn_close, False, False, 0)
        win.add(vbox)
        win.show_all()

    def toggle_extension(self, widget, ext_name):
        # Cambia el estado de la extensión (activar/suspender) en tiempo real
        estado = self.extensions_state.get(ext_name, True)
        self.extensions_state[ext_name] = not estado
        self.save_extensions_state()
        if not estado:
            # Activar: cargar extensión
            self.load_single_extension(ext_name)
            msg = f"La extensión '{ext_name}' ha sido activada."
        else:
            # Suspender: descargar extensión si es posible
            self.unload_single_extension(ext_name)
            msg = f"La extensión '{ext_name}' ha sido suspendida."
        dialog = Gtk.MessageDialog(
            parent=self,
            flags=Gtk.DialogFlags.MODAL,
            type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            message_format=msg
        )
        dialog.run()
        dialog.destroy()
        self.show_extensions(None)

    def load_single_extension(self, nombre):
        ext_dir = os.path.join(os.path.dirname(__file__), "extensions", nombre)
        manifest_path = os.path.join(ext_dir, "manifest.json")
        if not os.path.exists(manifest_path):
            return
        try:
            with open(manifest_path, "r") as f:
                manifest = json.load(f)
            module_name = manifest.get("module", "plugin.py")
            plugin_path = os.path.join(ext_dir, module_name)
            if os.path.exists(plugin_path):
                import importlib.util
                spec = importlib.util.spec_from_file_location(f"ext_{nombre}", plugin_path)
                mod = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(mod)
                if hasattr(mod, "setup"):
                    api = type("API", (), {"window": self})()
                    mod.setup(api)
                    print(f"[Extensiones] '{nombre}' activada en tiempo real.")
                self.loaded_extensions[nombre] = mod
            for script in manifest.get("content_scripts", []):
                for js_file in script.get("js", []):
                    js_path = os.path.join(ext_dir, js_file)
                    if os.path.exists(js_path):
                        with open(js_path, "r") as jsf:
                            js_code = jsf.read()
                        webview = getattr(self, "get_current_webview", lambda: None)()
                        if webview:
                            webview.run_javascript(js_code, None, None)
                            print(f"[Extensiones] JS '{js_file}' inyectado en '{nombre}'.")
        except Exception as e:
            print(f"[Extensiones] Error activando '{nombre}': {e}")

    def unload_single_extension(self, nombre):
        # Descarga la extensión si es posible (solo elimina referencia, no descarga JS)
        if hasattr(self, "loaded_extensions") and nombre in self.loaded_extensions:
            del self.loaded_extensions[nombre]
            print(f"[Extensiones] '{nombre}' suspendida en tiempo real.")

    def restart_extension(self, widget, ext_name):
        # Reinicia la extensión (simulado: desactiva y activa)
        self.extensions_state[ext_name] = False
        self.save_extensions_state()
        self.extensions_state[ext_name] = True
        self.save_extensions_state()
        dialog = Gtk.MessageDialog(
            parent=self,
            flags=Gtk.DialogFlags.MODAL,
            type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            message_format=f"La extensión '{ext_name}' ha sido reiniciada."
        )
        dialog.run()
        dialog.destroy()
        self.show_extensions(None)

    def load_extensions_state(self):
        # Carga el estado de las extensiones desde un archivo
        state_path = os.path.join(os.path.dirname(__file__), "extensions_state.json")
        if os.path.exists(state_path):
            try:
                with open(state_path, "r") as f:
                    self.extensions_state = json.load(f)
            except Exception:
                self.extensions_state = {}
        else:
            self.extensions_state = {}

    def save_extensions_state(self):
        # Guarda el estado de las extensiones en un archivo
        state_path = os.path.join(os.path.dirname(__file__), "extensions_state.json")
        try:
            with open(state_path, "w") as f:
                json.dump(self.extensions_state, f)
        except Exception:
            pass

    def show_history(self, widget):
        dialog = Gtk.Dialog(title="Historial", transient_for=self, flags=0)
        dialog.set_default_size(500, 350)
        dialog.add_button("Cerrar", Gtk.ResponseType.CLOSE)
        box = dialog.get_content_area()
        listbox = Gtk.ListBox()
        box.pack_start(listbox, True, True, 0)
        history = self.data.get("history", [])
        if not history:
            listbox.add(Gtk.Label(label="No hay historial."))
        else:
            for url in history:
                row = Gtk.ListBoxRow()
                hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
                lbl = Gtk.Label(label=url, xalign=0)
                hbox.pack_start(lbl, True, True, 0)
                row.add(hbox)
                listbox.add(row)
        listbox.show_all()

        def on_row_activated(listbox, row):
            if row:
                url = row.get_child().get_children()[0].get_text()
                self.load_url_from_history_or_bookmark(url)
                dialog.response(Gtk.ResponseType.CLOSE)
        listbox.connect("row-activated", on_row_activated)
        dialog.run()
        dialog.destroy()

    def show_bookmarks(self, widget):
        dialog = Gtk.Dialog(title="Marcadores", transient_for=self, flags=0)
        dialog.set_default_size(500, 350)
        dialog.add_button("Cerrar", Gtk.ResponseType.CLOSE)
        box = dialog.get_content_area()
        listbox = Gtk.ListBox()
        box.pack_start(listbox, True, True, 0)
        bookmarks = self.data.get("bookmarks", [])
        if not bookmarks:
            listbox.add(Gtk.Label(label="No hay marcadores."))
        else:
            for url in bookmarks:
                row = Gtk.ListBoxRow()
                hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
                lbl = Gtk.Label(label=url, xalign=0)
                hbox.pack_start(lbl, True, True, 0)
                row.add(hbox)
                listbox.add(row)
        listbox.show_all()

        def on_row_activated(listbox, row):
            if row:
                url = row.get_child().get_children()[0].get_text()
                self.load_url_from_history_or_bookmark(url)
                dialog.response(Gtk.ResponseType.CLOSE)
        listbox.connect("row-activated", on_row_activated)
        dialog.run()
        dialog.destroy()

    def load_url_from_history_or_bookmark(self, url):
        self.entry.set_text(url)
        self.load_url()

    def save_pdf(self, widget):
        web = self.get_current_webview()
        dialog = Gtk.FileChooserDialog(
            title="Guardar como PDF",
            action=Gtk.FileChooserAction.SAVE,
            buttons=(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                     Gtk.STOCK_SAVE, Gtk.ResponseType.OK)
        )
        dialog.set_current_name("pagina.pdf")
        response = dialog.run()

        if response == Gtk.ResponseType.OK:
            file_path = dialog.get_filename()
            web.print_to_pdf(file_path, None, None)
            print(f"Guardado como PDF: {file_path}")

        dialog.destroy()

    def open_settings(self, widget):
        # Diálogo de configuración mejorado con pestañas
        dialog = Gtk.Dialog(title="Ajustes", transient_for=self, flags=0)
        # Forzar tema oscuro en el diálogo de ajustes
        dialog.get_style_context().add_class("theme-dark")
        dialog.set_default_size(400, 300)
        dialog.add_button("Cancelar", Gtk.ResponseType.CANCEL)
        dialog.add_button("Guardar", Gtk.ResponseType.OK)
        box = dialog.get_content_area()

        notebook = Gtk.Notebook()
        box.pack_start(notebook, True, True, 0)

        # --- Pestaña Página Principal ---
        page_home = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        page_home.set_border_width(10)
        lbl_home = Gtk.Label(label="Página principal:")
        entry_home = Gtk.Entry()
        entry_home.set_text(self.data.get("homepage", "https://duckduckgo.com"))
        page_home.pack_start(lbl_home, False, False, 5)
        page_home.pack_start(entry_home, False, False, 5)
        notebook.append_page(page_home, Gtk.Label(label="Página Principal"))

        # --- Pestaña Limpiar Datos ---
        page_data = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        page_data.set_border_width(10)
        btn_clear_history = Gtk.Button(label="Limpiar historial")
        btn_clear_history.connect("clicked", self.clear_history)
        btn_clear_bookmarks = Gtk.Button(label="Limpiar marcadores")
        btn_clear_bookmarks.connect("clicked", self.clear_bookmarks)
        page_data.pack_start(btn_clear_history, False, False, 5)
        page_data.pack_start(btn_clear_bookmarks, False, False, 5)
        notebook.append_page(page_data, Gtk.Label(label="Limpiar Datos"))

        # --- Pestaña Proxy ---
        page_proxy = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        page_proxy.set_border_width(10)
        lbl_proxy = Gtk.Label(label="Proxy (ej: http://127.0.0.1:8080):")
        entry_proxy = Gtk.Entry()
        entry_proxy.set_text(self.data.get("proxy", ""))
        btn_clear_proxy = Gtk.Button(label="Borrar proxy")
        def clear_proxy(_):
            entry_proxy.set_text("")
        btn_clear_proxy.connect("clicked", clear_proxy)
        page_proxy.pack_start(lbl_proxy, False, False, 5)
        page_proxy.pack_start(entry_proxy, False, False, 5)
        page_proxy.pack_start(btn_clear_proxy, False, False, 5)
        notebook.append_page(page_proxy, Gtk.Label(label="Proxy"))

        # --- Pestaña Descargas ---
        page_downloads = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        page_downloads.set_border_width(10)
        lbl_path = Gtk.Label(label="Carpeta de descargas:")
        box_path = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5)
        entry_path = Gtk.Entry()
        entry_path.set_text(self.data.get("download_path", os.path.expanduser("~/Descargas")))
        btn_select = Gtk.Button(label="Seleccionar carpeta")
        def select_folder(_):
            dialog_folder = Gtk.FileChooserDialog(
                title="Seleccionar carpeta de descargas",
                parent=dialog,
                action=Gtk.FileChooserAction.SELECT_FOLDER,
                buttons=(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OK, Gtk.ResponseType.OK)
            )
            if dialog_folder.run() == Gtk.ResponseType.OK:
                entry_path.set_text(dialog_folder.get_filename())
            dialog_folder.destroy()
        btn_select.connect("clicked", select_folder)
        box_path.pack_start(entry_path, True, True, 0)
        box_path.pack_start(btn_select, False, False, 0)
        page_downloads.pack_start(lbl_path, False, False, 5)
        page_downloads.pack_start(box_path, False, False, 5)

        lbl_mode = Gtk.Label(label="Modo de descarga:")
        radio_ask = Gtk.RadioButton.new_with_label_from_widget(None, "Preguntar antes de descargar")
        radio_auto = Gtk.RadioButton.new_with_label_from_widget(radio_ask, "Descargar automáticamente")
        mode = self.data.get("download_mode", "ask")
        if mode == "auto":
            radio_auto.set_active(True)
        else:
            radio_ask.set_active(True)
        page_downloads.pack_start(lbl_mode, False, False, 5)
        page_downloads.pack_start(radio_ask, False, False, 0)
        page_downloads.pack_start(radio_auto, False, False, 0)
        notebook.append_page(page_downloads, Gtk.Label(label="Descargas"))

        box.show_all()
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            self.data["homepage"] = entry_home.get_text()
            self.data["proxy"] = entry_proxy.get_text()
            self.data["download_path"] = entry_path.get_text()
            self.data["download_mode"] = "auto" if radio_auto.get_active() else "ask"
            save_data(self.data)
            print("Ajustes guardados")
        dialog.destroy()

    def clear_history(self, widget):
        self.data["history"] = []
        save_data(self.data)
        print("Historial limpiado")

    def clear_bookmarks(self, widget):
        self.data["bookmarks"] = []
        save_data(self.data)
        print("Marcadores limpiados")

    def show_about(self, widget):
        dialog = Gtk.Dialog(title="Acerca de Navia Browser", transient_for=self, flags=0)
        dialog.add_button(Gtk.STOCK_OK, Gtk.ResponseType.OK)
        box = dialog.get_content_area()
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        # Cargar la imagen
        img_path = os.path.join(os.path.dirname(__file__), "icons", "about.png")
        pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(img_path, 64, 64, True)
        image = Gtk.Image.new_from_pixbuf(pixbuf)
        hbox.pack_start(image, False, False, 0)
        # Texto descriptivo
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        label_title = Gtk.Label()
        label_title.set_markup("<b>Navia Browser</b>")
        label_title.set_xalign(0)
        label_desc = Gtk.Label(label="Navegador ligero usando GTK y WebKit y diseñado con IA.")
        label_desc.set_xalign(0)
        vbox.pack_start(label_title, False, False, 0)
        vbox.pack_start(label_desc, False, False, 0)
        hbox.pack_start(vbox, True, True, 0)
        box.add(hbox)
        dialog.show_all()
        dialog.run()
        dialog.destroy()



    def create_tab(self, widget=None, url=None):
        if url is None:
            url = self.data.get("homepage", "https://duckduckgo.com")
        tab = BrowserTab(self, url)

        # Contenedor de la pestaña (tipo Chrome)
        tab_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        tab_box.set_name("chrome-tab")
        tab_box.get_style_context().add_class("chrome-tab")
        tab_box.set_valign(Gtk.Align.FILL)
        tab_box.set_halign(Gtk.Align.START)
        tab_box.set_border_width(0)
        tab_box.set_size_request(44, 24)  # Más angosta, estilo Chrome

        # Título
        tab_label = Gtk.Label(label="")
        tab_label.set_ellipsize(3)  # PANGO_ELLIPSIZE_END
        tab_label.set_max_width_chars(10)
        tab_label.set_margin_start(4)
        tab_label.set_margin_end(4)
        tab_label.get_style_context().add_class("chrome-tab-label")
        tab_box.pack_start(tab_label, True, True, 0)

        # Botón cerrar
        btn_close = Gtk.Button()
        btn_close.set_relief(Gtk.ReliefStyle.NONE)
        btn_close.set_tooltip_text("Cerrar pestaña")
        btn_close.set_focus_on_click(False)
        btn_close.set_size_request(12, 12)
        btn_close.get_style_context().add_class("chrome-tab-close")
        if os.path.exists("icons/close.png"):
            from gi.repository import GdkPixbuf
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size("icons/close.png", 20, 20)
            img_close = Gtk.Image.new_from_pixbuf(pixbuf)
        else:
            img_close = Gtk.Image.new_from_icon_name("window-close", Gtk.IconSize.MENU)
            img_close.set_pixel_size(10)
        btn_close.set_image(img_close)
        tab_box.pack_start(btn_close, False, False, 0)

        def close_tab(widget, event=None):
            idx = self.tabs_bar.get_children().index(tab_box)
            self.remove_tab(idx)
        btn_close.connect("clicked", close_tab)

        # Añadir la pestaña justo antes del botón de nueva pestaña
        children = self.tabs_bar.get_children()
        if self.btn_new_tab in children:
            idx = children.index(self.btn_new_tab)
            self.tabs_bar.pack_start(tab_box, False, False, 0)
            self.tabs_bar.reorder_child(tab_box, idx)
        else:
            self.tabs_bar.pack_start(tab_box, False, False, 0)
        self.tabs.append((tab, tab_box, tab_box))
        self.tabs_bar.show_all()

        # Seleccionar la nueva pestaña
        self.select_tab(len(self.tabs) - 1)

        tab.webview.connect("notify::title", lambda webview, _: self.update_tab_label(tab_label, webview))
        tab.webview.connect("notify::uri", self.update_url_entry)
        self.show_all()

    def remove_tab(self, idx):
        if idx < 0 or idx >= len(self.tabs):
            return
        tab, tab_box, _ = self.tabs.pop(idx)
        self.tabs_bar.remove(tab_box)
        if tab in self.tab_content.get_children():
            self.tab_content.remove(tab)
        # Destruir el WebView para detener audio/video
        if hasattr(tab, 'webview') and tab.webview:
            tab.webview.destroy()
        # Seleccionar otra pestaña si quedan
        if self.tabs:
            new_idx = min(idx, len(self.tabs) - 1)
            self.select_tab(new_idx)
        else:
            self.current_tab_index = -1
            for child in self.tab_content.get_children():
                self.tab_content.remove(child)

    def select_tab(self, idx):
        if idx < 0 or idx >= len(self.tabs):
            return
        self.current_tab_index = idx
        for child in self.tab_content.get_children():
            self.tab_content.remove(child)
        self.tab_content.pack_start(self.tabs[idx][0], True, True, 0)
        self.tab_content.show_all()
        # Resaltar la pestaña activa
        for i, (_, tab_box, _) in enumerate(self.tabs):
            if i == idx:
                tab_box.get_style_context().add_class("active-tab")
            else:
                tab_box.get_style_context().remove_class("active-tab")

    # Ya no se usa on_tab_selected, ahora se selecciona con select_tab

    # Manejar clic en la barra de pestañas para cambiar de pestaña
    def connect_tab_bar_events(self):
        def on_tab_box_event(tab_box, event):
            if event.type == Gdk.EventType.BUTTON_PRESS:
                idx = self.tabs_bar.get_children().index(tab_box)
                self.select_tab(idx)
        for _, tab_box, _ in self.tabs:
            tab_box.add_events(Gdk.EventMask.BUTTON_PRESS_MASK)
            tab_box.connect("button-press-event", on_tab_box_event)

    def update_tab_label(self, label, webview):
        max_len = 18
        def truncate(text):
            text = text.strip()
            return text[:max_len-3] + '...' if len(text) > max_len else text

        title = webview.get_title()
        if title and title.strip():
            label.set_text(truncate(title))
        else:
            uri = webview.get_uri()
            if uri:
                from urllib.parse import urlparse
                parsed = urlparse(uri)
                host = parsed.netloc or uri
                label.set_text(truncate(host if host else uri))


    def update_url_entry(self, webview, _):
        if webview == self.get_current_webview():
            uri = webview.get_uri()
            if uri:
                self.entry.set_text(uri)
                # Guardar en historial si es diferente al último
                history = self.data.get("history", [])
        icon_path = os.path.join(os.path.dirname(__file__), "icons", "icon.png")
        self.set_icon_from_file(icon_path)
        if not history or history[-1] != uri:
                    history.append(uri)
                    # Limitar historial a 100 entradas
                    if len(history) > 100:
                        history = history[-100:]
                    self.data["history"] = history
                    save_data(self.data)

    def on_destroy(self, widget):
        # Guardar el último tamaño conocido
        width, height = self._last_size
        try:
            with open(CONFIG_FILE, "w") as f:
                json.dump({"width": width, "height": height}, f)
        except Exception as e:
            print(f"Error guardando tamaño de ventana: {e}")
        Gtk.main_quit()

    def on_configure_event(self, widget, event):
        # Guardar el tamaño cada vez que se cambia
        width = event.width
        height = event.height
        self._last_size = (width, height)
        return False

    def load_window_size(self):
        try:
            with open(CONFIG_FILE, "r") as f:
                data = json.load(f)
                width = int(data.get("width", 1024))
                height = int(data.get("height", 720))
                return width, height
        except Exception:
            return 1024, 720

    def on_entry_changed(self, entry, *args):
        # Solo mostrar sugerencias si el entry tiene el foco
        if not entry.is_focus():
            self.hide_suggestions()
            return
        text = entry.get_text().strip()
        if not text:
            self.hide_suggestions()
            return

        def fetch_suggestions():
            try:
                res = requests.get(f"https://duckduckgo.com/ac/?q={requests.utils.quote(text)}", timeout=2)
                data = res.json()
                GLib.idle_add(self.show_suggestions, data)
            except Exception:
                GLib.idle_add(self.hide_suggestions)

        threading.Thread(target=fetch_suggestions, daemon=True).start()

    def show_suggestions(self, data):
        self.suggest_list.foreach(lambda row: self.suggest_list.remove(row))
        if not isinstance(data, list) or not data:
            self.hide_suggestions()
            return
        for item in data:
            phrase = item.get("phrase")
            if phrase:
                row = Gtk.ListBoxRow()
                label = Gtk.Label(label=phrase, xalign=0)
                row.add(label)
                self.suggest_list.add(row)
        self.suggest_list.show_all()
        self.position_suggestions()
        self.suggest_popup.set_visible(True)

    def on_suggestion_clicked(self, listbox, row):
        phrase = row.get_child().get_text()
        self.entry.set_text(phrase)
        self.hide_suggestions()
        self.load_url()

    def hide_suggestions(self, *args):
        self.suggest_popup.set_visible(False)

    def position_suggestions(self):
        # Posiciona el popup justo debajo del Gtk.Entry
        entry_allocation = self.entry.get_allocation()
        window = self.get_window()
        if window:
            origin = window.get_origin()
            x = origin.x + entry_allocation.x
            y = origin.y + entry_allocation.y + entry_allocation.height
            self.suggest_popup.move(x, y)
            self.suggest_popup.set_size_request(entry_allocation.width, -1)

if __name__ == "__main__":
    app = Navia()
    Gtk.main()
