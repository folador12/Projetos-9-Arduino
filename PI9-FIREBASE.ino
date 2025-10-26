#define ENABLE_RTDB
#define DISABLE_FCM
#define DISABLE_FIRESTORE
#define DISABLE_STORAGE


#include <Firebase_ESP_Client.h>
#include <WiFi.h>
#include <DHT.h> 
#include <time.h>

// ===== CONFIGURAÇÕES DO WIFI =====
#define WIFI_SSID "Luiz"
#define WIFI_PASSWORD "anamaria123"

// ===== CONFIGURAÇÕES DO FIREBASE =====
#define API_KEY "AIzaSyCQKn4WqEztR81ioubHPd078SQ16gDmRHk"
#define DATABASE_URL "https://smartlar-10b81-default-rtdb.firebaseio.com/"  // RTDB URL

// ===== INSTÂNCIAS =====
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ===== Pinos dos LEDs =====
const int ledLivingRoom = 15;
const int ledBedroom    = 12;
const int ledKitchen    = 4;
const int ledBathroom   = 25;
const int ledOffice     = 33;
const int ledGarage     = 32;

// ===== Pinos dos Sensores =====
#define DHTPIN 26
#define LDR_ANALOG 36
#define LDR_DIGITAL 13

DHT dht(DHTPIN, DHT22);

// ===== Timestamp =====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600; // Horário de Brasília (UTC-3)
const int daylightOffset_sec = 0;

// ===== Tempo para leitura dos sensores ===== 
unsigned long lastSendTime = 0;
//const unsigned long sendInterval = 5 * 60 * 1000; // 5 minutos
const unsigned long sendInterval = 60 * 1000;

// ===== Função auxiliar pra atualizar LEDs =====
void updateLEDs(bool living, bool bedroom, bool kitchen, bool bathroom, bool office, bool garage) {
  digitalWrite(ledLivingRoom, living ? HIGH : LOW);
  digitalWrite(ledBedroom, bedroom ? HIGH : LOW);
  digitalWrite(ledKitchen, kitchen ? HIGH : LOW);
  digitalWrite(ledBathroom, bathroom ? HIGH : LOW);
  digitalWrite(ledOffice, office ? HIGH : LOW);
  digitalWrite(ledGarage, garage ? HIGH : LOW);
}

// ===== Função para buscar timestamp =====
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "unknown";
  
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", &timeinfo);
  
  return String(timestamp);
}

// ===== Função para enviar dados dos sensores =====
void sendSensorData() {
  String timestamp = getTimestamp();

  // Leitura do DHT22
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Leitura do LDR
  int lightAnalog = analogRead(LDR_ANALOG);
  int lightDigital = digitalRead(LDR_DIGITAL);

  Serial.println("🌡️ Enviando dados ao Firebase...");
  Serial.printf("Temp: %.1f °C | Umidade: %.1f %% | Luz: %d / %d\n",
                temperature, humidity, lightAnalog, lightDigital);

  // ===== Envio dos dados para o Firebase =====
  if (!Firebase.RTDB.setFloat(&fbdo, "/history/dht/" + timestamp + "/temperature", temperature))
    Serial.println(fbdo.errorReason());

  if (!Firebase.RTDB.setFloat(&fbdo, "/history/dht/" + timestamp + "/humidity", humidity))
    Serial.println(fbdo.errorReason());

  if (!Firebase.RTDB.setInt(&fbdo, "/history/ldr/" + timestamp + "/analog", lightAnalog))
    Serial.println(fbdo.errorReason());

  if (!Firebase.RTDB.setInt(&fbdo, "/history/ldr/" + timestamp + "/digital", lightDigital))
    Serial.println(fbdo.errorReason());
}

void setup() {
  Serial.begin(115200);

  dht.begin();

  // Configura pinos dos LEDs como saída
  pinMode(ledLivingRoom, OUTPUT);
  pinMode(ledBedroom, OUTPUT);
  pinMode(ledKitchen, OUTPUT);
  pinMode(ledBathroom, OUTPUT);
  pinMode(ledOffice, OUTPUT);
  pinMode(ledGarage, OUTPUT);

  // Configura LDR
  pinMode(LDR_DIGITAL, INPUT);

  // ===== Conecta ao Wi-Fi =====
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ Wi-Fi conectado!");

  // ===== Configura o Firebase =====
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // ===== Listener em tempo real =====
  if (!Firebase.RTDB.beginStream(&fbdo, "/lights")) {
    Serial.println("Erro ao iniciar stream!");
    Serial.println(fbdo.errorReason());
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  // ===== Recebe atualizações de LEDs =====
  if (!Firebase.RTDB.readStream(&fbdo)) {
    Serial.println("❌ Erro no stream!");
    Serial.println(fbdo.errorReason());
  }

  if (fbdo.streamAvailable()) {
    Serial.println("📡 Atualização recebida do Firebase!");

    bool living   = Firebase.RTDB.getBool(&fbdo, "/lights/livingRoom") ? fbdo.boolData() : false;
    bool bedroom  = Firebase.RTDB.getBool(&fbdo, "/lights/bedroom")    ? fbdo.boolData() : false;
    bool kitchen  = Firebase.RTDB.getBool(&fbdo, "/lights/kitchen")    ? fbdo.boolData() : false;
    bool bathroom = Firebase.RTDB.getBool(&fbdo, "/lights/bathroom")   ? fbdo.boolData() : false;
    bool office   = Firebase.RTDB.getBool(&fbdo, "/lights/office")     ? fbdo.boolData() : false;
    bool garage   = Firebase.RTDB.getBool(&fbdo, "/lights/garage")     ? fbdo.boolData() : false;

    updateLEDs(living, bedroom, kitchen, bathroom, office, garage);
  }

  // ===== Timer para enviar dados dos sensores =====
  unsigned long currentMillis = millis();

  if (currentMillis - lastSendTime >= sendInterval) {
    lastSendTime = currentMillis;

    Serial.println("⏱️ 5 minutos passaram! Enviando dados...");

    sendSensorData();
  }
}
