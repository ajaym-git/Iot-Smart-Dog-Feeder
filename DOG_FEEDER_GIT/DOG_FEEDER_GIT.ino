#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/* ================= LCD ================= */
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* ================= WIFI ================= */
const char* sta_ssid     = "YOUR_WiFi_name";
const char* sta_password = "WiFi_password";

const char* ap_ssid     = "DogFeeder_AP";
const char* ap_password = "12345678";

/* ================= MQTT ================= */
#define MQTT_SERVER "57d98099abe74b37b2af2cc640b99e73.s1.eu.hivemq.cloud"
#define MQTT_PORT   8883
#define MQTT_USER   "USER_NAME"
#define MQTT_PASS   "PASSWORD"

/* ================= GPIO ================= */
#define RELAY_PIN   12 // D6
#define BUTTON_PIN  14  // D5
#define BUZZER_PIN  13   // D7

#define RELAY_ON    HIGH
#define RELAY_OFF   LOW
#define RELAY_TIME  3000

/* ================= OBJECTS ================= */
ESP8266WebServer server(80);
WiFiClientSecure espClient;
PubSubClient client(espClient);

/* ================= RELAY ================= */
bool relayActive = false;
unsigned long relayStartMillis = 0;

/* ================= BUTTON ================= */
volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;

void IRAM_ATTR handleButtonInterrupt() {
  if (millis() - lastInterruptTime > 200) {
    buttonPressed = true;
    lastInterruptTime = millis();
  }
}

/* ================= BUZZER ================= */
bool buzzerActive = false;
unsigned long buzzerMillis = 0;
int buzzerCount = 0;
int totalBeeps = 3;

void startBuzzer() {
  buzzerActive = true;
  buzzerMillis = millis();
  buzzerCount = 0;
}

void buzzerHandler() {
  if (!buzzerActive) return;

  if (millis() - buzzerMillis >= 150) {
    buzzerMillis = millis();

    digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    buzzerCount++;

    if (buzzerCount >= totalBeeps * 10) {
      buzzerActive = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

/* ================= STATUS ================= */
String currentTimeStr = "--:--";
String nextFeedTime = "--:--";
bool wifiConnected = false;
bool mqttConnected = false;

/* ================= LCD ================= */
void showIdle() {
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print(currentTimeStr);

  lcd.setCursor(10,0);
  if (wifiConnected) lcd.print("WiFi");
  else lcd.print("NoWi");

  lcd.setCursor(0,1);
  lcd.print("Next:");
  lcd.print(nextFeedTime);

  lcd.setCursor(12,1);
  if (mqttConnected) lcd.print("MQ");
  else lcd.print("--");
}

void showManualFeed() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Manual Feed");
  lcd.setCursor(0,1);
  lcd.print("Dispensing...");
}

void showTimerFeed() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Timer Feed");
  lcd.setCursor(0,1);
  lcd.print(currentTimeStr);
}

/* ================= TIMERS ================= */
String feedTime[3] = {"", "", ""};
bool fed[3] = {false, false, false};

/* ================= HTML ================= */
const char MAIN_page[] PROGMEM =  R"====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Dog Feeder</title>

<script src="https://unpkg.com/mqtt/dist/mqtt.min.js"></script>

<style>
*{box-sizing:border-box}
body{
  margin:0;
  height:100vh;
  background:linear-gradient(135deg,#667eea,#764ba2);
  display:flex;
  justify-content:center;
  align-items:center;
  font-family:Arial;
  overflow:hidden;
}

/* ---------- GLASS CARD ---------- */
.card{
  background:rgba(255,255,255,0.18);
  backdrop-filter:blur(15px);
  padding:25px;
  width:330px;
  border-radius:20px;
  text-align:center;
  box-shadow:0 25px 50px rgba(0,0,0,.3);
  animation:fadeUp 1s ease;
}

@keyframes fadeUp{
  from{opacity:0; transform:translateY(40px)}
  to{opacity:1; transform:translateY(0)}
}

h2{color:#fff;margin-bottom:10px}

/* ---------- STATUS ---------- */
.status{
  margin-bottom:15px;
  padding:8px;
  border-radius:10px;
  font-size:14px;
  animation:pulse 2s infinite;
}
.connected{background:#2ecc71;color:white;}
.disconnected{background:#e74c3c;color:white;}

@keyframes pulse{
  0%{opacity:1}
  50%{opacity:.6}
  100%{opacity:1}
}

/* ---------- BUTTON ---------- */
button{
  width:100%;
  padding:16px;
  font-size:16px;
  border:none;
  border-radius:15px;
  background:linear-gradient(45deg,#ff512f,#dd2476);
  color:white;
  cursor:pointer;
  margin-top:10px;
  transition:.3s;
}
button:hover{transform:scale(1.05)}
button:active{transform:scale(0.95)}

/* ---------- TIMER ---------- */
.timer{
  margin-top:15px;
  padding:10px;
  background:rgba(255,255,255,.25);
  border-radius:15px;
}
.timer input{
  width:100%;
  padding:8px;
  border-radius:10px;
  border:none;
  outline:none;
}

/* ---------- LOGIN ---------- */
#loginBox input{
  width:100%;
  padding:10px;
  border-radius:12px;
  border:none;
  margin-top:10px;
  outline:none;
}
.hidden{display:none}
</style>
</head>

<body>

<!-- ========== LOGIN PAGE ========== -->
<div class="card" id="loginBox">
  <h2>🔐 Login</h2>
  <input type="text" id="user" placeholder="Username">
  <input type="password" id="pass" placeholder="Password">
  <button onclick="login()">LOGIN</button>
</div>

<!-- ========== MAIN PAGE ========== -->
<div class="card hidden" id="mainBox">
  <h2>🐶 Smart Dog Feeder</h2>

  <div id="status" class="status disconnected">Connecting MQTT...</div>

  <button onclick="feed()">🍖 FEED NOW</button>

  <div class="timer">
    <label>⏰ Timer 1</label>
    <input type="time" id="t1">
    <button onclick="setTimer(1)">SET TIMER</button>
  </div>

  <div class="timer">
    <label>⏰ Timer 2</label>
    <input type="time" id="t2">
    <button onclick="setTimer(2)">SET TIMER</button>
  </div>

  <div class="timer">
    <label>⏰ Timer 3</label>
    <input type="time" id="t3">
    <button onclick="setTimer(3)">SET TIMER</button>
  </div>
</div>

<script>
/* ---------- LOGIN ---------- */
function login(){
  const u=document.getElementById("user").value;
  const p=document.getElementById("pass").value;
  if(u==="admin" && p==="1234"){
    document.getElementById("loginBox").classList.add("hidden");
    document.getElementById("mainBox").classList.remove("hidden");
  }else{
    alert("Invalid Login");
  }
}

/* ---------- MQTT ---------- */
const statusDiv=document.getElementById("status");

const client = mqtt.connect(
  "wss://57d98099abe74b37b2af2cc640b99e73.s1.eu.hivemq.cloud:8884/mqtt",
  { username:"dog_Fedder", password:"AJAY#2005m" }
);

client.on("connect", ()=>{
  statusDiv.className="status connected";
  statusDiv.innerHTML="🟢 Connected";
});

/* ---------- FUNCTIONS (UNCHANGED) ---------- */
function feed(){
  client.publish("dogfeeder/feed","1");
}

function setTimer(n){
  const t=document.getElementById("t"+n).value;
  if(!t){alert("Select time");return;}
  client.publish("dogfeeder/time"+n,t);
}
</script>

</body>
</html>


)====";

/* ================= RELAY ================= */
void startRelay(String source) {
  if (relayActive) return;

  relayActive = true;
  relayStartMillis = millis();

  digitalWrite(RELAY_PIN, RELAY_ON);
  startBuzzer();

  if (source == "manual") showManualFeed();
  else if (source == "timer") showTimerFeed();
}

void relayHandler() {
  if (relayActive && millis() - relayStartMillis >= RELAY_TIME) {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    relayActive = false;
  }
}

/* ================= MQTT ================= */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (uint8_t i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  if (String(topic) == "dogfeeder/feed" && msg == "1")
    startRelay("manual");

  if (String(topic) == "dogfeeder/time1") feedTime[0] = msg, fed[0] = false;
  if (String(topic) == "dogfeeder/time2") feedTime[1] = msg, fed[1] = false;
  if (String(topic) == "dogfeeder/time3") feedTime[2] = msg, fed[2] = false;
}

void connectMQTT() {
  if (client.connected()) return;

  if (client.connect("DogFeederESP", MQTT_USER, MQTT_PASS)) {
    client.subscribe("dogfeeder/feed");
    client.subscribe("dogfeeder/time1");
    client.subscribe("dogfeeder/time2");
    client.subscribe("dogfeeder/time3");
  }
}

/* ================= TIMER ================= */
void timerHandler() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  char now[6];
  strftime(now, sizeof(now), "%H:%M", &timeinfo);
  currentTimeStr = String(now);

  nextFeedTime = "--:--";
  for (int i = 0; i < 3; i++) {
    if (feedTime[i] != "") {
      nextFeedTime = feedTime[i];
      break;
    }
  }

  for (int i = 0; i < 3; i++) {
    if (feedTime[i] == currentTimeStr && !fed[i]) {
      startRelay("timer");
      fed[i] = true;
    }
  }

  if (currentTimeStr == "00:00") {
    for (int i = 0; i < 3; i++) fed[i] = false;
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  Wire.begin(4,5);  // SDA=D2, SCL=D1

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Dog Feeder");
  lcd.setCursor(0,1);
  lcd.print("Starting...");
  delay(2000);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  WiFi.begin(sta_ssid, sta_password);

  configTime(19800, 0, "pool.ntp.org");

  server.on("/", []() {
    server.send_P(200, "text/html", MAIN_page);
  });

  server.on("/feed", []() {
    startRelay("manual");
    server.send(200, "text/plain", "Feeding...");
  });

  server.begin();

  espClient.setInsecure();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);
}

/* ================= LOOP ================= */
void loop() {

  if (buttonPressed) {
    buttonPressed = false;
    startRelay("manual");
  }

  server.handleClient();

  wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (wifiConnected) {
    if (!client.connected()) connectMQTT();
    client.loop();
  }

  mqttConnected = client.connected();

  relayHandler();
  buzzerHandler();
  timerHandler();

  // LCD auto refresh
  static unsigned long lcdTimer = 0;
  if (millis() - lcdTimer > 1000) {
    lcdTimer = millis();
    if (!relayActive) showIdle();
  }
}