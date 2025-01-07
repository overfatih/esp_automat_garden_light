#include <ESP8266WiFi.h>
#include <Wire.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <sunset.h>

const char* ssid = "Fiber_2.4GHz";  // Wi-Fi ağ adı
const char* password = "******";  // Wi-Fi şifresi
const int relayPin = D6;  // Rölenin bağlı olduğu pin d6 (GPI12)

const float latitude = 38.479659;   // Enlem (İstanbul)
const float longitude = 28.134255;  // Boylam (İstanbul)
const int timeZone = 3;  // Türkiye için GMT+3

const int status =0; //status [0=auto, 1=manual]

RTC_DS3231 rtc;
DateTime currentTime;
SunSet sun;
double sunrise, sunset;



ESP8266WebServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000);  // 10800 = 3 saat farkı (3 * 3600 saniye)

void setup () {
  Serial.begin(9600);
  if (!rtc.begin()) {
    Serial.println("RTC çalışmıyor, lütfen kontrol edin!");
    while (1);  // Sonsuz döngüye sokarak durdur
  }
  
  if (rtc.lostPower()) {
    Serial.println("RTC gücünü kaybetti, zamanı ayarlamanız gerekiyor!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Tarihi ve saati ayarla
  }

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);  // Röleyi kapalı başlat

  // Wi-Fi bağlantısı
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("WiFi'ye bağlanıyor...");
  }
  Serial.println("WiFi bağlantısı sağlandı!");
  Serial.println(WiFi.localIP());
 
  syncRTCwithNTP();
  
  server.on("/", handleRoot);
  server.on("/on", handleRelayOn);
  server.on("/off", handleRelayOff);
  
  server.begin();  // Web server başlat
  Serial.println("Server başlatıldı.");

  sun.setPosition(latitude, longitude, timeZone);  // Konum ve saat dilimi ayarla
}

void loop () {
  server.handleClient();  // Web server işleyicisi 
  RTCNowPrint();
  
  // Gün doğumu ve batımı hesaplama
  sun.setCurrentDate(currentTime.year(), currentTime.month(), currentTime.day());
  sunrise = sun.calcSunrise();
  sunset = sun.calcSunset();
  // RTC saatini kontrol et
  int hourNow = currentTime.hour();
  int minuteNow = currentTime.minute();

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
  if ((hourNow*60) + minuteNow >= sunrise && (60*hourNow) + minuteNow < sunset) {
    digitalWrite(relayPin, LOW);  // Röleyi aç
    Serial.println("Gün doğumu: Röle açıldı.");
  } else {
    digitalWrite(relayPin, HIGH);  // Röleyi kapat
    Serial.println("Gün batımı: Röle kapatıldı.");
  }
 // delay(60000);  // Bir dakika bekle (performans için)
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
  String htmlContent = "<html><head><meta charset='UTF-8'></head><body>";
  htmlContent += "<h1>Röle Kontrolü</h1>";
  htmlContent += "<p>Saat: " + String(currentTime.hour()) + ":" + String(currentTime.minute()) + ":" + String(currentTime.second()) + "</p>";
  htmlContent += "<p>Tarih: " + String(currentTime.day()) + "/" + String(currentTime.month()) + "/" + String(currentTime.year()) + "</p>";
  htmlContent += "<button onclick=\"location.href='/on'\">Aç</button>";
  htmlContent += "<button onclick=\"location.href='/off'\">Kapat</button>";
  htmlContent += "</body></html>";

  server.send(200, "text/html", htmlContent);
}

void handleRelayOn() {
  digitalWrite(relayPin, LOW);  // Röleyi aç
  server.send(200, "text/html", "Role Acildi<br><button onclick=\"location.href='/'\">Geri</button>");
}

void handleRelayOff() {
  digitalWrite(relayPin, HIGH);  // Röleyi kapat
  server.send(200, "text/html", "Role Kapatildi<br><button onclick=\"location.href='/'\">Geri</button>");
}
