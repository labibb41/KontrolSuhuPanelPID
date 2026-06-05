#include <WiFi.h>
#include <DHT.h>
#include <PID_v1_bc.h>
#include <Firebase_ESP_Client.h>

#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// ==========================
// Konfigurasi WiFi & Firebase
// ==========================
#define WIFI_SSID "yoii"
#define WIFI_PASSWORD "123456789"

#define API_KEY "AIzaSyBdbq2LHD1n6smpjI67h2Um48ysPfVqUCo"
#define DATABASE_URL "https://kontrolpanel-f4a91-default-rtdb.firebaseio.com/"

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
// Objek
// ==========================
DHT dht(DHTPIN, DHTTYPE);

FirebaseData fbdo;
FirebaseData fbdo_stream;
FirebaseAuth auth;
FirebaseConfig config;

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
    Kp,
    Ki,
    Kd,
    Setpoint,
    modeOperasi.c_str()
  );
}

void applyKontrolValue(String path, String type, FirebaseStream &data)
{
  path.replace("/", "");

  if (path == "Kp")
  {
    Kp = data.doubleData();
    updatePidTunings();
  }
  else if (path == "Ki")
  {
    Ki = data.doubleData();
    updatePidTunings();
  }
  else if (path == "Kd")
  {
    Kd = data.doubleData();
    updatePidTunings();
  }
  else if (path == "Setpoint")
  {
    Setpoint = data.doubleData();
    Serial.printf("Setpoint dari web diperbarui: %.2f C\n", Setpoint);
  }
  else if (path == "mode")
  {
    modeOperasi = data.stringData();
    modeOperasi.toLowerCase();
    Serial.printf("Mode operasi diperbarui: %s\n", modeOperasi.c_str());
  }
  else if (path == "relay1_command")
  {
    relay1Command = data.stringData();
    relay1Command.toUpperCase();
    Serial.printf("Command Relay 1: %s\n", relay1Command.c_str());
  }
  else if (path == "relay2_command")
  {
    relay2Command = data.stringData();
    relay2Command.toUpperCase();
    Serial.printf("Command Relay 2: %s\n", relay2Command.c_str());
  }
}

// ==========================
// Callback Firebase Stream
// ==========================
void streamCallback(FirebaseStream data)
{
  String path = data.dataPath();
  String type = data.dataType();

  if (data.dataType() == "json")
  {
    FirebaseJson *json = data.to<FirebaseJson *>();
    FirebaseJsonData jsonData;

    if (json->get(jsonData, "Kp"))
      Kp = jsonData.doubleValue;

    if (json->get(jsonData, "Ki"))
      Ki = jsonData.doubleValue;

    if (json->get(jsonData, "Kd"))
      Kd = jsonData.doubleValue;

    if (json->get(jsonData, "Setpoint"))
      Setpoint = jsonData.doubleValue;

    if (json->get(jsonData, "mode"))
    {
      modeOperasi = jsonData.stringValue;
      modeOperasi.toLowerCase();
    }

    if (json->get(jsonData, "relay1_command"))
    {
      relay1Command = jsonData.stringValue;
      relay1Command.toUpperCase();
    }

    if (json->get(jsonData, "relay2_command"))
    {
      relay2Command = jsonData.stringValue;
      relay2Command.toUpperCase();
    }

    updatePidTunings();
  }
  else
  {
    applyKontrolValue(path, type, data);
  }
}

void streamTimeoutCallback(bool timeout)
{
  if (timeout)
  {
    Serial.println("Stream timeout, reconnect...");
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

  Serial.println("Menghubungkan WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Terhubung");
  Serial.println(WiFi.localIP());

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", ""))
  {
    Serial.println("Firebase SignUp Berhasil");
  }
  else
  {
    Serial.printf(
      "SignUp Gagal: %s\n",
      config.signer.signupError.message.c_str()
    );
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Stream parameter PID
  if (!Firebase.RTDB.beginStream(&fbdo_stream, "/kontrol"))
  {
    Serial.printf(
      "Gagal Stream: %s\n",
      fbdo_stream.errorReason().c_str()
    );
  }

  Firebase.RTDB.setStreamCallback(
    &fbdo_stream,
    streamCallback,
    streamTimeoutCallback
  );

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

    if (Firebase.ready())
    {
      Firebase.RTDB.setFloat(
        &fbdo,
        "/monitoring/suhu",
        InputSuhu
      );

      Firebase.RTDB.setFloat(
        &fbdo,
        "/monitoring/output_pid",
        OutputPID
      );

      Firebase.RTDB.setFloat(
        &fbdo,
        "/monitoring/setpoint",
        Setpoint
      );

      Firebase.RTDB.setString(
        &fbdo,
        "/monitoring/mode",
        modeOperasi
      );

      Firebase.RTDB.setString(
        &fbdo,
        "/monitoring/relay1",
        statusRelay1
      );

      Firebase.RTDB.setString(
        &fbdo,
        "/monitoring/relay2",
        statusRelay2
      );

      Firebase.RTDB.setString(
        &fbdo,
        "/monitoring/status_kipas",
        statusKipas
      );
    }
  }
}
