#include <FirebaseESP32.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Adafruit_NeoPixel.h>

// 가전 동작 상태
String applianceStatus = "00000000";
String receiveMsg = "018050000";

#define GAS_LED 16
#define GAS_VALVE 17
#define VALVE_CNT 10 // 1초
boolean gasLedStatus = LOW;
volatile boolean gasValveStatus = false;
boolean gasStatusFlag = false;
volatile unsigned char gasCnt = 0;

// Firebase
// Your Firebase Project Web API Key
#define API_KEY "PPUQ6zcZBZgoOr1FsRB6xceoNL8M39nqad5NRtBl"
// Your Firebase Realtime database URL
#define DATABASE_URL "threeamericano-default-rtdb.firebaseio.com"
//Define FirebaseESP32 data object
FirebaseData firebaseData;
FirebaseJson homeSensing_json;

//Airconditionor
unsigned char windPower = 8;
#define PWM_RESOLUTION 8
#define AIRCON_GPIO 21
#define AIRCON_PWM_CHANNEL 0
#define AIRCON_DUTY (26*windPower)
#define AIRCON_PWM_FREQUENCY 100

//WIFI, MQTT
WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "park";
const char* password = "01098964807";
const char* mqtt_server = "211.179.42.130"; 
const char* mqtt_user = "rabbit";
const char* mqtt_pass= "MQ321";

void callback(char* topic, byte* payload, unsigned int length) {  
  if(length != 9) {
    Serial.println("WrongData Received");
  }
  
  else {
      receiveMsg = "";
      for (int i = 0; i < length; i++) {
        receiveMsg +=(char)payload[i];
      }
      Serial.println(receiveMsg);

      for(int i = 1; i < 9; i++) {
          applianceStatus[i-1] = receiveMsg[i];
        }
        if(applianceStatus[0] == '0') ledcWrite(AIRCON_PWM_CHANNEL, 0);
        if(applianceStatus[0] == '1'){
            windPower = applianceStatus[1] - '0';
            Serial.print("windPower : ");
            Serial.println(windPower);
            ledcWrite(AIRCON_PWM_CHANNEL, (windPower*26));
        }

        if(applianceStatus[7] == '0') digitalWrite(GAS_LED, LOW);
        else digitalWrite(GAS_LED, HIGH);

        Firebase.setString(firebaseData, "/smarthome/status", applianceStatus);
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
volatile char timerInterruptCnt = 0;
volatile char applianceTimerCnt = 0;
hw_timer_t *timer = NULL;
#define TIMER_COUNT 100000

// 타이머 인터럽트 서비스 루틴
void IRAM_ATTR onTimer() {
  // 타이머 카운트 변수 오버플로 방지
  if (timerInterruptCnt < 0xFF) timerInterruptCnt++;
//  if (applianceTimerCnt < 0xFF) applianceTimerCnt++;
  if (!(digitalRead(GAS_VALVE))){
    if (gasCnt < 0xFF) gasCnt++;
  }else gasCnt = 0;
}

// while에서 체크하여 시간마다 실행할 것 작성
void chk_interrupt(){

   // 버튼이 1초 이상 눌린 경우 LED toggle 실행
    if(gasCnt >= 10) {
      if(gasValveStatus == false) {
          gasValveStatus = true;
          gasLedStatus = gasLedStatus? LOW : HIGH;
          applianceStatus[7] = gasLedStatus? '1' : '0';
          Firebase.setString(firebaseData, "/smarthome/status", applianceStatus);
          digitalWrite(GAS_LED, gasLedStatus);
      }
    }
    else {
      gasValveStatus = false;
    }
    
    // 10초마다 가전명령 수행
//    if(applianceTimerCnt >= 100) {
//        applianceTimerCnt = 0;
//        for(int i = 0; i < 9; i++) {
//          applianceStatus[i] = receiveMsg[i];
//        }
//        if(applianceStatus[1] == '0') ledcWrite(AIRCON_PWM_CHANNEL, 0);
//        if(applianceStatus[1] == '1'){
//            windPower = applianceStatus[2] - '0';
//            Serial.print("windPower : ");
//            Serial.println(windPower);
//            ledcWrite(AIRCON_PWM_CHANNEL, (windPower*26));
//        }
//
//        if(applianceStatus[8] == '0') digitalWrite(GAS_LED, LOW);
//        else digitalWrite(GAS_LED, HIGH); 
    
    //}
    
    if (timerInterruptCnt >= 300) {
      Serial.print("Firebase Status : ");
      Serial.println(Firebase.ready());
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

void setup() {
  Serial.begin(115200);
  pinMode(GAS_LED, OUTPUT);
  pinMode(GAS_VALVE, INPUT);
  setup_wifi();
  Firebase.begin(DATABASE_URL, API_KEY);
  Firebase.reconnectWiFi(true);
  Firebase.setFloat(firebaseData, "/sensor/hometemp/humi", humidity);
  Firebase.setFloat(firebaseData, "/sensor/hometemp/temp", temperature);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  Serial.println(client.subscribe("webos.smarthome.#"));
  
  //ledcSetup(ch, freq, resolution)
  ledcSetup(AIRCON_PWM_CHANNEL, AIRCON_PWM_FREQUENCY, PWM_RESOLUTION);
  //ledcAttachPin(GPIO, ch) : GPIO : GPIO Number, ch : PWM Channel
  ledcAttachPin(AIRCON_GPIO, AIRCON_PWM_CHANNEL);
  ledcWrite(AIRCON_PWM_CHANNEL, AIRCON_DUTY);
  
  dht.begin();
  interrupt_init();
}

void loop() {
  chk_interrupt();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
