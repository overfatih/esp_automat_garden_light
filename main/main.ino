#include <ESP8266WiFi.h>
#include <Wire.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>  // WiFiManager kütüphanesi
#include <sunset.h>
#include <EEPROM.h> /*hafızaya kalici yamak icin*/



float latitude = 38.479659;   // Enlem (SalihliBILSEM)
float longitude = 28.134255;  // Boylam (SalihliBILSEM)
int timeZone = 3;  // Türkiye için GMT+3

const int relayPin = 14;  // Rölenin bağlı olduğu pin d5 (d6:GPI12, d5:GPI14)

unsigned long currentMillis = millis();  // Şu anki zamanı al
unsigned long previousMillis = 0;  // Zaman kaydı için değişken
unsigned long previousMillis12Hours = 0;  // sunTime kontol için değişken
const long interval = 10000;  // 10 saniye (milisaniye cinsinden)
const long interval12Hours = (1000*60*60*12);  // 12 saat (milisaniye cinsinden)

const String deviceName = "M1K41L-01";
String htmlContent="";
int modeStatus = 0; //modeStatus [0=auto, 1=fixed-time, 2=on-off ]
int relayStatus=0;
int openHour; int openMinute; int closeHour; int closeMinute;
int openTimeHour; int openTimeMinute; int closeTimeHour; int closeTimeMinute;

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
int changePrintLowColumn =0;
// I2C LCD ayarları (I2C adresi 0x27 olabilir, ekranına göre değişebilir)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Bu kodu kullanırken ekranda yazı çıkmaz ise 0x27 yerine 0x3f yazınız !!  

ESP8266WebServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000);  // 10800 = 3 saat farkı (3 * 3600 saniye)
WiFiManager wifiManager;

void setup () {
  Serial.begin(9600);
  lcd.begin();
  lcd.backlight();  // LCD arka ışığı aç
  // Özel karakterleri LCD'ye yükleme
  lcd.createChar(0, sunShape);
  lcd.createChar(1, circleShape);  
  lcd.setCursor(0, 0);  // İlk satır, ilk sütuna konumlandır
  lcd.print(deviceName);

   // Wi-Fi ağına bağlanamazsa kendi Access Point'ini oluşturur
  if (!wifiManager.autoConnect("ESP8266_Config_AP")) {
    Serial.println("Bağlantı başarısız, cihazı resetleyin!");
    lcd.setCursor(0, 1);
    lcd.print("Bağlantı başarısız, cihazı resetleyin!");
    delay(3000);
    ESP.restart();
  }else{
    Serial.println("ESP8266_Config_AP yayında");
    lcd.setCursor(0, 1);
    lcd.print("ESP8266_Config_AP yayinda");
    }

  Serial.println("Wi-Fi'ya bağlandı!");
  Serial.println("IP adresi: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(WiFi.localIP());
  lcd.setCursor(0, 1);
  lcd.print("WiFi baglantisi saglandi!");
  
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
  digitalWrite(relayPin, LOW);  // Röleyi açık başlat
  syncRTCwithNTP();
  
  server.on("/", handleRoot);
  server.on("/resetwifi", handleResetWifi);
  server.on("/changemodestatus", handleChangeModeStatus);
  server.on("/relay", handleRelay);
  server.on("/setting-fixedtime", handleSettingFixedTime);
  server.on("/setting-coordinate", handleSettingCoordinate);
  
  server.begin();  // Web server başlat
  Serial.println("Server başlatıldı.");
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Server baslatildi.");

  loadSettings(openHour, openMinute, closeHour, closeMinute, latitude, longitude, timeZone, modeStatus);
  Serial.print("latitude:");
  Serial.println(latitude);
  calculateSunriseAndSunset();
}

void loop () {
  server.handleClient();  // Web server işleyicisi 
  RTCNowPrint();
  currentMillis = millis();  // Şu anki zamanı al
  // RTC saatini kontrol et
  int hourNow = currentTime.hour();
  int minuteNow = currentTime.minute();
  //calculateSunriseAndSunset();
  //test için
  /*int hourNow = 12; int minuteNow = 30;*/
  if (currentMillis - previousMillis12Hours >= interval12Hours) {
    previousMillis12Hours = currentMillis;  // Zaman kaydını güncelle
    //gün doğumu ve batımı hesaplama buradaydı....
    calculateSunriseAndSunset();/*12 saatte bir hesapla*/
  }
  
  
  // Röleyi tetikleme
  switch(modeStatus) {
    case 0:{
      if ((hourNow*60) + minuteNow >= sunrise && (60*hourNow) + minuteNow < sunset) {
        if(digitalRead(relayPin)==HIGH){
          digitalWrite(relayPin, LOW);  // Röleyi aç
          relayStatus==0;
          Serial.println("Gun dogumu: Röle açıldı.");
          clearArea(0, 0, 15);  // (row, startColunm, finishColumn)
          lcd.setCursor(0, 1);
          lcd.print("Gun dogumu");
        }  
      } else {
        if(digitalRead(relayPin)==LOW){
          digitalWrite(relayPin, HIGH);  // Röleyi kapat
          relayStatus=1;
          Serial.println("Gun batimi: Röle kapatıldı.");
          clearArea(0, 0, 15);  // (row, startColunm, finishColumn)
          lcd.setCursor(0, 1);
          lcd.print("Gun batimi");
        }        
      }
      break;
      }
    case 1:{
    /* Fixed Time Setting [open time, close time ]*/
    int sameday = ((closeHour*60) + closeMinute)>((openHour*60) + openMinute)?1:0;
    if(sameday==1){
      if (((hourNow*60) + minuteNow) >= ((openHour*60) + openMinute) && ((60*hourNow) + minuteNow) < ((closeHour*60) + closeMinute)) {
        if(digitalRead(relayPin)==LOW){
          digitalWrite(relayPin, HIGH);  // Röleyi aç
          relayStatus=1;
          Serial.println("Isik Acildi: Röle kapatıldı.");
          clearArea(0, 0, 15);  // (row, startColunm, finishColumn)
          lcd.setCursor(0, 1);
          lcd.print("Isik Acildi");
        }  
      } else {
        if(digitalRead(relayPin)==HIGH){
          digitalWrite(relayPin, LOW);  // Röleyi acildi
          relayStatus=0;
          Serial.println("Isik Kapandi: Röle acildi.");
          clearArea(0, 0, 15);  // (row, startColunm, finishColumn)
          lcd.setCursor(0, 1);
          lcd.print("Isik Kapandi");
        }   
      }
    }else{
      if (((hourNow*60) + minuteNow) >= ((closeHour*60) + closeMinute) && ((60*hourNow) + minuteNow) < ((openHour*60) + openMinute)) {
        if(digitalRead(relayPin)==HIGH){
          digitalWrite(relayPin, LOW);  // Röleyi Acildi
          relayStatus=0;
          Serial.println("Isik Kapandi: Röle acildi.");
          clearArea(0, 0, 15);  // (row, startColunm, finishColumn)
          lcd.setCursor(0, 1);
          lcd.print("Isik Kapandi");
        }  
      } else {
        if(digitalRead(relayPin)==LOW){
          digitalWrite(relayPin, HIGH);  // Röleyi kapandı
          relayStatus=1;
          Serial.println("Isik Acildi: Röle kapandi.");
          clearArea(0, 0, 15);  // (row, startColunm, finishColumn)
          lcd.setCursor(0, 1);
          lcd.print("Isik Acildi");
        }   
      }
    }
    
    break;
    }
  case 2:{
    /* on-off [on, off]*/
    if(relayStatus==1){digitalWrite(relayPin, HIGH);}
    else{digitalWrite(relayPin, LOW);}  /*Röleyi kapat*/
    break;
  }
    default:{
      // code block
    }
  }
  
 // delay(60000);  // Bir saat bekle (performans için)
}
/*loop bittiği yer*/

void calculateSunriseAndSunset(){
  sun.setPosition(latitude, longitude, timeZone);  // Konum ve saat dilimi ayarla
    // Gün doğumu ve batımı hesaplama
  sun.setCurrentDate(currentTime.year(), currentTime.month(), currentTime.day());
  sunrise = sun.calcSunrise();
  sunset = sun.calcSunset();

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
}

void clearArea(int row, int startCol, int endCol) {
  lcd.setCursor(startCol, row);
  for (int i = startCol; i <= endCol; i++) {
    lcd.print(" ");  // Boşluk ile hücreyi temizler
  }
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

  // LCD'ye tarih ve saati yazdır
  currentMillis = millis();  // Şu anki zamanı al
  clearArea(0, 0, 15);  // (row, startColunm, finishColumn)
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
  lcd.print(modeStatus);
  lcd.setCursor(0, 1);
  // 10 saniyede bir ekrandaki mesajı değiştir
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // Zaman kaydını güncelle
    clearArea(1, 0, 15);  // Ekranı temizle
    lcd.setCursor(0, 1);  // Kursörü başlangıç konumuna al
    static bool toggleMessage = false;  // Mesajı değiştirmek için değişken
    if (toggleMessage) {
      lcd.print(WiFi.localIP());
    } else {
      lcd.print(deviceName);
    }
    toggleMessage = !toggleMessage;  // Mesajı değiştir
  }  
  lcd.setCursor(15, 1);
  if (digitalRead(relayPin) == LOW) {
    lcd.write(byte(1));  // Boş yuvarlak sembolü
  } else {
    lcd.write(byte(0));  // Güneş sembolü
  }
  
  delay(1000);
}

void loadSettings(int &openHour, int &openMinute, int &closeHour, int &closeMinute, 
                  float &latitude, float &longitude, int &timeZone, int &modeStatus) {
  
  EEPROM.begin(512);  // EEPROM'u başlat
  
  EEPROM.get(0, openHour);        // Açma saati
  EEPROM.get(4, openMinute);      // Açma dakikası
  EEPROM.get(8, closeHour);       // Kapama saati
  EEPROM.get(12, closeMinute);    // Kapama dakikası
  EEPROM.get(16, latitude);       // Enlem
  EEPROM.get(20, longitude);      // Boylam
  EEPROM.get(24, timeZone);       // Zaman dilimi
  EEPROM.get(28, modeStatus);     // Çalışma modu
}

void loadSettingsModeStatus0(float &latitude, float &longitude, int &timeZone) {
  EEPROM.begin(512);  // EEPROM'u başlat
  EEPROM.get(16, latitude);       // Enlem
  EEPROM.get(20, longitude);      // Boylam
  EEPROM.get(24, timeZone);       // Zaman dilimi
}

void loadSettingsModeStatus1(int &openHour, int &openMinute, int &closeHour, int &closeMinute) {
  EEPROM.begin(512);  // EEPROM'u başlat
  EEPROM.get(0, openHour);        // Açma saati
  EEPROM.get(4, openMinute);      // Açma dakikası
  EEPROM.get(8, closeHour);       // Kapama saati
  EEPROM.get(12, closeMinute);    // Kapama dakikası
}

void saveDefaultModeStatus(int modeStatus) { 
  EEPROM.begin(512);          // EEPROM'u başlat
  EEPROM.put(28, modeStatus);  // Çalışma modu 
  EEPROM.commit();            // Verileri EEPROM'a yaz
}

void saveModeStatus0(float latitude, float longitude, int timeZone) {
  EEPROM.begin(512);  // EEPROM'u başlat
  EEPROM.put(16, latitude);       // Enlem
  EEPROM.put(20, longitude);      // Boylam
  EEPROM.put(24, timeZone);       // Zaman dilimi
  EEPROM.commit();  // Verileri EEPROM'a yaz
}

void saveModeStatus1(int openHour, int openMinute, int closeHour, int closeMinute) {
  EEPROM.begin(512);  // EEPROM'u başlat
  EEPROM.put(0, openHour);        // Açma saati
  EEPROM.put(4, openMinute);      // Açma dakikası
  EEPROM.put(8, closeHour);       // Kapama saati
  EEPROM.put(12, closeMinute);    // Kapama dakikası
  EEPROM.commit();  // Verileri EEPROM'a yaz
}

void handleRoot() {
  String statusString;
  String checked0="", checked1="", checked2="";
  
  if(modeStatus==0) {
    if (server.hasArg("latitudeForm")) {
      latitude = server.arg("latitudeForm").toFloat();
      longitude = server.arg("longitudeForm").toFloat();
      timeZone = server.arg("timeZoneForm").toInt();
      saveModeStatus0(latitude, longitude, timeZone);
    }
    loadSettingsModeStatus0(latitude, longitude, timeZone);
    calculateSunriseAndSunset();
    statusString = "Sun Adjusted";
    openTimeHour = int(sunset/60);
    openTimeMinute = int(60*((sunset/60)-openTimeHour));
    closeTimeHour = int(sunrise/60);
    closeTimeMinute = int(60*((sunrise/60)-closeTimeHour));
    checked0 = "checked";
  }
  if(modeStatus==1) {
    statusString = "Fixed Time ";
    if(server.hasArg("openHourForm")){
      openHour = server.arg("openHourForm").toInt();
      openMinute = server.arg("openMinuteForm").toInt();
      closeHour = server.arg("closeHourForm").toInt();
      closeMinute = server.arg("closeMinuteForm").toInt();
      saveModeStatus1(openHour, openMinute, closeHour, closeMinute);
    }
    loadSettingsModeStatus1(openHour, openMinute, closeHour, closeMinute);
    openTimeHour = openHour;
    openTimeMinute = openMinute;
    closeTimeHour = closeHour;
    closeTimeMinute = closeMinute;
    checked1 = "checked";
  }
  if(modeStatus==2) {
    statusString = "On/Off";
    String relayonoff = server.arg("relayonoff");
    relayStatus = relayonoff.toInt(); // Değeri int'e çevir
    checked2 = "checked";
  }
   
  htmlContent = "<html><head><meta charset='UTF-8'></head><body>";
  htmlContent += "<h1>WiFi Ayarları</h1>";
  htmlContent += "<p><a href='/resetwifi'><button>Wi-Fi Ayarlarını Sıfırla</button></a></p><hr>";
  htmlContent += "<p><a href='/changemodestatus'><button>Kalıcı Mode Değiştir</button></a></p><hr>";
  htmlContent += "<h1>Röle Kontrolü</h1>";
  htmlContent += "<p>Saat: " + String(currentTime.hour()) + ":" + String(currentTime.minute()) + ":" + String(currentTime.second()) + "</p>";
  htmlContent += "<p>Tarih: " + String(currentTime.day()) + "/" + String(currentTime.month()) + "/" + String(currentTime.year()) + "</p>";
  htmlContent += "<p>Mode Status: " + statusString + "</p>";
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

void handleResetWifi(){
   // WiFi ayarlarını sıfırlayan fonksiyon
  wifiManager.resetSettings();  // Daha önce kaydedilmiş Wi-Fi bilgilerini unut
  server.send(200, "text/plain", "Wi-Fi ayarları sıfırlandı. Cihaz yeniden başlatılacak.");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wi-Fi ayarlari sifirlandi.");
  lcd.setCursor(0, 1);
  lcd.print("Cihaz yeniden başlatilacak.");
  delay(3000);  // Kullanıcının mesajı görmesi için biraz bekleme
  ESP.restart();  // Cihazı yeniden başlat
}

void handleChangeModeStatus(){
   // kalıcı mode ayarlama fonksiyon
  saveDefaultModeStatus(modeStatus);
  htmlContent = "<html><head><meta charset='UTF-8'></head><body>";
  htmlContent += "<p>Mode Status ayarları kaydedildi.</p>";
  htmlContent += "<p><a href='/'><button>Ana Sayfaya Dön</button></a></p>";  
  htmlContent += "</body></html>";
  server.send(200, "text/html", htmlContent);
}

void handleRelay() {
  String statusForm = server.arg("statusForm");
  modeStatus = statusForm.toInt();
  htmlContent = "<html><head><meta charset='UTF-8'></head><body>";
  switch(modeStatus) {
    case 0:{
      //Sun Adjusted Setting [latitute, longitute, timezone]
      htmlContent += "<h1>Röle Güneş Ayarlı</h1>";
      htmlContent += "<p>Latitude: "  + String(latitude,6)  + "</p>";
      htmlContent += "<p>Longitude: " + String(longitude,6) + "</p>";
      htmlContent += "<p>Time Zone: " + String(timeZone)  + "</p>";
      htmlContent += "<p><a href='/'><button>Tamam</button></a></p>";
      htmlContent += "<p><a href='/setting-coordinate'><button>Değiştir</button></a></p>";
      break;
      }
    case 1:{
      // Fixed Time Setting [open time, close time ]
      htmlContent += "<h1>Röle Zaman Ayarlı</h1>";
      htmlContent += "<p>Aydınlatma Açılış Zamanı: " + String(openHour) + ":" + String(openMinute) + "</p>";
      htmlContent += "<p>Aydınlatma Kapanış Zamanı: " + String(closeHour) + ":" + String(closeMinute) + "</p>";
      htmlContent += "<p><a href='/'><button>Tamam</button></a></p>";
      htmlContent += "<p><a href='/setting-fixedtime'><button>Değiştir</button></a></p>";
      break;
      }
    case 2:{
      // on-off [on, off]
      String checkedOpen = relayStatus ? "checked" : "";
      String checkedClose = !relayStatus ? "checked" : "";
      
      htmlContent += "<h1>Röle On/Off</h1>";
      htmlContent += "<form action='/' method='get'>";
        htmlContent += "<input type='radio' id='open' name='relayonoff' value= '1' "+ checkedOpen +" >";
        htmlContent += "<label for='open'>Open Light</label><br>";
        htmlContent += "<input type='radio' id='close' name='relayonoff' value= '0' "+ checkedClose +" >";
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

void handleSettingFixedTime() {
  htmlContent = "<html><head><meta charset='UTF-8'></head><body>";
  htmlContent += "<h1>Röle Zaman Ayarlı</h1>";
  htmlContent += "<form action='/' method='get'>";
    htmlContent += "<label for='openHourForm'>Open Hour</label>";
    htmlContent += "<input type='text' id='openHourForm' name='openHourForm' value= "+ String(openHour) +" >";
    htmlContent += "<label for='openMinuteForm'> : </label>";
    htmlContent += "<input type='text' id='openMinuteForm' name='openMinuteForm' value= "+ String(openMinute) +" ><br>";
    htmlContent += "<label for='closeHourForm'>Close Hour</label>";
    htmlContent += "<input type='text' id='closeHourForm' name='closeHourForm' value= "+ String(closeHour) +"  >";
    htmlContent += "<label for='closeMinuteForm'> : </label>";
    htmlContent += "<input type='text' id='closeMinuteForm' name='closeMinuteForm' value= "+ String(closeMinute) +" ><br>";
    htmlContent += "<br><br>";
    htmlContent += "<input type='submit' value='Kaydet'>";
  htmlContent += "</form>";
  htmlContent += "<p><a href='/'><button>Ana Sayfaya Dön</button></a></p>";
  htmlContent += "</body></html>";
  server.send(200, "text/html", htmlContent);
}

void handleSettingCoordinate() {
  htmlContent = "<html><head><meta charset='UTF-8'></head><body>";
  htmlContent += "<h1>Koordinat Ayarı</h1>";
  htmlContent += "<form action='/' method='get'>";
    htmlContent += "<input type='text' id='latitudeForm' name='latitudeForm' value= "+ String(latitude,6) +" >";
    htmlContent += "<label for='latitudeForm'>Latitude</label>";
    htmlContent += "<input type='text' id='longitudeForm' name='longitudeForm' value= "+ String(longitude,6) +" >";
    htmlContent += "<label for='longitudeForm'>Longitude</label><br>";
    htmlContent += "<input type='text' id='timeZoneForm' name='timeZoneForm' value= "+ String(timeZone) +" >";
    htmlContent += "<label for='timeZoneForm'>Time Zone</label><br>";
    htmlContent += "<br><br>";
    htmlContent += "<input type='submit' value='Kaydet'>";
  htmlContent += "</form>";
  htmlContent += "<p><a href='/'><button>Ana Sayfaya Dön</button></a></p>";      
  htmlContent += "</body></html>";
  server.send(200, "text/html", htmlContent);
}
