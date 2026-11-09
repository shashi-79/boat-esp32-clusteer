#pragma once
// --- Wi-Fi SoftAP (Direct Phone & ESP32-CAM Connection) ---
#define WIFI_SSID           "RescueBoat-GCS"
#define WIFI_PASSWORD       "rescue2026"
#define WIFI_CHANNEL        1
#define WIFI_MAX_CONN       4

// --- Web Server & WebSocket ---
#define WEB_PORT            80
#define WS_PATH             "/ws"
#define CAM_NODE_IP         "192.168.4.2"
#define CAM_STREAM_PORT     81

// --- Failsafe & Operational Modes ---
#define FAILSAFE_TIMEOUT_MS 1500   // Trigger RTH if phone disconnects > 1.5s
#define LOW_BATT_3S_MV      10500  // 3.5V/cell
#define LOW_BATT_4S_MV      14000  // 3.5V/cell
#define RTH_CRUISE_PERCENT  30
#define RTH_ARRIVAL_RADIUS_M 3.0f

#define MODE_MANUAL         0
#define MODE_RTH            1
#define MODE_FAILSAFE       2

// --- Motor ESC Pins (Bidirectional, 1500µs neutral) ---
#define PIN_ESC_LEFT        25
#define PIN_ESC_RIGHT       26

// --- Camera Servo Pins ---
#define PIN_SERVO_PAN       27
#define PIN_SERVO_TILT      32

// --- Future Expansion Servo Pins ---
#define PIN_SERVO_AUX1      13
#define PIN_SERVO_AUX2      14
#define PIN_SERVO_AUX3      23

// --- I²C Bus (IMU / Compass) ---
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

// --- GPS UART1 ---
#define PIN_GPS_RX          16   // ESP32 RX ← GPS TX (WROOM without PSRAM)
#define PIN_GPS_TX          17   // ESP32 TX → GPS RX
#define GPS_BAUD            9600

// --- 24GHz Radar UART2 ---
#define PIN_RADAR_RX        18   // ESP32 RX ← Radar TX
#define PIN_RADAR_TX        19   // ESP32 TX → Radar RX
#define RADAR_BAUD          256000

// --- Battery Monitoring (INA219 I2C + ADC fallback) ---
#define INA219_I2C_ADDR     0x40   // Default I2C address for INA219
#define LIPO_CELL_COUNT     3      // 3 = 3S (11.1V/12.6V), 4 = 4S (14.8V/16.8V)

// ADC Fallback / Logic Battery Monitor
#define PIN_BATT_MOTOR      34     // ADC1 input-only, safe while Wi-Fi is active
#define PIN_BATT_LOGIC      35     // ADC1 input-only, safe while Wi-Fi is active
#define BATT_MOTOR_RATIO    11.0f  // 10k + 1k divider ratio
#define BATT_LOGIC_RATIO    6.0f   // 10k + 2k divider ratio
#define ADC_REF_MV          3300   // Classic ESP32 ADC reference voltage

// --- ESC PWM Configuration ---
#define ESC_FREQ_HZ         50
#define ESC_NEUTRAL_US      1500
#define ESC_MIN_US          1000
#define ESC_MAX_US          2000
#define ESC_DEADBAND         30    // ±30µs around neutral = dead zone

// --- Servo PWM Configuration ---
#define SERVO_FREQ_HZ       50
#define SERVO_MIN_US        500
#define SERVO_MAX_US        2500
#define SERVO_CENTER_US     1500

// --- LEDC Channel Assignment ---
#define LEDC_CH_ESC_LEFT    0
#define LEDC_CH_ESC_RIGHT   1
#define LEDC_CH_PAN         2
#define LEDC_CH_TILT        3
#define LEDC_CH_AUX1        4
#define LEDC_CH_AUX2        5
#define LEDC_CH_AUX3        6

// --- Safety Thresholds ---
#define ARM_MIN_SATS        6
#define ARM_MIN_HDOP        2.0f

// --- RTH PID Tuning ---
#define RTH_KP              2.0f
#define RTH_KI              0.1f
#define RTH_KD              0.5f

// --- Debug ---
#ifdef DEBUG_ENABLED
  #define DBG(fmt, ...) Serial.printf("[DBG] " fmt "\n", ##__VA_ARGS__)
#else
  #define DBG(fmt, ...)
#endif
