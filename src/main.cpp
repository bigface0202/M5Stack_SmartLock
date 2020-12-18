#include <Arduino.h>
#include <FirebaseESP32.h>
#include <M5Stack.h>
#include "RCS620S.h"

// WiFi, Firebaseの設定情報
#define FIREBASE_HOST "esp32-felica.firebaseio.com"
#define FIREBASE_AUTH "7AX6iO611QD6pHNiqvPnjkdOEPEYQsmVVPkiagyd"
#define WIFI_SSID "kishimako_24_1"
#define WIFI_PASSWORD "theshoreunder"

FirebaseData firebaseData;

// FeliCaの設定情報
#define COMMAND_TIMEOUT 400
#define POLLING_INTERVAL 500
RCS620S rcs620s;

bool statusA = true;
bool statusB = true;
bool statusC = true;

bool buttonProcess(String buttonName, bool status)
{
  M5.Lcd.fillScreen(BLACK);
  if (Firebase.setBool(firebaseData, "/" + buttonName + "/", status))
  {
    //Success
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Set bool data success");
  }
  else
  {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Error in setInt, ");
    M5.Lcd.print(firebaseData.errorReason());
  }
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.print(buttonName + " was pressed!");
  return status = !status;
}

void setup()
{
  M5.begin();
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int _cursorX = 0;
  M5.Lcd.setTextFont(4);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(0, 0);
  // WiFiに接続
  M5.Lcd.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    M5.Lcd.setCursor(0 + 5 * _cursorX, 30);
    M5.Lcd.print(".");
    delay(300);
    _cursorX++;
    if (_cursorX > 320)
    {
      _cursorX = 0;
    }
  }
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Connected with IP:");
  M5.Lcd.print(WiFi.localIP());
  delay(1000);

  // Firebase関連
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  // FeliCaへの接続確認
  int ret;
  ret = rcs620s.initDevice();
  while (!ret)
  {
    M5.Lcd.setTextColor(RED);
    ret = rcs620s.initDevice();
    Serial.println("Cannot find the RC-S620S");
    M5.Lcd.setCursor(0, 60);
    M5.Lcd.print("Cannot find the RC-S620S");
    delay(1000);
  }
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(0, 90);
  M5.Lcd.print("Felica reader is detected");
  delay(1000);
  M5.Lcd.fillScreen(BLACK);
}

void loop()
{
  M5.update();

  int ret, i;
  String felicaID = "";
  rcs620s.timeout = COMMAND_TIMEOUT;
  ret = rcs620s.polling();
  if (ret)
  {
    for (i = 0; i < 8; i++)
    {
      if (rcs620s.idm[i] / 0x10 == 0)
        felicaID += "0";
      felicaID += String(rcs620s.idm[i], HEX);
    }
  }
  if (!felicaID.isEmpty())
  {
    M5.Lcd.setCursor(0, 15);
    M5.Lcd.print(felicaID);
    delay(3000);
    M5.Lcd.fillScreen(BLACK);
  }

  if (M5.BtnA.wasPressed())
  {
    statusA = buttonProcess("BtnA", statusA);
  }
  if (M5.BtnB.wasPressed())
  {
    statusB = buttonProcess("BtnB", statusB);
  }
  if (M5.BtnC.wasPressed())
  {
    statusC = buttonProcess("BtnC", statusC);
  }
  rcs620s.rfOff();
  delay(POLLING_INTERVAL);
}