//#define DEBUG
//#define SETUP_CLOCK

#define OCTOBER 10
#define MARCH 3
#define DST 1
#define STD 0

#include "RTClib.h"
#include <EEPROM.h>

const int co2ButtonPin = 2;
const int co2LedPin = 3;
const int lightButtonPin = 4;
const int lightLedPin = 5;
const int co2RelayPin = 8;
const int lightRelayPin = 9;
const int rtcErrorPin = 7;
const int dstEepromAddr = 0;
const unsigned long checkClockPeriod = 60 * 1000UL;

typedef struct {
  int pin;
  int led;
  int state;
  bool on;
} ledButton;

typedef struct {
  int startHour;
  int startMinute;
  int stopHour;
  int stopMinute;
} prg;

ledButton lightButton = {lightButtonPin, lightLedPin, 0, false};
ledButton co2Button = {co2ButtonPin, co2LedPin, 0, false};

prg co2Prg = {11, 0, 15, 0};
prg lightPrg = {10, 0, 16, 30};

RTC_DS3231 rtc;
bool rtcOk;
DateTime now;
unsigned long lastCheckClock = 0UL;


void setup() {
  pinMode(lightButton.led, OUTPUT);
  pinMode(lightButton.pin, INPUT);
  digitalWrite(lightButton.led, LOW);

  pinMode(co2Button.led, OUTPUT);
  pinMode(co2Button.pin, INPUT);
  digitalWrite(co2Button.led, LOW);

  pinMode(lightRelayPin, OUTPUT);
  digitalWrite(lightRelayPin, HIGH);

  pinMode(co2RelayPin, OUTPUT);
  digitalWrite(co2RelayPin, HIGH);

  pinMode(rtcErrorPin, OUTPUT);
  digitalWrite(rtcErrorPin, HIGH);

  Serial.begin(9600);

  rtcOk = rtc.begin();
  if (!rtcOk) {
    Serial.println("rtc not found");
  }
#ifdef SETUP_CLOCK
  if (rtcOk) {
    DateTime date = DateTime(F(__DATE__), F(__TIME__));
    rtc.adjust(date);
    EEPROM.write(dstEepromAddr, STD); // set to DST if DST is now
  }
#endif
  if (rtcOk && rtc.lostPower()) {
    Serial.println("time is not set");
    rtcOk = false;
  }
  digitalWrite(rtcErrorPin, rtcOk ? HIGH : LOW);
}


void loop() {
  if (rtcOk) {
    now = rtc.now();
#ifdef DEBUG
    Serial.print(now.day());
    Serial.print("/");
    Serial.print(now.month());
    Serial.print("/");
    Serial.print(now.year());
    Serial.print(" ");
    Serial.print(now.hour());
    Serial.print(":");
    Serial.println(now.minute());
#endif
  }
  if ((millis() - lastCheckClock) > checkClockPeriod) {
    adjustRtcIfNeeded();
    lastCheckClock = millis();
  }
  bool lightOn = isButtonOn(&lightButton) || isTimeFor(&lightPrg);
  bool co2On = isButtonOn(&co2Button) || isTimeFor(&co2Prg);
  digitalWrite(lightRelayPin, lightOn ? LOW : HIGH);
  digitalWrite(co2RelayPin, co2On ? LOW : HIGH);
}


void adjustRtcIfNeeded() {
  if (rtcOk) {
    int month = now.month();
    if (month == MARCH || month == OCTOBER) {
      int day = now.day();
      int dayOfTheWeek = now.dayOfTheWeek();
      int hour = now.hour();
      int firstSunday = (day - dayOfTheWeek) % 7;
      // need it only for March and October, both have 31 days
      int lastSunday = firstSunday + ((31 - firstSunday) / 7) * 7;
      if (month == MARCH && day == lastSunday && hour >= 3 && EEPROM.read(dstEepromAddr) == STD) {
        rtc.adjust(DateTime(now.year(), month, day, hour + 1, now.minute(), now.second()));
        EEPROM.write(dstEepromAddr, DST);
        Serial.println("switch to daylight saving time (DST)");
      } else if (month == OCTOBER && day == lastSunday && hour >= 3 && EEPROM.read(dstEepromAddr) == DST) {
        rtc.adjust(DateTime(now.year(), month, day, hour - 1, now.minute(), now.second()));
        EEPROM.write(dstEepromAddr, STD);
        Serial.println("switch to standart time (STD)");
      }
    }
  }
}


bool isButtonOn(ledButton *b) {
  int newState = digitalRead(b->pin);
  if (newState != b->state) {
    if (newState == LOW) {
      b->on = !b->on;
    }
    delay(30);
  }
  b->state = newState;
  digitalWrite(b->led, b->on ? HIGH : LOW);
  return b->on;
}


bool isTimeFor(prg *p) {
  if (!rtcOk) {
    return false;
  }
  int nowMinute = now.hour() * 60 + now.minute();
  int stopMinute = p->stopHour * 60 + p->stopMinute;
  if (nowMinute >= stopMinute) {
    return false;
  } else {
    int startMinute = p->startHour * 60 + p->startMinute;
    return nowMinute >= startMinute;
  }
}
