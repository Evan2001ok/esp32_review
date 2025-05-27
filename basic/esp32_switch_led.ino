#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20,4);

#define LED_PIN 4
#define BUTTON_PIN 5

bool ledState = false;

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  lcd.init();
  lcd.backlight();
  
  Serial.println("按钮控制LED");
  lcd.setCursor(0,0);
  lcd.print("LED: OFF");
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // 消抖
    if (digitalRead(BUTTON_PIN) == LOW) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      Serial.println(ledState ? "LED 开启" : "LED 关闭");
      
      // 更新LCD显示
      lcd.setCursor(0,0);
      lcd.print(ledState ? "LED: ON " : "LED: OFF");
      
      // 等待按钮释放
      while(digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }
    }
  }
}