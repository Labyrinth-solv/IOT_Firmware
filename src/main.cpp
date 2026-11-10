#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <ArduinoJson.h>
#include <math.h>

#include "config.h"

// =============================================================================
// KHỞI TẠO ĐỐI TƯỢNG VÀ BIẾN TOÀN CỤC
// =============================================================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Cảm biến I2C phần cứng
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

// Cờ kiểm tra phần cứng vật lý có cắm hay không
bool hasRealAHT = false;
bool hasRealBMP = false;

// Trạng thái cơ cấu chấp hành (Relay 2 kênh) và cảm biến cửa
bool fanState = false;            // Kênh 1: false = OFF, true = ON
bool buzzerState = false;         // Kênh 2: false = OFF, true = ON
bool isDoorOpen = false;          // false = Cửa đóng, true = Cửa mở
String controlMode = "AUTO";      // "AUTO" hoặc "MANUAL"
String currentRiskLevel = "SAFE"; // "SAFE", "WARNING", "HIGH_RISK"

// Dữ liệu vi khí hậu hiện tại
float currentTemperature = 24.0f;
float currentHumidity = 60.0f;
float currentPressure = 1013.2f;  // Áp suất khí quyển (hPa)
float currentGasPPM = 400.0f;     // Nồng độ khí gas lên men (ppm)
float currentEMC = 12.5f;         // Độ ẩm cân bằng hạt (%MC - TCVN)

// Các bộ định thời (Non-blocking timers với millis)
unsigned long lastTelemetryTime = 0;
unsigned long lastMqttRetryTime = 0;
unsigned long lastStatusHeartbeatTime = 0;
unsigned long samplingIntervalMs = DEFAULT_SAMPLING_INTERVAL_MS;

// Topic MQTT động theo DEVICE_ID
String topicTelemetry = String("grain/devices/") + DEVICE_ID + "/telemetry";
String topicStatus    = String("grain/devices/") + DEVICE_ID + "/status";
String topicCmd       = String("grain/devices/") + DEVICE_ID + "/cmd";
String topicAck       = String("grain/devices/") + DEVICE_ID + "/ack";

// =============================================================================
// HÀM TÍNH TOÁN ĐỘ ẨM CÂN BẰNG HẠT (EMC) THEO MÔ HÌNH HALSEY / TCVN 8370
// =============================================================================
float calculateHalseyEMC(float tempC, float rhPercent) {
  float rhClamped = constrain(rhPercent, 1.0f, 99.0f);
  float rhDec = rhClamped / 100.0f;
  float num = -log(1.0f - rhDec);
  float den = 0.000105f * (tempC + 35.0f);
  if (den <= 0) return 12.0f;
  float emc = sqrt(num / den);
  return round(emc * 10.0f) / 10.0f;
}

// =============================================================================
// ĐIỀU KHIỂN MODULE RELAY 2 KÊNH (QUẠT THÔNG GIÓ & CÒI BÁO ĐỘNG 5V)
// =============================================================================
void setFanRelay(bool enable) {
  fanState = enable;
  if (RELAY_ACTIVE_LEVEL == LOW) {
    digitalWrite(PIN_RELAY_FAN, enable ? LOW : HIGH); // LOW kích hoạt
  } else {
    digitalWrite(PIN_RELAY_FAN, enable ? HIGH : LOW); // HIGH kích hoạt
  }
}

void setBuzzerRelay(bool enable) {
  buzzerState = enable;
  if (RELAY_ACTIVE_LEVEL == LOW) {
    digitalWrite(PIN_RELAY_BUZZER, enable ? LOW : HIGH); // LOW kích hoạt
  } else {
    digitalWrite(PIN_RELAY_BUZZER, enable ? HIGH : LOW); // HIGH kích hoạt
  }
}

// =============================================================================
// ĐỌC CẢM BIẾN (HỖ TRỢ AHT20, BMP280, MQ-135, CẢM BIẾN CỬA HALL A3122)
// =============================================================================
void readSensors() {
  // 1. Đọc Cảm biến Nhiệt độ & Độ ẩm AHT20 (I2C 0x38)
  if (hasRealAHT) {
    sensors_event_t humidityEvent, tempEvent;
    aht.getEvent(&humidityEvent, &tempEvent);
    if (!isnan(tempEvent.temperature) && !isnan(humidityEvent.relative_humidity)) {
      currentTemperature = round(tempEvent.temperature * 10.0f) / 10.0f;
      currentHumidity = round(humidityEvent.relative_humidity * 10.0f) / 10.0f;
    }
  } else {
    // Chế độ mô phỏng an toàn khi chưa cắm AHT20 vật lý
    currentTemperature = 24.2f + (random(-5, 6) / 10.0f);
    currentHumidity = 61.5f + (random(-8, 9) / 10.0f);
  }

  // 2. Đọc Cảm biến Áp suất khí quyển BMP280 (I2C 0x76 / 0x77)
  if (hasRealBMP) {
    float p = bmp.readPressure() / 100.0F; // Pa đổi sang hPa
    if (!isnan(p) && p > 300.0f && p < 1200.0f) {
      currentPressure = round(p * 10.0f) / 10.0f;
    }
  } else {
    currentPressure = 1013.2f + (random(-10, 11) / 10.0f);
  }

  // 3. Đọc Cảm biến chất lượng không khí & Gas lên men MQ-135 (ADC1 - GPIO 34)
  int rawADC = analogRead(PIN_MQ135_ANALOG);
  if (rawADC > 100) {
    currentGasPPM = map(rawADC, 0, 4095, 300, 1200);
  } else {
    currentGasPPM = 405.0f + random(-10, 11);
  }

  // 4. Đọc Cảm biến Cửa kho Hall A3122 (Digital Input - GPIO 27)
  int doorVal = digitalRead(PIN_DOOR_SENSOR);
  isDoorOpen = (doorVal == DOOR_STATE_OPEN_LOGIC);

  // 5. Tính toán độ ẩm hạt Halsey EMC
  currentEMC = calculateHalseyEMC(currentTemperature, currentHumidity);

  // 6. Đánh giá mức độ rủi ro theo quy chuẩn TCVN 8370:2018
  String evaluatedRisk = "SAFE";
  if (currentHumidity >= THRESHOLD_HUM_CRIT || currentTemperature >= THRESHOLD_TEMP_CRIT ||
      currentEMC >= THRESHOLD_EMC_CRIT || currentGasPPM >= THRESHOLD_GAS_CRIT) {
    evaluatedRisk = "HIGH_RISK";
  } else if (currentHumidity >= THRESHOLD_HUM_WARN || currentTemperature >= THRESHOLD_TEMP_WARN ||
             currentEMC >= THRESHOLD_EMC_WARN || currentGasPPM >= THRESHOLD_GAS_WARN) {
    evaluatedRisk = "WARNING";
  }

  currentRiskLevel = evaluatedRisk;

  // Cảnh báo Còi (Relay Kênh 2): Tự động hú khi HIGH_RISK hoặc cảnh báo mở cửa
  if (currentRiskLevel == "HIGH_RISK") {
    setBuzzerRelay(true);
  } else if (controlMode == "AUTO") {
    setBuzzerRelay(false);
  }

  // Vận hành tự chủ quạt tại biên (Edge Autonomous Fallback):
  // Khi ở chế độ AUTO, nếu rủi ro WARNING hoặc HIGH_RISK -> Tự động BẬT Quạt qua Relay Kênh 1
  if (controlMode == "AUTO") {
    if (evaluatedRisk == "WARNING" || evaluatedRisk == "HIGH_RISK") {
      setFanRelay(true);
    } else {
      setFanRelay(false);
    }
  }
}

// =============================================================================
// GỬI BẢN TIN TRẠNG THÁI /status VÀ TELEMETRY /telemetry
// =============================================================================
void sendStatusHeartbeat() {
  if (!mqttClient.connected()) return;

  JsonDocument doc;
  doc["device_id"]  = DEVICE_ID;
  doc["state"]      = "ONLINE";
  doc["ip_address"] = WiFi.localIP().toString();
  doc["fw_version"] = FIRMWARE_VERSION;
  doc["hw_version"] = HARDWARE_VERSION;
  doc["uptime_sec"] = millis() / 1000;
  doc["timestamp"]  = millis();

  char buffer[512];
  serializeJson(doc, buffer);
  mqttClient.publish(topicStatus.c_str(), buffer, true); // Retain flag
  Serial.printf("📡 [MQTT] Da phat ban tin Heartbeat ONLINE len %s\n", topicStatus.c_str());
}

void sendTelemetry() {
  if (!mqttClient.connected()) return;

  unsigned long nowSec = millis() / 1000;
  String msgId = "msg_esp_" + String(nowSec) + "_" + String(random(100, 999));

  JsonDocument doc;
  doc["msg_id"]     = msgId;
  doc["device_id"]  = DEVICE_ID;
  doc["fw_version"] = FIRMWARE_VERSION;
  doc["timestamp"]  = millis();

  // Metrics (Nhiệt độ, Độ ẩm không khí, Gas, EMC, Áp suất BMP280, Cửa kho)
  JsonObject metrics = doc["metrics"].to<JsonObject>();
  
  JsonObject mTemp = metrics["temperature"].to<JsonObject>();
  mTemp["value"] = currentTemperature;
  mTemp["unit"]  = "C";

  JsonObject mHum = metrics["humidity_air"].to<JsonObject>();
  mHum["value"] = currentHumidity;
  mHum["unit"]  = "%RH";

  JsonObject mGas = metrics["gas_ppm"].to<JsonObject>();
  mGas["value"] = currentGasPPM;
  mGas["unit"]  = "ppm";

  JsonObject mEMC = metrics["estimated_emc"].to<JsonObject>();
  mEMC["value"] = currentEMC;
  mEMC["unit"]  = "%MC";

  JsonObject mPress = metrics["pressure"].to<JsonObject>();
  mPress["value"] = currentPressure;
  mPress["unit"]  = "hPa";

  JsonObject mDoor = metrics["door_sensor"].to<JsonObject>();
  mDoor["is_open"] = isDoorOpen;
  mDoor["status"]  = isDoorOpen ? "OPEN" : "CLOSED";

  // Actuators & Risk Level
  JsonObject actuators = doc["actuators"].to<JsonObject>();
  actuators["fan_state"]    = fanState ? "ON" : "OFF";
  actuators["buzzer_state"] = buzzerState ? "ON" : "OFF";
  actuators["door_state"]   = isDoorOpen ? "OPEN" : "CLOSED";
  actuators["control_mode"] = controlMode;
  actuators["risk_level"]   = currentRiskLevel;

  doc["rssi"] = WiFi.RSSI();

  char buffer[768];
  serializeJson(doc, buffer);
  mqttClient.publish(topicTelemetry.c_str(), buffer, false);

  Serial.printf("📊 [Telemetry] T=%.1fC | RH=%.1f%% | P=%.1fhPa | EMC=%.1f%% | Gas=%.0f | Door=%s | Fan=%s | Buzzer=%s | Risk=%s\n",
                currentTemperature, currentHumidity, currentPressure, currentEMC, currentGasPPM,
                isDoorOpen ? "OPEN" : "CLOSED",
                fanState ? "ON" : "OFF",
                buzzerState ? "ON" : "OFF",
                currentRiskLevel.c_str());
}

// =============================================================================
// XỬ LÝ LỆNH ĐIỀU KHIỂN TỪ BACKEND (/cmd) VÀ PHẢN HỒI ACK (/ack)
// =============================================================================
void handleIncomingCommand(char* topic, byte* payload, unsigned int length) {
  char jsonBuffer[512];
  if (length >= sizeof(jsonBuffer)) length = sizeof(jsonBuffer) - 1;
  memcpy(jsonBuffer, payload, length);
  jsonBuffer[length] = '\0';

  Serial.printf("📥 [MQTT Command] Nhan lenh tu topic %s: %s\n", topic, jsonBuffer);

  JsonDocument cmdDoc;
  DeserializationError error = deserializeJson(cmdDoc, jsonBuffer);
  if (error) {
    Serial.println("❌ [MQTT] Giai ma JSON lenh that bai!");
    return;
  }

  const char* cmdId = cmdDoc["cmd_id"];
  const char* cmdType = cmdDoc["type"];
  JsonObject payloadObj = cmdDoc["payload"];

  String ackMessage = "Lenh thuc thi thanh cong";

  if (cmdType && strcmp(cmdType, "ACTUATOR_CONTROL") == 0) {
    const char* action = payloadObj["action"];
    const char* target = payloadObj["target"];
    const char* mode = payloadObj["control_mode"];

    if (mode) controlMode = String(mode);

    // 1. Điều khiển Quạt thông gió (Relay Kênh 1 - GPIO 26)
    if (!target || strcmp(target, "VENTILATION_FAN") == 0) {
      if (action && strcmp(action, "TURN_ON") == 0) {
        setFanRelay(true);
        ackMessage = "Relay K1 GPIO 26 activated (Quat thong gio BAT)";
      } else if (action && strcmp(action, "TURN_OFF") == 0) {
        setFanRelay(false);
        ackMessage = "Relay K1 GPIO 26 deactivated (Quat thong gio TAT)";
      }
    }
    // 2. Điều khiển Còi báo động (Relay Kênh 2 - GPIO 25)
    else if (strcmp(target, "ALARM_BUZZER") == 0) {
      if (action && strcmp(action, "TURN_ON") == 0) {
        setBuzzerRelay(true);
        ackMessage = "Relay K2 GPIO 25 activated (Coi bao dong BAT)";
      } else if (action && strcmp(action, "TURN_OFF") == 0) {
        setBuzzerRelay(false);
        ackMessage = "Relay K2 GPIO 25 deactivated (Coi bao dong TAT)";
      }
    }
  } else if (cmdType && strcmp(cmdType, "UPDATE_CONFIG") == 0) {
    unsigned long interval = payloadObj["sampling_interval_ms"];
    if (interval >= 1000) {
      samplingIntervalMs = interval;
      ackMessage = "Da cap nhat chu ky lay mau: " + String(samplingIntervalMs) + "ms";
    }
  }

  // Tự động phản hồi bản tin xác nhận /ack sau 300ms
  delay(300);

  JsonDocument ackDoc;
  ackDoc["cmd_id"]      = cmdId ? cmdId : "cmd_unknown";
  ackDoc["device_id"]   = DEVICE_ID;
  ackDoc["status"]      = "SUCCESS";
  ackDoc["executed_at"] = millis();

  JsonObject currentActs = ackDoc["current_actuators"].to<JsonObject>();
  currentActs["fan_state"]    = fanState ? "ON" : "OFF";
  currentActs["buzzer_state"] = buzzerState ? "ON" : "OFF";
  currentActs["door_state"]   = isDoorOpen ? "OPEN" : "CLOSED";
  currentActs["control_mode"] = controlMode;

  ackDoc["message"] = ackMessage;

  char ackBuffer[512];
  serializeJson(ackDoc, ackBuffer);
  mqttClient.publish(topicAck.c_str(), ackBuffer, false);

  Serial.printf("📤 [MQTT ACK] Da gui phan hoi ACK: %s\n", ackMessage.c_str());
}

// =============================================================================
// QUẢN LÝ KẾT NỐI WI-FI VÀ MQTT BROKER (NON-BLOCKING AUTO-RECONNECT)
// =============================================================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("📡 [Wi-Fi] Dang ket noi toi SSID: %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n✅ [Wi-Fi] Ket noi thanh cong! IP: %s | RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("\n⚠️ [Wi-Fi] Chua the ket noi Wi-Fi, se thu lai ngam...");
  }
}

void connectMQTT() {
  if (mqttClient.connected() || WiFi.status() != WL_CONNECTED) return;

  Serial.printf("🔌 [MQTT] Dang ket noi toi Broker: %s:%d...\n", MQTT_SERVER_IP, MQTT_SERVER_PORT);

  // Chuẩn bị bản tin Last Will and Testament (LWT) khi thiết bị đứt kết nối đột ngột
  String lwtPayload = "{\"device_id\":\"" + String(DEVICE_ID) + "\",\"state\":\"OFFLINE\"}";

  bool success = false;
  if (strlen(MQTT_USERNAME) > 0) {
    success = mqttClient.connect(DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD,
                                 topicStatus.c_str(), 1, true, lwtPayload.c_str());
  } else {
    success = mqttClient.connect(DEVICE_ID, topicStatus.c_str(), 1, true, lwtPayload.c_str());
  }

  if (success) {
    Serial.println("✅ [MQTT] Ket noi MQTT Broker thanh cong!");

    // Subscribe nhận lệnh điều khiển từ Backend (/cmd)
    mqttClient.subscribe(topicCmd.c_str(), 1);
    Serial.printf("📡 [MQTT] Da subscribe topic dieu khien: %s\n", topicCmd.c_str());

    // Phát bản tin ONLINE đầu tiên
    sendStatusHeartbeat();
  } else {
    Serial.printf("❌ [MQTT] Ket noi that bai, ma loi rc=%d\n", mqttClient.state());
  }
}

// =============================================================================
// SETUP KHỞI TẠO HỆ THỐNG
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================================");
  Serial.printf("🌾 SMART GRAIN STORAGE IOT NODE - %s (TCVN 8370)\n", DEVICE_ID);
  Serial.printf("Firmware v%s | Hardware: %s\n", FIRMWARE_VERSION, HARDWARE_VERSION);
  Serial.println("========================================================");

  // 1. Cấu hình chân Relay 2 kênh và Cảm biến cửa
  pinMode(PIN_RELAY_FAN, OUTPUT);
  pinMode(PIN_RELAY_BUZZER, OUTPUT);
  pinMode(PIN_DOOR_SENSOR, INPUT); // Chân cảm biến Hall A3122 (có trở kéo 10k về GND)

  // Trạng thái ban đầu: Tắt Quạt, Tắt Còi
  setFanRelay(false);
  setBuzzerRelay(false);

  // 2. Khởi tạo Bus I2C (SDA=21, SCL=22)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // 3. Khởi tạo Cảm biến AHT20 (Địa chỉ mặc định 0x38)
  if (aht.begin()) {
    hasRealAHT = true;
    Serial.println("✅ [Hardware] Cam bien nhiet am AHT20 san sang (I2C 0x38)!");
  } else {
    Serial.println("⚠️ [Hardware] Khong tim thay AHT20! Tu dong chay Che Do Gia Lap Test an toan.");
  }

  // 4. Khởi tạo Cảm biến Áp suất BMP280 (Hỗ trợ cả 2 địa chỉ 0x76 và 0x77)
  if (bmp.begin(0x76) || bmp.begin(0x77)) {
    hasRealBMP = true;
    Serial.println("✅ [Hardware] Cam bien ap suat BMP280 san sang (I2C)!");
    // Cài đặt tối ưu cho đo thời tiết / kho hàng
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  } else {
    Serial.println("⚠️ [Hardware] Khong tim thay BMP280 (Se dung gia lap ap suat tieu chuan).");
  }

  // 5. Cấu hình MQTT Client
  mqttClient.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  mqttClient.setCallback(handleIncomingCommand);
  mqttClient.setBufferSize(768);

  // 6. Kết nối Wi-Fi & MQTT
  connectWiFi();
  connectMQTT();
}

// =============================================================================
// VÒNG LẶP CHÍNH (NON-BLOCKING MAIN LOOP)
// =============================================================================
void loop() {
  unsigned long currentMillis = millis();

  // A. Tự động kết nối lại Wi-Fi và MQTT mỗi 5 giây nếu mất mạng
  if (currentMillis - lastMqttRetryTime >= 5000) {
    lastMqttRetryTime = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    } else if (!mqttClient.connected()) {
      connectMQTT();
    }
  }

  // B. Giữ nhịp MQTT Client loop lắng nghe lệnh từ Broker
  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  // C. Đọc cảm biến định kỳ và gửi Telemetry (Mặc định mỗi 3000ms)
  if (currentMillis - lastTelemetryTime >= samplingIntervalMs) {
    lastTelemetryTime = currentMillis;
    readSensors();
    sendTelemetry();
  }

  // D. Gửi Heartbeat định kỳ mỗi 30 giây để duy trì Online
  if (currentMillis - lastStatusHeartbeatTime >= 30000) {
    lastStatusHeartbeatTime = currentMillis;
    sendStatusHeartbeat();
  }
}
