#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <time.h>
#include <Time.h>
#include <WebServer.h>
#include <RTClib.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "website.h"
#include "secrets.h"

// ----- Zustandsdefinierungen -----

#define UNBEKANNTER_ZUSTAND 0
#define GESCHLOSSEN 1
#define OEFFNET 2
#define STECKT_FEST_AUF 3
#define HALT_AUF 4
#define GEOEFFNET 5
#define SCHLIEST 6
#define STECKT_FEST_ZU 7
#define HALT_ZU 8

String zustandstext[9] =
{
  "Unbekannter Zustand", 
  "Geschlossen", 
  "Geht Auf",
  "Steckt fest beim Aufgehen", 
  "Hat gehalten beim Aufgehen", 
  "Geoeffnet",
  "Geht Zu", 
  "Steckt fest bei Zugehen", 
  "Hat gehalten beim Zugehen"
};



// ----- Pin-definierungen -----

const int relayPin = 15; // GPIO-Pin des Relais
const int defaultReadKontaktAuf = 4; // GPIO-Pin des Magnet-Read Sensors AUF
const int defaultReadKontaktZu = 16; // GPIO-Pin des Magnet-Read Sensors ZU
int readKontaktAuf = defaultReadKontaktAuf; //kann durch Config auf Website getauscht werden
int readKontaktZu = defaultReadKontaktZu; //kann durch Config auf Website getauscht werden
const int piezoPin = 17; // GPIO-Pin des Piezo (Piepser)
const int triggerPin = 5; // GPIO-Pin des Trigger-Kabel um ein Garagentrigger zu erkennen






// ----- Settings -----

bool SETTING_STOP_STUCK = false; // Automatisch Anhalten, wenn das Tor feststeckt (default: Aus)
unsigned long SETTING_MAX_OPEN_TIME = 10000; // Zeit, bevor das Tor als STECKT_FEST_AUF erkannt wird (default: 10 Sekunden)
unsigned long SETTING_MAX_CLOSE_TIME = 10000; // Zeit, bevor das Tor als STECKT_FEST_ZU erkannt wird (default: 10 Sekunden)

bool SETTING_AUTOMATIC_CLOSE = false; // Automatisch nach definierbarer Zeit das Tor wieder schließen (default: Aus)
unsigned long SETTING_AUTOMATIC_CLOSE_TIME = 1200000; // Zeit nach dem das Tor geschlossen werden soll (default: 20 Minuten)
unsigned long SETTING_AUTOMATIC_CLOSE_WARNING_TIME = 60000; // Zeit, die vor dem Automatischen Schließen mit piepser gewarnt werden soll (default: 1 Minute)

bool SETTING_KONTAKT_CONFIG = false;

bool SETTING_CLOSE_ON_TIME = false;
int SETTING_CLOSE_HOUR = 22;
int SETTING_CLOSE_MINUTE = 0;





// ----- Programm-Logik -----

int zustand = UNBEKANNTER_ZUSTAND;
int maxLogs = 50;
volatile unsigned long lastInterruptTimeFalling = 0;               // Timer zum entprellen des Trigger-Signals
volatile unsigned long lastInterruptTimeRising = 0;
volatile unsigned long startMovingTime = 0;                 // letzte Zustandsänderung zu SCHLIEST oder OEFFNET (Timer)
volatile unsigned long openSinceTime = 0;                   // Zeitstempel seit dem das Tor offen ist
WiFiServer server(80);                                      // TCP-Server auf Port 80 für einfachen TCP-Toggle
WebServer httpServer(81);                                  // Webserver für erweiterte Einstellungsmöglichkeiten auf Port 81
Preferences preferences;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;                            // Offset in Sekunden für Zeitzone (z. B. MEZ: +1 Stunde)
const int daylightOffset_sec = 3600;                        // Sommerzeit-Offset (falls zutreffend)
unsigned long lastSync = 0;                                 // Zeitpunkt der letzten NTP-Synchronisierung
const unsigned long syncInterval = 1000*60*5 ;              // Intervall der NTP-Synchroniserung (5min)
//AsyncWebServer httpServer(81);
String logs = "Fehler beim Generieren der Logs.";
bool isAuthenticated = false;
bool isAdmin = false;
unsigned long lastLoginTime = 0;                            // Zeitstempel des letzten Logins
const unsigned long sessionTimeout = 5 * 60 * 1000;         // 5 Minuten in Millisekunden
unsigned long SESSION_AUTOMATIC_CLOSE_TIME = SETTING_AUTOMATIC_CLOSE_TIME;
bool SESSION_AUTOMATIC_CLOSE = SETTING_AUTOMATIC_CLOSE;
volatile bool defaultAufIntState = false;
volatile bool defaultZuIntState = false;




// ----- Log-Nachrichten -----

const char* logNachrichten[] = {
    "ESP Gestartet",                                                          //0
    "Tor oeffnen gestartet",                                                  //1
    "Tor schliessen gestartet",                                               //2
    "Fehler: STECKT FEST",                                                    //3
    "Einstellung geaendert - SETTING_STOP_STUCK",                             //4
    "Einstellung geaendert - SETTING_MAX_OPEN_TIME",                          //5
    "Einstellung geaendert - SETTING_MAX_CLOSE_TIME",                         //6
    "Einstellung geaendert - SETTING_AUTOMATIC_CLOSE",                        //7
    "Einstellung geaendert - SETTING_AUTOMATIC_CLOSE_TIME",                   //8
    "Einstellung geaendert - SETTING_AUTOMATIC_CLOSE_WARNING_TIME",           //9
    "Toggle - manuell - TCP",                                                 //10
    "Toggle - manuell - Webserver",                                           //11
    "Toggle - automatisch - Notstop",                                         //12
    "Toggle - automatisch - Automatisches Schliessen",                        //13
    "Toggle - automatisch - Wecker",                                          //14
    "Einstellung geandert - SETTING_KONTAKT_CONFIG",                          //15
    "Einstellung geandert - SETTING_CLOSE_ON_TIME",                           //16
    "Einstellung geandert - SETTING_CLOSE_HOUR",                              //17
    "Einstellung geandert - SETTING_CLOSE_MINUTE"                             //18
};
const int anzahlLogNachrichten = sizeof(logNachrichten) / sizeof(logNachrichten[0]);


void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(WiFiEvent);
  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWLAN verbunden!");

  WiFi.enableIPv6();
  delay(5000);

  // Globale IPv6-Adresse abrufen
  /*
  Serial.println("IPv6-Adressen:");
  for (int i = 0; i < WiFi.localIPv6().length(); i++) {
      Serial.println(WiFi.localIPv6()[i].toString());
  }
  */

  // ----- DDNS -----

  String ipv6 = getIPv6();
  Serial.println("Ermittelte IPv6: " + ipv6);

  if (updateCloudflareDNS(ipv6)) {
      Serial.println("Cloudflare DNS erfolgreich aktualisiert!");
  } else {
      Serial.println("Fehler beim Aktualisieren des Cloudflare DNS!");
  }




  attachInterrupt(digitalPinToInterrupt(triggerPin), handleTrigger, CHANGE); //Interrupt für Tor-Trigger
  attachInterrupt(digitalPinToInterrupt(defaultReadKontaktAuf), handleIntAuf, RISING); //Interrupt für Tor-Trigger
  attachInterrupt(digitalPinToInterrupt(defaultReadKontaktZu), handleIntZu, RISING); //Interrupt für Tor-Trigger

  // ----- KONFIGURATION FÜR OVER-THE-AIR-UPDATES -----

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("Garagen-ESP32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");



  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Pins konfigurieren
  pinMode(relayPin, OUTPUT);
  pinMode(readKontaktAuf, INPUT);
  pinMode(readKontaktZu, INPUT);
  pinMode(piezoPin, OUTPUT);
  pinMode(triggerPin, INPUT);
  digitalWrite(relayPin, HIGH); // Relais initial ausschalten

  // TCP-Server starten
  server.begin();
  Serial.println("TCP-Server gestartet.");

  // ----- Einstellungen aus EEPROM laden/speichern -----

  //prüfen ob Speicher voll ist, falls ja Speicher leeren um Fehler zu vermeiden.
  nvs_stats_t nvs_stats;
  esp_err_t err = nvs_get_stats(NULL, &nvs_stats);
  
  if(nvs_stats.free_entries < 10)
  {
    Serial.print("Nur noch ");
    Serial.print(nvs_stats.free_entries);
    Serial.println(" Eintraege frei, speicher wurde geloescht");
    nvs_flash_erase();
    nvs_flash_init(); 
  }
  else Serial.println("Keine Speicherprobleme festgestellt");

  preferences.begin("GaragenESP", false);

  if(preferences.isKey("S1"))
  {
    Serial.println("Einstellungen gefunden. Werden geladen.");
    SETTING_STOP_STUCK = preferences.getBool("S1");
    SETTING_AUTOMATIC_CLOSE = preferences.getBool("S2");
    SETTING_MAX_OPEN_TIME = preferences.getInt("S3");
    SETTING_MAX_CLOSE_TIME = preferences.getInt("S4");
    SETTING_AUTOMATIC_CLOSE_TIME = preferences.getInt("S5");
    SETTING_AUTOMATIC_CLOSE_WARNING_TIME = preferences.getInt("S6");
    SETTING_KONTAKT_CONFIG = preferences.getBool("S7");
    SETTING_CLOSE_ON_TIME = preferences.getBool("S8");
    SETTING_CLOSE_HOUR = preferences.getInt("S9");
    SETTING_CLOSE_MINUTE = preferences.getInt("S10");
  }
  else
  {
    Serial.println("Keine Einstellungen gefunden. Default-Einstellungen werden gespeichert.");
    preferences.putBool("S1", SETTING_STOP_STUCK);
    preferences.putBool("S2", SETTING_AUTOMATIC_CLOSE);
    preferences.putInt("S3", SETTING_MAX_OPEN_TIME);
    preferences.putInt("S4", SETTING_MAX_CLOSE_TIME);
    preferences.putInt("S5", SETTING_AUTOMATIC_CLOSE_TIME);
    preferences.putInt("S6", SETTING_AUTOMATIC_CLOSE_WARNING_TIME);
    preferences.putBool("S7", SETTING_KONTAKT_CONFIG);
    preferences.putBool("S8", SETTING_CLOSE_ON_TIME);
    preferences.putInt("S9", SETTING_CLOSE_HOUR);
    preferences.putInt("S10", SETTING_CLOSE_MINUTE);
  }

  preferences.end();
  readKontaktAuf = (!SETTING_KONTAKT_CONFIG)? defaultReadKontaktAuf:defaultReadKontaktZu;
  readKontaktZu = (!SETTING_KONTAKT_CONFIG)? defaultReadKontaktZu:defaultReadKontaktAuf;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi nicht verbunden, warte...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWiFi verbunden!");
  }
  Serial.print("Freier Heap vor httpServer.begin(): ");
  Serial.println(ESP.getFreeHeap());

  httpServer.on("/login", HTTP_GET, handleLoginPage);
  httpServer.on("/login", HTTP_POST, handleLogin);
  httpServer.on("/", HTTP_GET, handleRoot);
  httpServer.on("/setState", HTTP_POST, handleSetState);
  httpServer.on("/toggle", HTTP_POST, handleHttpToggle);
  httpServer.on("/open", HTTP_POST, handleOpen);
  httpServer.on("/close", HTTP_POST, handleClose);
  httpServer.on("/openTime", HTTP_POST, handleOpenTime);
  httpServer.on("/set_auto_close_time", HTTP_POST, handleSetAutoCloseTime);
  httpServer.on("/settings", HTTP_POST, handleSettings);
  httpServer.on("/read_config", HTTP_POST, handleReadConfig);
  httpServer.begin();

  setupTime();
  logNachricht(0);
}

void loop() {
  ArduinoOTA.handle();                        // OverTheAir Updates
  handleToggle(checkForToggle());             // TCP-Toggle
  zustandsMonitoring();
  httpServer.handleClient();

  if (millis() - lastSync > syncInterval)     //Einmal Stündlich:
  {
    setupTime(); // Zeit erneut synchronisieren
    lastSync = millis();

    String ipv6 = getIPv6();
    if (updateCloudflareDNS(ipv6)) {
        Serial.println("Cloudflare DNS erfolgreich aktualisiert!");
    } else {
        Serial.println("Fehler beim Aktualisieren des Cloudflare DNS!");
    }

  }

  if (isAuthenticated && (millis() - lastLoginTime > sessionTimeout)) //5 Minuten nach Anmelden Session beenden
  {
    isAuthenticated = false; // Abmelden
    isAdmin=false;
    Serial.println("Session abgelaufen. Benutzer wurde automatisch abgemeldet.");
  }
}

String checkForToggle()
{
  WiFiClient client = server.available();
  if (client) {
    
    // Daten vom Client lesen
    while (client.connected() && client.available() == 0) {
      delay(1); // Warten auf Daten
    }

    String request = client.readStringUntil('\n'); // Empfange Daten
    request.trim(); // Entferne überflüssige Leerzeichen

    Serial.print("Empfangene Nachricht: ");
    Serial.println(request);

    client.println("Empfangen");  
    client.flush();  // Stellt sicher, dass die Daten gesendet werden
    delay(10);
    client.stop();

    return request;
  }
  else return "";
}

void handleToggle(String command)
{
  if(command=="")return;
  if(command.equalsIgnoreCase("TOGGLE"))
  {
    toggle(10);
  }
  if(command.equalsIgnoreCase("OPEN"))
  {
    if(zustand==GESCHLOSSEN || zustand == HALT_ZU || zustand == UNBEKANNTER_ZUSTAND) toggle(10);
    openSinceTime=millis(); //resetet den Timer zur Automatischen Torschließung.
  }
  if(command.equalsIgnoreCase("CLOSE"))
  {
    if(zustand==GEOEFFNET || zustand == HALT_AUF || zustand == UNBEKANNTER_ZUSTAND) toggle(10);
  }
  
}

void zustandsMonitoring()
{
  static unsigned long lastTestMillis = 0;
  switch (zustand)
  {
    case UNBEKANNTER_ZUSTAND:
      if(digitalRead(readKontaktAuf)==HIGH)
      {
        zustand = GEOEFFNET;
        openSinceTime = millis();
      } 
      else if(digitalRead(readKontaktZu)==HIGH) zustand = GESCHLOSSEN;
      break;
      
    case GEOEFFNET:
    {
      time_t unixTimestamp = getUnixTimestamp();
      struct tm *timeInfo = localtime(&unixTimestamp);
      if (timeInfo->tm_hour == SETTING_CLOSE_HOUR && timeInfo->tm_min == SETTING_CLOSE_MINUTE &&  SETTING_CLOSE_ON_TIME) {
        toggle(14);
      }
      if(SESSION_AUTOMATIC_CLOSE && SESSION_AUTOMATIC_CLOSE_TIME - (millis() - openSinceTime) <= SETTING_AUTOMATIC_CLOSE_WARNING_TIME) digitalWrite(piezoPin, HIGH);
      else digitalWrite(piezoPin, LOW);

      if(SESSION_AUTOMATIC_CLOSE && millis() - openSinceTime > SESSION_AUTOMATIC_CLOSE_TIME) toggle(13);

      break;
    }
    case OEFFNET:
      if(defaultAufIntState == true)
      {
        zustand = GEOEFFNET;
        defaultAufIntState = false;
        openSinceTime = millis();
      } 
      if(SETTING_STOP_STUCK && millis()-startMovingTime > SETTING_MAX_OPEN_TIME)
      {
        zustand = STECKT_FEST_AUF;
        toggle(12);
      } 
      break;

    case GESCHLOSSEN:
      break;

    case SCHLIEST:
      if(defaultZuIntState == true)
      {
        zustand = GESCHLOSSEN;
        defaultZuIntState = false;
      }
      if(SETTING_STOP_STUCK && millis()-startMovingTime > SETTING_MAX_CLOSE_TIME)
      {
        zustand = STECKT_FEST_ZU;
        toggle(12);
      }
      break;

  	case STECKT_FEST_AUF:
      break;

    case STECKT_FEST_ZU:
      break;

    case HALT_AUF:
      break;

    case HALT_ZU:
      break;
  }
}

void toggle(int LogMessage) //togglelt das Relais und löst damit das Garagentor aus.
{
  logNachricht(LogMessage);

  SESSION_AUTOMATIC_CLOSE_TIME = SETTING_AUTOMATIC_CLOSE_TIME;
  SESSION_AUTOMATIC_CLOSE = SETTING_AUTOMATIC_CLOSE;
  digitalWrite(relayPin, LOW);
  delay(500);
  digitalWrite(relayPin, HIGH);
}

void toggle(int LogMessage, unsigned long SESSION_ACT) //togglelt das Relais und löst damit das Garagentor aus.
{
  logNachricht(LogMessage);

  SESSION_AUTOMATIC_CLOSE_TIME = SESSION_ACT;
  SESSION_AUTOMATIC_CLOSE = !SESSION_ACT == 0;
  digitalWrite(relayPin, LOW);
  delay(500);
  digitalWrite(relayPin, HIGH);
}


void handleTrigger() {
  //Serial.println("T0");
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTimeFalling > 1000) // 300ms Debounce Time
  {
    Serial.println("T");
    switch (zustand)
    {
      case GEOEFFNET:
        zustand = SCHLIEST;
        defaultZuIntState = false;
        startMovingTime = millis();
        digitalWrite(piezoPin,LOW);
        break;
      
      case GESCHLOSSEN:
        zustand = OEFFNET;
        defaultAufIntState = false;
        startMovingTime = millis();
        break;

      case SCHLIEST: 
        zustand = HALT_ZU;
        break;

      case STECKT_FEST_ZU:
        zustand = HALT_ZU;
        break;
      
      case HALT_ZU:
        zustand = OEFFNET;
        defaultAufIntState = false;
        startMovingTime = millis();
        break;

      case OEFFNET:
        zustand = HALT_AUF;
        break;

      case STECKT_FEST_AUF:
        zustand = HALT_AUF;
        break;

      case HALT_AUF:
        defaultZuIntState = false;
        zustand = SCHLIEST;
        startMovingTime = millis();
        break;
    }
  }
  lastInterruptTimeFalling = currentTime;
}

void handleIntAuf()
{
  if(defaultReadKontaktAuf == readKontaktAuf)defaultAufIntState = true;
  else defaultZuIntState = true;
}

void handleIntZu()
{
  if(defaultReadKontaktZu == readKontaktZu)defaultZuIntState = true;
  else defaultAufIntState = true;
}

void logNachricht(int index) {
  unsigned long unixTimestamp = getUnixTimestamp();
  Serial.print("Neuer Logeintrag: ");
  Serial.println(logNachrichten[index]);
  preferences.begin("log", false);

  // Anzahl aktueller Log-Einträge abrufen
  int logCount = preferences.getInt("lc", 0); // Kürzerer Key für Log-Zähler

  // Generiere kürzeren Key für den neuen Log-Eintrag
  String logKey = "l" + String(logCount % maxLogs); // Kürzerer Key: "lxx"

  // Byte-Array für die Daten (Index + Timestamp)
  uint8_t logData[5];
  logData[0] = (uint8_t)index;                       // 1 Byte für den Index
  memcpy(&logData[1], &unixTimestamp, sizeof(unixTimestamp)); // 4 Bytes für den Timestamp

  // Speichere das Byte-Array in Preferences
  preferences.putBytes(logKey.c_str(), logData, sizeof(logData));

  // Aktualisiere Log-Zähler
  preferences.putInt("lc", logCount + 1); // Kürzerer Key für Log-Zähler

  preferences.end();
}

void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case WIFI_EVENT_STA_DISCONNECTED:
      //Serial.println("WLAN getrennt! Versuche, neu zu verbinden...");
      WiFi.disconnect();
      WiFi.reconnect();
      break;
      
    default:
      break;
  }
}

String getIPv6() {
    HTTPClient http;
    http.begin("https://api64.ipify.org");
    int httpCode = http.GET();
    String ipv6 = "";

    if (httpCode == HTTP_CODE_OK) {
        ipv6 = http.getString();
    }
    http.end();
    return ipv6;
}

bool updateCloudflareDNS(String ipv6) {
    HTTPClient http;
    http.begin("https://api.cloudflare.com/client/v4/zones/" + String(cloudflare_zone_id) + "/dns_records/" + String(cloudflare_record_id));
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(cloudflare_api_token));

    // JSON-Daten für das Update
    StaticJsonDocument<512> doc;
    doc["type"] = "AAAA";
    doc["name"] = cloudflare_domain;
    doc["content"] = ipv6;
    doc["ttl"] = 0;  // Time-to-Live (120 Sekunden)
    doc["proxied"] = false;  // Falls Cloudflare-Proxy nicht genutzt wird

    String requestBody;
    serializeJson(doc, requestBody);

    int httpCode = http.PUT(requestBody);
    http.end();

    return httpCode == HTTP_CODE_OK;
}

String readLog() {
    String logString = ""; // Hier wird der gesamte Log gespeichert

    preferences.begin("log", false);
    int logCount = preferences.getInt("lc", 0); // Kürzerer Key für Log-Zähler

    // Älteste bis neueste Einträge abrufen (FIFO)
    for (int i = 0; i < min(logCount, maxLogs); i++) {
        String logKey = "l" + String(i); // Kürzerer Key: "lxx"

        // Byte-Array für gespeicherte Daten
        uint8_t logData[5];
        if (preferences.getBytes(logKey.c_str(), logData, sizeof(logData))) {
            int index = logData[0]; // Index ist das erste Byte

            // UNIX-Timestamp extrahieren (letzte 4 Bytes)
            unsigned long unixTimestamp;
            memcpy(&unixTimestamp, &logData[1], sizeof(unixTimestamp));
            time_t rawtime = unixTimestamp;
            struct tm *timeinfo = localtime(&rawtime);

            // String für das Zeitformat vorbereiten
            char formattedTime[20]; // dd:mm:yyyy hh:mm:ss hat 19 Zeichen + null-terminiert
            strftime(formattedTime, sizeof(formattedTime), "%d.%m.%Y %H:%M:%S", timeinfo);

            // Nachricht aus dem Index abrufen
            if (index >= 0 && index < anzahlLogNachrichten) {
              logString += "[";
              logString += String(formattedTime);
              logString += "] ";
              logString += logNachrichten[index];
              logString += "\n";
            }
        }
    }
    
    preferences.end();
    return logString; // Gibt den gesamten Log als String zurück
}

void setupTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("Zeit wird synchronisiert...");

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Fehler: Keine Zeit erhalten!");
        return;
    }
    Serial.println("Zeit synchronisiert:");
    Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
}

unsigned long getUnixTimestamp() {
    time_t now;
    time(&now); // Aktuelle Zeit abrufen
    return (unsigned long)now; // UNIX-Timestamp zurückgeben
}


String preprocessor(String page)
{
  page.replace("{STATE}", zustandstext[zustand]);
  page.replace("{AUTO_STOP}", (SETTING_STOP_STUCK ? "checked" : ""));
  page.replace("{MAX_OPEN_TIME}", String(SETTING_MAX_OPEN_TIME/1000));
  page.replace("{MAX_CLOSE_TIME}", String(SETTING_MAX_CLOSE_TIME/1000));
  page.replace("{AUTO_CLOSE}", (SETTING_AUTOMATIC_CLOSE ? "checked" : ""));
  page.replace("{AUTO_CLOSE_DELAY}", String(SETTING_AUTOMATIC_CLOSE_TIME/60000));
  page.replace("{WARNING_DELAY}", String(SETTING_AUTOMATIC_CLOSE_WARNING_TIME/1000));
  page.replace("{LOGS}", readLog());
  page.replace("{RELAY_STATE}", (digitalRead(relayPin)? "HIGH": "LOW"));
  page.replace("{READ_AUF_STATE}", (digitalRead(readKontaktAuf)? "HIGH": "LOW"));
  page.replace("{READ_ZU_STATE}", (digitalRead(readKontaktZu)? "HIGH": "LOW"));
  page.replace("{PIEZO_STATE}", (digitalRead(piezoPin)? "HIGH": "LOW"));
  page.replace("{TRIGGER_STATE}", (digitalRead(triggerPin)? "HIGH": "LOW"));
  page.replace("{PIN1}", (SETTING_KONTAKT_CONFIG? "checked": ""));
  page.replace("{PIN2}", (SETTING_KONTAKT_CONFIG? "": "checked"));
  page.replace("{AKTUELLER_ZUSTAND}", String(zustand));
  page.replace("{AUTO_CLOSE_CLOCK}", (SETTING_CLOSE_ON_TIME ? "checked" : ""));
  char timeBuffer[6];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", SETTING_CLOSE_HOUR, SETTING_CLOSE_MINUTE);
  page.replace("{CURRENT_TIME}", String(timeBuffer));
  return page;
}

void handleLoginPage() {
    String html = R"rawliteral(
      <html>
      <head><title>Login</title></head>
      <body>
          <h1>Bitte einloggen</h1>
          <form action="/login" method="POST">
              <label>Benutzername:</label><br>
              <input type="text" name="username"><br>
              <label>Passwort:</label><br>
              <input type="password" name="password"><br><br>
              <input type="submit" value="Login">
          </form>
      </body>
      </html>
    )rawliteral";
    httpServer.send(200, "text/html", html);
}

void handleLogin() {
    if (httpServer.hasArg("username") && httpServer.hasArg("password")) {
        String Susername = httpServer.arg("username");
        String Spassword = httpServer.arg("password");
        
        if (Susername == username && (Spassword == passwordLogin || Spassword == passwordAdmin)) {
            isAuthenticated = true;
            if (Spassword == passwordAdmin) isAdmin = true;
            lastLoginTime = millis();
            httpServer.sendHeader("Location", "/");
            httpServer.send(302, "text/plain", "");
            return;
        }
    }
    httpServer.send(403, "text/html", "Login fehlgeschlagen! <a href='/login'>Zur&uuml;ck</a>");
}

void handleRoot() {
  if (!isAuthenticated) {
      httpServer.sendHeader("Location", "/login");
      httpServer.send(302, "text/plain", "");
      return;
  }
  if (!isAdmin) httpServer.send(200, "text/html", preprocessor(htmlPage));
  else httpServer.send(200, "text/html", preprocessor(htmlAdminPage));
}

void handleSetState() {
  if (httpServer.hasArg("newState")) {
      zustand = httpServer.arg("newState").toInt();
  }
  httpServer.sendHeader("Location", "/");
  httpServer.send(302, "text/plain", "");
}

void handleHttpToggle() {
  toggle(11);
  httpServer.sendHeader("Location", "/");
  httpServer.send(302, "text/plain", "");
}

void handleOpen()
{
  if (zustand == GESCHLOSSEN || zustand == HALT_ZU || zustand == UNBEKANNTER_ZUSTAND) {
      toggle(11);
  }
  openSinceTime=millis(); //resetet den Timer zur Automatischen Torschließung.
  httpServer.sendHeader("Location", "/");
  httpServer.send(302, "text/plain", "");
}

void handleClose()
{
  if (zustand == GEOEFFNET || zustand == HALT_AUF || zustand == UNBEKANNTER_ZUSTAND)
  {
    toggle(11);
  }
}

void handleOpenTime()
{
  if(zustand==GESCHLOSSEN || zustand == HALT_ZU || zustand == UNBEKANNTER_ZUSTAND)
  {
    openSinceTime=millis(); //resetet den Timer zur Automatischen Torschließung.
    unsigned long temp = httpServer.arg("openTime").toInt()*1000*60;
    toggle(11, temp);
  } 
  httpServer.sendHeader("Location", "/");
  httpServer.send(302, "text/plain", "");
}

void handleSettings()
{
  if (httpServer.hasArg("autoStop"))
  {
    if(SETTING_STOP_STUCK != true)logNachricht(4);
    SETTING_STOP_STUCK = true;
  }
  else SETTING_STOP_STUCK = false;
  
  if (httpServer.hasArg("maxOpenTime"))
  {
    if(httpServer.arg("maxOpenTime").toInt()*1000 != SETTING_MAX_OPEN_TIME)logNachricht(5);
    SETTING_MAX_OPEN_TIME = httpServer.arg("maxOpenTime").toInt()*1000;
  }
  if (httpServer.hasArg("maxCloseTime"))
  {
    if(httpServer.arg("maxCloseTime").toInt()*1000 != SETTING_MAX_CLOSE_TIME)logNachricht(6);
    SETTING_MAX_CLOSE_TIME = httpServer.arg("maxCloseTime").toInt()*1000;
  }
  if (httpServer.hasArg("autoClose"))
  {
    if(SETTING_AUTOMATIC_CLOSE != true)logNachricht(7);
    SETTING_AUTOMATIC_CLOSE = true;
  }
  else SETTING_AUTOMATIC_CLOSE = false;
  
  if (httpServer.hasArg("autoCloseDelay"))
  {
    if(httpServer.arg("autoCloseDelay").toInt()*1000*60 != SETTING_AUTOMATIC_CLOSE_TIME)logNachricht(8);
    SETTING_AUTOMATIC_CLOSE_TIME = httpServer.arg("autoCloseDelay").toInt()*1000*60;
  }
  if (httpServer.hasArg("warningDelay"))
  {
    if(httpServer.arg("warningDelay").toInt()*1000 != SETTING_AUTOMATIC_CLOSE_WARNING_TIME)logNachricht(9);
    SETTING_AUTOMATIC_CLOSE_WARNING_TIME = httpServer.arg("warningDelay").toInt()*1000;
  }
  if (httpServer.hasArg("AutoCloseEnabled"))
  {
    if(SETTING_CLOSE_ON_TIME != true)logNachricht(16);
    SETTING_CLOSE_ON_TIME = true;
  }
  else SETTING_CLOSE_ON_TIME = false;

  preferences.begin("GaragenESP", false);
  preferences.putBool("S1", SETTING_STOP_STUCK);
  preferences.putBool("S2", SETTING_AUTOMATIC_CLOSE);
  preferences.putInt("S3", SETTING_MAX_OPEN_TIME);
  preferences.putInt("S4", SETTING_MAX_CLOSE_TIME);
  preferences.putInt("S5", SETTING_AUTOMATIC_CLOSE_TIME);
  preferences.putInt("S6", SETTING_AUTOMATIC_CLOSE_WARNING_TIME);
  preferences.putBool("S8", SETTING_CLOSE_ON_TIME);
  preferences.end();

  httpServer.sendHeader("Location", "/");
  httpServer.send(302, "text/plain", "");
}

void handleSetAutoCloseTime()
{
  if (httpServer.hasArg("time")) {
  String timeString = httpServer.arg("time"); // Format: hh:mm
  int separatorIndex = timeString.indexOf(':');
  if (separatorIndex != -1) {
    SETTING_CLOSE_HOUR = timeString.substring(0, separatorIndex).toInt(); // Stunden extrahieren
    SETTING_CLOSE_MINUTE = timeString.substring(separatorIndex + 1).toInt(); // Minuten extrahieren

    Serial.println(SETTING_CLOSE_HOUR);
    Serial.println(SETTING_CLOSE_MINUTE);
    
    // Uhrzeit speichern (z. B. in Preferences)
    preferences.begin("GaragenESP", false);
    preferences.putInt("S9", SETTING_CLOSE_HOUR);
    preferences.putInt("S10", SETTING_CLOSE_MINUTE);
    preferences.end();
    logNachricht(17);
    logNachricht(18);
  }
}

httpServer.sendHeader("Location", "/");
httpServer.send(302, "text/plain", "");
}

void handleReadConfig()
{
  if (httpServer.hasArg("zuContact")) {
    String selectedPin = httpServer.arg("zuContact");
    if (selectedPin == "1")
    {
      SETTING_KONTAKT_CONFIG = true;
    }
    else if (selectedPin == "2")
    {
      SETTING_KONTAKT_CONFIG = false;
    }
    preferences.begin("GaragenESP", false);
    preferences.putBool("S7", SETTING_KONTAKT_CONFIG);
    preferences.end();

    readKontaktAuf = (!SETTING_KONTAKT_CONFIG)? defaultReadKontaktAuf:defaultReadKontaktZu;
    readKontaktZu = (!SETTING_KONTAKT_CONFIG)? defaultReadKontaktZu:defaultReadKontaktAuf;
  }

  httpServer.sendHeader("Location", "/");
  httpServer.send(302, "text/plain", "");
}