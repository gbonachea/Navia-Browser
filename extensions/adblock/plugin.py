# plugin.py
import os
import re
import time
import threading
import pathlib
import json

EASYLIST_URL = "https://easylist.to/easylist/easylist.txt"
CACHE_NAME = "easylist_cached.txt"
CACHE_TTL = 60 * 60 * 24   # 24 horas

# Reglas compiladas
compiled_rules = []
whitelist_rules = []

def download_easylist(dest_path):
    """Intenta descargar EasyList (usa requests si está, si no urllib)."""
    try:
        import requests
        r = requests.get(EASYLIST_URL, timeout=20)
        r.raise_for_status()
        dest_path.write_text(r.text, encoding="utf-8")
        return True
    except Exception:
        try:
            from urllib.request import urlopen
            with urlopen(EASYLIST_URL, timeout=20) as resp:
                data = resp.read().decode("utf-8", errors="ignore")
                dest_path.write_text(data, encoding="utf-8")
                return True
        except Exception as e:
            print("[AdBlock] Error descargando EasyList:", e)
            return False

def parse_easylist(filepath):
    """
    Parsea de forma simple reglas útiles:
    - Ignora comentarios y reglas complejas.
    - Captura líneas con "||domain^" -> convierte a regex r'(^|\\.)domain'
    - Captura líneas que contienen dominos o texto con wildcards.
    - Ignora excepciones que empiezan con @@ (las guarda en whitelist).
    """
    rules = []
    whitelist = []
    text = filepath.read_text(encoding="utf-8", errors="ignore")
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith('!'):
            continue
        if line.startswith('@@'):
            rule = line[2:].strip()
            # Simplificar: extraer dominio-like
            m = re.search(r'\|\|([^\^/]+)', rule)
            if m:
                whitelist.append(re.compile(re.escape(m.group(1))))
            continue

        # domain rule: ||domain^
        m = re.match(r'\|\|([^\^/]+)', line)
        if m:
            domain = re.escape(m.group(1))
            # regex match subdomain or exact
            regex = re.compile(r'(^|\.)' + domain, re.IGNORECASE)
            rules.append(regex)
            continue

        # simple contains rule (e.g. ads.example.com/ads.js or /adbanner)
        if '/' in line or '*' in line:
            # try to create a contains regex for the main token
            token = re.sub(r'[\*\^]+', '', line)
            token = token.strip()
            if len(token) >= 3 and not token.startswith('@'):
                try:
                    rules.append(re.compile(re.escape(token), re.IGNORECASE))
                except Exception:
                    pass
            continue

        # fallback: treat as hostname-like
        if re.match(r'^[\w\.\-]{3,}$', line):
            try:
                rules.append(re.compile(re.escape(line), re.IGNORECASE))
            except Exception:
                pass

    return rules, whitelist

def ensure_rules(ext_dir):
    """Asegura que exista cache con reglas y que esté actualizada."""
    global compiled_rules, whitelist_rules
    cache_path = ext_dir / CACHE_NAME

    need_download = True
    if cache_path.exists():
        age = time.time() - cache_path.stat().st_mtime
        if age < CACHE_TTL:
            need_download = False

    if need_download:
        print("[AdBlock] Descargando EasyList...")
        ok = download_easylist(cache_path)
        if not ok and not cache_path.exists():
            print("[AdBlock] No se pudo descargar EasyList y no hay cache.")
            return

    rules, whitelist = parse_easylist(cache_path)
    compiled_rules = rules
    whitelist_rules = whitelist
    print(f"[AdBlock] Cargadas {len(compiled_rules)} reglas, {len(whitelist_rules)} excepciones.")

def matches_block(uri):
    """Devuelve True si la uri coincide con alguna regla (y no está en whitelist)."""
    if not uri:
        return False
    lower = uri.lower()
    for w in whitelist_rules:
        try:
            if w.search(lower):
                return False
        except Exception:
            pass
    for r in compiled_rules:
        try:
            if r.search(lower):
                return True
        except Exception:
            pass
    return False

def setup(api):
    """
    setup(api) será llamado por el ExtensionManager.
    'api' tiene: api.window (con api.window.webview y .ucm)
    """
    ext_dir = pathlib.Path(__file__).resolve().parent
    # Cargar reglas en background para no bloquear UI
    def _load_rules():
        try:
            ensure_rules(ext_dir)
        except Exception as e:
            print("[AdBlock] Error cargando reglas:", e)
    threading.Thread(target=_load_rules, daemon=True).start()

    webview = getattr(api.window, "webview", None)
    if webview is None:
        print("[AdBlock] No se encontró webview en api.window")
        return

    # Handler para resource-load-started (si está disponible)
    def on_resource_load_started(wv, resource, request):
        try:
            uri = None
            # request puede ser GLib.Bytes o WebKit2.Request dependiendo de versión
            if hasattr(request, "get_uri"):
                uri = request.get_uri()
            else:
                # attempt attribute access
                uri = getattr(request, "uri", None)
            if matches_block(uri):
                print("[AdBlock] resource-load-started -> bloqueado:", uri)
                # Intentar abortar recurso si el objeto lo permite
                if hasattr(resource, "stop"):
                    try:
                        resource.stop()
                    except Exception:
                        pass
                if hasattr(resource, "abort"):
                    try:
                        resource.abort()
                    except Exception:
                        pass
                # No hay un 'return False/True' universal aquí
        except Exception as e:
            print("[AdBlock] error en resource handler:", e)

    # Handler para decide-policy (más portable)
    def on_decide_policy(wv, decision, decision_type):
        try:
            # decision_type: WebKit2.PolicyDecisionType.NAVIGATION_ACTION etc; a veces se recibe como int
            if hasattr(decision, "get_request"):
                req = decision.get_request()
                uri = req.get_uri() if hasattr(req, "get_uri") else None
                if matches_block(uri):
                    print("[AdBlock] decide-policy -> bloqueado:", uri)
                    try:
                        decision.ignore()
                    except Exception:
                        pass
                    return True
        except Exception as e:
            print("[AdBlock] error en decide-policy:", e)
        return False

    # Intentamos conectar múltiples señales con guardias para evitar excepciones si no existen
    try:
        webview.connect("resource-load-started", on_resource_load_started)
        print("[AdBlock] Conectado a resource-load-started")
    except Exception:
        try:
            webview.connect("send-request", on_resource_load_started)
            print("[AdBlock] Conectado a send-request (fallback)")
        except Exception:
            print("[AdBlock] resource-load-started y send-request no disponibles; usando decide-policy fallback")

    try:
        webview.connect("decide-policy", on_decide_policy)
        print("[AdBlock] Conectado a decide-policy (fallback).")
    except Exception:
        print("[AdBlock] No se pudo conectar a decide-policy; puede que el bloqueo no funcione en esta versión.")

    # Método expuesto para recargar reglas desde UI o desde host
    def reload_rules():
        try:
            ensure_rules(ext_dir)
            print("[AdBlock] Reglas recargadas.")
        except Exception as e:
            print("[AdBlock] Error recargando reglas:", e)

    # Si deseas permitir que la página pida recargar reglas desde JS, podríamos registrar un message handler.
    # Por ahora solo lo dejamos en el plugin.
    api.adblock_reload = reload_rules

    print("[AdBlock] Extensión cargada.")
