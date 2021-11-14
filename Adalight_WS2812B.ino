/*
   Arduino/NodeMCU/ESP8266 interface for use with WS2811/WS2812/WS2812B strip LEDs
   Uses Adalight protocol, compatible with Prismatik, AmbiBox, etc.
   "Magic Word" for synchronization is 'Ada' followed by LED High, Low and Checksum
   Supports full debugging, for those having trouble getting their lights working.
   Highly recommend using the Arduino IDE, as the Com Port Monitor works very well with this.
   If this works well for you, send me a picture of your completed setup.
   @author: Edward Webber <arduino@webbsense.com>
   @library: FastLED v3.1.x
   @date: 11/5/2017
*/

// --------------- LEAVE THIS SECTION ALONE ------------------------
#define FASTLED_INTERNAL
#undef __GNUC__
#define __GNUC__ 3
#define FASTLED_USING_NAMESPACE
#include "FastLED.h"
// ------------ END LEAVE THIS SECTION ALONE ------------------------

// -------------------------------------------------------------------
// --------------- User Configuration Section ------------------------
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER // Uncomment for NodeMCU/ESP82XX boards
#define DATA_PIN    8           // GPIO# for Arduino, D# for NodeMCU/ESP82XX
#define LED_TYPE    NEOPIXEL    // For WS2812 use NEOPIXEL, For WS2811 use either WS2811 _or_ NEOPIXEL (YMMV), For WS2801 use WS2801 (see FastLED docs for full list)
#define NUM_LEDS    226 //34          // number of LED *controllers* (WS82811/WS2801 has 3 LEDs _per controller_, WS2812B has one LED per controller)
#define DEBUGGING_ENABLED false // true for verbose (debugging) output, false for normal mode
#define SHOW_INTRO true         // true for a cool little start-up lightshow, false for no lightshow
#define SERIAL_RATE 500000 //115200      // Baudrate, higher rate allows faster refresh rate and more LEDs (must match your adalight compatible software)

// ------------ Display lightshow when idle ------------------------
#define IDLE_LIGHTS_ENABLE true  // true for a lightshow when the lights stop receiving updates
#define IDLE_STYLE "random"      // lightshow style options: rainbow, sweep, confetti, pulse, snake, random
#define IDLE_WAIT 6500           // update cycles to wait before lights considered 'idle'
#define IDLE_EFFECT_SPEED 'T'    // speed of the lightshow effects: 'S' = slow, 'N' = normal, 'F' = fast, 'T' = trippy
// -------------------------------------------------------------------
// -------------------------------------------------------------------


// =================== Don't edit below here unless you know what you're doing ======================================



#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

const char* magicWord = "Ada";

// Initialise LED-array
CRGB leds[NUM_LEDS];

// Create debugging functions
#define DSerial if(DEBUGGING_ENABLED)Serial
//#define DSerial Serial

void setup() {
  static uint8_t gHue = 0; // rotating "base color" used by the rainbow pattern
  //FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, DATA_PIN>(leds, NUM_LEDS);
  Serial.begin(SERIAL_RATE);
  RGBinit(); // Initial RGB flash
  randomSeed(analogRead(0)); // initialize random seed
}

uint8_t hi, lo, chk, i, expected_chk, gHue = 0;
int idleCount = 0;
//const double LED_TTL = (NUM_LEDS * 3);
const byte LED_TTL = (NUM_LEDS);
bool fetchComplete = false;
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void colorTest(int delayMs) {
  LEDS.showColor(CRGB(255, 0, 0)); delay(delayMs);
  LEDS.showColor(CRGB(0, 255, 0)); delay(delayMs);
  LEDS.showColor(CRGB(0, 0, 255)); delay(delayMs);
  LEDS.showColor(CRGB(0, 0, 0));
}

void RGBinit() {
  if (!DEBUGGING_ENABLED && SHOW_INTRO) {
    delay(5000);
    while (gHue < 200) {
      fill_rainbow(leds, NUM_LEDS, gHue, 7);
      FastLED.show();
      gHue++; // slowly cycle the "base color" through the rainbow
      delay(20);
    }
    LEDS.showColor(CRGB(15, 100, 10)); // Light blue 'waiting' background
  } else if (DEBUGGING_ENABLED && SHOW_INTRO) {
    delay(5000);
    DSerial.println("Color test in 5 seconds..");
    delay(5000);
    DSerial.println("500ms RGB test");
    colorTest(500);
    DSerial.println("1-sec RGB test");
    colorTest(1000);
    DSerial.println("Tests complete");
  } else if (DEBUGGING_ENABLED && !SHOW_INTRO) {
    delay(5000);
    DSerial.println("Color test in 5 seconds..");
    delay(5000);
    LEDS.showColor(CRGB(255, 0, 0)); delay(700);
    LEDS.showColor(CRGB(0, 255, 0)); delay(700);
    LEDS.showColor(CRGB(0, 0, 255)); delay(700);
    LEDS.showColor(CRGB(0, 0, 0));
    DSerial.println("Color test complete");
  }
}

void idleLights(String idleStyle) {
  if (idleCount >= IDLE_WAIT) {
    if (idleStyle == "rainbow") {
      fill_rainbow(leds, NUM_LEDS, gHue, 7);
    } else if (idleStyle == "sweep") {
      // a colored dot sweeping back and forth, with fading trails
      fadeToBlackBy( leds, NUM_LEDS, 20);
      int pos = beatsin16( 13, 0, NUM_LEDS-1 );
      leds[pos] += CHSV( gHue, 255, 192);
    } else if (idleStyle == "confetti") {
      // random colored speckles that blink in and fade smoothly
      fadeToBlackBy( leds, NUM_LEDS, 10);
      int pos = random16(NUM_LEDS);
      leds[pos] += CHSV( gHue + random8(64), 200, 255);
    } else if (idleStyle == "snake") {
      // eight colored dots, weaving in and out of sync with each other
      fadeToBlackBy( leds, NUM_LEDS, 20);
      byte dothue = 0;
      for( int i = 0; i < 8; i++) {
        leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
        dothue += 32;
      }
    } else {
      // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
      uint8_t BeatsPerMinute = 62;
      CRGBPalette16 palette = PartyColors_p;
      uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
      for( int i = 0; i < NUM_LEDS; i++) { //9948
        leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
      }
    }
    FastLED.show();
    switch (IDLE_EFFECT_SPEED) {
      case 'S':
        gHue++; // slowly cycle the "base color" through the rainbow
        delay(50);
        break;
      case 'F':
        gHue++; // slowly cycle the "base color" through the rainbow
        delay(10);
        break;
      case 'T':
        gHue = (rand() % 255); // slowly cycle the "base color" through the rainbow
        delay(10);
        break;
      default:
        gHue++; // slowly cycle the "base color" through the rainbow
        delay(20);
    }
  }
}

void fetchData() {
  static byte inProgress = 0;
  char readChar;

  while (Serial.available() && !fetchComplete) {
    if (inProgress == 6) {
      memset(leds, 0, NUM_LEDS * sizeof(struct CRGB));
      for (i = 1; i <= NUM_LEDS; i++) {
        byte r, g, b;
        while (!Serial.available()) { yield(); }
        r = Serial.read();
        while (!Serial.available()) { yield(); }
        g = Serial.read();
        while (!Serial.available()) { yield(); }
        b = Serial.read();
        DSerial.print("Setting RGB values: ");
        DSerial.print(r, DEC); DSerial.print(", ");
        DSerial.print(g, DEC); DSerial.print(", ");
        DSerial.print(g, DEC); DSerial.print(" for LED #"); DSerial.println(i);
        leds[i].r = r;
        leds[i].g = g;
        leds[i].b = b;
      }
      inProgress = 0;
      fetchComplete = true;
    } else {
      for (i = 0; i < 3; i++) {
        while (!Serial.available()) { yield(); }
        readChar = Serial.read();
        if (readChar == magicWord[i]) {
          DSerial.print("Magic word match: "); DSerial.println(magicWord[i]);
          inProgress = (i + 1);
          if (inProgress == 3) {
            while (!Serial.available()) { yield(); }
            hi = Serial.read();
            while (!Serial.available()) { yield(); }
            lo = Serial.read();
            while (!Serial.available()) { yield(); }
            chk = Serial.read();
            // If checksum does not match go back to the beginning
            //expected_chk = (byte((NUM_LEDS - 1) >> 8) ^ byte((NUM_LEDS - 1) & 0xff) ^ 0x55);
            expected_chk = (byte(00) ^ lo ^ 0x55);
            if (chk == expected_chk) {
              DSerial.print("Checksum Passed: (");
              DSerial.print(hi, HEX); DSerial.print(","); DSerial.print(lo, HEX); DSerial.print(","); DSerial.print(chk, HEX); DSerial.print(") ("); DSerial.print(NUM_LEDS); DSerial.println(" LEDs)");
              inProgress = 6;
            } else {
              DSerial.print("Checksum Failed:");
              DSerial.print(" Received: ("); DSerial.print(hi, HEX); DSerial.print(","); DSerial.print(lo, HEX); DSerial.print(","); DSerial.print(chk, HEX); DSerial.print(") ("); DSerial.print(lo + 1, DEC); DSerial.print(" LEDs)");
              DSerial.print(" Expected: ("); DSerial.print(byte((NUM_LEDS - 1) >> 8), HEX); DSerial.print(","); DSerial.print(byte((NUM_LEDS - 1) & 0xff), HEX); DSerial.print(","); DSerial.print(expected_chk, HEX); DSerial.print(") ("); DSerial.print(NUM_LEDS, DEC); DSerial.println(" LEDs)");
              DSerial.println("Resetting..");
              inProgress = 0;
            }
            break;
          }
        } else {
          DSerial.print("Magic word failed to match: "); DSerial.print(magicWord[i]); DSerial.println(", resetting..");
          inProgress = 0;
          break;
        }
      }
    }
  }
}

void setColors() {
  static const String idleStyles[] = { "rainbow", "sweep", "confetti", "pulse", "snake" }; // List of patterns to cycle through.
  static String currentStyle;
  static unsigned long nextStyle = 0;
  if (fetchComplete == true) {
    // Shows new values
    DSerial.println("Displaying colors now..");
    FastLED.show();
    fetchComplete = false;
    idleCount = 0;
  } else if (IDLE_LIGHTS_ENABLE) {
    if (idleCount == IDLE_WAIT || (millis() > (nextStyle + 240000) && idleCount > IDLE_WAIT)) {
      DSerial.println("Idle wait reached..");
      if (IDLE_STYLE == "random") {
        int randResult = ( random(ARRAY_SIZE(idleStyles)) );
        currentStyle = idleStyles[randResult];
      } else {
        String currentStyle = IDLE_STYLE;
      }
      DSerial.print("Style set to "); DSerial.println(currentStyle);
      nextStyle = millis();
      idleCount++;
    } else if (idleCount < IDLE_WAIT && (idleCount % 1000) == 0) {
      DSerial.print("IdleCount is now "); DSerial.println(idleCount);
      idleCount++;
    } else if (idleCount > IDLE_WAIT) {
      idleLights(currentStyle);
    } else {
      idleCount++;
    }
  }
}

void sendMagicWord() {
  static unsigned long magicWordSent = 0;
  if (!Serial.available() && millis() > (magicWordSent + 30000)) {
    Serial.println(magicWord); // Send "Magic Word" string to host
    magicWordSent = millis();
  }
}

void loop() {
  while (Serial.available() > 1000) { Serial.read(); yield(); }
  delay(10);
  fetchData();
  setColors();
  sendMagicWord();
}
