#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// LCD配置
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 引脚定义
#define LED_PIN 4
#define BUTTON_PIN 5

// WiFi配置
const char* ssid = "51 Glenville";
const char* password = "welc0meh0me";

// Open-Meteo API配置
const float latitude = 42.3584;
const float longitude = -71.1259;
const char* city_name = "Boston";

// 显示模式
enum DisplayMode {
  MODE_TIME,
  MODE_WEATHER,
  MODE_WEATHER_DETAIL,
  MODE_SYSTEM,
  MODE_COUNT
};

DisplayMode currentMode = MODE_TIME;
bool ledState = false;
unsigned long lastUpdate = 0;
unsigned long lastButtonPress = 0;
const unsigned long UPDATE_INTERVAL = 30000;

// 数据存储结构
struct WeatherData {
  String location_name = "N/A";
  float temperature = 0.0;
  float humidity = 0.0;
  float windSpeed = 0.0;
  int weatherCode = 0;
  String weatherDescription = "N/A";
  bool valid = false;
};

WeatherData weather;

// WiFi检查变量
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000;

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  delay(1000);
  Serial.println("================================");
  Serial.println("ESP32 波士顿天气显示系统");
  Serial.println("================================");
  
  // 初始化LCD
  Serial.println("[INIT] 初始化2行LCD显示屏...");
  lcd.init();
  lcd.backlight();
  
  // 清除显示并测试
  lcd.clear();
  delay(500);
  
  // 测试显示
  lcd.setCursor(0, 0);
  lcd.print("System Start");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
  
  Serial.println("[INIT] LCD初始化完成");
  
  // 连接WiFi
  Serial.println("[WIFI] 开始连接WiFi...");
  connectToWiFi();
  
  // 配置时间服务器 (美国东部夏令时 UTC-4)
  Serial.println("[TIME] 配置时间服务器...");
  configTime(-4 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  
  Serial.println("[INIT] 系统初始化完成");
  Serial.println("================================");
  
  delay(2000);
  updateDisplay();
}

void loop() {
  checkWiFiConnection();
  checkSerialCommands();
  checkButton();
  
  if (millis() - lastUpdate > UPDATE_INTERVAL) {
    Serial.println("[LOOP] 开始定期数据更新...");
    updateData();
    updateDisplay();
    lastUpdate = millis();
    Serial.println("[LOOP] 数据更新完成");
  }
  
  if (currentMode == MODE_TIME && millis() % 1000 < 100) {
    updateDisplay();
    delay(100);
  }
  
  delay(50);
}

void connectToWiFi() {
  Serial.println("[WIFI] SSID: " + String(ssid));
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  
  int maxRetries = 3;
  int currentRetry = 0;
  
  while (currentRetry < maxRetries && WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] 尝试连接 (" + String(currentRetry + 1) + "/" + String(maxRetries) + ")");
    
    lcd.setCursor(0, 1);
    lcd.print("WiFi Try " + String(currentRetry + 1) + "/3   ");
    
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WIFI] ✓ WiFi连接成功!");
      Serial.println("[WIFI] IP地址: " + WiFi.localIP().toString());
      Serial.println("[WIFI] 信号强度: " + String(WiFi.RSSI()) + " dBm");
      
      lcd.setCursor(0, 1);
      lcd.print("WiFi Connected! ");
      delay(2000);
      return;
    } else {
      currentRetry++;
      Serial.println("[WIFI] ✗ 连接失败，状态码: " + String(WiFi.status()));
      
      if (currentRetry < maxRetries) {
        Serial.println("[WIFI] 等待5秒后重试...");
        lcd.setCursor(0, 1);
        lcd.print("Retry in 5s... ");
        delay(5000);
      }
    }
  }
  
  Serial.println("[WIFI] ✗ WiFi连接完全失败!");
  lcd.setCursor(0, 1);
  lcd.print("WiFi Failed!    ");
  delay(3000);
}

void checkButton() {
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > 200) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("[BUTTON] 按钮被按下");
      
      DisplayMode oldMode = currentMode;
      currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      
      String modeNames[] = {"时间模式", "基本天气", "详细天气", "系统信息"};
      Serial.println("[MODE] 模式切换: " + modeNames[oldMode] + " -> " + modeNames[currentMode]);
      Serial.println("[LED] LED状态: " + String(ledState ? "开启" : "关闭"));
      
      updateDisplay();
      lastButtonPress = millis();
      
      while(digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }
      Serial.println("[BUTTON] 按钮释放");
    }
  }
}

void updateDisplay() {
  Serial.println("[DISPLAY] 更新显示内容...");
  lcd.clear();
  
  String modeNames[] = {"时间", "天气", "详细", "系统"};
  Serial.println("[DISPLAY] 当前模式: " + modeNames[currentMode]);
  
  switch (currentMode) {
    case MODE_TIME:
      displayTime();
      break;
    case MODE_WEATHER:
      displayWeather();
      break;
    case MODE_WEATHER_DETAIL:
      displayWeatherDetail();
      break;
    case MODE_SYSTEM:
      displaySystem();
      break;
  }
  
  Serial.println("[DISPLAY] 显示更新完成");
}

void displayTime() {
  lcd.setCursor(0, 0);
  lcd.print("Time M" + String(currentMode + 1) + " L" + (ledState ? "1" : "0"));
  
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  if (timeinfo.tm_year > 100) {
    // 12小时制格式显示
    int hour12 = timeinfo.tm_hour;
    String ampm = "AM";
    
    if (hour12 == 0) {
      hour12 = 12; // 午夜12点
    } else if (hour12 > 12) {
      hour12 -= 12; // 下午时间
      ampm = "PM";
    } else if (hour12 == 12) {
      ampm = "PM"; // 中午12点
    }
    
    // 格式: "2:30:25PM 5/28"
    String timeStr = String(hour12) + ":" + 
                     (timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min) + ":" +
                     (timeinfo.tm_sec < 10 ? "0" : "") + String(timeinfo.tm_sec) + ampm + " " +
                     String(timeinfo.tm_mon + 1) + "/" + String(timeinfo.tm_mday);
    
    lcd.setCursor(0, 1);
    // 确保不超过16字符
    if (timeStr.length() > 16) {
      timeStr = timeStr.substring(0, 16);
    }
    lcd.print(timeStr);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Time sync...    ");
  }
}

void displayWeather() {
  lcd.setCursor(0, 0);
  lcd.print("Weather M" + String(currentMode + 1) + " L" + (ledState ? "1" : "0"));
  
  if (weather.valid) {
    lcd.setCursor(0, 1);
    // 显示摄氏度
    String tempStr = String(weather.temperature, 1) + "C";
    String line = weather.location_name.substring(0, 7) + " " + tempStr;
    if (line.length() > 16) {
      line = line.substring(0, 16);
    }
    lcd.print(line);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("No weather data ");
  }
}

void displayWeatherDetail() {
  lcd.setCursor(0, 0);
  lcd.print("Detail M" + String(currentMode + 1) + " L" + (ledState ? "1" : "0"));
  
  if (weather.valid) {
    lcd.setCursor(0, 1);
    // 显示摄氏度、湿度和风速(km/h)
    String line = String(weather.temperature, 1) + "C " + String((int)weather.humidity) + "% " + String(weather.windSpeed, 1) + "kmh";
    if (line.length() > 16) {
      line = line.substring(0, 16);
    }
    lcd.print(line);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("No data         ");
  }
}

void displaySystem() {
  lcd.setCursor(0, 0);
  lcd.print("System M" + String(currentMode + 1) + " L" + (ledState ? "1" : "0"));
  
  lcd.setCursor(0, 1);
  String wifiStatus = WiFi.status() == WL_CONNECTED ? "OK" : "NO";
  String line = "WiFi:" + wifiStatus + " " + String(millis() / 1000) + "s";
  if (line.length() > 16) {
    line = line.substring(0, 16);
  }
  lcd.print(line);
  
  Serial.println("[SYSTEM] WiFi状态: " + wifiStatus);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[SYSTEM] IP地址: " + WiFi.localIP().toString());
    Serial.println("[SYSTEM] 信号强度: " + String(WiFi.RSSI()) + " dBm");
  }
  Serial.println("[SYSTEM] 运行时间: " + String(millis() / 1000) + " 秒");
  Serial.println("[SYSTEM] 空闲内存: " + String(ESP.getFreeHeap()) + " 字节");
}

void updateData() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[DATA] WiFi已连接，开始更新天气数据...");
    updateWeatherData();
  } else {
    Serial.println("[DATA] ✗ WiFi未连接，跳过数据更新");
  }
}

void updateWeatherData() {
  Serial.println("[API] 开始获取天气数据...");
  
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + 
               String(latitude, 4) + "&longitude=" + String(longitude, 4) + 
               "&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code" +
               "&timezone=America%2FNew_York&forecast_days=1";
  
  Serial.println("[API] 请求URL: " + url);
  
  http.begin(url);
  http.addHeader("User-Agent", "ESP32-WeatherStation");
  http.setTimeout(10000);
  
  int httpResponseCode = http.GET();
  Serial.println("[API] HTTP响应码: " + String(httpResponseCode));
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("[API] 响应长度: " + String(response.length()) + " 字节");
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      Serial.println("[JSON] ✓ JSON解析成功");
      
      if (doc.containsKey("current")) {
        weather.location_name = city_name;
        weather.temperature = doc["current"]["temperature_2m"].as<float>();
        weather.humidity = doc["current"]["relative_humidity_2m"].as<float>();
        weather.windSpeed = doc["current"]["wind_speed_10m"].as<float>();
        weather.weatherCode = doc["current"]["weather_code"].as<int>();
        weather.weatherDescription = getWeatherDescription(weather.weatherCode);
        weather.valid = true;
        
        Serial.println("[WEATHER] ✓ 天气数据更新成功");
        Serial.println("[WEATHER] 温度: " + String(weather.temperature, 1) + "°C");
        Serial.println("[WEATHER] 湿度: " + String(weather.humidity, 0) + "%");
        Serial.println("[WEATHER] 风速: " + String(weather.windSpeed, 1) + " km/h");
        Serial.println("[WEATHER] 天气: " + weather.weatherDescription);
      } else {
        Serial.println("[JSON] ✗ API响应中没有current数据");
        weather.valid = false;
      }
    } else {
      Serial.println("[JSON] ✗ JSON解析失败: " + String(error.c_str()));
      weather.valid = false;
    }
  } else {
    Serial.println("[API] ✗ HTTP请求失败，错误码: " + String(httpResponseCode));
    weather.valid = false;
  }
  
  http.end();
}

String getWeatherDescription(int code) {
  switch(code) {
    case 0: return "Clear";
    case 1: case 2: case 3: return "Cloudy";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: return "Drizzle";
    case 61: case 63: case 65: return "Rain";
    case 71: case 73: case 75: return "Snow";
    case 80: case 81: case 82: return "Showers";
    case 95: return "Thunder";
    default: return "Unknown";
  }
}

void checkWiFiConnection() {
  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WIFI] ⚠️ WiFi连接丢失，尝试重连...");
      reconnectWiFi();
    }
    lastWiFiCheck = millis();
  }
}

void reconnectWiFi() {
  Serial.println("[WIFI] 开始快速重连...");
  WiFi.disconnect();
  delay(1000);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] ✓ 重连成功! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("[WIFI] ✗ 快速重连失败");
  }
}

void checkSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "help") {
      Serial.println("可用命令: help, status, weather, restart, wifi, time");
    } else if (command == "status") {
      Serial.println("运行时间: " + String(millis() / 1000) + "s");
      Serial.println("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "连接" : "断开"));
      Serial.println("天气数据: " + String(weather.valid ? "有效" : "无效"));
      if (weather.valid) {
        Serial.println("波士顿温度: " + String(weather.temperature, 1) + "°C");
      }
    } else if (command == "weather") {
      updateWeatherData();
    } else if (command == "restart") {
      ESP.restart();
    } else if (command == "wifi") {
      Serial.println("WiFi状态: " + String(WiFi.status()));
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("IP: " + WiFi.localIP().toString());
        Serial.println("信号: " + String(WiFi.RSSI()) + "dBm");
      }
    } else if (command == "time") {
      time_t now = time(nullptr);
      Serial.println("当前时间: " + String(ctime(&now)));
    }
  }
}