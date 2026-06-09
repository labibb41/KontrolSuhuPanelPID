#include <WiFi.h>
#include <WiFiClientSecure.h> // Wajib untuk port 8883 (TLS/SSL)
#include <PubSubClient.h>     // Library MQTT
#include <DHT.h>
#include <PID_v1_bc.h>
#include <ArduinoJson.h>      // Digunakan untuk parsing data kontrol JSON dari Cloud

// ==========================
// Konfigurasi WiFi & EMQX Cloud
// ==========================
#define WIFI_SSID "yoii"
#define WIFI_PASSWORD "123456789"

const char* mqtt_server = "s6ddf312.ala.asia-southeast1.emqxsl.com"; 
const int mqtt_port = 8883; 
const char* mqtt_user = "ESP32_ZAQI";                          
const char* mqtt_pass = "Zaqi123"; 

// Topic MQTT
#define TOPIC_MONITORING "monitoring/data"
#define TOPIC_KONTROL    "kontrol/#"         // Menerima semua data kontrol di bawah prefix /kontrol

// ==========================
// Konfigurasi Hardware
// ==========================
#define DHTPIN 4
#define DHTTYPE DHT22

#define RELAY_PIN_1 14
#define RELAY_PIN_2 13

// ==========================
// Variabel PID
// ==========================
double Setpoint = 28.0;
double InputSuhu = 0;
double OutputPID = 0;

double Kp = 2000;
double Ki = 5;
double Kd = 1;

PID myPID(&InputSuhu, &OutputPID, &Setpoint, Kp, Ki, Kd, REVERSE);

int WindowSize = 5000;
unsigned long windowStartTime;

// ==========================
// Objek Klien
// ==========================
DHT dht(DHTPIN, DHTTYPE);

WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long sendDataPrevMillis = 0;
const unsigned long updateInterval = 5000;

bool relay1State = false;
bool relay2State = false;
String modeOperasi = "auto";
String relay1Command = "OFF";
String relay2Command = "OFF";

// ==========================
// Fungsi bantu kontrol
// ==========================
void setRelayState(bool relay1On, bool relay2On)
{
  relay1State = relay1On;
  relay2State = relay2On;

  digitalWrite(RELAY_PIN_1, relay1State ? HIGH : LOW);
  digitalWrite(RELAY_PIN_2, relay2State ? HIGH : LOW);
}

void updatePidTunings()
{
  myPID.SetTunings(Kp, Ki, Kd);

  Serial.printf(
    "Parameter Baru -> Kp=%.2f Ki=%.2f Kd=%.2f Setpoint=%.2f Mode=%s\n",
    Kp, Ki, Kd, Setpoint, modeOperasi.c_str()
  );
}

// ==========================
// Callback MQTT (Pengganti Stream Firebase)
// ==========================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  String strTopic = String(topic);
  Serial.printf("Message arrived [%s]: %s\n", topic, message.c_str());

  // Memproses data kontrol tunggal (Contoh jika dikirim via topic terpisah: kontrol/Kp)
  if (strTopic.endsWith("Kp")) {
    Kp = message.toDouble();
    updatePidTunings();
  } 
  else if (strTopic.endsWith("Ki")) {
    Ki = message.toDouble();
    updatePidTunings();
  } 
  else if (strTopic.endsWith("Kd")) {
    Kd = message.toDouble();
    updatePidTunings();
  } 
  else if (strTopic.endsWith("Setpoint")) {
    Setpoint = message.toDouble();
    Serial.printf("Setpoint diperbarui: %.2f C\n", Setpoint);
  } 
  else if (strTopic.endsWith("mode")) {
    modeOperasi = message;
    modeOperasi.toLowerCase();
    Serial.printf("Mode operasi diperbarui: %s\n", modeOperasi.c_str());
  } 
  else if (strTopic.endsWith("relay1_command")) {
    relay1Command = message;
    relay1Command.toUpperCase();
    Serial.printf("Command Relay 1: %s\n", relay1Command.c_str());
  } 
  else if (strTopic.endsWith("relay2_command")) {
    relay2Command = message;
    relay2Command.toUpperCase();
    Serial.printf("Command Relay 2: %s\n", relay2Command.c_str());
  }
  
  // OPSI JSON: Jika Anda mengirim konfigurasi sekaligus berbentuk JSON ke topic "kontrol"
  else if (strTopic == "kontrol") {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (!error) {
      if (doc.containsKey("Kp")) Kp = doc["Kp"].as<double>();
      if (doc.containsKey("Ki")) Ki = doc["Ki"].as<double>();
      if (doc.containsKey("Kd")) Kd = doc["Kd"].as<double>();
      if (doc.containsKey("Setpoint")) Setpoint = doc["Setpoint"].as<double>();
      if (doc.containsKey("mode")) {
        modeOperasi = doc["mode"].as<String>();
        modeOperasi.toLowerCase();
      }
      if (doc.containsKey("relay1_command")) {
        relay1Command = doc["relay1_command"].as<String>();
        relay1Command.toUpperCase();
      }
      if (doc.containsKey("relay2_command")) {
        relay2Command = doc["relay2_command"].as<String>();
        relay2Command.toUpperCase();
      }
      updatePidTunings();
    }
  }
}

void setup_wifi() {
  delay(10);
  Serial.println("\nMenghubungkan WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Mencoba koneksi MQTT ke EMQX Cloud...");
    
    // Membuat Client ID Unik
    String clientId = "ESP32-ZAQI-";
    clientId += String(random(0, 0xffff), HEX);
    
    // Connect menggunakan Username dan Password EMQX Cloud
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("CONNECTED to EMQX Cloud!");
      
      // Subscribe kembali ke topic kontrol setelah berhasil terhubung
      client.subscribe(TOPIC_KONTROL);
      client.subscribe("kontrol"); // Menjaga jika payload berbentuk JSON utuh
    } else {
      Serial.print("gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 5 detik");
      delay(5000);
    }
  }
}

// ==========================
// Setup
// ==========================
void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);

  digitalWrite(RELAY_PIN_1, LOW);
  digitalWrite(RELAY_PIN_2, LOW);

  dht.begin();
  setup_wifi();

  // BAGIAN PENTING: Mengabaikan pengecekan sertifikat SSL agar ESP32 lancar di port 8883
  espClient.setInsecure(); 

  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  windowStartTime = millis();

  myPID.SetOutputLimits(0, WindowSize);
  myPID.SetMode(AUTOMATIC);

  Serial.println("Sistem Siap");
}

// ==========================
// Loop
// ==========================
void loop()
{
  // Pastikan koneksi MQTT tetap terjaga
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  float suhu = dht.readTemperature();

  if (!isnan(suhu))
  {
    InputSuhu = suhu;
  }

  myPID.Compute();

  unsigned long now = millis();

  if ((now - windowStartTime) > WindowSize)
  {
    windowStartTime += WindowSize;
  }

  if (modeOperasi == "manual")
  {
    setRelayState(
      relay1Command == "ON",
      relay2Command == "ON"
    );
  }
  // Kontrol relay otomatis berdasarkan PID
  else if (OutputPID > (now - windowStartTime))
  {
    setRelayState(true, true);
  }
  else
  {
    setRelayState(false, false);
  }

  // Kirim data monitoring ke EMQX Cloud setiap 5 detik
  if (millis() - sendDataPrevMillis > updateInterval)
  {
    sendDataPrevMillis = millis();

    String statusRelay1 = relay1State ? "ON" : "OFF";
    String statusRelay2 = relay2State ? "ON" : "OFF";
    String statusKipas = (relay1State || relay2State) ? "ON" : "OFF";

    Serial.println("==========");
    Serial.printf("Suhu sekarang : %.2f C\n", InputSuhu);
    Serial.printf("Setpoint      : %.2f C\n", Setpoint);
    Serial.printf("Mode operasi  : %s\n", modeOperasi.c_str());
    Serial.printf("Relay 1       : %s\n", statusRelay1.c_str());
    Serial.printf("Relay 2       : %s\n", statusRelay2.c_str());
    Serial.printf("PID Output    : %.2f\n", OutputPID);

    // Membunder data monitoring menjadi satu JSON utuh untuk dikirim ke MQTT Broker
    StaticJsonDocument<256> doc;
    doc["suhu"] = InputSuhu;
    doc["output_pid"] = OutputPID;
    doc["setpoint"] = Setpoint;
    doc["mode"] = modeOperasi;
    doc["relay1"] = statusRelay1;
    doc["relay2"] = statusRelay2;
    doc["status_kipas"] = statusKipas;

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);

    // Publish data ke topic "monitoring/data"
    if (client.connected()) {
      client.publish(TOPIC_MONITORING, jsonBuffer);
      Serial.println("Data Berhasil di-Publish ke EMQX Cloud.");
    }
  }
}