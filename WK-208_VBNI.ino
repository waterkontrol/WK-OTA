/*
 * TOPIC : mk-210/VBNI/E86BEADEED74/in
 * MAC   : E86BEADEED74
 * 
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

unsigned long pressStartTime              = 0;
const unsigned long holdDuration          = 5000;
const unsigned long WIFI_CONNECT_TIMEOUT  = 10000; // 10 segundos
unsigned long ignoreButtonUntil           = 0;

//*********************************************************
float nivel_deseado      = 25;   // NIVEL DEL TANQUE EN CM
const int UMBRAL_CAMBIO  = 5;    // 2% de cambio mínimo
//*********************************************************

bool apModeStarted       = false;
bool wifiTried           = false;
bool wifiConnected       = false;
bool tryingToConnectMQTT = false;
bool ingreso_aux    = 0;
bool ultimoEstadoIngreso = false;

String versionURL  = "https://waterkontrol.github.io/WK-OTA/version.txt";
String firmwareURL = "https://waterkontrol.github.io/WK-OTA/WK-208_VBNI.ino.esp32.bin";

String   ssid            = "";
String   password        = "";
String   title           = "";
int      seriesTypeId    = 3 ;
String   userId          = "";

String topic       = "";
String topic_url   = "";
String topicIn     = "";
String topicOut    = "";
String bombaStr    = "";
String valvulaStr  = "";

String v = "";
String t = "";
String m = "";

byte  a             = 0;
bool  bomba         = 0;
bool  valvula       = 0; 
int   nivel         = 0;
float nivel_cm      = 0;
float nivel_mqtt    = 0;
int   nivel_actual  = 0;

const int led_error = 2;
const int buttonPin = 5;
const int ingreso   = 13;
const int rele_esp  = 16;
const int led_wifi  = 19;
const int led_com   = 18;
const int rele1     = 26;

String versionActual;
String macAddress;
unsigned long ultimaVerificacion = 0;
const unsigned long intervalo = 5000;
bool actualizando = false;  

int ultimo_nivel_enviado = -1;   // -1 para forzar primer envío

bool lastButtonState = false;  // estado anterior del botón
unsigned long previousMillis = 0;
const unsigned long interval = 1000;  // 1 segundos 
unsigned long pressTime=0;
unsigned long tiempoAnterior = 0; 
static unsigned long ultimoPing100 = 0; // Control de frecuencia

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
    Serial.print("EL CONTENIDO DEL TXT ES: ");
    Serial.println(payload);
    
    int idx  = payload.indexOf("version=");
    int tipo = payload.indexOf("tipo=");
    int mac  = payload.indexOf("mac=");
      Serial.println(idx);
      Serial.println(tipo);
      Serial.println(mac);

      if (idx != -1) {
      v = payload.substring(idx + 8);
      v = v.substring(0, v.indexOf('\n'));
      v.trim();
      Serial.print("la version es : ");
      Serial.println(v);
      }

      if (tipo != -1) {
      t = payload.substring(tipo + 5);   // ✅ "tipo=" = 5 caracteres
      t = t.substring(0, t.indexOf('\n'));
      t.trim();
      Serial.print("tipo:  ");
      Serial.println(t);  // "masivo"
    }

      if (mac != -1) {
      m = payload.substring(mac + 4);    // ✅ "mac=" = 4 caracteres
      m = m.substring(0, m.indexOf('\n'));
      m.trim();
      m.toLowerCase();  // ✅ opcional
      Serial.print("la mac es:  ");
      Serial.println(m);  // "E86BEADEF480"
      }

      macAddress = WiFi.macAddress();
      macAddress.replace(":", "");
      macAddress.toLowerCase();
      Serial.print("MAC dispositivo: ");
      Serial.println(macAddress);  // 👈 DEBE MOSTRAR ALGO

      Serial.print("VERSION ACTUAL ES: ");
      Serial.println(versionActual);  // 👈 DEBE MOSTRAR ALGO

      if((v != versionActual) && (t == "masivo") && (m == macAddress)){
        Serial.println("si entra en lo que hicimos");
      }
      else {
      Serial.println("no coinciden con el txt");
      }
//================================  QUEDAMOS ACA =========================      
    
    if (idx != -1) {
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
    }
  }
  http.end();
}

float sonda_nivel() {
  int16_t raw = ads.readADC_SingleEnded(2);
  float voltage = raw * 0.1875 / 1000.0;

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
   
 return nivel_cm;
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
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Conectado y confirmado\"}");
      digitalWrite(led_wifi  , HIGH);
      digitalWrite(led_error , LOW);
      title="";
      ssid="";
      password=""; 
      delay(1500);
      ESP.restart();

    } else {
      Serial.println("❌ El servidor no respondió correctamente. Cancelando.");
      server.send(200, "application/json", "{\"status\":\"error\",\"message\":\"server error\"}");
      delay(500);
      WiFi.disconnect(true);
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
    server.send(200, "application/json", "{\"status\":\"error\",\"message\":\"invalid credentials\"}");
    delay(500);
    WiFi.disconnect(true);
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

void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  Serial.print("📩 Mensaje recibido en topic ");
  Serial.print(topic);
  Serial.print(": ");

  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  //Serial.println(message);

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
        StaticJsonDocument<128> doc; 
        bombaStr   = bomba ? "encendida" : "apagada";
        valvulaStr = valvula ? "abierta" : "cerrada";
        ingreso_aux = digitalRead(ingreso);
        
         if(ingreso_aux == true){  
             doc["ingreso"]  = "no";
           }
           else doc["ingreso"]  = "si";       
                        
            doc["nivel"]    = nivel_actual;
            doc["bomba"]    = bombaStr;
            doc["valvula"]  = valvulaStr;                             
                    
            String jsonString;
            serializeJson(doc, jsonString);
          
        if (client.publish(topicOut.c_str(), jsonString.c_str())) {
            //Serial.println(" actualizado " + jsonString);
          } else {
            //Serial.println("error en actualizar ingreso");
          }    
       
          }
    }

  // Guardar los nuevos estados recibidos
        if (doc.containsKey("bomba") && doc.containsKey("valvula")) {
    
            String bomba1     = doc["bomba"];
            String valvula1   = doc["valvula"];
    

        if ((valvula1 == "abierta") && (bomba1 == "apagada")){   
            //Serial.println("valvula abierta, bomba apagada");
            digitalWrite(rele_esp, HIGH);   // RELE DE LA VALVULA
            digitalWrite(rele1, LOW);     // RELE DE LA BOMBA
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
        preferences.putBool("valvula", valvula);
        preferences.putBool("bomba", bomba);
      preferences.end();        
      
    }

    if ((valvula1 == "cerrada") && (bomba1 == "encendida")){    
      //Serial.println("valvula cerrada, bomba encendida");
      digitalWrite(rele_esp, LOW);  // Por ejemplo, encender
      digitalWrite(rele1, HIGH);    // Apagar otro
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
        preferences.putBool("valvula", valvula);
        preferences.putBool("bomba", bomba);
      preferences.end();
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

   if (doc.containsKey("admin_version")) {        
    String respuesta = "{\"version\":" + String(versionActual) + "}";    
    client.publish(topicOut.c_str(), respuesta.c_str());    
}

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
    //Serial.println("Respuesta del servidor: " + response);

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

void setup() {
  Serial.begin(115200);
  delay(500);
  
  pinMode(buttonPin , INPUT_PULLUP);
  pinMode(ingreso   , INPUT_PULLUP);
  pinMode(led_error , OUTPUT);
  pinMode(led_com   , OUTPUT);
  pinMode(led_wifi  , OUTPUT);
  pinMode(rele_esp  , OUTPUT);
  pinMode(rele1     , OUTPUT);

  preferences.begin("ota", false);
      versionActual = preferences.getString("version_actual", "1.0");
  preferences.end();

  // Recuperar credenciales y topics de Preferences
  preferences.begin("wifi", true);
      ssid          = preferences.getString ("ssid"          , "");
      password      = preferences.getString ("password"      , "");
      topicOut      = preferences.getString ("topicOut"      , "");
      topicIn       = preferences.getString ("topicIn"       , "");
      topic_url     = preferences.getString ("topic_url"     , "");
      valvula       = preferences.getBool   ("valvula"       , "");
      bomba         = preferences.getBool   ("bomba"         , "");
      nivel_deseado = preferences.getFloat  ("nivel_deseado" , 500.0);      
  preferences.end();

  Serial.print("ssid:");
  Serial.println(ssid);
  Serial.print("pass:");  
  Serial.println(password);
  Serial.print("version actual:");  
  Serial.println(versionActual);

  ssid = "Flia. Ramirez";
  password = "M&M1920*";
  

  if (bomba == 0 && valvula == 1) {        
     digitalWrite(rele1     , HIGH);
     digitalWrite(rele_esp  , LOW);            
  } else {        
     digitalWrite(rele1     , LOW);
     digitalWrite(rele_esp  , HIGH);            
   }  

  Serial.print("el topic es:");
  Serial.println(topicOut);
  Serial.println(topicIn);
  Serial.print("el valor de  la valvula es:");
  Serial.println(valvula);
  Serial.print("el valor de  la bomba es:");
  Serial.println(bomba);

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  
  if (!ads.begin()) {
    Serial.println("No se encontró el ADS1115. Verifica conexiones.");
    while (1);
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
  
  ingreso_aux = digitalRead(ingreso);
  ultimoEstadoIngreso = ingreso_aux;  // Guardar estado inicial

  
        if (client.connected() && topicOut != "") {

          if (bomba == 0 && valvula == 1) {        
            digitalWrite(rele1     , HIGH);
            digitalWrite(rele_esp  , LOW);            
          } else {        
            digitalWrite(rele1     , LOW);
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
        
}

void loop() {

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
    if (!pressed && lastButtonState == true) {
        pressTime = millis() - pressStartTime;
        
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
                    digitalWrite(rele1, HIGH);
                    digitalWrite(rele_esp, LOW);
                } else {        
                    digitalWrite(rele1, LOW);
                    digitalWrite(rele_esp, HIGH);
                }
                
                // Guardar en preferencias
                preferences.begin("wifi", false);
                preferences.putBool("valvula", valvula);
                preferences.putBool("bomba", bomba);
                preferences.end();
                
                // Enviar por MQTT si conectado
                if (client.connected() && topicOut != "") {
                    bombaStr = bomba ? "encendida" : "apagada";
                    valvulaStr = valvula ? "abierta" : "cerrada";
                    
                    StaticJsonDocument<200> doc;
                    doc["bomba"] = bombaStr;
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
    
    // SEGUNDO: Sensores (cada 1 segundo)
    unsigned long currentMillis = millis();  
    if (currentMillis - previousMillis >= interval) {    
        previousMillis = currentMillis; 
        
        ingreso_aux = digitalRead(ingreso);
        nivel_actual = sonda_nivel();
        
        // Enviar nivel si cambió
        if (abs(nivel_actual - ultimo_nivel_enviado) >= UMBRAL_CAMBIO) {
            ultimo_nivel_enviado = nivel_actual;        
            String respuesta = "{\"nivel\":" + String(nivel_actual) + "}";
            
            if (client.connected() && topicOut != "") {
                client.publish(topicOut.c_str(), respuesta.c_str());
            }
        }
        
        // Enviar estado ingreso si cambió
        if(ingreso_aux != ultimoEstadoIngreso) {
            ultimoEstadoIngreso = ingreso_aux;
            
            if (client.connected() && topicOut != "") {
                StaticJsonDocument<128> doc;
                doc["ingreso"] = ingreso_aux ? "no" : "si";
                
                String jsonString;
                serializeJson(doc, jsonString);
                client.publish(topicOut.c_str(), jsonString.c_str());
            }
        }
    }
    
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
