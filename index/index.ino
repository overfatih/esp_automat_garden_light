#include <ESP8266WiFi.h>
#include <Wire.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WebServer.h>
#include <sunset.h>

const char* ssid = "Fiber_2.4GHz";  // Wi-Fi ağ adı
const char* password = "******";  // Wi-Fi şifresi
const int relayPin = D6;  // Rölenin bağlı olduğu pin d6 (GPI12)

const float latitude = 38.479659;   // Enlem (İstanbul)
const float longitude = 28.134255;  // Boylam (İstanbul)
const int timeZone = 3;  // Türkiye için GMT+3

String htmlContent="";
int status = 0; //status [0=auto, 1=fixed-time, 2=on-off ]

// Özel karakter tanımlamaları
byte sunShape[8] = {
  0b00100,
  0b10101,
  0b01110,
  0b11111,
  0b01110,
  0b10101,
  0b00100,
  0b00000
};

byte circleShape[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b10001,
  0b10001,
  0b01110,
  0b00000,
  0b00000
};

RTC_DS3231 rtc;
DateTime currentTime;
SunSet sun;
double sunrise, sunset;

// I2C LCD ayarları (I2C adresi 0x27 olabilir, ekranına göre değişebilir)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Bu kodu kullanırken ekranda yazı çıkmaz ise 0x27 yerine 0x3f yazınız !!  

ESP8266WebServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000);  // 10800 = 3 saat farkı (3 * 3600 saniye)

void setup () {
  Serial.begin(9600);
  lcd.begin();
  lcd.backlight();  // LCD arka ışığı aç
  // Özel karakterleri LCD'ye yükleme
  lcd.createChar(0, sunShape);
  lcd.createChar(1, circleShape);  
  lcd.setCursor(0, 0);  // İlk satır, ilk sütuna konumlandır
  lcd.print("M1K41L-01");
  
  if (!rtc.begin()) {
    Serial.println("RTC çalışmıyor, lütfen kontrol edin!");
    lcd.setCursor(0, 1);  // İlk satır, ilk sütuna konumlandır
    lcd.print("RTC calismiyor, lutfen kontrol edin!");
    while (1);  // Sonsuz döngüye sokarak durdur
  }
  
  if (rtc.lostPower()) {
    Serial.println("RTC gücünü kaybetti, zamanı ayarlamanız gerekiyor!");
    lcd.setCursor(0, 1);  // İlk satır, ilk sütuna konumlandır
    lcd.print("RTC gucunu kaybetti, zamani ayarlamaniz gerekiyor!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Tarihi ve saati ayarla
  }
 
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);  // Röleyi kapalı başlat

  // Wi-Fi bağlantısı
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("WiFi'ye baglaniyor...");
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("WiFi'ye baglaniyor...");
  }
  Serial.println("WiFi bağlantısı sağlandı!");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(WiFi.localIP());
  lcd.setCursor(0, 1);
  lcd.print("WiFi baglantisi saglandi!");
 
  syncRTCwithNTP();
  
  server.on("/", handleRoot);
  server.on("/relay", handleRelay);
  //server.on("/relay?status=1", handleRelayFixedTime);
  //server.on("/relay?status=2", handleRelayOnOff);
  
  server.begin();  // Web server başlat
  Serial.println("Server başlatıldı.");
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Server baslatildi.");

  sun.setPosition(latitude, longitude, timeZone);  // Konum ve saat dilimi ayarla
}

void loop () {
  server.handleClient();  // Web server işleyicisi 
  RTCNowPrint();

  // LCD'ye tarih ve saati yazdır
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(currentTime.month());
  lcd.print('/');
  lcd.print(currentTime.day());
  lcd.print(' ');
  lcd.print(currentTime.hour());
  lcd.print(':');
  lcd.print(currentTime.minute());
  lcd.print(':');
  lcd.print(currentTime.second());
  lcd.setCursor(15, 0);
  lcd.print(status);
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  lcd.setCursor(15, 1);
  if (digitalRead(relayPin) == LOW) {
    lcd.write(byte(1));  // Boş yuvarlak sembolü
  } else {
    lcd.write(byte(0));  // Güneş sembolü
  }
  
  // Gün doğumu ve batımı hesaplama
  sun.setCurrentDate(currentTime.year(), currentTime.month(), currentTime.day());
  sunrise = sun.calcSunrise();
  sunset = sun.calcSunset();
  // RTC saatini kontrol et
  int hourNow = currentTime.hour();
  int minuteNow = currentTime.minute();
  
  //test için
  //int hourNow = 12;
  //int minuteNow = 30;

  // Gün doğumu ve batımı saatlerini hesapla
  int sunriseHour = int(sunrise/60);  // Saat kısmı
  int sunriseMinute = int(60*((sunrise/60)-sunriseHour));  // Dakika kısmı

  int sunsetHour = int(sunset/60);  // Saat kısmı
  int sunsetMinute = int(60*((sunset/60)-sunsetHour));  // Dakika kısmı
  
  // Seri monitörde gün doğumu ve batımı saatlerini yazdır  
  Serial.print("Gün doğumu: ");
  Serial.print(sunriseHour);  // Saat kısmı
  Serial.print(":");
  if (sunriseMinute < 10) {
    Serial.print("0");  // Dakika kısmını düzgün formatla
  }
  Serial.println(sunriseMinute);

  Serial.print("Gün batımı: ");
  Serial.print(sunsetHour);  // Saat kısmı
  Serial.print(":");
  if (sunsetMinute < 10) {
    Serial.print("0");  // Dakika kısmını düzgün formatla
  }
  Serial.println(sunsetMinute);
  
  // Röleyi tetikleme

  switch(status) {
    case 0:{
      if ((hourNow*60) + minuteNow >= sunrise && (60*hourNow) + minuteNow < sunset) {
        digitalWrite(relayPin, LOW);  // Röleyi aç
        Serial.println("Gün doğumu: Röle açıldı.");
      } else {
        digitalWrite(relayPin, HIGH);  // Röleyi kapat
        Serial.println("Gün batımı: Röle kapatıldı.");
      }
      break;
      }
    case 1:{
    // Fixed Time Setting [open time, close time ]
    break;
    }
  case 2:{
    // on-off [on, off]
    break;
  }
    default:{
      // code block
    }
  }
  
 // delay(60000);  // Bir saat bekle (performans için)
}

void syncRTCwithNTP() {
  currentTime = rtc.now();
  //seri monitore yazdır
  Serial.println("Eski RTC saati:***");
  Serial.print(currentTime.year(), DEC);
  Serial.print('/');
  Serial.print(currentTime.month(), DEC);
  Serial.print('/');
  Serial.print(currentTime.day(), DEC);
  Serial.print(" ");
  Serial.print(currentTime.hour(), DEC);
  Serial.print(':');
  Serial.print(currentTime.minute(), DEC);
  Serial.print(':');
  Serial.print(currentTime.second(), DEC);
  Serial.println();
  
  delay(1000);
  timeClient.update();
  if(rtc.now() == timeClient.getEpochTime()){
    Serial.println("rtc saati güncellemeye gerek yok");
  }else{
    unsigned long epochTime = timeClient.getEpochTime();
    DateTime ntpDateTime = DateTime(epochTime);
    rtc.adjust(DateTime(epochTime));  // NTP'den alınan zamanı RTC'ye ayarla
    currentTime  = rtc.now();
    //seri monitore yazdır
    Serial.println("npt saati");
    Serial.print(ntpDateTime.year(), DEC);
    Serial.print('/');
    Serial.print(ntpDateTime.month(), DEC);
    Serial.print('/');
    Serial.print(ntpDateTime.day(), DEC);
    Serial.print(" ");
    Serial.print(ntpDateTime.hour(), DEC);
    Serial.print(':');
    Serial.print(ntpDateTime.minute(), DEC);
    Serial.print(':');
    Serial.print(ntpDateTime.second(), DEC);
    Serial.println();
    Serial.println("rtc saati güncellendi...");
  }
}

void RTCNowPrint(){
  currentTime = rtc.now();
  Serial.print(currentTime.year(), DEC);
  Serial.print('/');
  Serial.print(currentTime.month(), DEC);
  Serial.print('/');
  Serial.print(currentTime.day(), DEC);
  Serial.print(" ");
  Serial.print(currentTime.hour(), DEC);
  Serial.print(':');
  Serial.print(currentTime.minute(), DEC);
  Serial.print(':');
  Serial.print(currentTime.second(), DEC);
  Serial.println();
  
  delay(1000);
}

void handleRoot() {
  String statusString;
  int openTimeHour=0;
  int openTimeMinute=0;
  int closeTimeHour=0;
  int closeTimeMinute=0;
  String checked0="", checked1="", checked2="";
  
  if(status==0) {
    statusString = "Sun Adjusted";
    openTimeHour = int(sunset/60);
    openTimeMinute = int(60*((sunset/60)-openTimeHour));
    closeTimeHour = int(sunrise/60);
    closeTimeMinute = int(60*((sunrise/60)-closeTimeHour));
    checked0 = "checked";
  }
  if(status==1) {
    statusString = "Fixed Time ";
    checked1 = "checked";
  }
  if(status==2) {
    statusString = "On/Off";
    String relayonoff = server.arg("relayonoff");
    int relayStatus = relayonoff.toInt(); // Değeri int'e çevir
    if(relayStatus==1){digitalWrite(relayPin, LOW);}  // Röleyi Aç
    if(relayStatus==0){digitalWrite(relayPin, HIGH);}  // Röleyi kapat
    checked2 = "checked";
  }
  
  
  
  htmlContent = "<html><head><meta charset='UTF-8'></head><body>";
  htmlContent += "<h1>Röle Kontrolü</h1>";
  htmlContent += "<p>Saat: " + String(currentTime.hour()) + ":" + String(currentTime.minute()) + ":" + String(currentTime.second()) + "</p>";
  htmlContent += "<p>Tarih: " + String(currentTime.day()) + "/" + String(currentTime.month()) + "/" + String(currentTime.year()) + "</p>";
  htmlContent += "<p>Status: " + statusString + "</p>";
  htmlContent += "<p>Aydınlatma Açılış Zamanı: " + String(openTimeHour) + ":" + String(openTimeMinute) + "</p>";
  htmlContent += "<p>Aydınlatma Kapanış Zamanı: " + String(closeTimeHour) + ":" + String(closeTimeMinute) + "</p>";
  htmlContent += "<form action='/relay' method='get'>";
    htmlContent += "<input type='radio' id='sunadjusted' name='statusForm' value='0' "+ checked0 +">";
    htmlContent += "<label for='sunadjusted'>Sun Adjusted</label><br>";
    htmlContent += "<input type='radio' id='fixedtime' name='statusForm' value='1' "+ checked1 +">";
    htmlContent += "<label for='fixedtime'>Fixed Time</label><br>";
    htmlContent += "<input type='radio' id='onoff' name='statusForm' value='2' "+ checked2 +">";
    htmlContent += "<label for='onoff'>On-Off</label>";
    htmlContent += "<br><br>";
    htmlContent += "<input type='submit' value='Submit'>";
  htmlContent += "</form>";
  
  htmlContent += "</body></html>";

  server.send(200, "text/html", htmlContent);
}

void handleRelay() {
  String statusForm = server.arg("statusForm");
  status = statusForm.toInt();
  htmlContent = "<html><head><meta charset='UTF-8'></head><body>";
  switch(status) {
    case 0:{
      //Sun Adjusted Setting [latitute, longitute, timezone]
      htmlContent += "<h1>Röle Güneş Ayarlı</h1>";
      htmlContent += "<form action='/' method='get'>";
        //todo: latitute, longitute, timezone için input olacak
        htmlContent += "<br><br>";
        htmlContent += "<input type='submit' value='Submit'>";
      htmlContent += "</form>";
      break;
      }
    case 1:{
      // Fixed Time Setting [open time, close time ]
      htmlContent += "<h1>Röle Zaman Ayarlı</h1>";
      htmlContent += "<form action='/' method='get'>";
        //todo: açılma ve kapanma saatileri için input olacak
        htmlContent += "<br><br>";
        htmlContent += "<input type='submit' value='Submit'>";
      htmlContent += "</form>";
      break;
      }
    case 2:{
      // on-off [on, off]
      htmlContent += "<h1>Röle On/Off</h1>";
      htmlContent += "<form action='/' method='get'>";
        htmlContent += "<input type='radio' id='open' name='relayonoff' value='0' checked>";
        htmlContent += "<label for='open'>Open Light</label><br>";
        htmlContent += "<input type='radio' id='close' name='relayonoff' value='1'>";
        htmlContent += "<label for='close'>Close Light</label><br>";
        htmlContent += "<br><br>";
        htmlContent += "<input type='submit' value='Submit'>";
      htmlContent += "</form>";
      break;
      }
    default:{
      // handleRoot'a geri gönder
      }
  }
  htmlContent += "</body></html>";
  server.send(200, "text/html", htmlContent);
}

void handleRelayFixedTime() {
  digitalWrite(relayPin, LOW);  // Röleyi aç
  server.send(200, "text/html", "Role Acildi<br><button onclick=\"location.href='/'\">Geri</button>");
}

void handleRelayOnOff() {
  digitalWrite(relayPin, HIGH);  // Röleyi kapat
  server.send(200, "text/html", "Role Kapatildi<br><button onclick=\"location.href='/'\">Geri</button>");
}
