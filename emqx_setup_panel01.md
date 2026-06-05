# Setup EMQX untuk ESP32 DHT22 PID Relay

Saya coba akses API EMQX lokal, status broker aktif, tetapi API menolak kredensial default `admin:public`. Jadi konfigurasi berikut dibuat agar bisa langsung diisi lewat dashboard EMQX yang sedang terbuka.

## 1. Built-in Database Users

Buka `Authentication -> Built-in Database -> User Management -> Add`.

Tambahkan user ESP32:

```text
Username: esp_panel01
Password: EspPID2026!
Is superuser: false
```

Tambahkan user web:

```text
Username: web_panel01
Password: WebPanel2026!
Is superuser: false
```

Kode ESP memakai `esp_panel01`, sedangkan `script.js` web sudah saya set memakai `web_panel01`.

## 2. Topic MQTT

ESP32 publish:

```text
panel01/suhu
panel01/kelembapan
panel01/status
panel01/relay
panel01/relay2
panel01/pid/output
```

ESP32 subscribe:

```text
panel01/setpoint
panel01/mode
panel01/relay/set
panel01/relay2/set
panel01/pid/kp
panel01/pid/ki
panel01/pid/kd
panel01/calibration/offset
```

Web subscribe:

```text
panel01/#
```

Web publish:

```text
panel01/setpoint
panel01/mode
panel01/relay/set
panel01/relay2/set
```

## 3. ACL File

Buka `Authorization -> File -> Settings`, lalu masukkan isi file:

```text
emqx_acl_panel01.conf
```

Penting: kalau di ACL lama masih ada `{allow, all}.`, ganti menjadi `{deny, all}.` atau letakkan rule deny terakhir seperti di file ACL yang saya buat.

## 4. Rules EMQX

Rule ini opsional untuk logging/monitoring dari EMQX. Buka `Rules -> Create`.

Rule suhu:

```sql
SELECT
  clientid,
  topic,
  payload,
  timestamp
FROM
  "panel01/suhu"
```

Action yang disarankan:

```text
Action: Republish
Topic: panel01/rules/suhu_log
QoS: 0
Retain: false
Payload:
{"clientid":"${clientid}","topic":"${topic}","suhu":${payload},"timestamp":${timestamp}}
```

Rule status relay:

```sql
SELECT
  clientid,
  topic,
  payload,
  timestamp
FROM
  "panel01/relay", "panel01/relay2"
```

Action:

```text
Action: Republish
Topic: panel01/rules/relay_log
QoS: 0
Retain: false
Payload:
{"clientid":"${clientid}","topic":"${topic}","state":"${payload}","timestamp":${timestamp}}
```

## 5. Arduino IDE

Install library:

```text
PubSubClient by Nick O'Leary
DHT sensor library by Adafruit
Adafruit Unified Sensor
```

Buka file:

```text
esp32_dht22_pid_relay.ino
```

Ubah bagian ini:

```cpp
const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";
const char* MQTT_HOST = "10.159.203.52";
```

Pin default:

```text
DHT22 DATA: GPIO 4
Relay 1: GPIO 26
Relay 2: GPIO 27
```

Kalau relay kamu aktif HIGH, ubah:

```cpp
const bool RELAY_ACTIVE_LOW = false;
```
