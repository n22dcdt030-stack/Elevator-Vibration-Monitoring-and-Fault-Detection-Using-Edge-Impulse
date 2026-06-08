// ======================================================
// ELEVATOR VIBRATION MONITORING & FAULT DETECTION
// ESP32-S3 + MPU6050 (Raw I2C, addr 0x68) + Edge Impulse
// Project: vibration | Owner: Duy-Khang
// ======================================================

#include <Wire.h>
#include <vibration_inferencing.h>

// ======================================================
// CONFIGURATION
// ======================================================

#define SDA_PIN 8
#define SCL_PIN 9

// MPU6050 I2C address (AD0 = GND -> 0x68)
#define MPU6050_ADDR        0x68

// MPU6050 Register map
#define REG_SMPLRT_DIV      0x19
#define REG_CONFIG          0x1A
#define REG_GYRO_CONFIG     0x1B
#define REG_ACCEL_CONFIG    0x1C
#define REG_ACCEL_XOUT_H    0x3B   // 6 bytes: AX_H AX_L AY_H AY_L AZ_H AZ_L
#define REG_PWR_MGMT_1      0x6B

// Từ metadata:
// EI_CLASSIFIER_RAW_SAMPLE_COUNT      = 200
// EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME = 3
// EI_CLASSIFIER_INTERVAL_MS           = 10   (100 Hz)
// EI_CLASSIFIER_FREQUENCY             = 100

#define SAMPLE_COUNT (EI_CLASSIFIER_RAW_SAMPLE_COUNT * EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME)
// = 200 * 3 = 600 features

// Accel Full-Scale ±2g  -> sensitivity = 16384 LSB/g
#define ACCEL_SENSITIVITY   16384.0f

// ======================================================
// GLOBAL VARIABLES
// ======================================================

float features[SAMPLE_COUNT];

// ======================================================
// MPU6050 RAW I2C HELPERS
// ======================================================

// Ghi 1 byte vào register
bool mpuWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

// Đọc nhiều byte liên tiếp từ register
bool mpuRead(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;

    Wire.requestFrom((uint8_t)MPU6050_ADDR, len);
    uint8_t i = 0;
    while (Wire.available() && i < len) {
        buf[i++] = Wire.read();
    }
    return (i == len);
}

// Combine 2 bytes -> int16
inline int16_t toInt16(uint8_t hi, uint8_t lo) {
    return (int16_t)((hi << 8) | lo);
}

// ======================================================
// MPU6050 INIT
// ======================================================

bool mpuInit() {

    // Wake up: xoá SLEEP bit (bit 6) trong PWR_MGMT_1
    // Dùng PLL with X-axis gyro (0x01) thay clock nội
    if (!mpuWrite(REG_PWR_MGMT_1, 0x01)) return false;
    delay(100);

    // Sample Rate Divider
    // SMPLRT_DIV = (Gyro rate / target) - 1
    // Gyro output rate khi DLPF on = 1kHz
    // Target 100 Hz -> divider = 1000/100 - 1 = 9
    if (!mpuWrite(REG_SMPLRT_DIV, 9)) return false;

    // DLPF_CFG = 3 -> bandwidth ~44Hz, delay 4.9ms
    // Phù hợp Nyquist 50Hz cho 100Hz sampling
    if (!mpuWrite(REG_CONFIG, 0x03)) return false;

    // Gyro Full-Scale ±500°/s (FS_SEL = 1)
    if (!mpuWrite(REG_GYRO_CONFIG, 0x08)) return false;

    // Accel Full-Scale ±2g (AFS_SEL = 0)
    if (!mpuWrite(REG_ACCEL_CONFIG, 0x00)) return false;

    delay(100);
    return true;
}

// ======================================================
// ĐỌC GIA TỐC (3 TRỤC) -> đơn vị g
// ======================================================

bool mpuReadAccel(float &ax, float &ay, float &az) {
    uint8_t buf[6];
    if (!mpuRead(REG_ACCEL_XOUT_H, buf, 6)) return false;

    int16_t raw_x = toInt16(buf[0], buf[1]);
    int16_t raw_y = toInt16(buf[2], buf[3]);
    int16_t raw_z = toInt16(buf[4], buf[5]);

    ax = raw_x / ACCEL_SENSITIVITY;
    ay = raw_y / ACCEL_SENSITIVITY;
    az = raw_z / ACCEL_SENSITIVITY;

    return true;
}

// ======================================================
// SETUP
// ======================================================

void setup() {

    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("ELEVATOR VIBRATION MONITOR");
    Serial.println("Project : " EI_CLASSIFIER_PROJECT_NAME);
    Serial.println("Owner   : " EI_CLASSIFIER_PROJECT_OWNER);
    Serial.println("Engine  : TFLite EON Compiled");
    Serial.println("I2C     : Raw (no Adafruit lib)");
    Serial.println("=================================");
    Serial.println();

    Serial.print("Sample rate : ");
    Serial.print(EI_CLASSIFIER_FREQUENCY);
    Serial.println(" Hz");

    Serial.print("Window      : ");
    Serial.print(EI_CLASSIFIER_RAW_SAMPLE_COUNT * EI_CLASSIFIER_INTERVAL_MS);
    Serial.println(" ms");

    Serial.print("Features    : ");
    Serial.println(SAMPLE_COUNT);
    Serial.println();

    // Initialize I2C
    Serial.print("Initializing I2C (SDA=");
    Serial.print(SDA_PIN);
    Serial.print(", SCL=");
    Serial.print(SCL_PIN);
    Serial.print(")... ");
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);  // 400kHz Fast Mode
    delay(100);
    Serial.println("OK");

    // Initialize MPU6050 via raw I2C
    Serial.print("Initializing MPU6050 (0x68)... ");
    if (!mpuInit()) {
        Serial.println("FAILED!");
        while (1) {
            Serial.println("ERROR: MPU6050 init failed!");
            delay(1000);
        }
    }
    Serial.println("OK");

    Serial.println("System Ready!");
    Serial.println();
}

// ======================================================
// MAIN LOOP
// ======================================================

void loop() {

    if (!collectSensorData()) {
        Serial.println("Sensor collection failed!");
        delay(500);
        return;
    }

    if (!runInference()) {
        Serial.println("Inference failed!");
        delay(500);
        return;
    }

    // Interval giữa các lần inference
    delay(500);
}

// ======================================================
// COLLECT SENSOR DATA
// ======================================================

bool collectSensorData() {

    Serial.println("---------------------------------");
    Serial.println("Collecting vibration data...");

    int sample_index = 0;
    unsigned long last_sample_ms = millis();

    while (sample_index < EI_CLASSIFIER_RAW_SAMPLE_COUNT) {

        if (millis() - last_sample_ms >= EI_CLASSIFIER_INTERVAL_MS) {

            last_sample_ms += EI_CLASSIFIER_INTERVAL_MS;

            float ax, ay, az;
            if (!mpuReadAccel(ax, ay, az)) {
                Serial.println("Read error!");
                return false;
            }

            int base = sample_index * EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;

            features[base + 0] = ax;
            features[base + 1] = ay;
            features[base + 2] = az - 1.0f;   // bù trọng lực trục Z

            sample_index++;
        }
    }

    Serial.print("Samples collected : ");
    Serial.println(sample_index);
    Serial.print("Features total    : ");
    Serial.println(SAMPLE_COUNT);

    return true;
}

// ======================================================
// RUN EDGE IMPULSE INFERENCE
// ======================================================

bool runInference() {

    Serial.println("Running TFLite inference...");

    signal_t signal;
    ei_impulse_result_t result;

    if (numpy::signal_from_buffer(features,
                                  SAMPLE_COUNT,
                                  &signal) != EIDSP_OK) {
        Serial.println("Signal creation FAILED!");
        return false;
    }

    uint32_t t_start = millis();
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    uint32_t t_infer = millis() - t_start;

    if (res != EI_IMPULSE_OK) {
        Serial.print("Classifier error: ");
        Serial.println(res);
        return false;
    }

    Serial.print("Inference time : ");
    Serial.print(t_infer);
    Serial.println(" ms");

    printResults(result);
    return true;
}

// ======================================================
// PRINT RESULTS
// ======================================================

void printResults(ei_impulse_result_t result) {

    Serial.println();
    Serial.println("========== RESULTS ==========");

    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        Serial.print("  ");
        Serial.print(result.classification[ix].label);
        Serial.print(": ");
        Serial.print(result.classification[ix].value * 100.0f, 1);
        Serial.println("%");
    }

    Serial.println();

    float    best_val = 0.0f;
    uint8_t  best_idx = 0;

    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if (result.classification[ix].value > best_val) {
            best_val = result.classification[ix].value;
            best_idx = ix;
        }
    }

    const char* status;

    if (best_val < EI_CLASSIFIER_THRESHOLD) {
        status = "UNCERTAIN";
    } else {
        // Categories: misalignment[0], normal[1], unbalance[2]
        if      (best_idx == 0) status = "FAULT: MISALIGNMENT";
        else if (best_idx == 1) status = "NORMAL";
        else if (best_idx == 2) status = "FAULT: UNBALANCE";
        else                    status = "UNKNOWN";
    }

    Serial.print("SYSTEM STATUS: ");
    Serial.println(status);
    Serial.print("Confidence    : ");
    Serial.print(best_val * 100.0f, 1);
    Serial.println("%");
    Serial.println("=============================");
    Serial.println();
}

// ======================================================
// NOTES
// ======================================================

/*

HARDWARE CONNECTIONS
====================

MPU6050 -> ESP32-S3

VCC -> 3.3V
GND -> GND
SDA -> GPIO 8
SCL -> GPIO 9
AD0 -> GND  (địa chỉ 0x68)

----------------------------------------------------

THƯ VIỆN CẦN CÀI
=================

1. Edge Impulse: vibration_inferencing (Arduino .zip)
   (KHÔNG cần Adafruit MPU6050 / Adafruit Unified Sensor)

----------------------------------------------------

REGISTER SETTINGS (tóm tắt)
============================

PWR_MGMT_1  (0x6B) = 0x01  -> wake up, dùng PLL X-gyro clock
SMPLRT_DIV  (0x19) = 9     -> 1000Hz / (9+1) = 100Hz
CONFIG      (0x1A) = 0x03  -> DLPF bandwidth ~44Hz
GYRO_CONFIG (0x1B) = 0x08  -> FS = ±500°/s
ACCEL_CONFIG(0x1C) = 0x00  -> FS = ±2g, sensitivity=16384 LSB/g

----------------------------------------------------

THÔNG SỐ MODEL (từ model_metadata.h)
=====================================

Project      : vibration
Owner        : Duy-Khang
Frequency    : 100 Hz
Window       : 200 samples = 2000 ms
Features     : 600 (200 x 3 axes)
NN input     : 39 (sau DSP Spectral Analysis)
Engine       : TFLite EON Compiled / INT8
Threshold    : 0.6
Classes      : misalignment, normal, unbalance

*/
