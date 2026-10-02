#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <M5Unified.h>

// =====================================================
// WIFI
// =====================================================

const char* WIFI_SSID = "IZZI-C365";
const char* WIFI_PASSWORD = "cristovive";

const char* SERVER = "http://192.168.0.103:8000";

// =====================================================
// DATOS DE LA CANCION
// =====================================================

String artista = "Artista desconocido";
String titulo = "Sin titulo";
String album = "Sin album";

int duracion = 0;
int tiempo = 0;

// =====================================================
// ACTUALIZACION
// =====================================================

unsigned long ultimaConsulta = 0;
unsigned long ultimoScroll = 0;
unsigned long ultimoTiempo = 0;

const unsigned long INTERVALO_DATOS = 1000;
const unsigned long INTERVALO_SCROLL = 80;

// =====================================================
// SCROLL
// =====================================================

int posicionArtista = 0;
int posicionTitulo = 0;
int posicionAlbum = 0;

bool artistaScroll = false;
bool tituloScroll = false;
bool albumScroll = false;

const int VELOCIDAD_SCROLL = 1;

// =====================================================
// CAMBIO DE CANCION
// =====================================================

bool cambioCancion = true;

// =====================================================
// FORMATO DE TIEMPO
// =====================================================

String formatoTiempo(int segundos) {
    int minutos = segundos / 60;
    int segundosRestantes = segundos % 60;

    char buffer[10];
    sprintf(buffer, "%02d:%02d", minutos, segundosRestantes);

    return String(buffer);
}

// =====================================================
// QUITAR TILDES Y CARACTERES ESPECIALES
// =====================================================

String quitarTildes(String texto) {
    String resultado = "";

    for (int i = 0; i < texto.length(); i++) {
        unsigned char c = texto[i];

        if (c < 128) {
            resultado += (char)c;
            continue;
        }

        if (i + 1 < texto.length()) {
            unsigned char c2 = texto[i + 1];

            if (c == 0xC3) {
                if (c2 == 0xA1) { resultado += "a"; i++; continue; }
                if (c2 == 0xA9) { resultado += "e"; i++; continue; }
                if (c2 == 0xAD) { resultado += "i"; i++; continue; }
                if (c2 == 0xB3) { resultado += "o"; i++; continue; }
                if (c2 == 0xBA) { resultado += "u"; i++; continue; }

                if (c2 == 0x81) { resultado += "A"; i++; continue; }
                if (c2 == 0x89) { resultado += "E"; i++; continue; }
                if (c2 == 0x8D) { resultado += "I"; i++; continue; }
                if (c2 == 0x93) { resultado += "O"; i++; continue; }
                if (c2 == 0x9A) { resultado += "U"; i++; continue; }

                if (c2 == 0xB1) { resultado += "n"; i++; continue; }
                if (c2 == 0x91) { resultado += "N"; i++; continue; }

                if (c2 == 0xBC) { resultado += "u"; i++; continue; }
                if (c2 == 0x9C) { resultado += "U"; i++; continue; }
            }
        }
    }

    return resultado;
}

// =====================================================
// CONECTAR WIFI
// =====================================================

void conectarWiFi() {
    Serial.println();
    Serial.println("Conectando a WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi conectado");
    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());
}

// =====================================================
// CONTROL DE REPRODUCCION (POST)
// =====================================================

void enviarComandoControl(const char* endpoint) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = String(SERVER) + endpoint;

    http.begin(url);
    // Realizar la petición POST enviando un body vacío (o cabeceras si tu server lo requiere)
    int httpCode = http.POST("");

    Serial.print("POST ");
    Serial.print(endpoint);
    Serial.print(" -> HTTP: ");
    Serial.println(httpCode);

    http.end();

    // Consultar inmediatamente el estado para actualizar la pantalla sin esperar el intervalo
    obtenerCancion();
    dibujarPantallaCompleta();
}

// =====================================================
// OBTENER CANCION
// =====================================================

void obtenerCancion() {
    HTTPClient http;
    String url = String(SERVER) + "/now-playing";

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String response = http.getString();
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, response);

        if (!error) {
            String nuevoArtista = doc["artista"] | "Artista desconocido";
            String nuevoTitulo  = doc["titulo"]  | "Sin titulo";
            String nuevoAlbum   = doc["album"]   | "Sin album";

            int nuevaDuracion   = doc["duracion"] | 0;
            int nuevoTiempo     = doc["tiempo"]   | 0;

            nuevoArtista = quitarTildes(nuevoArtista);
            nuevoTitulo  = quitarTildes(nuevoTitulo);
            nuevoAlbum   = quitarTildes(nuevoAlbum);

            if (nuevoArtista != artista || nuevoTitulo != titulo || nuevoAlbum != album) {
                cambioCancion = true;
                posicionArtista = 0;
                posicionTitulo = 0;
                posicionAlbum = 0;
            }

            artista = nuevoArtista;
            titulo = nuevoTitulo;
            album = nuevoAlbum;
            duracion = nuevaDuracion;
            tiempo = nuevoTiempo;
        } else {
            Serial.print("Error JSON: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.println("No se pudieron obtener los datos");
    }

    http.end();
}

// =====================================================
// COMPROBAR SI NECESITA SCROLL
// =====================================================

void comprobarScroll() {
    M5.Display.setTextSize(2);

    int anchoPantalla = M5.Display.width();
    int anchoArtista  = M5.Display.textWidth(artista);
    int anchoTitulo   = M5.Display.textWidth(titulo);
    int anchoAlbum    = M5.Display.textWidth(album);

    artistaScroll = anchoArtista > anchoPantalla;
    tituloScroll  = anchoTitulo > anchoPantalla;
    albumScroll   = anchoAlbum > anchoPantalla;
}

// =====================================================
// DIBUJAR TEXTO
// =====================================================

void dibujarTexto(String texto, int y, int& posicion, bool necesitaScroll) {
    int anchoPantalla = M5.Display.width();

    M5.Display.fillRect(0, y - 10, anchoPantalla, 20, TFT_BLACK);

    if (!necesitaScroll) {
        M5.Display.setTextDatum(middle_center);
        M5.Display.drawString(texto, anchoPantalla / 2, y);
        return;
    }

    String textoScroll = "    " + texto + "    ";
    int anchoTexto = M5.Display.textWidth(textoScroll);

    posicion -= VELOCIDAD_SCROLL;

    if (posicion < -anchoTexto) {
        posicion = anchoPantalla;
    }

    M5.Display.setTextDatum(middle_left);
    M5.Display.drawString(textoScroll, posicion, y);
}

// =====================================================
// DIBUJAR TIEMPO
// =====================================================

void dibujarTiempo() {
    int anchoPantalla = M5.Display.width();
    int altoPantalla  = M5.Display.height();

    M5.Display.fillRect(0, altoPantalla - 20, anchoPantalla, 20, TFT_BLACK);

    String textoTiempo = formatoTiempo(tiempo) + " / " + formatoTiempo(duracion);

    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(textoTiempo, anchoPantalla / 2, altoPantalla - 10);
}

// =====================================================
// DIBUJAR PANTALLA COMPLETA
// =====================================================

void dibujarPantallaCompleta() {
    M5.Display.fillScreen(TFT_BLACK);
    comprobarScroll();

    dibujarTexto(artista, 14, posicionArtista, artistaScroll);
    dibujarTexto(titulo, 34, posicionTitulo, tituloScroll);
    dibujarTexto(album, 54, posicionAlbum, albumScroll);

    dibujarTiempo();
}

// =====================================================
// SETUP
// =====================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==========================\n MUSIC STICK ESP32 \n==========================");

    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(2);
    M5.Display.fillScreen(TFT_BLACK);

    conectarWiFi();
    obtenerCancion();
    dibujarPantallaCompleta();

    cambioCancion = false;
    ultimaConsulta = millis();
    ultimoScroll = millis();
    ultimoTiempo = millis();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
    M5.update();

    // =================================================
    // ACCIONES DE LOS BOTONES
    // =================================================

    // Botón A: Central (debajo de la pantalla) -> Play/Pausa
    if (M5.BtnA.wasPressed()) {
        Serial.println("Boton A presionado: Play / Pausa");
        enviarComandoControl("/control/play-pause");
    }

    // Botón B: Lateral derecho -> Siguiente canción
    if (M5.BtnB.wasPressed()) {
        Serial.println("Boton B presionado: Siguiente cancion");
        enviarComandoControl("/control/next");
    }

    // =================================================
    // COMPROBAR WIFI
    // =================================================

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi desconectado");
        WiFi.reconnect();
        delay(3000);
        return;
    }

    // =================================================
    // CONSULTAR SERVIDOR
    // =================================================

    if (millis() - ultimaConsulta >= INTERVALO_DATOS) {
        ultimaConsulta = millis();
        obtenerCancion();

        if (cambioCancion) {
            dibujarPantallaCompleta();
            cambioCancion = false;
        }
    }

    // =================================================
    // ACTUALIZAR TIEMPO
    // =================================================

    if (millis() - ultimoTiempo >= 1000) {
        ultimoTiempo = millis();
        dibujarTiempo();
    }

    // =================================================
    // SCROLL
    // =================================================

    if (millis() - ultimoScroll >= INTERVALO_SCROLL) {
        ultimoScroll = millis();

        if (artistaScroll) {
            dibujarTexto(artista, 14, posicionArtista, artistaScroll);
        }
        if (tituloScroll) {
            dibujarTexto(titulo, 34, posicionTitulo, tituloScroll);
        }
        if (albumScroll) {
            dibujarTexto(album, 54, posicionAlbum, albumScroll);
        }
    }

    delay(5);
}