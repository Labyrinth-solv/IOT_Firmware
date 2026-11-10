#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// 1. CẤU HÌNH KẾT NỐI WI-FI VÀ MQTT BROKER
// =============================================================================
// Thay đổi thông tin mạng Wi-Fi của bạn tại đây:
#define WIFI_SSID           "VT_VN"
#define WIFI_PASSWORD       "duc12345678"

// Địa chỉ IP của máy tính chạy Docker Mosquitto MQTT Broker (VD: "192.168.1.100")
// Lưu ý: KHÔNG dùng "localhost" hay "127.0.0.1" vì ESP32 kết nối qua mạng LAN
#define MQTT_SERVER_IP      "10.250.190.42"
#define MQTT_SERVER_PORT    1883
#define MQTT_USERNAME       ""
#define MQTT_PASSWORD       ""

// =============================================================================
// 2. ĐỊNH DANH THIẾT BỊ (DEVICE IDENTITY & PROVISIONING)
// =============================================================================
#define DEVICE_ID           "DEV_SILO_01"
#define FIRMWARE_VERSION    "1.0.2"
#define HARDWARE_VERSION    "ESP32-WROOM-32-v1"

// Chu kỳ lấy mẫu và gửi bản tin Telemetry mặc định (ms)
#define DEFAULT_SAMPLING_INTERVAL_MS 3000

// =============================================================================
// 3. SƠ ĐỒ ĐẤU NỐI CHÂN GPIO (HARDWARE PINOUT TCVN 8370)
// =============================================================================
// Bus I2C dùng chung cho Cảm biến SHT31 và Màn hình OLED SSD1306
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22
#define SHT31_I2C_ADDR      0x44
#define OLED_I2C_ADDR       0x3C
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64

// Cảm biến chất lượng không khí & khí Gas lên men MQ-135
// Sử dụng chân ADC1 (GPIO 34) an toàn khi đang bật Wi-Fi
#define PIN_MQ135_ANALOG    34

// Cơ cấu chấp hành: Module Relay 5V điều khiển Quạt thông gió DC
#define PIN_RELAY_FAN       26
// Mức kích hoạt Relay (Nhiều module Relay 5V kích hoạt ở mức LOW = LOW_LEVEL_TRIGGER)
#define RELAY_ACTIVE_LEVEL  HIGH  // Đổi thành LOW nếu module Relay của bạn kích hoạt mức LOW

// Còi báo động Buzzer chủ động
#define PIN_BUZZER          25

// Cụm 3 Đèn LED hiển thị mức rủi ro TCVN
#define PIN_LED_GREEN       14    // Mức SAFE (An toàn)
#define PIN_LED_YELLOW      27    // Mức WARNING (Cảnh báo)
#define PIN_LED_RED         12    // Mức HIGH_RISK (Nguy hiểm)

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
