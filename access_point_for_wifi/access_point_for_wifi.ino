#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

// ESP8266'nın web server ve Wi-Fi manager kütüphanesini kullanarak Captive Portal oluşturma

void setup() {
  // Seri haberleşme başlatılıyor
  Serial.begin(115200);
  
  // WiFiManager nesnesi oluşturuluyor
  WiFiManager wifiManager;
  
  // Daha önce kaydedilmiş Wi-Fi ağlarını unutmak için (opsiyonel)
  // wifiManager.resetSettings();
  
  // Wi-Fi ağına bağlanamazsa kendi Access Point'ini oluşturur
  if (!wifiManager.autoConnect("ESP8266_Config_AP")) {
    Serial.println("Bağlantı başarısız, cihazı resetleyin!");
    delay(3000);
    ESP.restart();
  }
  
  // Wi-Fi'ya bağlandıktan sonra
  Serial.println("Wi-Fi'ya bağlandı!");
}

void loop() {
  // Burada ana programın diğer kısımlarını yazabilirsiniz
}
