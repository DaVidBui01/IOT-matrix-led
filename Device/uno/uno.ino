#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <SoftwareSerial.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 10
MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// SoftwareSerial (UNO side) - RX, TX
#define UNO_SW_RX 2
#define UNO_SW_TX 3
SoftwareSerial espSerial(UNO_SW_RX, UNO_SW_TX);

// Buttons
const uint8_t BTN_NEXT = 6;
const uint8_t BTN_PREV = 7;
const uint8_t BTN_MODE = 8;
const uint8_t BTN_DHT  = 9;

bool modeAuto = true;
unsigned long lastDebounce = 0;
const unsigned long debounceDelay = 50;

// Messages storage
#define MSG_SLOTS 8
String messages[MSG_SLOTS];
uint8_t writeIndex = 0; // last written slot
uint8_t msgIndex = 0;   // currently displayed slot

// DHT last read string
String lastDHT = "T:--.-C H:--.%";

// Display settings
uint16_t scrollSpeedMsgs = 25;
uint16_t scrollSpeedDHT  = 75;
uint16_t displayPause    = 1000;

enum ViewType { VIEW_MSG, VIEW_DHT };
ViewType activeView = VIEW_MSG;

String incomingLine = "";

void showCurrent() {
  String txt;
  uint16_t speed;
  if (activeView == VIEW_DHT) {
    txt = lastDHT;
    speed = scrollSpeedDHT;
  } else {
    txt = messages[msgIndex];
    speed = scrollSpeedMsgs;
  }
  if (txt.length() == 0) txt = " ";
  uint8_t eff = modeAuto ? PA_SCROLL_LEFT : PA_PRINT;
  display.displayText(txt.c_str(), display.getTextAlignment(), speed, displayPause, eff);
}

void showModeBrief(bool isAuto) {
  display.displayClear();
  display.displayText(isAuto ? "AUTO" : "MANUAL", PA_CENTER, 0, 700, PA_PRINT);
  while (!display.displayAnimate()) { delay(5); }
  showCurrent();
}

void setup() {
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_DHT, INPUT_PULLUP);

  Serial.begin(115200);
  espSerial.begin(9600);

  // default messages
  messages[0] = "Chao mung!";
  messages[1] = "He thong dang hoat dong";
  messages[2] = "Nhiet do: --.-C  Do am: --.%";
  messages[3] = "Gui len Server bang ESP32";
  for (int i = 4; i < MSG_SLOTS; i++) messages[i] = "";

  display.begin();
  display.setIntensity(3);
  display.displayClear();
  display.setTextAlignment(PA_CENTER);

  activeView = VIEW_MSG;
  showCurrent();
}

void handleButtons() {
  static bool lastNext=HIGH, lastPrev=HIGH, lastMode=HIGH, lastDht=HIGH;
  bool curNext = digitalRead(BTN_NEXT);
  bool curPrev = digitalRead(BTN_PREV);
  bool curMode = digitalRead(BTN_MODE);
  bool curDht  = digitalRead(BTN_DHT);
  unsigned long now = millis();

  if (curNext != lastNext && now - lastDebounce > debounceDelay) {
    lastDebounce = now;
    if (curNext == LOW) {
      activeView = VIEW_MSG;
      msgIndex = (msgIndex + 1) % MSG_SLOTS;
      showCurrent();
    }
  }
  if (curPrev != lastPrev && now - lastDebounce > debounceDelay) {
    lastDebounce = now;
    if (curPrev == LOW) {
      activeView = VIEW_MSG;
      msgIndex = (msgIndex == 0) ? (MSG_SLOTS - 1) : (msgIndex - 1);
      showCurrent();
    }
  }
  if (curDht != lastDht && now - lastDebounce > debounceDelay) {
    lastDebounce = now;
    if (curDht == LOW) {
      activeView = VIEW_DHT;
      showCurrent();
    }
  }
  if (curMode != lastMode && now - lastDebounce > debounceDelay) {
    lastDebounce = now;
    if (curMode == LOW) {
      modeAuto = !modeAuto;
      espSerial.print("MODE:");
      espSerial.println(modeAuto ? "AUTO" : "MANUAL");
      Serial.print("Local mode changed: ");
      Serial.println(modeAuto ? "AUTO" : "MANUAL");
      showModeBrief(modeAuto);
    }
  }
  lastNext = curNext; lastPrev = curPrev; lastMode = curMode; lastDht = curDht;
}

void processIncoming(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd.startsWith("MODE:")) {
    String m = cmd.substring(5); m.trim();
    modeAuto = (m == "AUTO");
    Serial.print("Set mode from ESP: "); Serial.println(modeAuto ? "AUTO":"MANUAL");
    showModeBrief(modeAuto);
    return;
  }

  if (cmd.startsWith("MSG:")) {
    String txt = cmd.substring(4); txt.trim();
    if (txt.length() == 0) return;

    // If contains DHT pattern then update lastDHT
    if (txt.indexOf("T:") >= 0 && txt.indexOf("H:") >= 0) {
      lastDHT = txt;
      Serial.print("Updated lastDHT: ");
      Serial.println(lastDHT);
    }

    // store in circular buffer at next writeIndex
    writeIndex = (writeIndex + 1) % MSG_SLOTS;
    messages[writeIndex] = txt;

    // If Auto -> update displayed index to newest; if Manual -> keep user's selection
    if (modeAuto) {
      msgIndex = writeIndex;
      if (activeView == VIEW_MSG) showCurrent();
      if (activeView == VIEW_DHT && txt.indexOf("T:") >= 0 && txt.indexOf("H:") >= 0) showCurrent();
    } else {
      Serial.println("Stored message while Manual (no auto-display).");
    }
    return;
  }

  if (cmd.startsWith("SETIDX:")) {
    int sep = cmd.indexOf('|');
    if (sep > 0) {
      String left = cmd.substring(7, sep);
      String val = cmd.substring(sep + 1);
      int idx = left.toInt();
      if (idx >=0 && idx < MSG_SLOTS) messages[idx] = val;
    }
    return;
  }

  if (cmd == "CLEAR") {
    for (int i = 0; i < MSG_SLOTS; i++) messages[i] = "";
    messages[0] = "Empty";
    writeIndex = 0; msgIndex = 0;
    showCurrent();
    return;
  }

  // default treat as message
  writeIndex = (writeIndex + 1) % MSG_SLOTS;
  messages[writeIndex] = cmd;
  if (modeAuto) { msgIndex = writeIndex; showCurrent(); }
}

void loop() {
  handleButtons();

  while (espSerial.available()) {
    char c = espSerial.read();
    if (c == '\n' || c == '\r') {
      if (incomingLine.length() > 0) {
        processIncoming(incomingLine);
        incomingLine = "";
      }
    } else {
      incomingLine += c;
      if (incomingLine.length() > 256) incomingLine = incomingLine.substring(0,256);
    }
  }

  if (display.displayAnimate()) {
    showCurrent();
  }

  delay(10);
}
