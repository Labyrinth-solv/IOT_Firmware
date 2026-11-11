#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// 1. CẤU HÌNH KẾT NỐI WI-FI VÀ MQTT BROKER
// =============================================================================
#define WIFI_SSID           "VT_VN"
#define WIFI_PASSWORD       "duc12345678"

// Địa chỉ IP của máy tính chạy Docker Mosquitto MQTT Broker
#define MQTT_SERVER_IP      "10.250.190.42"
#define MQTT_SERVER_PORT    1883
#define MQTT_USERNAME       ""
#define MQTT_PASSWORD       ""

// =============================================================================
// 2. ĐỊNH DANH THIẾT BỊ (DEVICE IDENTITY & PROVISIONING)
// =============================================================================
#define DEVICE_ID           "DEV_SILO_01"
#define FIRMWARE_VERSION    "1.1.1"
#define HARDWARE_VERSION    "ESP32-WROOM-32D-v2"

// Chu kỳ lấy mẫu và gửi bản tin Telemetry mặc định (ms)
#define DEFAULT_SAMPLING_INTERVAL_MS 3000

// =============================================================================
// 3. SƠ ĐỒ ĐẤU NỐI CHÂN GPIO (KHỚP BẢNG ĐẤU NỐI THỰC TẾ)
// =============================================================================
// Cảm biến nhiệt độ & độ ẩm AHT20 (Nguồn 3.3V ESP32, Bus I2C địa chỉ 0x38)
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

// Cảm biến chất lượng không khí & khí Gas MQ-135 (Nguồn 5V)
// Chân AO qua cầu chia áp (15kΩ + 10kΩ) -> GPIO 34 (ADC1 an toàn khi bật Wi-Fi)
// Điện áp tối đa sau cầu chia áp: 5V * 10/(15+10) = 2.0V -> rawADC ~ 2482
#define PIN_MQ135_ANALOG    34
#define MQ135_ADC_MAX       2482

// Cảm biến từ cửa kho (D0 -> GPIO 27, VCC -> 3.3V nếu có, GND -> GND)
#define PIN_DOOR_SENSOR     27
// Dùng INPUT_PULLUP để chống nhiễu cho cả module D0 và công tắc từ 2 dây nối GND
// (Nếu dùng mạch có điện trở kéo xuống 10kΩ ngoài, đổi thành INPUT)
#define DOOR_PIN_MODE       INPUT_PULLUP
// Trạng thái logic khi CỬA MỞ (đổi thành LOW nếu cảm biến xuất LOW khi mở cửa)
#define DOOR_STATE_OPEN_LOGIC HIGH

// Module Relay 2 kênh 5VDC (Nguồn 5V tổ ong, GND chung):
// - Kênh 1 (IN1): Điều khiển Quạt 5V qua tiếp điểm COM1 - NO1
#define PIN_RELAY_FAN       25
// - Kênh 2 (IN2): Điều khiển Còi 5V qua tiếp điểm COM2 - NO2
//   (Theo Bảng 2 & 5 dùng GPIO 33; nếu dây thực tế cắm ở GPIO 26 như Bảng 4 thì đổi thành 26)
#define PIN_RELAY_BUZZER    33

// Mức kích hoạt Module Relay (Đa số module Relay 5V kích mức LOW = LOW_LEVEL_TRIGGER)
// Nếu module của bạn kích mức HIGH, hãy đổi thành HIGH
#define RELAY_ACTIVE_LEVEL  LOW

// =============================================================================
// 4. NGƯỠNG AN TOÀN VI KHÍ HẬU MẶC ĐỊNH THEO TCVN 8370:2018
// =============================================================================
#define THRESHOLD_TEMP_WARN 25.0f // Ngưỡng nhiệt độ cảnh báo (°C)
#define THRESHOLD_TEMP_CRIT 28.0f // Ngưỡng nhiệt độ nguy hiểm (°C)
#define THRESHOLD_HUM_WARN  65.0f // Độ ẩm không khí cảnh báo (%RH)
#define THRESHOLD_HUM_CRIT  70.0f // Độ ẩm không khí nguy hiểm (%RH)
#define THRESHOLD_EMC_WARN  14.0f // Độ ẩm hạt cảnh báo (%MC - TCVN)
#define THRESHOLD_EMC_CRIT  14.8f // Độ ẩm hạt nguy hiểm (%MC)
#define THRESHOLD_GAS_WARN  600.0f // Nồng độ khí gas lên men cảnh báo (ppm)
#define THRESHOLD_GAS_CRIT  700.0f // Nồng độ khí gas nguy hiểm (ppm)

#endif // CONFIG_H
