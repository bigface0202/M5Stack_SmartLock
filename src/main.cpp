#include <Arduino.h>
#include <FirebaseESP32.h>
#include <M5Stack.h>
#include "RCS620S.h"
#include <time.h>

// WiFi, Firebaseの設定情報
#define FIREBASE_HOST "Your firebase host name"
#define FIREBASE_AUTH "Your firebase auth key"
#define WIFI_SSID "Your WiFi SSID name"
#define WIFI_PASSWORD "Your WiFi Password"

FirebaseData firebaseData;
FirebaseJson json;

// FeliCaの設定情報
#define COMMAND_TIMEOUT 400
#define POLLING_INTERVAL 500
RCS620S rcs620s;

// 時間の取得
const char *ntpServer = "ntp.jst.mfeed.ad.jp";
const long gmtOffset_sec = 9 * 3600;
const int daylightOffset_sec = 0;

// フェーズの設定
bool _registerPhase = false;

// 時間をStringの形で取得
String getTimeAsString()
{
  struct tm timeinfo;
  String _timeNow = "";
  if (!getLocalTime(&timeinfo))
  {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Failed to obtain time");
    return _timeNow;
  }
  _timeNow = String(1900 + timeinfo.tm_year) + "," + String(1 + timeinfo.tm_mon) + "," + String(timeinfo.tm_mday) + "," + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec);
  return _timeNow;
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

  //時間の初期化と取得
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop()
{
  int ret;
  rcs620s.timeout = COMMAND_TIMEOUT;
  M5.update();
  M5.Lcd.setTextColor(WHITE);
  
  // ボタンAを押すことで登録フェーズに入る
  if (M5.BtnA.wasPressed())
  {
    _registerPhase = true;
    M5.Lcd.fillScreen(BLACK);
  }
  
  // 通信待ち
  ret = rcs620s.polling();
  // 待機フェーズ
  if (!_registerPhase)
  {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Press any key");
  }
  // 登録フェーズ
  else
  {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Waiting for card...");
    String felicaID = "";
    if (ret)
    {
      for (int i = 0; i < 8; i++)
      {
        if (rcs620s.idm[i] / 0x10 == 0)
          felicaID += "0";
        felicaID += String(rcs620s.idm[i], HEX);
      }
    }
    // FeliCaが置かれた場合は、Firebaseへ登録するためのjsonを作成する
    if (!felicaID.isEmpty())
    {
      // "id"というフィールドにFeliCaのmIDを登録する
      json.set("id", felicaID);
      json.set("time", getTimeAsString());
      // "User"というテーブルにユーザーの情報を登録していく
      if (Firebase.pushJSON(firebaseData, "/User", json))
      {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextColor(YELLOW);
        M5.Lcd.setCursor(0, 30);
        M5.Lcd.print(felicaID);
        M5.Lcd.setCursor(0, 60);
        M5.Lcd.print("Your card is registered now!!");
        delay(3000);
      }
      // Firebaseと通信できなければエラーを返す
      else
      {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.setCursor(0, 30);
        M5.Lcd.print("FAILED");
        M5.Lcd.setCursor(0, 60);
        M5.Lcd.print("REASON: " + firebaseData.errorReason());
        delay(10000);
      }
      // 登録が終了したら待機フェーズに戻る
      _registerPhase = false;
      M5.Lcd.fillScreen(BLACK);
    }
  }
  rcs620s.rfOff();
  delay(POLLING_INTERVAL);
}