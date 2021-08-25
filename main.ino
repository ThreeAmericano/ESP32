#include <FirebaseESP32.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// Firebase
// Your Firebase Project Web API Key
#define API_KEY "moU7k8MWLddAOylnhw66gbauTd3LcOZ7D1go1v2C"
// Your Firebase Realtime database URL
#define DATABASE_URL "https://threeamericano-default-rtdb.firebaseio.com/"
//Define FirebaseESP32 data object
FirebaseData firebaseData;
FirebaseJson homeSensing_json;

//homeSensing_json.add("humi","test");
//homeSensing_json.add("temp","test");

//WIFI, MQTT
WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "park";
const char* password = "01098964807";
const char* mqtt_server = "211.179.42.130"; 
const char* mqtt_user = "rabbit";
const char* mqtt_pass= "MQ321";

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
hw_timer_t *timer = NULL;
#define TIMER_COUNT 5000000

// 타이머 인터럽트 서비스 루틴
void IRAM_ATTR onTimer() {
  // 타이머 카운트 변수 오버플로 방지
  if (timerInterruptCnt < 0xFF) timerInterruptCnt++;
}

// while에서 체크하여 시간마다 실행할 것 작성
void chk_interrupt(){
    if (timerInterruptCnt >= 12) {
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
        //homeSensing_json.set("humi",humidity);
        //homeSensing_json.set("temp",temperature);
        homeSensing_json.set("/humi",humidity);
        homeSensing_json.set("/temp",temperature);
        Firebase.updateNode(firebaseData, "/sensor/hometemp", homeSensing_json);
        Serial.println("Firebase Upload Complete");
      }

      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.print("Temperature: ");
      Serial.print(temperature);
      }
}

void interrupt_init() {
  //프리스케일러 80 (80MHz/80 = 1MHz)
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  // 카운트 수 오백만번 -> 5초 후 인터럽트 서비스 루틴 실행
  timerAlarmWrite(timer, TIMER_COUNT, true);
  timerAlarmEnable(timer);
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  Firebase.begin(DATABASE_URL, API_KEY);
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  Firebase.setwriteSizeLimit(firebaseData, "small");
  homeSensing_json.set("/humi",humidity);
  homeSensing_json.set("/temp",temperature);
  Firebase.updateNode(firebaseData, "/sensor/hometemp", homeSensing_json);
  
  dht.begin();
  interrupt_init();
}

void loop() {
  chk_interrupt();
}
