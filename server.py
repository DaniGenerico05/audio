from fastapi import FastAPI, Query
from fastapi.responses import HTMLResponse, FileResponse
import requests
from requests.auth import HTTPBasicAuth
import os
from urllib.parse import unquote


# =========================================================
# FASTAPI
# =========================================================

app = FastAPI(title="Music Stick")


# =========================================================
# VLC
# =========================================================

VLC_URL = "http://127.0.0.1:8080/requests/status.json"
VLC_PASSWORD = "peeslosh"


# =========================================================
# HEALTH
# =========================================================

@app.get("/health")
def health():

    return {
        "status": "ok"
    }


# =========================================================
# OBTENER INFORMACION DE VLC
# =========================================================

def obtener_datos_vlc():

    response = requests.get(
        VLC_URL,
        auth=HTTPBasicAuth(
            "",
            VLC_PASSWORD
        ),
        timeout=5
    )

    response.raise_for_status()

    return response.json()


# =========================================================
# ENVIAR COMANDO A VLC
# =========================================================

def enviar_comando_vlc(
    comando,
    valor=None
):

    params = {
        "command": comando
    }

    if valor is not None:

        params["val"] = valor

    response = requests.get(
        VLC_URL,
        params=params,
        auth=HTTPBasicAuth(
            "",
            VLC_PASSWORD
        ),
        timeout=5
    )

    response.raise_for_status()

    return response.json()


# =========================================================
# INFORMACION DE LA CANCION
# =========================================================

@app.get("/now-playing")
def now_playing():

    data = obtener_datos_vlc()

    meta = (
        data
        .get("information", {})
        .get("category", {})
        .get("meta", {})
    )

    return {

        "artista": meta.get(
            "artist",
            "Artista desconocido"
        ),

        "titulo": meta.get(
            "title",
            "Sin título"
        ),

        "album": meta.get(
            "album",
            "Sin álbum"
        ),

        "duracion": data.get(
            "length",
            0
        ),

        "tiempo": data.get(
            "time",
            0
        ),

        "estado": data.get(
            "state",
            "unknown"
        ),

        "volumen": data.get(
            "volume",
            0
        )
    }


# =========================================================
# PORTADA
# =========================================================

@app.get("/cover")
def cover():

    data = obtener_datos_vlc()

    meta = (
        data
        .get("information", {})
        .get("category", {})
        .get("meta", {})
    )

    artwork_url = meta.get(
        "artwork_url"
    )

    if not artwork_url:

        return HTMLResponse(
            "No hay portada disponible",
            status_code=404
        )

    # VLC devuelve algo como:
    #
    # file:///C:/Users/user/.../art.jpg
    #

    ruta = artwork_url.replace(
        "file:///",
        ""
    )

    ruta = unquote(ruta)

    ruta = ruta.replace(
        "/",
        os.sep
    )

    if not os.path.exists(ruta):

        return HTMLResponse(
            f"No se encontró la portada en: {ruta}",
            status_code=404
        )

    return FileResponse(
        ruta,
        media_type="image/jpeg"
    )


# =========================================================
# PLAY / PAUSE
# =========================================================

@app.post("/control/play-pause")
def play_pause():

    data = obtener_datos_vlc()

    estado = data.get(
        "state",
        "unknown"
    )

    if estado == "playing":

        enviar_comando_vlc(
            "pl_pause"
        )

        accion = "pause"

    else:

        enviar_comando_vlc(
            "pl_play"
        )

        accion = "play"

    return {

        "ok": True,

        "accion": accion
    }


# =========================================================
# SIGUIENTE CANCION
# =========================================================

@app.post("/control/next")
def siguiente():

    enviar_comando_vlc(
        "pl_next"
    )

    return {

        "ok": True,

        "accion": "next"
    }


# =========================================================
# CANCION ANTERIOR
# =========================================================

@app.post("/control/previous")
def anterior():

    enviar_comando_vlc(
        "pl_previous"
    )

    return {

        "ok": True,

        "accion": "previous"
    }


# =========================================================
# ADELANTAR
# =========================================================

@app.post("/control/forward")
def adelantar(
    segundos: int = Query(
        10,
        ge=1,
        le=300
    )
):

    data = obtener_datos_vlc()

    tiempo_actual = data.get(
        "time",
        0
    )

    duracion = data.get(
        "length",
        0
    )

    nuevo_tiempo = min(
        tiempo_actual + segundos,
        duracion
    )

    enviar_comando_vlc(
        "seek",
        nuevo_tiempo
    )

    return {

        "ok": True,

        "accion": "forward",

        "segundos": segundos,

        "tiempo": nuevo_tiempo
    }


# =========================================================
# REGRESAR
# =========================================================

@app.post("/control/backward")
def regresar(
    segundos: int = Query(
        10,
        ge=1,
        le=300
    )
):

    data = obtener_datos_vlc()

    tiempo_actual = data.get(
        "time",
        0
    )

    nuevo_tiempo = max(
        tiempo_actual - segundos,
        0
    )

    enviar_comando_vlc(
        "seek",
        nuevo_tiempo
    )

    return {

        "ok": True,

        "accion": "backward",

        "segundos": segundos,

        "tiempo": nuevo_tiempo
    }


# =========================================================
# ESTABLECER VOLUMEN
# =========================================================

@app.post("/control/volume")
def volumen(
    valor: int = Query(
        ...,
        ge=0,
        le=512
    )
):

    enviar_comando_vlc(
        "volume",
        valor
    )

    return {

        "ok": True,

        "volumen": valor
    }


# =========================================================
# SUBIR VOLUMEN
# =========================================================

@app.post("/control/volume/up")
def subir_volumen():

    data = obtener_datos_vlc()

    volumen_actual = data.get(
        "volume",
        0
    )

    nuevo_volumen = min(
        volumen_actual + 25,
        512
    )

    enviar_comando_vlc(
        "volume",
        nuevo_volumen
    )

    return {

        "ok": True,

        "volumen": nuevo_volumen
    }


# =========================================================
# BAJAR VOLUMEN
# =========================================================

@app.post("/control/volume/down")
def bajar_volumen():

    data = obtener_datos_vlc()

    volumen_actual = data.get(
        "volume",
        0
    )

    nuevo_volumen = max(
        volumen_actual - 25,
        0
    )

    enviar_comando_vlc(
        "volume",
        nuevo_volumen
    )

    return {

        "ok": True,

        "volumen": nuevo_volumen
    }


# =========================================================
# PAGINA WEB
# =========================================================

@app.get(
    "/",
    response_class=HTMLResponse
)
def pagina_principal():

    ruta_html = os.path.join(
        "static",
        "index.html"
    )

    with open(
        ruta_html,
        "r",
        encoding="utf-8"
    ) as archivo:

        return archivo.read()