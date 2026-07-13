/*
 * TOPIC : mk-210/VBNI/E86BEADEED74/in
 * MAC   : E86BEADEED74
 * 
 * codigos:
 * 
 * horario_ingreso      a320
 * verifica ingreso     a321
 * verifica nivel       a322
 * endpoint cuando se va el agua para cerrar la valvula automaticamente a323
 * MQTT HORARIOS        a323             
 * PULSADOR             a324
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <Adafruit_ADS1X15.h>

const char* mqtt_server    = "broker.emqx.io";
const int   mqtt_port      = 1883;
const char* mqtt_user      = "";
const char* mqtt_pass      = "";
const char* client_id      = "ESP32_Client";

//const char* mqtt_server    = "q286b00e.ala.us-east-1.emqxsl.com";
//const int   mqtt_port      = 8883;
//const char* mqtt_user      = "iny";       
//const char* mqtt_pass      = "14892423**";

WebServer server(80);
Preferences preferences;
Adafruit_ADS1115 ads;
//WiFiClientSecure secureClient;
//PubSubClient client(secureClient);

WiFiClient espClient;
PubSubClient client(espClient);

// Configuración del Access Point
const char* apSSID      = "WK-208_VBNI";
const char* apPassword  = "12345678";

#define SDA_PIN 27
#define SCL_PIN 33

//*********************************************************
//#define SIMULACION_SERIAL
#define medir_flujo

//********************************************************

int seriesTypeId         = 1 ;      // SERIES TYPE
float nivel_deseado      = 25;      // NIVEL DEL TANQUE EN CM
const int UMBRAL_CAMBIO  = 3;       // % DE CAMBIO DE NIVEL
const int umbral_presion = 3;       // % DE CAMBIO DE PRESION
const int t_caudal       = 20000;   // TIEMPO PARA VERIFICAR CAUDAL EN mS, SON 3 MEDICIONES DE 20 SEG
                                    // para un total de 1 min.

int   nivel_superior     = 80;
int   nivel_inferior     = 50;
int   nivel_critico      = 10;
int   presion_actual     = 0;

bool adsDisponible       = false;
bool modo_automatico     = false;
bool modo_ingreso        = false;
bool modo_nivel          = false;
bool horario             = false;
bool apModeStarted          = false;
bool wifiTried              = false;
bool wifiConnected          = false;
bool tryingToConnectMQTT    = false;
bool ingreso_aux            = 0;
bool ingreso_aux1           = false;
bool ingreso                = 0;
bool ingreso_confirmado     = false;
bool h_inicio               = "";
bool h_fin                  = ""; 
bool bomba                  = 0;
bool valvula                = 0;
bool flag_superior_activado = false;
bool flag_inferior_activado = false;
bool actualizando           = false;
bool lastButtonState        = false;  // estado anterior del botón

byte  a             = 0; 
byte operacion      = 0;
byte operacion1     = 0; 

//*********************************************************

#ifdef medir_flujo  
    unsigned long timer_caudalimetro  = 0;
    const int pin_caudal              = 32; // pin del caudalimetro
    bool primerPaso                   = true;
#endif


volatile unsigned long pulsos         = 0;  // <-- IMPORTANTE: volatile 
volatile unsigned long pulsos_caudal  = 0;

unsigned long pressStartTime              = 0;
const unsigned long holdDuration          = 5000;
const unsigned long WIFI_CONNECT_TIMEOUT  = 10000; // 10 segundos
unsigned long ignoreButtonUntil           = 0;


String versionURL  = "https://waterkontrol.github.io/WK-OTA/version.txt";
String firmwareURL = "https://raw.githubusercontent.com/waterkontrol/WK-OTA/refs/heads/main/produccion.ino.esp32.bin";

String   ssid            = "";
String   password        = "";
String   title           = "";
String   userId          = "";

String versionActual  = "";
String topic          = "";
String topic_url      = "";
String topicIn        = "";
String topicOut       = "";
String bombaStr       = "";
String ingresoStr     = "";
String valvulaStr     = "";
String bomba1         = "";    
String valvula1       = "";
String v = "";
String t = "";
String m = "";
String macAddress;

float nivel_cm           = 0;
float nivel_mqtt         = 0;
float voltage_presion    = 0;
float current_mA_presion = 0;

const int led_error      = 2;
const int buttonPin      = 5;
const int rele_esp       = 16;
const int led_wifi       = 19;
const int led_com        = 18;
const int rele_valvula   = 26;
int   nivel_actual       = 0;
int   nivel              = 0;
int ultimo_nivel_enviado = -1;   // -1 para forzar primer envío
int16_t raw1 = 0;
int j = 0;


const unsigned long interval        = 1000;  // 1 segundos
const unsigned long intervalo = 5000;
unsigned long ultimaVerificacion = 0;
unsigned long pressTime             = 0;
unsigned long tiempoAnterior        = 0; 
unsigned long previousMillis        = 0; 

static unsigned long ultimoPing100  = 0; // Control de frecuencia
static byte contador_minutos        = 0;
static bool ultimo_estado           = false; 


#ifdef medir_flujo  
void pulseInterrupt(){
  pulsos++;  // Incrementar el contador de pulsos en el minuto actual
  pulsos_caudal++;
}
#endif

void realizarOTA(String url, String nuevaVersion) {
  //Serial.println("si entra");
  HTTPClient http;
  http.begin(url);
  http.addHeader("Cache-Control", "no-cache");
  http.setTimeout(60000);
  
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
        versionActual = nuevaVersion;
        preferences.begin("ota", false);
          preferences.putString("nueva_version"   , nuevaVersion); 
          preferences.putString("version_actual"  , versionActual);// ✅ GUARDA 1.7 EN "wifi"
        preferences.end();

        
        Serial.println("✅ Versión guardada: " + nuevaVersion);
        
        Serial.println("🔄 Reiniciando...");
        delay(2000);
        ESP.restart();
      }
    }
  }
  http.end();
}

#ifdef SIMULACION_SERIAL
void procesarSerial() {
  if (Serial.available() > 0) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    //Serial.print("el comando es: ");
    //Serial.println(comando);
    //Serial.println(nivel_actual);

   if(comando == "a") {  // "a" es un String literal
        nivel_actual = nivel_actual + 5;
        Serial.println(nivel_actual);
      }

   if(comando == "b") {  // "b" es un String literal
       nivel_actual = nivel_actual - 5;
       Serial.println(nivel_actual);
     }

     if(comando == "c") { // "c" es un String literal
       modo_ingreso = true;
       Serial.println("modo ingreso activo");
     }

     if(comando == "d") { // "c" es un String literal
       modo_ingreso = false;
       Serial.println("modo ingreso inactivo");
     }
  }
}
#endif

bool evaluar_ingreso() {
  
  if (pulsos > 1) {
    //Serial.println("pulsos mayor a 1");
    
    // Solo incrementar si AÚN NO hemos confirmado ingreso
    if (!ingreso_confirmado) {
      contador_minutos++; 
      //Serial.print("contador: ");
      //Serial.println(contador_minutos);
      
      // Si ya pasaron 3 períodos, confirmamos el ingreso
      if (contador_minutos >= 3) {
        ingreso_confirmado = true;  // ¡Ya confirmamos! Deja de incrementar
        contador_minutos = 3;       // Opcional: fijarlo en 3 como referencia
        return true;  // ¡HAY INGRESO!
      }
    } else {
      // Ya estaba confirmado, solo retornamos true sin incrementar
      //Serial.println("ingreso ya confirmado - manteniendo true");
      return true;
    }
  } 
  else {
    // NO hay pulsos - RESETEAR TODO
    contador_minutos = 0;
    ingreso_confirmado = false;  // Importante: resetear la confirmación
    //Serial.println("sin pulsos - reset completo");
    return false;
  }
  
  // Hay pulsos pero aún no pasaron 3 períodos
  return false;
}

int sensor_presion(){  
  raw1       = ads.readADC_SingleEnded(1);
  voltage_presion    = raw1 * 0.1875 / 1000.0;
  current_mA_presion = (voltage_presion / 150.0) * 1000.0;
  float presion_psi  = (current_mA_presion - 4.0) * 9.375;
  
  return (int)presion_psi;  // Trunca la parte decimal
}

void v_abierta_b_apagada() { 
      
  //Serial.println("valvula abierta, bomba apagada");
  digitalWrite(rele_esp, HIGH);        // RELE DE BOMBA
  digitalWrite(rele_valvula, LOW);     // RELE VALVULA
  bomba   = 1;
  valvula = 0; 

  StaticJsonDocument<128> doc; 
  doc["bomba"]   = "encendida";
  doc["valvula"] = "cerrada";
            
  String jsonString1;
  serializeJson(doc, jsonString1);
          
       if (client.publish(topicOut.c_str(), jsonString1.c_str())) {
            //Serial.println("📤 JSON enviado: " + jsonString1);                    
          } else {
            //Serial.println("❌ Error enviando JSON por MQTT");
          }
      
      // GUARDAR los nuevos estados en preferencias
      preferences.begin("wifi", false);  // false = modo escritura
        preferences.putBool("valvula" , valvula);
        preferences.putBool("bomba"   , bomba);
      preferences.end();  
  
}

void v_cerrada_b_encendida(){
     //Serial.println("valvula cerrada, bomba encendida");
     digitalWrite(rele_esp, LOW);  // Por ejemplo, encender
     digitalWrite(rele_valvula, HIGH);    // Apagar otro
     bomba   = 0;
     valvula = 1; 

     StaticJsonDocument<128> doc; 
     doc["bomba"]   = "apagada";
     doc["valvula"] = "abierta";
          
     String jsonString2;
     serializeJson(doc, jsonString2);
          
          if (client.publish(topicOut.c_str(), jsonString2.c_str())) {
            //Serial.println("📤 JSON enviado: " + jsonString2);                    
          } else {
            //Serial.println("❌ Error enviando JSON por MQTT");
          }
      
      // GUARDAR los nuevos estados en preferencias
      preferences.begin("wifi", false);  // false = modo escritura
        preferences.putBool("valvula" , valvula);
        preferences.putBool("bomba"   , bomba);
      preferences.end();
}

void modo_por_ingreso(){  // a320
//Serial.println("si entra en modo por ingreso");

// ==================== RESET DE FLAGS ====================
// Cuando hay ingreso de agua y el nivel está en rango normal, resetear flags
if(ingreso_aux == false) {  // Hay ingreso de agua
  if(nivel_actual > nivel_inferior && nivel_actual < nivel_superior) {
    flag_inferior_activado = false;
    flag_superior_activado = false;    
    // Serial.println("✅ Flags reseteados - nivel normal");
  }
}
// ========================================================

if((ingreso_aux == false)||(horario == true)){ // ingresando agua de la calle
  //Serial.println("si hay ingreso");   

//********************* LIMITE SUPERIOR *****************************
  if(nivel_actual >= nivel_superior && !flag_superior_activado){
    flag_superior_activado = true;
    flag_inferior_activado = false;  // Resetear la otra bandera
  
    delay(100);
    digitalWrite(rele_valvula, HIGH);
    digitalWrite(rele_esp    , LOW);
    Serial.println("el nivel llego a su punto superior"); 
    valvula = 1;
    bomba   = 0;
    
    valvulaStr = valvula ? "abierta"   : "cerrada";
    bombaStr   = bomba   ? "encendida" : "apagada";   
    
    preferences.begin("wifi", false);
      preferences.putBool("valvula", valvula);
      preferences.putBool("bomba"  , bomba);     
    preferences.end();
         
    if (client.connected()) {                    
      StaticJsonDocument<64> doc;                    
      doc["valvula"] = valvulaStr; 
      doc["bomba"]   = bombaStr;                    
      char buffer[64];
      size_t len = serializeJson(doc, buffer);
      client.publish(topicOut.c_str(), buffer, len);            
    }
  }
//*********************************************************************
//********************** LIMITE INFERIOR ******************************

  if(nivel_actual <= nivel_inferior && !flag_inferior_activado){
    flag_inferior_activado = true;
    flag_superior_activado = false;  // Resetear la otra bandera

    /*Serial.println("el nivel bajo a su punto inferior"); 
    Serial.print("el valor de horario es: ");
    Serial.println(horario);*/ 

    if(horario == true){
       horario    = false;
       operacion  = 1;
       operacion1 = 1;
    } 
     
    delay(100);
    digitalWrite(rele_valvula, LOW);
    digitalWrite(rele_esp    , HIGH);    
    valvula = 0;
    bomba   = 1;
    
    valvulaStr = valvula ? "abierta"   : "cerrada";
    bombaStr   = bomba   ? "encendida" : "apagada";   

    preferences.begin("wifi", false);
      preferences.putBool("valvula", valvula);
      preferences.putBool("bomba"  , bomba);    
    preferences.end();

    if (client.connected()) {                    
      StaticJsonDocument<64> doc;                    
      doc["valvula"] = valvulaStr; 
      doc["bomba"]   = bombaStr;                    
      char buffer[64];
      size_t len = serializeJson(doc, buffer);
      client.publish(topicOut.c_str(), buffer, len);
    }
  }
//*****************************************************************
  operacion = 1;
}

//********************** END POINT PARA CUANDO SE VA EL AGUA  a323 ***********************
else if ( horario == false){
   
   if(((ingreso_aux == true) && (operacion == 1)) 
                                              || ((ingreso_aux == true) && (operacion1 == 0))) {
  delay(100);
  digitalWrite(rele_valvula , LOW);
  digitalWrite(rele_esp     , HIGH); 
  operacion  = 0;
  operacion1 = 1;
  Serial.println("no hay ingreso");
   
    valvula = 0;
    bomba   = 1;
    
    valvulaStr = valvula ? "abierta"   : "cerrada";
    bombaStr   = bomba   ? "encendida" : "apagada"; 
  
  preferences.begin("wifi", false);
    preferences.putBool("valvula", valvula);
    preferences.putBool("bomba"  , bomba);      
  preferences.end();

  flag_inferior_activado = false;
  flag_superior_activado = false;

  if (client.connected()) {                    
    StaticJsonDocument<64> doc;                    
    doc["valvula"] = valvulaStr;
    doc["bomba"]   = bombaStr;                    
    char buffer[64];
    size_t len = serializeJson(doc, buffer);
    client.publish(topicOut.c_str(), buffer, len);
   }
  }
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
    //Serial.print("EL CONTENIDO DEL TXT ES: ");
    //Serial.println(payload);
    
    int idx  = payload.indexOf("version=");
    int tipo = payload.indexOf("tipo=");
    int mac  = payload.indexOf("mac=");
      //Serial.println(idx);
      //Serial.println(tipo);
      //Serial.println(mac);

      if (idx != -1) {
      v = payload.substring(idx + 8);
      v = v.substring(0, v.indexOf('\n'));
      v.trim();
      //Serial.print("la version txt : ");
      //Serial.println(v);
      }

      if (tipo != -1) {
      t = payload.substring(tipo + 5);   // ✅ "tipo=" = 5 caracteres
      t = t.substring(0, t.indexOf('\n'));
      t.trim();
      //Serial.print("tipo:  ");
      //Serial.println(t);  // "masivo"
    }

      if (mac != -1) {
      m = payload.substring(mac + 4);    // ✅ "mac=" = 4 caracteres
      m = m.substring(0, m.indexOf('\n'));
      m.trim();
      m.toLowerCase();  // ✅ opcional
      //Serial.print("la mac es:  ");
      //Serial.println(m);  // "E86BEADEF480"
      }

      macAddress = WiFi.macAddress();
      macAddress.replace(":", "");
      macAddress.toLowerCase();
      //Serial.print("MAC dispositivo: ");
      //Serial.println(macAddress);  // 👈 DEBE MOSTRAR ALGO

      //Serial.print("VERSION ACTUAL ES: ");
      //Serial.println(versionActual);  // 👈 DEBE MOSTRAR ALGO

      if((v != versionActual) && (t == "masivo") && (m == macAddress)){
        //Serial.println("si entra en lo que hicimos");
        realizarOTA(firmwareURL, v);
      }
      else {
      //Serial.println("no coinciden con el txt");
      }
//================================  QUEDAMOS ACA =========================      
    
   /* if (idx != -1) {
      String v = payload.substring(idx + 8);
      v = v.substring(0, v.indexOf('\n'));
      v.trim();
      
      if (v != versionActual && v.length() > 0) {
        Serial.println();
        Serial.println("🚀 ACTUALIZACIÓN: " + versionActual + " → " + v);
        Serial.println("⬇️ Descargando firmware...");
        
        // ===== ACTIVAR BANDERA =====
        actualizando = true;
        
        // ===== EJECUTAR OTA =====
        realizarOTA(firmwareURL, v);
        
        // ===== SI FALLA, DESACTIVAR BANDERA =====
        actualizando = false;
        Serial.println("❌ OTA FALLÓ - Continuando...");
      }
    }*/
  }
  http.end();
}

int sonda_temperatura() {
  int16_t raw = ads.readADC_SingleEnded(2);
  float voltage = raw * 0.1875 / 1000.0;
  //Serial.println(voltage);

  // Calculamos corriente en mA
  float current_mA = voltage / 150.0 * 1000.0;

  // ✅ CONFIGURACIÓN PARA SONDA DE 5 METROS (500 cm)
  const float CORRIENTE_MIN = 4.0;     // 4 mA = 0 cm (vacío)
  const float CORRIENTE_MAX = 20.0;    // 20 mA = 500 cm (lleno)
  const float ALTURA_MAX_CM = 500;     // 5 metros = 500 cm
  
  // Calculamos porcentaje escalado
  float nivel_pct = (current_mA - CORRIENTE_MIN) / (CORRIENTE_MAX - CORRIENTE_MIN) * 100.0;  
  
  // Limitamos entre 0-100%
  nivel_pct = constrain(nivel_pct, 0.0, 100.0);
  
  // Convertimos a cm
        nivel_cm = nivel_pct / 100.0 * ALTURA_MAX_CM;
        nivel_mqtt = nivel_cm;
        //Serial.println("antes de dividir");
        //Serial.println(nivel_deseado);
        nivel_cm = (nivel_cm / nivel_deseado) * 100;
        //Serial.println(nivel_cm);
   
 return nivel_cm;// OJO ESTO RETORNA % NO CM
}

float sonda_nivel() {
  int16_t raw = ads.readADC_SingleEnded(2);
  float voltage = raw * 0.1875 / 1000.0;
  //Serial.println(voltage);

  // Calculamos corriente en mA
  float current_mA = voltage / 150.0 * 1000.0;

  // ✅ CONFIGURACIÓN PARA SONDA DE 5 METROS (500 cm)
  const float CORRIENTE_MIN = 4.0;     // 4 mA = 0 cm (vacío)
  const float CORRIENTE_MAX = 20.0;    // 20 mA = 500 cm (lleno)
  const float ALTURA_MAX_CM = 500;     // 5 metros = 500 cm
  
  // Calculamos porcentaje escalado
  float nivel_pct = (current_mA - CORRIENTE_MIN) / (CORRIENTE_MAX - CORRIENTE_MIN) * 100.0;  
  
  // Limitamos entre 0-100%
  nivel_pct = constrain(nivel_pct, 0.0, 100.0);
  
  // Convertimos a cm
        nivel_cm = nivel_pct / 100.0 * ALTURA_MAX_CM;
        nivel_mqtt = nivel_cm;
        //Serial.println("antes de dividir");
        //Serial.println(nivel_deseado);
        nivel_cm = (nivel_cm / nivel_deseado) * 100;
        //Serial.println(nivel_cm);
   
 return nivel_cm;// OJO ESTO RETORNA % NO CM
} 

bool sendPostRequest() {
  HTTPClient http;
  String url = "https://waterkontrolapp-production.up.railway.app/api/dispositivo/registro";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  Serial.println("Datos a enviar:");
  Serial.print("title: "); Serial.println(title);
  Serial.print("seriesTypeId: "); Serial.println(seriesTypeId);
  Serial.print("userId: "); Serial.println(userId);

  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");

  DynamicJsonDocument doc(256);
  doc["serial"]   = macAddress;
  doc["tipo"]     = seriesTypeId;
  doc["nombre"]   = title;
  doc["userId"]   = userId;

  String jsonBody;
  serializeJson(doc, jsonBody);
  //Serial.println("JSON enviado: " + jsonBody);   

  int httpResponseCode = http.POST(jsonBody);

  if (httpResponseCode > 0) {
    //Serial.print("📡 Código de respuesta HTTP: ");
    //Serial.println(httpResponseCode);

    String response = http.getString();
    Serial.println("Respuesta del servidor: " + response);
    server.send(httpResponseCode, "application/json", response);

    // SOLUCIÓN: Solo procesar si es una respuesta exitosa
    if (httpResponseCode == 200 || httpResponseCode == 201) {
      // Respuesta exitosa (dispositivo creado)
      StaticJsonDocument<512> docResp;
      DeserializationError error = deserializeJson(docResp, response);

      if (!error) {
        // Verificar que el campo "topic" existe y no es nulo
        if (docResp.containsKey("topic") && !docResp["topic"].isNull()) {
          const char* rawTopic = docResp["topic"];
          topic       = String(rawTopic);
          topic_url   = topic;
          topicOut    = topic + "/out";
          topicIn     = topic + "/in";  

          //Serial.print("📡 Topics del backend: ");
          //Serial.print("topicOut: "); Serial.print(topicOut);
          //Serial.print(", topicIn: "); Serial.println(topicIn);

          // Guardar en Preferences
          preferences.begin("wifi", false);
            preferences.putString("topic_url", topic_url);
            preferences.putString("topicOut", topicOut);
            preferences.putString("topicIn", topicIn);     
          preferences.end();

          //Serial.println("💾 Topics guardados en Preferences");
          
          http.end();
          return true;
        } else {
          //Serial.println("⚠️  Respuesta exitosa pero sin campo 'topic'");
        }
      } else {
        //Serial.print("❌ Error al parsear JSON: ");
        //Serial.println(error.c_str());
      }
    } 
    else if (httpResponseCode == 400 || httpResponseCode == 409) {
       server.send(httpResponseCode, "application/json", response);
      // SOLUCIÓN IMPORTANTE: Códigos 400/409 usualmente indican que el dispositivo ya existe
      // NO debemos guardar nada en Preferences aquí
      //Serial.println("⚠️  Dispositivo ya registrado o datos inválidos");
      //Serial.println("ℹ️  Manteniendo los valores actuales de Preferences");
    }
    else {
      // Otros códigos de error
      //Serial.print("⚠️  Error HTTP no manejado: ");
      //Serial.println(httpResponseCode);
    }

    http.end();
    return false;
  } else {
    //Serial.print("❌ Error en solicitud HTTP: ");
    //Serial.println(http.errorToString(httpResponseCode));
    http.end();
    return false;
  }
}

void handleAddDevice() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Falta cuerpo de la petición");
    return;
  }

  String body = server.arg("plain");
  //Serial.print(body);  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error) {
    server.send(400, "text/plain", "JSON inválido");
    return;
  }

  ssid           = doc["ssid"] |   "";
  password       = doc["pass"] |   "";
  title          = doc["titulo"] | "";
  userId         = doc["usr_id"] | "";

  Serial.println("\n📥 Datos recibidos vía POST /add-device:");
  Serial.print("SSID: ");          Serial.println(ssid);
  Serial.print("Password: ");      Serial.println(password);
  Serial.print("Title: ");         Serial.println(title);
  Serial.print("SeriesTypeId: ");  Serial.println(seriesTypeId);
  Serial.print("UserId: ");        Serial.println(userId);

  ssid.trim();
  password.trim();
  title.trim();
  delay(10);

  WiFi.mode(WIFI_AP_STA);
  delay(500);
  WiFi.begin(ssid.c_str(), password.c_str());

  //Serial.println("🔄 Verificando credenciales WiFi...");

  unsigned long startAttemptTime = millis();
  bool connected = false;

  while ((millis() - startAttemptTime) < WIFI_CONNECT_TIMEOUT) {
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(100);  // pequeña espera para no saturar el CPU
  }

  wifiTried = true;

  if (connected) {
    Serial.println("\n✅ Conectado a la red WiFi");    

    if (sendPostRequest()) {
      bomba    = 1;
      valvula  = 0; 
      preferences.begin("wifi", false);
          preferences.putString ("ssid"     , ssid);
          preferences.putString ("password" , password);
          preferences.putBool   ("bomba"    , bomba);
          preferences.putBool   ("valvula"  , valvula);
      preferences.end();

      Serial.println("💾 Credenciales guardadas");
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"REGISTRO EXITOSO\"}");
      digitalWrite(led_wifi  , HIGH);
      digitalWrite(led_error , LOW);
      title="";
      ssid="";
      password=""; 
      delay(1500);
      ESP.restart();

    } else {
      Serial.println("❌ El servidor no respondió correctamente. Cancelando.");
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"server error\"}");
      delay(500);
      //WiFi.disconnect(true);
      WiFi.mode(WIFI_AP);
      
    }

  } else {
    //Serial.println("\n❌ Falló la conexión al WiFi.");
    digitalWrite(led_error , HIGH);
    title    ="";
    ssid     ="";
    password =""; 
    delay(2000);
    digitalWrite(led_error , LOW);
    delay(1000);
    digitalWrite(led_error , HIGH);
    server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"CREDENCIALES WIFI INVALIDAS\"}");
    delay(500);
    //WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    
  }
}

void startAccessPoint() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);

  /*Serial.println("\n🚀 Modo Access Point ACTIVADO");
  Serial.print("Red: ");
  Serial.println(apSSID);
  Serial.print("IP del AP: ");
  Serial.println(WiFi.softAPIP());*/
  digitalWrite(led_com   ,HIGH);
  digitalWrite(led_wifi  ,HIGH);
  digitalWrite(led_error ,HIGH);
  

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "ESP32 en modo Access Point");
  });

  server.on("/device", HTTP_POST, handleAddDevice);
  //server.on("/add-device", HTTP_POST, handleAddDevice);
  server.begin();
  //Serial.println("🌐 Servidor web iniciado en modo AP");
}

void callbackMQTT(char* topic, byte* payload, unsigned int length) { // f-mqtt
  Serial.print("📩 Mensaje recibido en topic ");
  Serial.print(topic);
  Serial.println(": ");

  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println(message);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    //Serial.print("❌ Error al parsear JSON: ");
    //Serial.println(error.c_str());
    return;
  }
  
    if (doc.containsKey("actualizar")) {    
      int actualizar = doc["actualizar"]; 
         
      if (actualizar == 1){ 
        StaticJsonDocument<500> doc; 
        bombaStr   = bomba ? "encendida" : "apagada";
        valvulaStr = valvula ? "abierta" : "cerrada"; 
        ingresoStr = ingreso ? "si" : "no"; 
        
        doc["ingreso"         ] = ingresoStr;                
        doc["nivel"           ] = nivel_actual;
        doc["bomba"           ] = bombaStr;
        doc["valvula"         ] = valvulaStr;
        doc["modo_automatico" ] = modo_automatico; 
        doc["modo_ingreso"    ] = modo_ingreso;
        doc["modo_nivel"      ] = modo_nivel;
        //doc["version"         ] = versionActual; 
        //doc["presion_actual"  ] = presion_actual;
        //doc["nivel_deseado"   ] = nivel_deseado; 
        //doc["temperatura1"    ] = 25; 
        //doc["temperatura2"    ] = 18;           
        //doc["mA_presion"    ] = round(current_mA_presion * 10) / 10;            
                    
            String jsonString;
            serializeJson(doc, jsonString);
          
        if (client.publish(topicOut.c_str(), jsonString.c_str())) {
            //Serial.println(" actualizado " + jsonString);
          } else {
            //Serial.println("error en actualizar ingreso");
          }    
       
          }
    }

    if (doc.containsKey("tipo")) {
        String tipo = doc["tipo"].as<String>();
           h_inicio = doc["h_inicio"];        
           h_fin    = doc["h_fin"];            
           valvula1 = doc["valvula"].as<String>();  
           bomba1   = doc["bomba"].as<String>();             
  
      if (tipo == "horario") {    
    
        if (modo_automatico == true) {
          Serial.println("Modo automático - ejecutar acción por horario");
          
          if ((valvula1 == "abierta") && (bomba1 == "apagada")){
            //Serial.println("entro en valvula abierta bomba apagada");
            v_abierta_b_apagada();
          }
          
          if ((valvula1 == "cerrada") && (bomba1 == "encendida")) { 
            //Serial.println("entro en valvula cerrada bomba encendida");           
            v_cerrada_b_encendida();
          }       
      }
       
        else if (modo_ingreso == true) {
          Serial.println("Modo ingreso - ir a función de ingreso");
          horario = true;
          
          if(h_inicio == true){
            if(nivel_actual > nivel_inferior){
               v_cerrada_b_encendida(); 
               flag_inferior_activado = false;
               flag_superior_activado = false;
               operacion  = 0;
               operacion1 = 0;
            }               
          }

          if(h_fin == true){
             v_abierta_b_apagada();
             horario = false;
             flag_inferior_activado = false;
             flag_superior_activado = false;
             operacion  = 0;
             operacion1 = 0;   
  }
}
       
        else if (modo_nivel == true) {
          Serial.println("Modo nivel - ir a función de nivel");
          //funcionModoNivel();  // Llama a tu función que busca otras variables          
    }
  }
}

    else {
    
    if (doc.containsKey("bomba") && doc.containsKey("valvula")) {
      bomba1   = doc["bomba"].as<String>();        
      valvula1 = doc["valvula"].as<String>();

    if ((valvula1 == "abierta") && (bomba1 == "apagada")){
      //Serial.println("valvula abierta, bomba apagada")
        v_abierta_b_apagada();
    }
    
    if ((valvula1 == "cerrada") && (bomba1 == "encendida")){    
      //Serial.println("valvula cerrada, bomba encendida");      
        v_cerrada_b_encendida();     
   }
  }
 }

    if (doc.containsKey("admin_get_nivel")) {        
      String respuesta = "{\"nivel_cm\":" + String(nivel_mqtt) + "}";    
        client.publish(topicOut.c_str(), respuesta.c_str());
    
    // Para debug
    //Serial.print("Nivel enviado: ");
    //Serial.print(nivel_mqtt);
    //Serial.println(" cm");
}

    if (doc.containsKey("admin_set_altura_tanque")) {
     nivel_deseado = doc["admin_set_altura_tanque"].as<float>();
      delay(100);
      preferences.begin("wifi", false);  // false = modo escritura
        preferences.putFloat("nivel_deseado", nivel_deseado );        
      preferences.end();
     //Serial.print("nivel deseado en cm actualizado :");
     //Serial.print(nivel_deseado);    
  }

    if (doc.containsKey("admin_set_nivel_superior")) {
     nivel_superior = doc["admin_set_nivel_superior"].as<int>();
      delay(100);
      preferences.begin("wifi", false);  // false = modo escritura
        preferences.putInt("nivel_superior", nivel_superior );        
      preferences.end();
     //Serial.print("limite superior es :");
     //Serial.print(nivel_superior);    
  }

    if (doc.containsKey("admin_set_nivel_inferior")) {
     nivel_inferior = doc["admin_set_nivel_inferior"].as<int>();
      delay(100);
      preferences.begin("wifi", false);  // false = modo escritura
        preferences.putInt("nivel_inferior", nivel_inferior );        
      preferences.end();
     //Serial.print("nivel inferior es :");
     //Serial.print(nivel_inferior);    
  }

    if (doc.containsKey("admin_limites_tanque")) {
    // Crear documento JSON para la respuesta
      DynamicJsonDocument respuestaDoc(256);
        respuestaDoc["nivel_superior"] = nivel_superior;
        respuestaDoc["nivel_inferior"] = nivel_inferior;
    
    // Serializar a String
    String respuesta;
    serializeJson(respuestaDoc, respuesta);
    
    client.publish(topicOut.c_str(), respuesta.c_str());
}

    if (doc.containsKey("admin_version")) {        
      String respuesta = "{\"version\":" + String(versionActual) + "}";    
        client.publish(topicOut.c_str(), respuesta.c_str());    
}

//======================== MODOS DE OPERACION =========================== a323

    if (doc.containsKey("modo_automatico")&& doc["modo_automatico"] == true) {
      modo_automatico = true;
      modo_ingreso    = false;
      modo_nivel      = false;

      String respuesta = "{\"modo_automatico\":\"ok\"}";    
      client.publish(topicOut.c_str(), respuesta.c_str()); 
      
      Serial.print("modo_automatico activo :");
      Serial.print(modo_automatico);    
      preferences.begin("wifi", false);
        preferences.putBool("modo_automatico" , modo_automatico);
        preferences.putBool("modo_ingreso"    , modo_ingreso);
        preferences.putBool("modo_nivel"      , modo_nivel);        
      preferences.end();
      
}

    if (doc.containsKey("modo_ingreso")&& doc["modo_ingreso"] == true) {
      modo_automatico = false;
      modo_ingreso    = true;
      modo_nivel      = false;
      Serial.print("modo_ingreso activo :");
      Serial.print(modo_ingreso); 

      String respuesta = "{\"modo_ingreso\":\"ok\"}";    
      client.publish(topicOut.c_str(), respuesta.c_str()); 
      
      preferences.begin("wifi", false);  // false = modo escritura
        preferences.putBool("modo_automatico" , modo_automatico);
        preferences.putBool("modo_ingreso"    , modo_ingreso);
        preferences.putBool("modo_nivel"      , modo_nivel);    
      preferences.end();
}

    if (doc.containsKey("modo_nivel")&& doc["modo_nivel"] == true) {
      modo_automatico = false;
      modo_ingreso    = false;
      modo_nivel      = true;

      String respuesta = "{\"modo_nivel\":\"ok\"}";    
      client.publish(topicOut.c_str(), respuesta.c_str()); 
      
      Serial.print("modo_nivel activo :");
      Serial.print(modo_nivel); 
      preferences.begin("wifi", false);  // false = modo escritura
        preferences.putBool("modo_automatico" , modo_automatico);
        preferences.putBool("modo_ingreso"    , modo_ingreso);
        preferences.putBool("modo_nivel"      , modo_nivel);      
      preferences.end();
}

//===================================================================
//                   RESPUESTA PARA NIVEL SUPERIOR E INFERIOR
//===================================================================

 if (doc.containsKey("valormin") && doc.containsKey("valormax")) {
    Serial.println("si entra en niveles");
    int valormin = doc["valormin"];
    int valormax = doc["valormax"];

    Serial.print("el valor min es:");
    Serial.println(valormin);
    Serial.print("el valor max es:");
    Serial.println(valormax);

    String respuesta = "{\"valormin\":\"ok\", \"valormax\":\"ok\"}";
    client.publish(topicOut.c_str(), respuesta.c_str()); 
    
}

//===================================================================
//          RESPUESTA PARA TEMPERATURA SUPERIOR E INFERIOR
//===================================================================

if (doc.containsKey("tempmin") && doc.containsKey("tempmax")) {
    Serial.println("si entra en temp");
    int tempmin = doc["tempmin"];
    int tempmax = doc["tempmax"];

    Serial.print("el valor min de temp es:");
    Serial.println(tempmin);
    Serial.print("el valor max de temp es:");
    Serial.println(tempmax);

    String respuesta = "{\"tempmin\":\"ok\", \"tempmax\":\"ok\"}";
    client.publish(topicOut.c_str(), respuesta.c_str()); 
    
}

//===================================================================

    if(message == "admin_reset"){
      ESP.restart();
  } 
  
  delay(100);
}

void reconnectMQTT() {
  // Solo este cambio:
  client.setSocketTimeout(2000); // ⭐ AÑADE ESTA LÍNEA
  
  String macAddress = WiFi.macAddress(); 

  if (client.connect(macAddress.c_str(), mqtt_user, mqtt_pass)) {   
    digitalWrite(led_com, HIGH);
    
    if (topicIn != "") {
      client.subscribe(topicIn.c_str());      
    }
  } else {
    digitalWrite(led_com, LOW);
    // ⭐ ELIMINA CUALQUIER delay() QUE HAYA AQUÍ ⭐
  }
}

void setup() { // f-setup
  Serial.begin(115200);
  delay(500);

  pinMode(pin_caudal   , INPUT);
  pinMode(buttonPin    , INPUT_PULLUP);
  pinMode(led_error    , OUTPUT);
  pinMode(led_com      , OUTPUT);
  pinMode(led_wifi     , OUTPUT);
  pinMode(rele_esp     , OUTPUT);
  pinMode(rele_valvula , OUTPUT);

  preferences.begin("ota", false);
      versionActual = preferences.getString("version_actual", "1.0");
  preferences.end();

  // Recuperar credenciales y topics de Preferences
  preferences.begin("wifi", true);
      ssid            = preferences.getString ("ssid"            , "");
      password        = preferences.getString ("password"        , "");
      topicOut        = preferences.getString ("topicOut"        , "");
      topicIn         = preferences.getString ("topicIn"         , "");
      topic_url       = preferences.getString ("topic_url"       , "");
      valvula         = preferences.getBool   ("valvula"         , false);
      bomba           = preferences.getBool   ("bomba"           , true);
      nivel_deseado   = preferences.getFloat  ("nivel_deseado"   , 500.0); 
      nivel_superior  = preferences.getInt    ("nivel_superior"  , 80);
      nivel_inferior  = preferences.getInt    ("nivel_inferior"  , 50); 
      modo_automatico = preferences.getBool   ("modo_automatico" , false);
      modo_ingreso    = preferences.getBool   ("modo_ingreso"    , false);
      modo_nivel      = preferences.getBool   ("modo_nivel"      , false);    
  preferences.end();

  //Serial.print   ("ssid:");
  //Serial.println (ssid);
  //Serial.print   ("pass:");  
  //Serial.println (password);
  //Serial.print   ("version actual:");  
  //Serial.println (versionActual);
  //Serial.print   ("nivel_superior es :");  
  //Serial.println (nivel_superior);
  //Serial.print   ("nivel_inferior es :");  
  //Serial.println (nivel_inferior);
  Serial.print("el topic es:");
  Serial.println(topicOut);
  Serial.println(topicIn);

  //ssid     = "Flia. Ramirez";  
  //password = "M&M1920*";

  if ((bomba == 0 && valvula == 0) || (bomba == 1 && valvula == 1)) { 
      bomba   = 0;
      valvula = 1;
      Serial.println("variables iguales");
  }

  if (bomba == 0 && valvula == 1) {        
     digitalWrite(rele_valvula , HIGH);
     digitalWrite(rele_esp     , LOW);            
  } else {        
     digitalWrite(rele_valvula , LOW);
     digitalWrite(rele_esp     , HIGH);            
   }  

  /*Serial.print("el topic es:");
  Serial.println(topicOut);
  Serial.println(topicIn);
  Serial.print("el valor de  la valvula es:");
  Serial.println(valvula);
  Serial.print("el valor de  la bomba es:");
  Serial.println(bomba);*/

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  
   if (!ads.begin()) {
    Serial.println("ADVERTENCIA: ADS1115 no detectado. Continuando sin él.");
    adsDisponible = false;
    // ¡NO hay while(1); aquí!
  } else {
    Serial.println("ADS1115 detectado correctamente.");
    adsDisponible = true;
    // Configuración adicional del ADS si es necesario
    ads.setGain(GAIN_TWOTHIRDS);
  }

  if (ssid != "" && password != "") {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.println("🔌 Intentando conectar con credenciales guardadas...");

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("\n✅ Conectado a WiFi");
      digitalWrite(led_wifi  ,HIGH);
      digitalWrite(led_error ,LOW);
      digitalWrite(led_com   ,LOW);
      //Serial.print("IP: ");
      //Serial.println(WiFi.localIP());

      //secureClient.setInsecure();
      client.setServer(mqtt_server, mqtt_port);
      client.setCallback(callbackMQTT);      
      

      // Publicar mensaje inicial en topicOut si está definido
      if (topicOut != "") {        
        delay(500);              
        //Serial.println("📤 Mensaje inicial publicado en topicOut");
        //Serial.println("📂 TopicOut: " + topicOut);
      }

    } else {
      //Serial.println("\n⚠️ No se pudo conectar a WiFi con credenciales guardadas");
      //Serial.println("Presione botón para entrar en modo AP.");
      digitalWrite(led_error ,HIGH);
    }

    ignoreButtonUntil = millis() + 2000;  // Ignorar botón durante los primeros 4 segundos
    
  } else {
    Serial.println("⚠️ No hay credenciales guardadas. Presione botón para modo AP.");
    digitalWrite(led_error ,HIGH);
  }  
  
  //ingreso_aux = digitalRead(ingreso);
  //ultimoEstadoIngreso = ingreso_aux;  // Guardar estado inicial

  
        if (client.connected() && topicOut != "") {

          if (bomba == 0 && valvula == 1) {        
            digitalWrite(rele_valvula , HIGH);
            digitalWrite(rele_esp     , LOW);            
          } else {        
            digitalWrite(rele_valvula , LOW);
            digitalWrite(rele_esp  , HIGH);            
          }
      
          String bombaStr   = bomba ? "encendida" : "apagada";
          String valvulaStr = valvula ? "abierta" : "cerrada";
          
          StaticJsonDocument<128> doc; 
          doc["bomba"]   = bombaStr;
          doc["valvula"] = valvulaStr;
          
          String jsonString;
          serializeJson(doc, jsonString);
          
          if (client.publish(topicOut.c_str(), jsonString.c_str())) {
            //Serial.println("📤 JSON enviado: " + jsonString);                    
          } else {
            //Serial.println("❌ Error enviando JSON por MQTT");
          }
      }
  #ifdef medir_flujo  
    attachInterrupt(digitalPinToInterrupt(pin_caudal), pulseInterrupt, FALLING);  // Interrupción en flanco descendente 
  #endif  
 
}

void loop() { // f-loop
  
  #ifdef SIMULACION_SERIAL
    procesarSerial();
  #endif

// ---------------------- FUNCION PARA DETECTAR INGRESO -------------------------

#ifdef medir_flujo
  
  if (primerPaso) {
    timer_caudalimetro = millis();  // Inicia el contador AHORA
    primerPaso = false;
  }
  
/* 
 =============================================================
              EVALUA EL ESTADO DEL INGRESO DEL AGUA
 =============================================================
 */ 
if (millis() - timer_caudalimetro >= t_caudal) {
  //Serial.println("evaluando ingreso");
  
     ingreso = evaluar_ingreso();
  
  // Solo publicar cuando hay CAMBIO de estado
  if (ingreso != ingreso_aux1) {
      ingreso_aux1 = ingreso;
      ingreso_aux = !ingreso; // ingreso_aux solo se usa para el modo por ingreso      
    
    // Publicar el nuevo estado
    String respuesta = "{\"ingreso\":\"" + String(ingreso_aux1 ? "si" : "no") + "\"}";
    client.publish(topicOut.c_str(), respuesta.c_str());
    
    //Serial.print("✅ ESTADO CAMBIADO a: ");
    //Serial.println(ingreso_aux1? "SI (hay ingreso)" : "NO (sin ingreso)");
  }
  
  pulsos = 0;  // Resetear contador de pulsos para el próximo período
  timer_caudalimetro = millis();   
}
//=================================================================
  
#endif 
    
//--------------------------------------------------------------------------------

   if (actualizando) {
    delay(10);        // Pequeña pausa
    return;           // 🛑 SALIR, NO HACER NADA MÁS
  }

//*************** MODULO DE ACTUALIZACION *******************
//***********************************************************
  if (millis() - ultimaVerificacion >= intervalo) {
    ultimaVerificacion = millis();
    verificarActualizacion();
    //Serial.println("ahora si, estamos super bien");
 }
//***********************************************************
    // PRIMERO: Manejo del botón (sin delays)
    bool pressed = digitalRead(buttonPin) == LOW;
    
    // Pulsación larga para modo AP
    if (pressed && pressStartTime == 0) {
        pressStartTime = millis();
    }
    
    if (pressed && (millis() - pressStartTime >= holdDuration)) {
        if (!apModeStarted) {
            startAccessPoint();
            apModeStarted = true;
            // No necesitas wifiConnected aquí
        }
    }
    
    // Pulsación corta para toggle bomba/válvula
    if (!pressed && lastButtonState == true) { // a324 
        pressTime = millis() - pressStartTime;
        //Serial.println("si entra en boton");
        
        if (pressTime > 80 && pressTime < holdDuration) {
            // Cambiar estados
            valvula = !valvula;
            bomba   = !bomba;
            
            // ⭐⭐ ELIMINA EL delay(100) ⭐⭐
            // En su lugar, usa millis() para control de tiempo:
            static unsigned long tiempoUltimoToggle = 0;
            if (millis() - tiempoUltimoToggle > 100) {
                tiempoUltimoToggle = millis();
                
                if (bomba == 0 && valvula == 1) {        
                    digitalWrite(rele_valvula , HIGH);
                    digitalWrite(rele_esp     , LOW);
                } else {        
                    digitalWrite(rele_valvula , LOW);
                    digitalWrite(rele_esp     , HIGH);
                }
                
                // Guardar en preferencias
                preferences.begin("wifi", false);
                  preferences.putBool("valvula", valvula);
                  preferences.putBool("bomba"  , bomba);
                preferences.end();
                
                // Enviar por MQTT si conectado
                if (client.connected() && topicOut != "") {
                    bombaStr   = bomba ? "encendida" : "apagada";
                    valvulaStr = valvula ? "abierta" : "cerrada";
                    
                    StaticJsonDocument<200> doc;
                    doc["bomba"]   = bombaStr;
                    doc["valvula"] = valvulaStr;
                    
                    char buffer[200];
                    size_t len = serializeJson(doc, buffer);
                    client.publish(topicOut.c_str(), buffer, len);
                }
            }
        }
        pressStartTime = 0;
    }
    
    lastButtonState = pressed;
    
/* 
===============================================================
                VERIFICAR SENSORES CADA 1 SEG
===============================================================
     */
    unsigned long currentMillis = millis();  
    if (currentMillis - previousMillis >= interval) {    
        previousMillis = currentMillis;       
      
        nivel_actual   = sonda_nivel();        //----------> a322
        presion_actual = sensor_presion();           

        if(modo_ingreso == true){
          modo_por_ingreso(); // a320
          Serial.println("entra en modo ingreso");          
        }       
        
        // Enviar nivel si cambió
        if (abs(nivel_actual - ultimo_nivel_enviado) >= UMBRAL_CAMBIO) {
            ultimo_nivel_enviado = nivel_actual;                    
            String respuesta = "{\"nivel\":" + String(nivel_actual) + "}";
            
            if (client.connected() && topicOut != "") {
                client.publish(topicOut.c_str(), respuesta.c_str());
            }
        }
        
    }
//====================================================================
    
    // TERCERO: Manejo de conexiones (solo si no está en modo AP)
    if (!apModeStarted && ssid != "") {
        bool wifiOk = (WiFi.status() == WL_CONNECTED);
        
        // LEDs según WiFi
        digitalWrite(led_wifi, wifiOk ? HIGH : LOW);
        digitalWrite(led_error, wifiOk ? LOW : HIGH);
        
        if (wifiOk) {
            // WiFi conectado
            
            if (!client.connected()) {
                static unsigned long ultimoIntentoMQTT = 0;
                
                // Solo intentar MQTT cada 15 segundos
                if (millis() - ultimoIntentoMQTT > 15000) {
                    ultimoIntentoMQTT = millis();
                    
                    // Configurar timeout CORTO
                    client.setSocketTimeout(2000); // 2 segundos máximo
                    
                    String macAddress = WiFi.macAddress();
                    bool conectado = client.connect(macAddress.c_str(), mqtt_user, mqtt_pass);
                    
                    if (conectado) {
                        digitalWrite(led_com, HIGH);
                        if (topicIn != "") {
                            client.subscribe(topicIn.c_str());
                        }
                        Serial.println("✅ MQTT conectado");
                    } else {
                        digitalWrite(led_com, LOW);
                        Serial.println("❌ MQTT falló");
                    }
                }
            } else {
                // MQTT ya está conectado
                digitalWrite(led_com, HIGH);
                client.loop(); // Procesar mensajes entrantes
            }
            
        } else {
            // WiFi desconectado
            digitalWrite(led_com, LOW);
            
            // Intentar reconectar WiFi cada 20 segundos
            static unsigned long ultimoIntentoWifi = 0;
            if (millis() - ultimoIntentoWifi > 20000) {
                ultimoIntentoWifi = millis();
                WiFi.reconnect();
            }
        }
    }
    
    // CUARTO: Modo AP
    if (apModeStarted) {
        server.handleClient();
    }
    
    // Pequeña pausa para estabilidad
    delay(10);
}
