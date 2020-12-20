#include <Arduino.h>
#include <FirebaseESP32.h>
#include <M5Stack.h>
#include "RCS620S.h"
#include <time.h>
#include <ArduinoJson.h>

// WiFi, Firebaseの設定情報
// ID and Auth key for Firebase
#define FIREBASE_HOST ""
#define FIREBASE_AUTH ""
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Instantiate related with Firebase
// Firebaseのインスタンスを生成
FirebaseData firebaseData;
// FirebaseJsonのインスタンスを生成
FirebaseJson json;
// QueryFilterのインスタンスを生成
QueryFilter query;

// FeliCaの設定情報
// For FeliCa
#define COMMAND_TIMEOUT 400
#define POLLING_INTERVAL 500
RCS620S rcs620s;

// 時間取得用の定数
// Constant value for getting the time
const char *ntpServer = "ntp.jst.mfeed.ad.jp";
const long gmtOffset_sec = 9 * 3600;
const int daylightOffset_sec = 0;

// フェーズの設定
// Phase valuable
bool _registerPhase = false;
bool _authenticatePhase = false;

// 時間をStringの形で取得
// Get the time as String
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
  // Connect to WiFi
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
  // Related with Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  // FeliCaへの接続確認
  // Check FeliCa Reader is connected or not
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

  // 時間の初期化と取得
  // Initialize and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // どのフィールドから探すかを指定
  // Specify field name which you want to search
  query.orderBy("id");
}

void loop()
{
  int ret;
  rcs620s.timeout = COMMAND_TIMEOUT;
  M5.update();
  M5.Lcd.setTextColor(WHITE);

  // ボタンで機能を変える
  // Push the button to select the function
  if (M5.BtnA.wasPressed())
  {
    _registerPhase = true;
    M5.Lcd.fillScreen(BLACK);
  }

  if (M5.BtnB.wasPressed())
  {
    _authenticatePhase = true;
    M5.Lcd.fillScreen(BLACK);
  }

  // 通信待ち
  // Polling
  ret = rcs620s.polling();
  // 待機フェーズ
  // Waiting for pressing any key
  if (!_authenticatePhase)
  {
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Press any key");
  }
  // 認証フェーズ
  // Authentication phase
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
    // 検索キーワードの指定
    // 今回はリーダーに置かれたICカードのmIDと一致するidを検索する
    // Specify searching keyword
    // In this time, I want to search "id" which is matched mID of IC Card
    query.equalTo(felicaID);
    if (!felicaID.isEmpty())
    {
      // "User"というテーブルから検索
      // Search from "User" table
      if (Firebase.getJSON(firebaseData, "User", query))
      {
        String key, value = "";
        String time = "";
        int type = 0;
        size_t len = firebaseData.jsonObject().iteratorBegin();
        // ArduinoJson用の変数
        // Valuable for ArduinoJson
        const size_t capacity = 500;
        DynamicJsonDocument recordInfo(capacity);
        // 今回の場合、
        // i=0で検索と一致する全てのフィールド（json）、
        // i=1で1番目のフィールド（今回だとid）、
        // i=2で2番目のフィールド（今回だとtime）が返って来る
        // In this time, i = 0: json which is matched the mID, i = 1: 1st field value of matched mID("id"), i = 2: 2nd field value of matched mID("time")
        for (size_t i = 0; i < len; i++)
        {
          firebaseData.jsonObject().iteratorGet(i, type, key, value);
          // iが0のときはmIDと一致するjsonが返ってくる
          // When i = 0, json which is matched the mID is returned
          if (i == 0)
          {
            // jsonとして取得したい場合は、ArduinoJsonライブラリを使う
            // If you want to get data as json, you should use ArudinoJson library
            DeserializationError error = deserializeJson(recordInfo, value);
            if (error)
            {
              M5.Lcd.fillScreen(BLACK);
              M5.Lcd.setCursor(0, 0);
              M5.Lcd.print(error.c_str());
              delay(5000);
              return;
            }
          }
          // キーを指定して取得することもできる
          // You can get the value by using key name
          if (key == "time")
          {
            time = value;
          }
        }
        // 一致するmIDをjsonファイルから取り出す
        // Extract mID from json
        const char* mID = recordInfo["id"];
        if (len != 0)
        {
          json.iteratorEnd();
          M5.Lcd.fillScreen(BLACK);
          M5.Lcd.setCursor(0, 0);
          M5.Lcd.print("ID:");
          M5.Lcd.println(mID);
          M5.Lcd.print("time:");
          M5.Lcd.println(time);
          delay(3000);
        }
        else
        {
          M5.Lcd.fillScreen(BLACK);
          M5.Lcd.setCursor(0, 0);
          M5.Lcd.print("This card is not registered");
          delay(3000);
        }
      }
      else
      {
        // Firebaseから取得に失敗した場合はエラーを返す
        // Return error if you cannot get RDB from Firebase
        Serial.println(firebaseData.errorReason());
      }
      // 登録が終了したら待機フェーズに戻る
      _authenticatePhase = false;
      M5.Lcd.fillScreen(BLACK);
    }
  }
  rcs620s.rfOff();
  delay(POLLING_INTERVAL);

  // 待機フェーズ
  // if (!_registerPhase)
  // {
  //   M5.Lcd.setCursor(0, 0);
  //   M5.Lcd.print("Press any key");
  // }
  // // 登録フェーズ
  // else
  // {
  //   M5.Lcd.setCursor(0, 0);
  //   M5.Lcd.print("Waiting for card...");
  //   String felicaID = "";
  //   if (ret)
  //   {
  //     for (int i = 0; i < 8; i++)
  //     {
  //       if (rcs620s.idm[i] / 0x10 == 0)
  //         felicaID += "0";
  //       felicaID += String(rcs620s.idm[i], HEX);
  //     }
  //   }
  //   // FeliCaが置かれた場合は、Firebaseへ登録するためのjsonを作成する
  //   if (!felicaID.isEmpty())
  //   {
  //     // "id"というフィールドにFeliCaのmIDを登録する
  //     json.set("id", felicaID);
  //     json.set("time", getTimeAsString());
  //     // "User"というテーブルにユーザーの情報を登録していく
  //     if (Firebase.pushJSON(firebaseData, "/User", json))
  //     {
  //       M5.Lcd.fillScreen(BLACK);
  //       M5.Lcd.setTextColor(YELLOW);
  //       M5.Lcd.setCursor(0, 30);
  //       M5.Lcd.print(felicaID);
  //       M5.Lcd.setCursor(0, 60);
  //       M5.Lcd.print("Your card is registered now!!");
  //       delay(3000);
  //     }
  //     // Firebaseと通信できなければエラーを返す
  //     else
  //     {
  //       M5.Lcd.setTextColor(RED);
  //       M5.Lcd.setCursor(0, 30);
  //       M5.Lcd.print("FAILED");
  //       M5.Lcd.setCursor(0, 60);
  //       M5.Lcd.print("REASON: " + firebaseData.errorReason());
  //       delay(10000);
  //     }
  //     // 登録が終了したら待機フェーズに戻る
  //     _registerPhase = false;
  //     M5.Lcd.fillScreen(BLACK);
  //   }
  // }
  // rcs620s.rfOff();
  // delay(POLLING_INTERVAL);
}