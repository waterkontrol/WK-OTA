#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

// CONFIGURACIÓN WI-FI
const char* ssid = "Flia. Ramirez";
const char* password = "M&M1920*";

// ===== GITHUB PAGES =====
String versionURL  = "https://waterkontrol.github.io/WK-OTA/version.txt";
String firmwareURL = "https://waterkontrol.github.io/WK-OTA/PRUEBA.ino.esp32.bin";

Preferences preferencias;
String versionActual;
String macAddress;
unsigned long ultimaVerificacion = 0;
const unsigned long intervalo = 5000;

void setup() {
  Serial.begin(115200);
  
  // ===== INICIAR PREFERENCES =====
  preferencias.begin("ota", false);
  
  // ===== LEER VERSIÓN GUARDADA =====
  versionActual = preferencias.getString("version", "1.0");
  
  macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  macAddress.toLowerCase();
  
  Serial.print("Conectando WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectado");
  Serial.println("Versión actual: " + versionActual);
}

void loop() {
  if (millis() - ultimaVerificacion >= intervalo) {
    ultimaVerificacion = millis();
    verificarActualizacion();
    Serial.println("hola");
 }
}

void verificarActualizacion() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String urlCompleta = versionURL + "?nocache=" + String(random(1000000, 9999999));
  
  HTTPClient http;
  http.begin(urlCompleta);
  http.setTimeout(3000);
  http.addHeader("Cache-Control", "no-cache");
  
  int codigo = http.GET();
  
  if (codigo == 200) {
    String payload = http.getString();
    payload.trim();
    
    int idx = payload.indexOf("version=");
    if (idx != -1) {
      String v = payload.substring(idx + 8);
      v = v.substring(0, v.indexOf('\n'));
      v.trim();
      
      if (v != versionActual && v.length() > 0) {
        Serial.println();
        Serial.println("🚀 ACTUALIZACIÓN: " + versionActual + " → " + v);
        Serial.println("⬇️ Descargando firmware...");
        realizarOTA(firmwareURL, v);
      } else {
        Serial.println("✓ Versión " + versionActual + " OK");
      }
    }
  }
  http.end();
}

void realizarOTA(String url, String nuevaVersion) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Cache-Control", "no-cache");
  http.setTimeout(10000);
  
  int codigo = http.GET();
  
  if (codigo == 200) {
    int tamano = http.getSize();
    Serial.println("📦 Tamaño: " + String(tamano / 1024) + " KB");
    
    if (tamano > 0 && Update.begin(tamano)) {
      WiFiClient* cliente = http.getStreamPtr();
      size_t escrito = Update.writeStream(*cliente);
      
      if (escrito == tamano && Update.end()) {
        Serial.println("✅ Actualización OK");
        
        // ===== GUARDAR VERSIÓN =====
        preferencias.putString("version", nuevaVersion);
        Serial.println("✅ Versión guardada: " + nuevaVersion);
        
        Serial.println("🔄 Reiniciando...");
        delay(2000);
        ESP.restart();
      }
    }
  }
  http.end();
}
