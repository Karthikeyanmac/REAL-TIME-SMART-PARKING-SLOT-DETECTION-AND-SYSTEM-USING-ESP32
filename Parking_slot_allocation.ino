#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// === WiFi Credentials ===
const char* ssid     = "KARTHI";
const char* password = "karthi police";

// === InfluxDB credentials ===
const char* influxURL = "https://us-east-1-1.aws.cloud2.influxdata.com/api/v2/write?org=Data_base&bucket=esp32&precision=s";
const char* token     = "7geVbMvR9y7Xhu6EChuOs68-yzQF4KGUoZFA2z2KlTzlZlygBIVy3mhQpS4zuPRCdXKCjGrFn4gZW3Ai4Ws-8A==";

// === CircuitDigest SMS credentials ===
const char* apiKey      = "XXXXXXXX";       // replace
const char* templateID  = "101";   // replace
const char* mobileNumber= "910000000000";       // replace

// === Hardware Pins ===
const int irPins[3]  = {32, 33, 25}; // IR sensor OUT pins
const int ledPins[3] = {26, 27, 14}; // LED pins

// Previous states to detect change
int prevStatus[3] = {-1, -1, -1}; // -1 means uninitialized

// --- URL-encode helper for SMS ---
String urlencode(const String &str) {
  String encodedString = "";
  char c;
  char code0, code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// --- SMS function ---
void sendSMS(const String &message) {
  WiFiClientSecure client;
  client.setInsecure(); // skip SSL certificate check

  HTTPClient https;
  String url = "https://circuitdigestapi.com/api/v1/?api_key=" + String(apiKey) +
               "&template_id=" + String(templateID) +
               "&mobile_number=" + String(mobileNumber) +
               "&var1=" + urlencode(message) +
               "&var2=" + urlencode(" ");  // blank if not used

  Serial.println("SMS URL: " + url);

  https.begin(client, url);
  int httpResponseCode = https.GET();
  Serial.print("SMS HTTP Response: ");
  Serial.println(httpResponseCode);
  https.end();
}

// --- InfluxDB function ---
void sendToInflux(int slot, bool free) {
  HTTPClient http;
  http.begin(influxURL);
  http.addHeader("Authorization", String("Token ") + token);
  http.addHeader("Content-Type", "text/plain");

  String data = "parking,slot=" + String(slot) + " status=" + String(free ? 1 : 0);

  int httpResponseCode = http.POST(data);
  Serial.print("InfluxDB HTTP Response (Slot ");
  Serial.print(slot);
  Serial.print("): ");
  Serial.println(httpResponseCode);
  http.end();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Parking Slot Monitor Started");

  // Setup pins
  for (int i = 0; i < 3; i++) {
    pinMode(irPins[i], INPUT);
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  String smsMessage = ""; // build SMS message
  bool stateChanged = false;

  for (int i = 0; i < 3; i++) {
    int irValue = digitalRead(irPins[i]);
    bool slotFree = (irValue == HIGH); // HIGH = free, LOW = occupied

    // LED ON when free, OFF when occupied
    digitalWrite(ledPins[i], slotFree ? HIGH : LOW);

    // Serial output
    Serial.print("Slot ");
    Serial.print(i + 1);
    if (slotFree) {
      Serial.print(" Available  | LED=HIGH  ");
      smsMessage += "Slot " + String(i + 1) + " Available  | LED=HIGH  ";
    } else {
      Serial.print(" Occupied   | LED=LOW   ");
      smsMessage += "Slot " + String(i + 1) + " Occupied   | LED=LOW   ";
    }

    // Check if state changed
    int currentState = slotFree ? 1 : 0;
    if (prevStatus[i] != -1 && currentState != prevStatus[i]) {
      stateChanged = true;
    }
    prevStatus[i] = currentState;

    // Send to Influx
    sendToInflux(i + 1, slotFree);
  }

  Serial.println();

  // Send SMS once per loop if any slot changed
  if (stateChanged) {
    sendSMS(smsMessage); // send full line as var1
  }

  delay(2000); // update every 2 sec
}
