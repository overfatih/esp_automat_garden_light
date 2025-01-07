const int relayPin = 14;  // d5/D6, GPIO14/GPIO12'ye karşılık gelir
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(9600);
  lcd.begin();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("role kontrol");
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);  // Röleyi kapalı başlat
}

void loop() {
  digitalWrite(relayPin, LOW);  // Röleyi aç (LOW röleyi aktif yapar)
  Serial.println("Röleyi aç (LOW röleyi aktif yapar)");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("isiklar acik");
  delay(5000);  // 1 saniye bekle
  lcd.clear();
  lcd.setCursor(0, 0);
  digitalWrite(relayPin, HIGH);  // Röleyi kapat
  Serial.println("Röleyi kapat");
  lcd.print("isiklar kapali");
  delay(5000);  // 1 saniye bekle
}
