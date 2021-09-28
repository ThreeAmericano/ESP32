#include <FirebaseESP32.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal.h>
#include <ESP32_Servo.h>

LiquidCrystal LCD(13, 14, 27, 26, 25, 33);

// 가전 동작 상태
String statusMsg = "000000000";
String lastStatusMsg = "000000000";
#define GAS_LED 16
#define GAS_VALVE 17
#define LIGHT_SWITCH 4
#define AIRCON_SWITCH 5
#define VALVE_CNT 10 // 1초

volatile boolean gasValveStatus = false;
boolean gasStatusFlag = false;

volatile boolean lightSwitchStatus = false;
boolean lightStatusFlag = false;

volatile boolean airconSwitchStatus = false;
boolean airconStatusFlag = false;

volatile unsigned char gasCnt = 0;
volatile unsigned char lightCnt = 0;
volatile unsigned char airconCnt = 0;

//WS2813
#define LIGHT_GPIO 32
#define LIGHT_NUM  29 
#define LIGHT_SLEEP_BRIGHT 40

boolean lightBrightnessUpFlag = false;
unsigned char lightBrightness = 40;
unsigned char curLightColor = 0;
unsigned char curLightMode = 0;
unsigned char lightModeColor = 0;
unsigned char lightModeCnt = 0;
unsigned char lightCurPos = 0;

unsigned char lightModeMaxCnt = 5;
const unsigned char lightColor[8][3] = {
  {255,255,255},
  {255,0,0},
  {0,0,255},
  {0,255,0},
  {255,255,80},
  {130,70,190},
  {255,100,60},
  {0,170,255},
};

const String lightColorName[8] = {
  "WHITE",
  "RED",
  "BLUE",
  "GREEN",
  "YELLOW",
  "PURPLE",
  "ORANGE",
  "SKY"
};

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LIGHT_NUM,LIGHT_GPIO, NEO_GRB + NEO_KHZ800);

// Firebase
// Your Firebase Project Web API Key

#define API_KEY "iWvqmeNnKgPBCLLEv6aASnb3wk3zSDp55bdToKiE"
// Your Firebase Realtime database URL
#define DATABASE_URL "threeamericano-default-rtdb.firebaseio.com"

//Define FirebaseESP32 data object
FirebaseData firebaseData;
FirebaseJson homeSensing_json;

//Airconditionor
unsigned char windPower = 8;
#define PWM_RESOLUTION 8
#define AIRCON_GPIO 21
#define AIRCON_PWM_CHANNEL 2
#define AIRCON_DUTY (25*windPower)
#define AIRCON_PWM_FREQUENCY 100

//window
#define WINDOW_DETECT 1
#define WINDOW_PWM_CHANNEL 0
#define WINDOW_PWM_FREQUENCY 100
#define WINDOW_MOTOR_CON 22
#define WINDOW_MOTOR_POW 23
volatile boolean windowOpenFlag = false;
volatile boolean windowCloseFlag = false;
volatile boolean lastWindowStatus = LOW;
volatile unsigned char windowDetectTimerCnt = 0;
volatile unsigned char windowAngle = 0;

#define RAINDROP_SENSOR 2
volatile unsigned char raindrop_time = 0;
volatile boolean raindrop_sensing = false;

volatile boolean servoFlag = false;
volatile unsigned char modeFlagCnt = 0;
Servo servo1;

//WIFI, MQTT
WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "park";
const char* password = "01098964807";
const char* mqtt_server = "211.179.42.130"; 
const char* mqtt_user = "rabbit";
const char* mqtt_pass= "MQ321";

void LCD_Clear(int line)
{
  LCD.setCursor(0,line);
  LCD.print("                    ");
  delay(10);
}

void windowOpen() {
  LCD.setCursor(14,3);
  LCD.print("OPEN ");
  //Serial.println("windowOpen");

  windowAngle = 0;
  windowCloseFlag = false;
  windowOpenFlag = true;
  digitalWrite(WINDOW_MOTOR_POW, HIGH);
}

void windowClose() {
  LCD.setCursor(14,3);
  LCD.print("CLOSE");
  //Serial.println("windowClose");

  windowAngle = 110;
  windowOpenFlag = false;
  windowCloseFlag = true;
  digitalWrite(WINDOW_MOTOR_POW, HIGH);
}


void airconOff()
{
  ledcWrite(AIRCON_PWM_CHANNEL, 0);
  LCD_Clear(1);
  LCD.setCursor(0,1);
  LCD.print("AIRCON:OFF,POWER:00%");
}

void airconOn()
{
  windPower = statusMsg[2] - '0';
  ledcWrite(AIRCON_PWM_CHANNEL, (windPower*25));
  LCD_Clear(1);
  LCD.setCursor(0,1);
  LCD.print("AIRCON:ON,");
  LCD.setCursor(10,1);
  LCD.print("POWER:");
  LCD.print(windPower*10);
  LCD.print("%");
}

void lightOff()
{
   for(int j=0; j<LIGHT_NUM; j++) {
    pixels.setPixelColor(j, 0,0,0);
    pixels.show();
    delay(1);
   }
  curLightMode = 0;
  pixels.show();
  LCD_Clear(2);
  LCD.setCursor(0,2);
  LCD.print("LIGHT:OFF,00%");
}

void lightOn()
{
   pixels.clear();
   lightBrightness = (statusMsg[4] - '0');
   pixels.setBrightness(lightBrightness * 25);
   curLightColor = statusMsg[5]- '0';
   curLightMode = statusMsg[6] - '0';
   if(curLightMode == 1) lightModeMaxCnt = 15;
   else if(curLightMode == 2) lightModeMaxCnt = 5;
   for(int j=0; j<LIGHT_NUM; j++) {
    pixels.setPixelColor(j, pixels.Color(lightColor[curLightColor][0],lightColor[curLightColor][1],lightColor[curLightColor][2]));
    pixels.show();
    delay(3);
   }
   
   LCD_Clear(2);
   LCD.setCursor(0,2);
   LCD.print("LIGHT:ON,");
   LCD.print(lightBrightness*10);
   LCD.print("%,");
   LCD.print(lightColorName[curLightColor]);
}

void gasOff()
{
  digitalWrite(GAS_LED, LOW);
  LCD.setCursor(4,3);
  LCD.print("OFF,");
}

void gasOn()
{
  digitalWrite(GAS_LED, HIGH);
  LCD.setCursor(4,3);
  LCD.print(" ON,");
}

void statusUpdate(String str)
{
    if(str[1] == '0')
    {
      airconOff();
    }
  
    else if(str[1] == '1'){
      airconOn();
    }

  //light
  if(str[3] != lastStatusMsg[3] ||
     str[4] != lastStatusMsg[4] || 
     str[5] != lastStatusMsg[5] ||
     str[6] != lastStatusMsg[6])
  {
    if(str[3] == '0')
    {
      lightOff();
    }
    else if(str[3] == '1'){
      lightOn();
    }
  }

  if(str[7] != lastStatusMsg[7])
  {
    if(str[7] == '0'){
      windowClose();
    }
    else if(str[7] == '1'){
      windowOpen();
    }
  }

    if(str[8] == '0'){
      gasOff();
    }
    else if(str[8] == '1'){
      gasOn();
    }
  
  
  Firebase.setString(firebaseData, "/smarthome/status", statusMsg);
  lastStatusMsg = statusMsg;
}

void MQTT_Received(char* topic, byte* payload, unsigned int length) {  
  if(length != 9) {
    Serial.println("WrongData Received");
  }
  
  else {
      String receiveMsg = "";
      for (int i = 0; i < length; i++) {
        receiveMsg +=(char)payload[i];
      }

//      Serial.print("receiveMsg : ");
//      Serial.println(receiveMsg);
      statusMsg[0] = receiveMsg[0];
      
      if(receiveMsg[1] == '0')
        statusMsg[1] = '0';
      
      else if(receiveMsg[1] == '1'){
        statusMsg[1] = '1';
        statusMsg[2] = receiveMsg[2];
      }

      if(receiveMsg[3] == '0')
        statusMsg[3] = '0';
      
        
      else if(receiveMsg[3] == '1') {
        for(int i=3; i<7; i++) {
          statusMsg[i] = receiveMsg[i];
        }
      }

      if(receiveMsg[7] == '0')
      {
        statusMsg[7] = receiveMsg[7];
      }

      else if(receiveMsg[7] == '1')
      {
        statusMsg[7] = receiveMsg[7];
      }


      if(receiveMsg[8] == '0')
      {
        statusMsg[8] = receiveMsg[8];
      }

      else if(receiveMsg[8] == '1')
      {
        statusMsg[8] = receiveMsg[8];
      }
      
      statusUpdate(receiveMsg);      
  }
}

void setup_wifi() {
  // Connecting to a WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


// 온습도센서
#define DHTPIN 15
#define DHTTYPE DHT11   // DHT 11, DHT시리즈중 11을 선택합니다.
DHT dht(DHTPIN, DHTTYPE); 
float humidity = 75;
float temperature = 24.5;

// 타이머
volatile unsigned int timerInterruptCnt = 0;
hw_timer_t *timer = NULL;
#define TIMER_COUNT 10000

// 타이머 인터럽트 서비스 루틴
void IRAM_ATTR onTimer() {
  // 타이머 카운트 변수 오버플로 방지
  if (timerInterruptCnt < 0xFFFF) timerInterruptCnt++;
  if (raindrop_time < 0xFF) raindrop_time++;
  servoFlag = true;

  if(curLightMode != 0) {
    if(lightModeCnt < 0xFF) lightModeCnt++;
  }
  if(lastWindowStatus == (digitalRead(WINDOW_DETECT))){
    if(windowDetectTimerCnt < 0xFF) windowDetectTimerCnt++;
  }else{
    lastWindowStatus = !lastWindowStatus;
    windowDetectTimerCnt = 0;
  }
  
  if(lightModeCnt >= lightModeMaxCnt) {
    lightModeCnt = 0;
    if(curLightMode == 1)
    {
      if(lightBrightnessUpFlag){
        if(lightBrightness < 60) lightBrightness++;
        else {
          lightBrightnessUpFlag = false;
        }
      }
      else{
        if(lightBrightness > 0) lightBrightness--;
        else {
          lightBrightnessUpFlag = true;
        }
      }
      //Serial.println(lightBrightness);
      pixels.setBrightness(lightBrightness);
      for(int i =0; i < LIGHT_NUM; i++) {
        pixels.setPixelColor(i, pixels.Color(lightColor[lightModeColor][0],lightColor[lightModeColor][1],lightColor[lightModeColor][2]));
        pixels.show();
      }
    }
    
    else if(curLightMode == 2)
    {
      if(lightCurPos <= LIGHT_NUM) lightCurPos++;
        else {
                lightCurPos = 0;
                if(lightModeColor < 7) lightModeColor++;
                else lightModeColor = 0;
        }
        pixels.setPixelColor(lightCurPos, pixels.Color(lightColor[lightModeColor][0],lightColor[lightModeColor][1],lightColor[lightModeColor][2]));
        pixels.show();
    }
  }
  
  if (!(digitalRead(GAS_VALVE))){
    if (gasCnt < 0xFF) gasCnt++;
  }else gasCnt = 0;
  if (!(digitalRead(LIGHT_SWITCH))){
    if (lightCnt < 0xFF) lightCnt++;
  }else lightCnt = 0;
  if (!(digitalRead(AIRCON_SWITCH))){
    if (airconCnt < 0xFF) airconCnt++;
  }else airconCnt = 0;
}

// while에서 체크하여 시간마다 실행할 것 작성
void chk_interrupt(){
    if(servoFlag) {
      servoFlag = false;

    if(raindrop_time >= 200){
      raindrop_time = 0;
      if(digitalRead(RAINDROP_SENSOR)){
        Firebase.setString(firebaseData, "/sensor/hometemp/rain", "0");
      }else{
        Firebase.setString(firebaseData, "/sensor/hometemp/rain", "1");
      }
    }

    if(windowOpenFlag) {
      windowAngle+=1;
      servo1.write(windowAngle);
      if(windowAngle >= 110) {
        windowOpenFlag = false;
        digitalWrite(WINDOW_MOTOR_POW, LOW);
      }
    }

    if(windowCloseFlag) {
      windowAngle-=1;
      servo1.write(windowAngle);
      if(windowAngle <= 0) {
        windowCloseFlag = false;
        digitalWrite(WINDOW_MOTOR_POW, LOW);
      }
    }
  }

//    
//    if(lightModeCnt >= 5) {
//      lightModeCnt = 0;
//      if(lightCurPos <= LIGHT_NUM) lightCurPos++;
//      else {
//              lightCurPos = 0;
//              if(lightModeColor < 7) lightModeColor++;
//              else lightModeColor = 0;
//      }
//      pixels.setPixelColor(lightCurPos, pixels.Color(lightColor[lightModeColor][0],lightColor[lightModeColor][1],lightColor[lightModeColor][2]));
//      pixels.show();
//    }

////        }
////        pixels.setPixelColor(lightModeCnt, pixels.Color(lightColor[lightModeColor][0],lightColor[lightModeColor][1],lightColor[lightModeColor][2]));
////        pixels.show();
////        delay(2);
//      if(lightBrightnessUpFlag) {
//        lightBrightness++;
//        if(lightBrightness >= LIGHT_SLEEP_BRIGHT) {
//          lightBrightnessUpFlag = false;
//        }
//         
//      }else{
//        lightBrightness--;
//        if(lightBrightness == 0) {
//          lightBrightnessUpFlag = true;
//        }
//      }
//      pixels.setBrightness(lightBrightness);
//      pixels.show();
//      }

    // 창문 감지가 2초 이상 된 경우
    if(windowDetectTimerCnt >= 200) {
        windowDetectTimerCnt = 0;
        
        // 창문 열린 경우
    if( (!windowOpenFlag) && (!windowCloseFlag)) 
    {
        if(lastWindowStatus) {
            if(statusMsg[7] != '1') {
                statusMsg[7] = '1';
                Firebase.setString(firebaseData, "/smarthome/status", statusMsg);
                windowAngle = 110;
            }
            
            LCD.setCursor(14,3);
            LCD.print("OPEN ");
        }

        // 창문 닫힌 경우
        else{
            if(statusMsg[7] != '0') {
                statusMsg[7] = '0';
                Firebase.setString(firebaseData, "/smarthome/status", statusMsg);
                windowAngle = 0;
            }
            LCD.setCursor(14,3);
            LCD.print("CLOSE");
        }
    }
   }

   // 버튼이 1초 이상 눌린 경우 LED toggle 실행
    if(gasCnt >= 100) {
      if(gasValveStatus == false) {
          gasValveStatus = true;
          if(statusMsg[8] == '1'){
            statusMsg[8] = '0';
            gasOff();
          }
          else{
            statusMsg[8] = '1';
            gasOn();
          }
          statusMsg[0] = '0';
          Firebase.setString(firebaseData, "/smarthome/status", statusMsg);
      }
    }
    else {
      gasValveStatus = false;
    }

    if(lightCnt >= 100) {
      if(lightSwitchStatus == false) {
          lightSwitchStatus = true;
          if(statusMsg[3] == '1'){
            statusMsg[3] = '0';
            lightOff();
          }
          else{
            statusMsg[3] = '1';
            lightOn();            
          }
          statusMsg[0] = '0';
          Firebase.setString(firebaseData, "/smarthome/status", statusMsg);
      }
    }
    else {
      lightSwitchStatus = false;
    }

    if(airconCnt >= 100) {
      if(airconSwitchStatus == false) {
          airconSwitchStatus = true;
          if(statusMsg[1] == '1'){
            statusMsg[1] = '0';
            airconOff();
          }
          else{
            statusMsg[1] = '1';
            airconOn();            
          }
          statusMsg[0] = '0';
          Firebase.setString(firebaseData, "/smarthome/status", statusMsg);
      }
    }
    else {
      airconSwitchStatus = false;
    }
    
    if (timerInterruptCnt >= 6000) {
      timerInterruptCnt = 0;
      
      //습도 측정
      humidity = dht.readHumidity();
      //온도 측정
      temperature = dht.readTemperature();

      // 측정에 오류가 발생하면 오류를 출력
      if(isnan(humidity) || isnan(temperature)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
      }

      // 측정에 문제가 없을시 데이터를 firebase에 저장
      else {
          Firebase.setFloat(firebaseData, "/sensor/hometemp/humi", humidity);
          Firebase.setFloat(firebaseData, "/sensor/hometemp/temp", temperature);
      }

      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.print("Temperature: ");
      Serial.println(temperature);

      LCD.setCursor(4,0);
      LCD.print(humidity,1);
      LCD.print("% ");
      LCD.setCursor(15,0);
      LCD.print(temperature,1);
      LCD.print("C");
      }
}

void interrupt_init() {
  //프리스케일러 80 (80MHz/80 = 1MHz)
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  // 카운트 수 십만번 -> 0.1초 후 인터럽트 서비스 루틴 실행
  timerAlarmWrite(timer, TIMER_COUNT, true);
  timerAlarmEnable(timer);
}

void reconnect() {
  // Loop until we're reconnected
  Serial.println("In reconnect...");
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("PSW_Arduino", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      client.subscribe("webos.smarthome.#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void LCD_Init(){
  LCD.begin(20,4);
  LCD.clear();
  LCD.setCursor(0,0);
  LCD.print("HDT:");
  LCD.print(humidity,1);
  LCD.print("% ");
  LCD.print("TEMP:");
  LCD.print(temperature,1);
  LCD.print("C");
  LCD.setCursor(0,1);
  LCD.print("AIRCON:OFF ");
  LCD.setCursor(11,1);
  LCD.print("POWER:00%");
  LCD.setCursor(17,1);
  LCD.setCursor(0,2);
  LCD.print("LAMP:OFF,");
  LCD.print("00%, WHITE");
  LCD.setCursor(0,3);
  LCD.print("GAS:OFF, WIDW:CLOSE");
}

void setup() {
  Serial.begin(115200);
  pinMode(GAS_LED, OUTPUT);
  pinMode(GAS_VALVE, INPUT);
  pinMode(LIGHT_SWITCH, INPUT);
  pinMode(AIRCON_SWITCH, INPUT);
  pinMode(WINDOW_DETECT, INPUT);
  pinMode(WINDOW_MOTOR_POW, OUTPUT);
  pinMode(RAINDROP_SENSOR, INPUT);
  digitalWrite(WINDOW_MOTOR_POW, LOW);
  servo1.attach(WINDOW_MOTOR_CON);
  
  setup_wifi();
  Firebase.begin(DATABASE_URL, API_KEY);
  Firebase.reconnectWiFi(true);


//  while(!(Firebase.getString(firebaseData, "smarthome/status"))){
//    Serial.println("FIREBASE FAULT");
//    delay(500);
//    Firebase.begin(DATABASE_URL, API_KEY);
//  }
//  
//  statusMsg = firebaseData.stringData();
//  Serial.print("statusMsg : ");
//  Serial.println(statusMsg);

    if(Firebase.getString(firebaseData, "smarthome/status")){ 
        statusMsg = firebaseData.stringData();
//        Serial.print("statusMsg : ");
//        Serial.println(statusMsg);
    }
    else {
      Serial.println("FIREBASE FAULT");
    }
  
      
  delay(500);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(MQTT_Received);
  Serial.println(client.subscribe("webos.smarthome.#"));

  //ledcSetup(ch, freq, resolution)
  ledcSetup(AIRCON_PWM_CHANNEL, AIRCON_PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(AIRCON_GPIO, AIRCON_PWM_CHANNEL);

  LCD_Init();

  pixels.begin();
  pixels.show();
  
  dht.begin();
  interrupt_init();
  statusUpdate(statusMsg);
}

void loop() {
  chk_interrupt();
  if (!client.connected()) {
    reconnect();
  }
  //client.disconnect();
  client.loop();
}
