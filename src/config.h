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
#define FIRMWARE_VERSION    "1.1.0"
#define HARDWARE_VERSION    "ESP32-WROOM-32D-v2"

// Chu kỳ lấy mẫu và gửi bản tin Telemetry mặc định (ms)
#define DEFAULT_SAMPLING_INTERVAL_MS 3000

// =============================================================================
// 3. SƠ ĐỒ ĐẤU NỐI CHÂN GPIO (HARDWARE PINOUT KHỚP SƠ ĐỒ LẮP MẠCH THỰC TẾ)
// =============================================================================
// Bus I2C dùng chung cho Cảm biến AHT20 và BMP280
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

// Cảm biến chất lượng không khí & khí Gas lên men MQ-135
// Chân AO nối vào GPIO 34 (kênh ADC1 an toàn khi bật Wi-Fi)
#define PIN_MQ135_ANALOG    34

// Cảm biến cửa kho (Hall Sensor A3122)
// Chân OUT nối vào GPIO 27 (kèm điện trở kéo xuống 10k ohm về GND)
#define PIN_DOOR_SENSOR     27
// Trạng thái mức logic khi CỬA MỞ (với trở kéo xuống 10k: HIGH = Hở mạch/Mở cửa)
#define DOOR_STATE_OPEN_LOGIC HIGH

// Module Relay 2 kênh 5VDC:
// - Kênh 1: Điều khiển Quạt thông gió 5V
#define PIN_RELAY_FAN       26
// - Kênh 2: Điều khiển Còi báo động 5V
#define PIN_RELAY_BUZZER    25

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
