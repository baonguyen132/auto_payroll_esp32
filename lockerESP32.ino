#include <WiFi.h>         // Thư viện điều khiển WiFi cho ESP32
#include <HTTPClient.h>   // Thư viện hỗ trợ HTTP client (GET/POST)
#include <ArduinoJson.h>  // Thư viện xử lý JSON (nếu cần parse/build JSON)
#include <Keypad.h>       // Thư viện đọc bàn phím ma trận (4x4)
#include <SPI.h>          // Thư viện SPI để giao tiếp với MFRC522 (RFID)
#include <MFRC522.h>      // Thư viện điều khiển module RFID MFRC522
#include <LiquidCrystal_I2C.h>

// ------------------------- CẤU HÌNH BÀN PHÍM -------------------------
#define ROW_NUM 4 
#define COLUMN_NUM 4

// Bàn phím 4x4 - ma trận kí tự
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Chọn chân GPIO cho các hàng và cột (phù hợp ESP32; tránh chân chỉ input như 34,35,...)
byte pin_rows[] = {14, 27, 26, 25};    // 4 chân nối hàng (ROW) của keypad
byte pin_column[] = {33, 32, 16, 17};  // 4 chân nối cột (COLUMN) của keypad

// Khởi tạo object Keypad với layout + chân
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

// ------------------------- CẤU HÌNH RFID MFRC522 -------------------------
#define SS_PIN 5   // Chân SDA (SS) nối module RC522
#define RST_PIN 0   // Chân RST nối module RC522

MFRC522 mfrc522(SS_PIN, RST_PIN);     // Object điều khiển MFRC522

// ------------------------- CẤU HÌNH LCD -------------------------

LiquidCrystal_I2C lcd(0x27,16,2);

// ------------------------- CẤU HÌNH WIFI -------------------------
const char* ssid = "Bao Nguyen";           // SSID WiFi
const char* password = "hbnguyen0213";     // Mật khẩu WiFi

char jsonOutput[128] ;

// ------------------------- CẤU HÌNH RADIO -------------------------

#define LED_RADIO 2 

// ------------------------- BIẾN TOÀN CỤC -------------------------
String number = "";   // Lưu chuỗi số người dùng nhập từ keypad
char status = ' ';    // Trạng thái hệ thống (A/B/C/D hoặc ' ' khi rảnh)

// ------------------------- HÀM KHỞI TẠO -------------------------
void setup() {
  Serial.begin(115200);   // Khởi động Serial Monitor để debug

  // --- Khởi tạo SPI và RFID ---
  SPI.begin();            // Bắt đầu giao tiếp SPI (RC522 dùng SPI)
  mfrc522.PCD_Init();     // Khởi tạo RC522

  // --- Kết nối WiFi ---
  WiFi.mode(WIFI_STA);    // Đặt module ở chế độ Station (client)
  WiFi.begin(ssid, password); // Kết nối tới router

  // Chờ kết nối thành công (hiển thị dấu chấm khi chờ)
  Serial.println("\nConnecting to WiFi Network ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  // Khi đã connect
  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // --- Cấu hình keypad ---
  keypad.setDebounceTime(50);  // Debounce để tránh đọc trùng phím

  // --- Cấu hình radio and led ---
  pinMode(LED_RADIO , OUTPUT) ;

  // Setup LCD with backlight and initialize
  lcd.init();
  lcd.backlight();
  lcd.print("Creating...");
  delay(1000);
  lcd.clear();


}

// ------------------------- HÀM THAY ĐỔI TRẠNG THÁI (A/B/C/D) -------------------------
// Đọc phím và nếu là A..D thì cập nhật `status`
void handleChangeStatus() {
  char key = keypad.getKey();
  if (key && key >= 'A' && key <= 'D') {
    status = key;    // đặt trạng thái, để loop xử lý sau
    if(key == 'A') Serial.println("Đang ở trạng thái A - quét thẻ RFID"); // Trạng thái A: quét thẻ RFID và in ID nếu có
    else if(key == 'B') Serial.println("Đang ở trạng thái B - Quét thẻ vào"); // Trạng thái B: Quét thẻ vào
    else if(key == 'C') Serial.println("Đang ở trạng thái C - Quét thẻ ra"); // Trạng thái C: nhập số từ keypad, nhấn # để gửi
    else if(key == 'D') {
      Serial.println("Đang ở trạng thái D");
      lcd.print("Assign user");
      delay(1000);
      lcd.clear();
      lcd.print("Input number");
      delay(1000);
      lcd.clear();
    } // Trạng thái D: placeholder (bạn có thể thêm hành động cụ thể)
  }
}

// ------------------------- HÀM XỬ LÝ NHẬP SỐ TỪ KEYPAD -------------------------
// Trả về true nếu người dùng nhấn '#', tức yêu cầu gửi; false nếu chỉ nhập/sửa số
bool handleChangeNumber() {
  char key = keypad.getKey();

  // Chỉ xử lý khi có phím được nhấn
  if (key) {
    // Nếu là số 0-9 → gán vào chuỗi number
    if (key >= '0' && key <= '9') {  
      number += key;
      Serial.println("Number: " + number);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Number: ");
      lcd.print(number);
    }
    // Nếu nhấn '*' → xóa chuỗi hiện tại
    else if (key == '*') {  
      number = "";  
      Serial.println("Cleared number.");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Cleared");
    }
    // Nếu nhấn '#' → báo là cần gửi
    else if (key == '#') {  
      Serial.println("Send request with number: " + number);
      return true;
    }
  }

  // Mặc định không gửi nếu chưa nhấn '#'
  return false;
}


// ------------------------- HÀM ĐỌC RFID -------------------------
// Trả về chuỗi ID thẻ (HEX) nếu có thẻ mới, hoặc rỗng nếu không có
String handleReadRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return  "";
  }

  // Gửi UID qua Serial
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
      content += String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      content += String(mfrc522.uid.uidByte[i], HEX);
  }
  digitalWrite(LED_RADIO, HIGH) ;
  delay(100);
  digitalWrite(LED_RADIO, LOW) ;
  return content;
}

String requestPostAddCard(String url, String id) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient client;

    client.begin(url);
    client.addHeader("Content-Type", "application/json");

    // Gửi dữ liệu JSON lên server
    StaticJsonDocument<64> doc;
    doc["code"] = id;

    String jsonOutput;
    serializeJson(doc, jsonOutput);

    int httpCode = client.POST(jsonOutput);
    String payload = "";

    if (httpCode > 0) {
      payload = client.getString();

      Serial.println("✅ POST thành công:");
      Serial.println("URL: " + url);
      Serial.println("Status code: " + String(httpCode));
      Serial.println("Response: " + payload);
    } else {
      Serial.println("❌ POST thất bại:");
      Serial.println("URL: " + url);
      Serial.println("Error code: " + String(httpCode));
      payload = "POST failed, error: " + String(httpCode);
    }

    client.end(); // luôn đóng kết nối
    return payload;

  } else {
    Serial.println("⚠️ WiFi không kết nối!");
    return "Connection lost";
  }
}

void requestPostScanCard(String url, String id , int access_type ) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient client;

    client.begin(url);
    client.addHeader("Content-Type", "application/json");

    // Gửi dữ liệu JSON lên server
    StaticJsonDocument<64> doc;
    doc["code"] = id;
    doc["access_type"] = access_type ;

    String jsonOutput;
    serializeJson(doc, jsonOutput);

    int httpCode = client.POST(jsonOutput);
    String payload = "";

    if (httpCode == 200) {
      payload = client.getString();

      Serial.println("✅ POST thành công:");
      Serial.println("URL: " + url);
      Serial.println("Status code: " + String(httpCode));
      Serial.println("Response: " + payload);

      lcd.print("Open Door");
      digitalWrite(LED_RADIO, HIGH) ;
      delay(500);
      digitalWrite(LED_RADIO, LOW) ;
      
    }
    else {
      Serial.println("❌ POST thất bại:");
      Serial.println("URL: " + url);
      Serial.println("Error code: " + String(httpCode));
      payload = "POST failed, error: " + String(httpCode);

      if(httpCode == 404) lcd.print("Not user") ;
      else if(httpCode == 400) lcd.print("Card not user") ;
      else if(httpCode == 500) lcd.print("Server error") ;

      digitalWrite(LED_RADIO, HIGH) ;
      delay(100);
      digitalWrite(LED_RADIO, LOW) ;
      delay(100);
      digitalWrite(LED_RADIO, HIGH) ;
      delay(100);
      digitalWrite(LED_RADIO, LOW) ;

    }
    client.end();

  } else {
    Serial.println("⚠️ WiFi không kết nối!");
  }
}

void requestPostAssign(String url, String id) { 
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient client;

    client.begin(url);
    client.addHeader("Content-Type", "application/json");

    // Chuẩn bị JSON gửi lên server
    StaticJsonDocument<128> doc;
    doc["code"] = id;        // API của bạn dùng "card_id", không phải "code"
    doc["user_id"] = number;    // number là biến toàn cục đang lưu user_id
    String jsonOutput;
    serializeJson(doc, jsonOutput);

    Serial.println("📤 Đang gửi request POST...");
    Serial.println(jsonOutput);

    int httpCode = client.POST(jsonOutput);
    String payload = client.getString();

    if (httpCode == 200) {
      Serial.println("✅ POST thành công:");
      Serial.println("URL: " + url);
      Serial.println("Status code: " + String(httpCode));
      Serial.println("Response: " + payload);

      lcd.clear();
      lcd.print("Assign success");

      // Nháy đèn 5 lần khi thành công
      for (int i = 0; i < 5; i++) {
        digitalWrite(LED_RADIO, HIGH);
        delay(100);
        digitalWrite(LED_RADIO, LOW);
        delay(100);
      }

    } else {
      Serial.println("❌ POST thất bại:");
      Serial.println("URL: " + url);
      Serial.println("HTTP Code: " + String(httpCode));
      Serial.println("Response: " + payload);

      lcd.clear();
      if (httpCode == 500) {
        lcd.print("Server error");
      } else {
        lcd.print("POST failed");
      }

      // Nháy đèn 2 lần khi lỗi
      for (int i = 0; i < 2; i++) {
        digitalWrite(LED_RADIO, HIGH);
        delay(200);
        digitalWrite(LED_RADIO, LOW);
        delay(200);
      }
    }

    client.end();

  } else {
    Serial.println("⚠️ WiFi không kết nối!");
    lcd.clear();
    lcd.print("WiFi error");
  }
}




// ------------------------- HÀM XỬ LÝ TRẠNG THÁI -------------------------
void handleStatus() {
  if (status == 'A') {
    String id = handleReadRFID();
    if (id != "") {
      Serial.println("Mã thẻ: " + id);

      // Gửi POST request
      String data = requestPostAddCard("https://intentional-entirely-darci.ngrok-free.dev/api/cards", id);

      // Gỡ bỏ ký tự thừa (nếu có BOM hoặc newline)
      data.trim();
      Serial.println("Phản hồi từ server: " + data);

      // Parse JSON phản hồi
      StaticJsonDocument<512> docResult;
      DeserializationError error = deserializeJson(docResult, data);

      if (error) {
        Serial.print("❌ Lỗi phân tích JSON: ");
        Serial.println(error.c_str());
        return;
      }

      // Lấy dữ liệu từ JSON
      const char* code = docResult["code"];
      const char* message = docResult["message"];

      if (message) {
        Serial.print("✅ Phản hồi: ");
        Serial.println(message);
        lcd.clear();
        lcd.print(message);
      }
      

      if (code) {
        Serial.print("🔁 Mã phản hồi: ");
        Serial.println(code);
        lcd.setCursor(0, 1);
        lcd.print(code);
      }
    }
    delay(1000); // tránh đọc liên tục
    status = ' '; // reset trạng thái
    lcd.clear();
  }
  else if (status == 'B') {
    String id = handleReadRFID();
    if(id != "") {
      requestPostScanCard("https://intentional-entirely-darci.ngrok-free.dev/api/access-log", id , 0 );
      delay(1000); // tránh đọc liên tục
      status = ' '; // reset trạng thái
      lcd.clear();
    }
    
  }
  else if (status == 'C') {
    String id = handleReadRFID();
    if(id != "") {
      requestPostScanCard("https://intentional-entirely-darci.ngrok-free.dev/api/access-log", id , 1);
      delay(1000); // tránh đọc liên tục
      status = ' '; // reset trạng thái
      lcd.clear();
    }
  }
  else if (status == 'D') {
    bool needSend = handleChangeNumber();
    while(!needSend) {
      needSend = handleChangeNumber();
    }
    lcd.clear();
    
    if (number.length() > 0) {
      String id = "";
      while (id == "") {
        id = handleReadRFID();
        delay(100);  // tránh vòng lặp quá nhanh gây treo CPU
      }

      Serial.println("📡 Đã đọc thẻ RFID: " + id);
      Serial.println("👤 User ID: " + number);

      requestPostAssign("https://intentional-entirely-darci.ngrok-free.dev/api/cards/assign", id);
    }
    number = "";     // reset sau khi gửi
    delay(500);      // tránh gửi liên tiếp
    status = ' ';  
  }
}

// ------------------------- VÒNG LẶP CHÍNH -------------------------
void loop() {
  // Nếu rảnh thì chỉ lắng nghe thay đổi trạng thái (A..D)
  if (status == ' ') {
    handleChangeStatus();
  } else {
    // Nếu đang ở trạng thái nào đó -> thực hiện hành động tương ứng
    handleStatus();
  }
  delay(50); // ngắt vòng lặp nhỏ để tránh đọc phím quá nhanh
}


