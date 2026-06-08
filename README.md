# Elevator Vibration Monitoring and Fault Detection Using Edge Impulse

## Giới thiệu

Dự án xây dựng hệ thống giám sát rung động và phát hiện lỗi thang máy bằng TinyML chạy trực tiếp trên vi điều khiển ESP32-S3. Mô hình được huấn luyện bằng Edge Impulse để nhận diện các trạng thái rung động bất thường và thực hiện suy luận thời gian thực ngay trên thiết bị nhúng.

## Mục tiêu

* Giám sát rung động thang máy theo thời gian thực.
* Phân loại 3 trạng thái hoạt động:

  * `normal` – Hoạt động bình thường.
  * `misalignment` – Lệch trục cơ khí.
  * `unbalance` – Mất cân bằng tải.
* Độ chính xác tối thiểu 95%.
* Độ trễ suy luận dưới 50 ms.
* Tối ưu tài nguyên cho hệ thống nhúng.

## Công nghệ sử dụng

### Phần mềm

* Edge Impulse Studio
* Arduino IDE 2.x
* TinyML
* Keras Neural Network
* EON Compiler
* TensorFlow Lite for Microcontrollers

### Phần cứng

| Thiết bị            | Vai trò                           |
| ------------------- | --------------------------------- |
| ESP32-S3 DevKit     | Chạy firmware và mô hình TinyML   |
| MPU6050             | Thu thập dữ liệu rung động 3 trục |
| Breadboard & Jumper | Kết nối phần cứng                 |
| USB-C 5V            | Cấp nguồn hệ thống                |

## Dataset

Nguồn dữ liệu:

* Public Dataset trên Edge Impulse Public Projects (#966202)

Thông tin:

* Tổng số mẫu: 300
* Số lớp: 3
* 100 mẫu/lớp
* Tần số lấy mẫu: 100 Hz
* Tỷ lệ Train/Test: 80% / 20%

## Kiến trúc mô hình

### Input

* Gia tốc 3 trục:

  * accX
  * accY
  * accZ

### Feature Extraction

* Spectral Analysis
* FFT 128 điểm
* Trích xuất:

  * RMS
  * Peak Frequency
  * Peak Power
  * Standard Deviation

### Classifier

Keras Neural Network:

* Input: 39 features
* Dense(40, ReLU)
* Dense(20, ReLU)
* Dense(10, ReLU)
* Dropout(0.25)
* Output Softmax (3 lớp)

## Cấu hình huấn luyện

| Thông số             | Giá trị |
| -------------------- | ------- |
| Epochs               | 100     |
| Learning Rate        | 0.0005  |
| Validation Split     | 20%     |
| Confidence Threshold | 0.6     |

## Kết quả

### Hiệu năng mô hình

| Chỉ số              | Giá trị |
| ------------------- | ------- |
| Validation Accuracy | 100%    |
| Test Accuracy       | 98.33%  |
| F1-Score            | 0.98    |

### Hiệu năng trên ESP32-S3

| Thông số       | Giá trị |
| -------------- | ------- |
| Latency        | 6 ms    |
| RAM            | 3.1 KB  |
| Flash          | 16.9 KB |
| Runtime Errors | 0%      |

## Kiểm thử

### TC01 – Normal

* Điều kiện: Hệ thống hoạt động bình thường.
* Kết quả: Nhận dạng đúng `normal` với confidence 100%.

### TC02 – Misalignment

* Điều kiện: Mô phỏng lệch trục.
* Kết quả: Nhận dạng đúng `misalignment` với confidence 92.5%.

### TC03 – Sensor Error

* Điều kiện: Ngắt kết nối SDA.
* Kết quả: Firmware phát hiện lỗi và tiếp tục hoạt động.

### TC04 – Stress Test

* Điều kiện: Chạy liên tục 12 giờ.
* Kết quả:

  * Không treo hệ thống.
  * Không rò rỉ bộ nhớ.
  * Latency ổn định.

## Kết luận

Dự án đã triển khai thành công hệ thống TinyML phát hiện lỗi rung động thang máy trên ESP32-S3. Mô hình đạt độ chính xác 98.33%, sử dụng ít tài nguyên và đáp ứng tốt yêu cầu thời gian thực.

## Hướng phát triển

* Thu thập thêm dữ liệu thực tế từ thang máy công nghiệp.
* Tích hợp MQTT/WiFi để gửi cảnh báo từ xa.
* Bổ sung Watchdog Timer.
* Tối ưu tiêu thụ năng lượng bằng chế độ Sleep của ESP32-S3.

## Tài liệu tham khảo

* TinyML – O'Reilly Media
* Edge Impulse Documentation
* TensorFlow Lite for Microcontrollers
* ESP-IDF Documentation
* CMSIS Documentation

## Tác giả

**Trương Nguyễn Duy Khang**
