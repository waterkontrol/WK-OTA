#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

// CONFIGURACIÓN WI-FI
const char* ssid = "Flia. Ramirez";
const char* password = "M&M1920*";

// ===== GITHUB PAGES - ACTUALIZACIÓN INSTANTÁNEA =====
String versionURL    = "https://waterkontrol.github.io/WK-OTA/version.txt";
String firmwareURL   = "https://waterkontrol.github.io/WK-OTA/PRUEBA.ino.esp32.bin";                     
String versionActual = "1.0";

String macAddress;

void setup() {
  Serial.begin(115200);
  
  macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  macAddress.toLowerCase();
  Serial.println("MAC: " + macAddress);
  
  Serial.print("Conectando WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectado");
  Serial.println("🚀 GitHub Pages activado - Versión actual: " + versionActual);
}

void loop() {
  verificarActualizacion();
  delay(5000);

  Serial.println("prueba1");
}

void verificarActualizacion() {
  if (WiFi.status() == WL_CONNECTED) {
    
    String urlCompleta = versionURL + "?nocache=" + String(random(1000000, 9999999));
    
    HTTPClient http;
    http.begin(urlCompleta);
    http.setTimeout(3000);
    http.addHeader("Cache-Control", "no-cache");
    
    Serial.println("\n=== VERIFICANDO GITHUB PAGES ===");
    int codigo = http.GET();
    
    if (codigo == 200) {
      String payload = http.getString();
      payload.trim();
      
      Serial.println("✅ CONTENIDO DEL TXT:");
      Serial.println("══════════════════════");
      Serial.println(payload);
      Serial.println("══════════════════════");
      
      if (payload.indexOf("version=") >= 0) {
        String v = payload.substring(payload.indexOf("version=") + 8);
        v = v.substring(0, v.indexOf('\n'));
        v.trim();
        
        Serial.println("Versión remota : " + v);
        Serial.println("Versión actual  : " + versionActual);
        
        if (v == versionActual) {
          Serial.println("✓ Ya tienes la última versión");
        } else {
          Serial.println("🚀 ¡NUEVA VERSIÓN DISPONIBLE!");
          Serial.println("⬇️ Descargando firmware...");
          realizarOTA(firmwareURL);
        }
      }
    } else {
      Serial.print("❌ Error HTTP: ");
      Serial.println(codigo);
    }
    
    http.end();
  }
}

void realizarOTA(String url) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Cache-Control", "no-cache");
  http.setTimeout(10000);
  
  int codigo = http.GET();
  
  if (codigo == 200) {
    int tamano = http.getSize();
    Serial.print("📦 Tamaño del firmware: ");
    Serial.print(tamano / 1024);
    Serial.println(" KB");
    
    if (tamano > 0) {
      if (Update.begin(tamano)) {
        WiFiClient* cliente = http.getStreamPtr();
        size_t escrito = Update.writeStream(*cliente);
        
        if (escrito == tamano) {
          Serial.println("✅ ¡Actualización completada!");
          if (Update.end()) {
            Serial.println("🔄 Reiniciando en 3 segundos...");
            delay(3000);
            ESP.restart();
          }
        } else {
          Serial.println("❌ Error: tamaño escrito no coincide");
        }
      } else {
        Serial.println("❌ Error al iniciar Update");
      }
    }
  } else {
    Serial.print("❌ Error HTTP en descarga: ");
    Serial.println(codigo);
  }
  http.end();
}
