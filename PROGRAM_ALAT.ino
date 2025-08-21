#include "SH1106Wire.h"
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define RFPin   34
#define buzzer  18
#define pinSW   25

int button =0;

const float VRef = 3.3;
const int ADCRes = 4095;
const float limit_vMax = 2.0;
const float limit_vMin = 0.9;
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
const long sensorInterval = 100;
const long displayInterval = 100;
float voltage = 0.0;

const int graphWidth = SCREEN_WIDTH;
const int graphHeight = 40;        // tinggi area grafik
const int graphYStart = SCREEN_HEIGHT - graphHeight; // posisi y awal grafik
const int graph_Xoffset = 5;
const int graphRenderWidth = SCREEN_WIDTH - graph_Xoffset;
uint8_t graphData[SCREEN_WIDTH];   // buffer grafik
int graphIndex = 0;

//buzzer global variable
bool buzzerActive = false;
unsigned long buzzerStartTime = 0;
const unsigned long buzzerDuration = 300; // total durasi bunyi buzzer (ms)
const unsigned long buzzerBlinkInterval = 30;
bool buzzerState = false;
unsigned long lastBuzzerToggle = 0;

unsigned long lastTelegramSent = 0;
const unsigned long telegramTimeCoolDown = 10000;

bool signalDetected = false;
bool ignoreRF = false;
unsigned long ignoreUntil = 0;
const int timeToIgnore = 3000;

const char* ssid = "SHAWQI";
const char* password_wifi = "200320JRR";




#define BOTtoken "7907913973:AAFM1HZ0QJZyzKTU6JJ3XC_p_IgbkETcR0k"

#define CHAT_ID "1137027730"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

SH1106Wire display(0x3c, 21, 22);

void setup() {
  Serial.begin(9600);
  pinMode (pinSW,INPUT_PULLUP);
  buzzer_off();
  analogReadResolution(12);
  delay(500);
  Serial.println("Starting...");
  display.init();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.flipScreenVertically();
  connectToWiFi();
  bot.sendMessage(CHAT_ID, "Bot started up", "");
  //display.drawString(0, 0, "Goto System.. ");
  display.display();
  delay(1000);
  // Inisialisasi data grafik dengan nilai bawah
  for (int i = 0; i < graphWidth; i++) {
    graphData[i] = graphYStart + graphHeight;
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password_wifi);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Sertifikat root Telegram

  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Connecting...");
  display.display();

  const int barX = 10;
  const int barY = 40;
  const int barWidth = SCREEN_WIDTH - 2 * barX;
  const int barHeight = 10;
  int progress = 0;

  while (WiFi.status() != WL_CONNECTED) {
    //igitalWrite(pin_led, LOW);

    // Gambar outline kotak bar
    display.drawRect(barX, barY, barWidth, barHeight);

    // Gambar progress yang meningkat
    progress += 5; // naik 5px tiap loop
    if (progress > barWidth) progress = 0;

    display.fillRect(barX + 1, barY + 1, progress, barHeight - 2);
    display.display();
    delay(300);

    // Hapus isi bar tanpa hapus outline
    display.setColor(BLACK);
    display.fillRect(barX + 1, barY + 1, barWidth - 2, barHeight - 2);
    display.setColor(WHITE);
  }

  //digitalWrite(pin_led, HIGH);

  // Tampilan setelah berhasil
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Connected to:");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 16, ssid);
  display.display();

  Serial.println("Terhubung ke WiFi");
  Serial.print("SSID: ");
  Serial.println(ssid);

  delay(1500);
  display.clear();
}

void buzzer_on() {
  pinMode(buzzer, OUTPUT);
  buzzerActive = true;
  buzzerStartTime = millis();
  lastBuzzerToggle = millis();
  buzzerState = false; // awal mati
}



void updateBuzzer() {
  if (buzzerActive) {
    unsigned long currentMillis = millis();

    // Toggle buzzer setiap 50ms
    if (currentMillis - lastBuzzerToggle >= buzzerBlinkInterval) {
      buzzerState = !buzzerState;
      digitalWrite(buzzer, buzzerState);
      lastBuzzerToggle = currentMillis;
    }

    // Matikan setelah durasi selesai
    if (currentMillis - buzzerStartTime >= buzzerDuration) {
      digitalWrite(buzzer, LOW);
      buzzerActive = false;
    }
  }
}

void buzzer_off() {
  pinMode(buzzer, INPUT);
}

void test_Send_notif(){

  button = digitalRead(pinSW);

  if(button==0){
    
         bot.sendMessage(CHAT_ID, "Signal Phone Detected !", "");
  
  }
  
}

void loop() {
  unsigned long currentMillis = millis();

  // test with button to send notif
  /*
   if (currentMillis - lastSensorRead >= sensorInterval) {
    int adcVal = analogRead(RFPin);
    voltage = ((float)adcVal / ADCRes) * VRef;

    test_Send_notif();

    if ((voltage > limit_vMax) || (voltage < limit_vMin)) {
      Serial.println("Signal detected !!!");
      buzzer_on();
    }

    else{
      buzzer_off();
    }

    graphData[graphIndex] = graphYStart + graphHeight - (voltage / VRef) * graphHeight;
    graphIndex = (graphIndex + 1) % graphWidth;

    lastSensorRead = currentMillis;
   }
*/

  if(ignoreRF && currentMillis < ignoreUntil){
    updateBuzzer();
    return;
  }
  // TASK 1: Pembacaan Sensor
  if (currentMillis - lastSensorRead >= sensorInterval) {
    int adcVal = analogRead(RFPin);
    voltage = ((float)adcVal / ADCRes) * VRef;

    bool signalNow = ((voltage > limit_vMax) || (voltage < limit_vMin));

    if(signalNow){
    
      if(!signalDetected){

    //if ((voltage > limit_vMax) || (voltage < limit_vMin)) {
      Serial.println("Signal detected !!!");
      buzzer_on();

    
      if(currentMillis - lastTelegramSent >= telegramTimeCoolDown){
      bot.sendMessage(CHAT_ID, "Signal Phone Detected !", "");
      lastTelegramSent = currentMillis;
      ignoreRF = true;
      ignoreUntil = currentMillis + timeToIgnore; // mengabaikan 3 detik pembacaan
      
      }

      signalDetected = true;
      
    } 
    }
    else {
      signalDetected = false;
      buzzer_off();
    }

    Serial.print("ADC: ");
    Serial.print(adcVal);
    Serial.print(" | Voltage: ");
    Serial.print(voltage, 1);
    Serial.println(" V");

    // Simpan nilai grafik, skala dibalik agar atas = tegangan tinggi
    graphData[graphIndex] = graphYStart + graphHeight - (voltage / VRef) * graphHeight;
    graphIndex = (graphIndex + 1) % graphWidth;

    lastSensorRead = currentMillis;
  }
  

  updateBuzzer();

  // TASK 2: Update Tampilan OLED
 if (currentMillis - lastDisplayUpdate >= displayInterval) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  //display.drawString(0, 0, "Voltage:");
  //display.drawString(70, 0, String(voltage, 1));
  //display.drawString(100, 0, "V");
  String textVoltage = "Voltage: " + String(voltage,1) + " volt";
  display.drawString (0, 0, textVoltage);
  

  // Gambar sumbu Y (vertikal) di x = 4
display.drawVerticalLine(4, graphYStart, graphHeight);

// Tambahkan strip-strip kecil di sumbu Y (setiap 1/3 dari tinggi grafik)
for (int i = 0; i <= 3; i++) {
  int y = graphYStart + (i * graphHeight / 3);
  display.drawHorizontalLine(2, y, 4); // strip horizontal kecil (panjang 4 px)
}

// Gambar sumbu X (horizontal) di y = graphYStart + graphHeight - 1
int sumbuX_Y = graphYStart + graphHeight - 1;
display.drawHorizontalLine(4, sumbuX_Y, graphWidth - 4);

// Tambahkan strip-strip kecil di sumbu X setiap 10px
for (int x = 10; x < graphWidth; x += 10) {
  display.drawVerticalLine(x, sumbuX_Y - 2, 3); // strip vertikal kecil (tinggi 3 px)
}

  // Gambar grafik berdasarkan graphData
    //int lastY = graphData[(graphIndex + 0) % graphWidth];
    int lastY = graphData[(graphIndex + 0) % SCREEN_WIDTH];
    for (int i = 1; i < graphRenderWidth; i++) {
      int dataIndex = (graphIndex + i) % SCREEN_WIDTH;
      int y = graphData[dataIndex];
      int x = graph_Xoffset+i;

      // Gambar garis vertikal antar titik (agar grafik smooth)
      int yMin = min(lastY, y);
      int yMax = max(lastY, y);
      for (int j = yMin; j <= yMax; j++) {
        if (j >= 0 && j < SCREEN_HEIGHT) {
          display.setPixel(x, j);
        }
      }

      lastY = y;
    }

  display.display();
  lastDisplayUpdate = currentMillis;
}
}


