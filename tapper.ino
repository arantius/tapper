#include "LiquidCrystal.h"

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define DBG Serial
#define OUT lcd

#define MIN_BUFSIZE 20
#define MAX_BUFSIZE 250
uint8_t bufSize = 151;
uint32_t buf[MAX_BUFSIZE + 1];
uint8_t bufFirst = 0;
uint8_t bufLast = 0;
uint8_t bufEmpty = 1;

#define MODE_RUN 1
#define MODE_SET 2
uint8_t mode = MODE_RUN;

#define MIN_DROPLEN 80
#define MAX_DROPLEN 150
uint8_t dropLen = 100;
uint8_t ignoreNextPress = 0;
int16_t numPresses = 0;
float dropRate, dur, hz;

uint32_t tmDown = 0;
uint32_t tmFirstPress = 0;
uint32_t tmReset = 0;
uint32_t tmUp = -1;


void m_display() {
  if (numPresses < 1) {
    lcd.setCursor(0, 0);
    if (bufSize == 151 && dropLen == 100) {
      lcd.print(F("Taps Rate  Miss%"));
    } else {
      lcd.print(F("TAPS RATE  MISS%"));
    }
    
    lcd.setCursor(0, 1);
    lcd.print(F("---- ----  ---- "));
  } else {
    lcd.setCursor(0, 1);
    lcd.print(numPresses);
    lcd.print(F("   "));

    lcd.setCursor(5, 1);
    lcd.print(hz);
    lcd.print(F("   "));

    lcd.setCursor(11, 1);
    lcd.print(dropRate);
    lcd.print(F("   "));
  }
}

void m_record(uint32_t now) {
  if (bufEmpty) {
    buf[bufLast] = now;
    bufEmpty = 0;
  } else {
    /*
    // I want to ignore pauses, but would need different data.
    uint16_t tapLen = tmUp - buf[bufLast];
    //DBG.print("Record tap len = "); DBG.println(tapLen);
    if (tapLen < 750) {
      // A long delay means somebody paused; ignore.
      return;
    }
    */

    bufLast++;
    if (bufLast >= bufSize) bufLast = 0;

    if (bufFirst == bufLast) {
      bufFirst++;
      if (bufFirst >= bufSize) bufFirst = 0;
    }

    buf[bufLast] = now;
  }

  dur = (float)(buf[bufLast] - buf[bufFirst]) / 1000.0;
  numPresses = bufLast - bufFirst;
  if (numPresses == -1) numPresses = bufSize - 1;
  hz = (float)numPresses / dur;

  //uint32_t us_calc = micros();
  //uint32_t us_drop = us_start;
  
  if (numPresses > 0) {
    uint8_t dropNum = 0;
    uint8_t dropDem = 0;

    //DBG.print("bufFirst = "); DBG.println(bufFirst);
    //DBG.print("bufLast = "); DBG.println(bufLast);
    for (uint8_t i = bufFirst + 1; ; i = (i + 1) % bufSize) {
      uint8_t j = i - 1;
      if (i == 0) j = bufSize - 1;

      /*
      DBG.print("i = "); DBG.print(i); DBG.print("; j = "); DBG.print(j);
      DBG.print(i % 5 == 0 ? "\n" : "\t");
      */

      if (buf[i] - buf[j] >= dropLen) dropNum++;
      dropDem++;

      if (i == bufLast) break;
    }
    //DBG.print("dropNum = "); DBG.println(dropNum);
    //DBG.print("dropDem = "); DBG.println(dropDem);
    dropRate = (float)dropNum / (float)dropDem * 100.0;

    //us_drop = micros();

    m_display();
  }
}

void m_reset() {
  DBG.println("m_reset()");

  tmDown = 0;
  tmUp = -1;
  tmFirstPress = 0;
  numPresses = 0;
  tmReset = 0;
  ignoreNextPress = 1;

  bufFirst = 0;
  bufLast = 0;
  bufEmpty = 1;
  for (uint8_t i; i < bufSize; i++) buf[i] = 0;
  
  m_display();
}

// \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ //

uint8_t setStage = 0;

void s_display() {
  lcd.clear();
  switch (setStage) {
    case 0:
      lcd.setCursor(3, 0);
      lcd.print(F("BUFFER SIZE"));
      lcd.setCursor(7, 1);
      lcd.print(bufSize);
      break;
    case 1:
      lcd.setCursor(4, 0);
      lcd.print(F("MISS TIME"));
      lcd.setCursor(7, 1);
      lcd.print(dropLen);
      break;
  }
}

void s_incr() {
  switch (setStage) {
    case 0:
      bufSize += 5;
      if (bufSize > MAX_BUFSIZE) bufSize = MIN_BUFSIZE;
      break;
    case 1:
      dropLen += 5;
      if (dropLen > MAX_DROPLEN) dropLen = MIN_DROPLEN;
      break;
  }
  s_display();
}

void s_next() {
  DBG.print("s_next() at ");
  DBG.println(setStage);

  setStage++;

  if (setStage == 2) {
    bufSize++;
    mode = MODE_RUN;
    m_reset();
  } else {
    tmDown = 0;
    tmUp = -1;
    ignoreNextPress = 1;
    
    s_display();
  }
}

// \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ // \\ //

void setup() {
  pinMode(2, INPUT_PULLUP);

#if defined(__AVR_ATmega32U4__)
  // Disable TX/RX LEDs by setting them to (pullup) inputs.
  PORTD |= (1<<PORTD5);
  PORTB |= (1<<PORTB0);
  DDRD &= ~(1<<DDD5);
  DDRB &= ~(1<<DDB0);
#else  
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
#endif

  lcd.begin(16, 2);
  lcd.clear();

  if (0) {
    Serial.begin(74880);
    while (!Serial) { }
    DBG.println("Starting tapper ...");
  } else {
    Serial.end();
  }
  
  if (digitalRead(2) == LOW) {
    mode = MODE_SET;
    bufSize = 100;
    tmDown = 0;
    tmUp = -1;
    ignoreNextPress = 1;
    s_display();
  } else {
    mode = MODE_RUN;
    m_display();
  }
}


void loop() {
  uint32_t now = millis();
  uint8_t btn = digitalRead(2);

  if (ignoreNextPress) {
    if (btn == HIGH) {
      ignoreNextPress = 0;
    }
    return;
  }

  if (btn == LOW && tmDown == 0 && (now - tmUp >= 3)) {
    tmDown = now;
    tmUp = 0;
  }

  if (btn == HIGH && tmUp == 0 && (now - tmDown >= 3)) {
    tmDown = 0;
    tmUp = now;
    tmReset = now + 15000;

    if (mode == MODE_RUN) {
      m_record(now);
    } else if (mode == MODE_SET) {
      s_incr();
    }
  }

  if (// No presses for 15 seconds.
      (tmReset != 0 && now > tmReset)
      // Hold down for 1 second.
      || (tmUp == 0 && (now - tmDown) > 1000)
      ) {
    if (mode == MODE_RUN) {
      m_reset();
    } else if (mode == MODE_SET) {
      s_next();
    }
  }
}

