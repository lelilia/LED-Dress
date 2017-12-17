#include <Adafruit_CircuitPlayground.h>
#include <FastLED.h>  // version 3.1.0
#include <Wire.h>
#include <SPI.h>
#include <math.h>

// LED Dress Code - adapted from Adafruit Sparkle Skirt Playground Tutorial: https://learn.adafruit.com/sparkle-skirt-playground

// tell FastLED all about the Circuit Playground's layout

#define CP_PIN            17   //circuit playground's neopixels live on pin 17
#define DATA_PIN_LEFT     3    //LED data on pin 3
#define CLK_PIN_LEFT      2     // Clock pin on pin 2
#define DATA_PIN_RIGHT    6
#define CLK_PIN_RIGHT     9
#define NUM_CP_LEDS       10 
#define NUM_LEDS          15   // total number of LEDs in your strip
#define COLOR_ORDER BGR    // Dotstar color order -- change this if your colors are coming out wrong
#define COLOR_ORDER_CP GRB  // Circuit Playground color order

CRGB cpLEDS[NUM_CP_LEDS]; // Built in to circuit playground
CRGB ltLEDS[NUM_LEDS];
CRGB rtLEDS[NUM_LEDS];

//---------Global variables to control the mode--------------------------------------
enum    displayModes {MODE_FADE = 0, MODE_RAINBOW_BEAT, MODE_SINELON, MODE_MOTION, MODE_SOUND_REACT, MODE_HEARTBEAT, NUM_MODES};
uint8_t ledMode = 0;       //Initial mode 
bool    leftButtonState = false; 
bool    rightButtonState = false;
bool    switchLeft;



//---------FastLED Palette Setup-----------------------------------------------------
CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette;
TBlendType    currentBlending;
uint8_t       paletteIndex;
CRGBPalette16 paletteList[] = {RainbowColors_p, OceanColors_p, LavaColors_p, ForestColors_p, PartyColors_p};
uint8_t       nPalettes = sizeof(paletteList)/sizeof(CRGBPalette16);
uint8_t       cpIndex = 0;


//---------Heartbeat Variable Setup--------------------------------------------------
uint8_t  heartbeatsPerMin = 55;
uint8_t  heartMin = 30;
uint8_t  heartMax = 90;
uint8_t  heartIncrement = 5;
unsigned long lastBeat = 0;
uint8_t beatColorIndex = 16;

//---------Sound Control Setup-(from Adafruit)--------------------------------------
#define MIC_PIN         A4                                    // Analog port for microphone
#define DC_OFFSET  0                                          // DC offset in mic signal - if unusure, leave 0
                                                              // I calculated this value by serialprintln lots of mic values
#define NOISE     200                                         // Noise/hum/interference in mic signal and increased value until it went quiet
#define SAMPLES   60                                          // Length of buffer for dynamic level adjustment
#define TOP (NUM_LEDS + 2)                                    // Allow dot to go slightly off scale
#define PEAK_FALL 10                                          // Rate of peak falling dot
 
byte
  peak      = 0,                                              // Used for falling dot
  dotCount  = 0,                                              // Frame counter for delaying dot-falling speed
  volCount  = 0;                                              // Frame counter for storing past volume data
int
  vol[SAMPLES],                                               // Collection of prior volume samples
  lvl       = 10,                                             // Current audio level, change this number to adjust sensitivity
  minLvlAvg = 0,                                              // For dynamic adjustment of graph low & high
  maxLvlAvg = 512;



//----------Other Setup---------------------------------------------------------------
uint8_t brightness = 255;
uint8_t cpBrightness = 20;  // Set brightness level

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  delay(1000);
  CircuitPlayground.begin();
  FastLED.addLeds<WS2812, CP_PIN, COLOR_ORDER_CP>(cpLEDS, NUM_CP_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<APA102, DATA_PIN_LEFT, CLK_PIN_LEFT, COLOR_ORDER>(ltLEDS, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<APA102, DATA_PIN_RIGHT, CLK_PIN_RIGHT, COLOR_ORDER>(rtLEDS, NUM_LEDS).setCorrection(TypicalLEDStrip);
  currentBlending = LINEARBLEND;
  currentPalette = RainbowColors_p;
  FastLED.setBrightness(100);
  set_max_power_in_volts_and_milliamps(5, 1800);  // FastLED 2.1 Power management

}

void clearPixels() {
  for (int i = 0; i < NUM_CP_LEDS; i++) {
    cpLEDS[i] = CRGB::Black;
  }
  for (int i = 0; i < NUM_LEDS; i++) {
    ltLEDS[i] = CRGB::Black;
    rtLEDS[i] = CRGB::Black;
  }
  FastLED.show();
}


void fillLedsFromPalette(CRGB *leds, uint8_t nLeds, uint8_t brightVal, uint8_t startIndex) {
  startIndex = constrain(startIndex, 0, 255);
  uint8_t increment = 256/nLeds;
  for (int i = 0; i < nLeds; i++) {
    leds[i] = ColorFromPalette(currentPalette, startIndex, brightVal, currentBlending);
    startIndex = (startIndex + increment) % 256;
  }
}


void soundreactive() {
 
  uint8_t  i, inverted_i;
  uint16_t minLvl, maxLvl;
  int      n, height;
   
  n = analogRead(MIC_PIN);                                    // Raw reading from mic
  n = abs(n - 512 - DC_OFFSET);                               // Center on zero
  
  n = (n <= NOISE) ? 0 : (n - NOISE);                         // Remove noise/hum
  lvl = ((lvl * 7) + n) >> 3;                                 // "Dampened" reading (else looks twitchy)
 
  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);
 
  if (height < 0L)       height = 0;                          // Clip output
  else if (height > TOP) height = TOP;
  if (height > peak)     peak   = height;                     // Keep 'peak' dot at top
 
 
  // Color pixels based on rainbow gradient -- led strand
  for (i=0; i<NUM_LEDS; i++) {
    inverted_i = NUM_LEDS - i - 1;
    if (i >= height) {
      rtLEDS[inverted_i].setRGB( 0, 0,0);
      ltLEDS[inverted_i] = rtLEDS[inverted_i];
    }
    else {
      rtLEDS[inverted_i] = CHSV(map(i,0,NUM_LEDS-1,0,255), 255, brightness);  //constrain colors here by changing HSV values
      ltLEDS[inverted_i] = rtLEDS[inverted_i];
    }
  }
 
  // Draw peak dot  -- led strand
  if (peak > 0 && peak <= NUM_LEDS-1) {
    int inverted_peak = NUM_LEDS - peak - 1;
    rtLEDS[inverted_peak] = CHSV(map(peak,0,NUM_LEDS-1,0,255), 255, brightness);
    ltLEDS[inverted_peak] = rtLEDS[inverted_peak];
  }
 
  // Color pixels based on rainbow gradient  -- circuit playground
  for (i=0; i<NUM_CP_LEDS; i++) {
    if (i >= height)   cpLEDS[i].setRGB( 0, 0,0);
    else cpLEDS[i] = CHSV(map(i,0,NUM_CP_LEDS-1,0,255), 255, cpBrightness);  //constrain colors here by changing HSV values
  }
 
  // Draw peak dot  -- circuit playground
  if (peak > 0 && peak <= NUM_CP_LEDS-1) cpLEDS[peak] = CHSV(map(peak,0,NUM_LEDS-1,0,255), 255, cpBrightness);
 
// Every few frames, make the peak pixel drop by 1:
 
    if (++dotCount >= PEAK_FALL) {                            // fall rate 
      if(peak > 0) peak--;
      dotCount = 0;
    }
  
  vol[volCount] = n;                                          // Save sample for dynamic leveling
  if (++volCount >= SAMPLES) volCount = 0;                    // Advance/rollover sample counter
 
  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for (i=1; i<SAMPLES; i++) {
    if (vol[i] < minLvl)      minLvl = vol[i];
    else if (vol[i] > maxLvl) maxLvl = vol[i];
  }
  // minLvl and maxLvl indicate the volume range over prior frames, used
  // for vertically scaling the output graph (so it looks interesting
  // regardless of volume level).  If they're too close together though
  // (e.g. at very low volume levels) the graph becomes super coarse
  // and 'jumpy'...so keep some minimum distance between them (this
  // also lets the graph go to zero when no sound is playing):
  if((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;                 // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;                 // (fake rolling average)
 
 
  //show_at_max_brightness_for_power();                         // Power managed FastLED display
  //Serial.println(LEDS.getFPS()); 
  show_at_max_brightness_for_power();
  //FastLED.show();
} // fastbracelet()
 

void rainbow_beat() {
  
  uint8_t beatA = beatsin8(17, 0, 255);                        // Starting hue
  uint8_t beatB = beatsin8(13, 0, 255);
  uint8_t avg = (beatA + beatB)/2;
  fillLedsFromPalette(ltLEDS, NUM_LEDS, 255, avg);
  fillLedsFromPalette(rtLEDS, NUM_LEDS, 255, avg);
  CRGB col = ColorFromPalette(currentPalette, avg, 16, currentBlending);
  fill_solid(cpLEDS, NUM_CP_LEDS, col); 
  FastLED.show();

} // rainbow_beat()



void heartbeat(boolean doLeftRight = false) {
  unsigned long curTime = millis();
  int msPerBeat = (int) (60.0*1000)/heartbeatsPerMin;
  if ((curTime - lastBeat) > msPerBeat) {
    lastBeat = curTime;
    
    beatColorIndex = (beatColorIndex + 24) % 256;
    if (doLeftRight) {
      fillLedsFromPalette(ltLEDS, NUM_LEDS, 255, beatColorIndex);
      fillLedsFromPalette(rtLEDS, NUM_LEDS, 255, beatColorIndex);
    }
    CRGB col = ColorFromPalette(currentPalette, beatColorIndex, 32, currentBlending);
    fill_solid(cpLEDS, NUM_CP_LEDS, col);    
  } else if ((curTime - lastBeat) > ( msPerBeat*35.0/100)) {  //Secondary beat    
    if (doLeftRight) {
      fillLedsFromPalette(ltLEDS, NUM_LEDS, 255, beatColorIndex);
      fillLedsFromPalette(rtLEDS, NUM_LEDS, 255, beatColorIndex);
    }
    CRGB col = ColorFromPalette(currentPalette, beatColorIndex, 32, currentBlending);
    fill_solid(cpLEDS, NUM_CP_LEDS, col);
  } else {
    nscale8(cpLEDS, NUM_CP_LEDS, 192);
    if (doLeftRight) {
      nscale8(ltLEDS, NUM_LEDS, 192);
      nscale8(rtLEDS, NUM_LEDS, 192);
    }
  }
  
  FastLED.show();
  
} // heartbeat();


uint8_t baseHue = 0;
uint8_t sinelonBPM = 33;
uint8_t bpmIncrement = 5;
uint8_t bpmMax = 90;
uint8_t bpmMin = 6;
void sinelon() {  // colored line sweeping back and forth with fading trails - from https://gist.github.com/kriegsman/062e10f7f07ba8518af6
    
    fadeToBlackBy(rtLEDS, NUM_LEDS, 32);
    fadeToBlackBy(ltLEDS, NUM_LEDS, 32);
    fadeToBlackBy(cpLEDS, NUM_CP_LEDS, 32);
    int pos = beatsin16(sinelonBPM, 0, NUM_LEDS-1);
    rtLEDS[pos] += CHSV( baseHue, 255, 255);
    ltLEDS[pos] = rtLEDS[pos];
    if (pos == 0) {
      CRGB col = rtLEDS[0];
      col.fadeToBlackBy(225);
      fill_solid(cpLEDS, NUM_CP_LEDS, col);
      baseHue++;
    } else {
      fill_solid(cpLEDS, NUM_CP_LEDS, CRGB::Black);
    }
    FastLED.show();
} // sinelon()

//------Motion setup and functions-------------------------------------------------
#define MOVE_THRESHOLD 5 // movement sensitivity.  lower number = less twinklitude
 
float X, Y, Z;
void motion() {
  
  X = CircuitPlayground.motionX();
  Y = CircuitPlayground.motionY();
  Z = CircuitPlayground.motionZ();
 
   // Get the magnitude (length) of the 3 axis vector
  // http://en.wikipedia.org/wiki/Euclidean_vector#Length
  double storedVector = X*X;
  storedVector += Y*Y;
  storedVector += Z*Z;
  storedVector = sqrt(storedVector);
  //Serial.print("Len: "); Serial.println(storedVector);
  
  // wait a bit
  delay(100);
  
  // get new data!
  X = CircuitPlayground.motionX();
  Y = CircuitPlayground.motionY();
  Z = CircuitPlayground.motionZ();
  double newVector = X*X;
  newVector += Y*Y;
  newVector += Z*Z;
  newVector = sqrt(newVector);
  //Serial.print("New Len: "); Serial.println(newVector);
  
  // are we moving 
  if (abs(10*newVector - 10*storedVector) > MOVE_THRESHOLD) {
    //Serial.println("Twinkle!");
    flashRandom(7, 2);  // first number is 'wait' delay, shorter num == shorter twinkle
    flashRandom(7, 6);  // second number is how many neopixels to simultaneously light up
    flashRandom(7, 4);
  }
}

void flashRandom(int wait, uint8_t howmany) {
 
  for(uint16_t i=0; i<howmany; i++) {
    // pick a random favorite color
    uint8_t hue = random8();
    CRGB col;
 
    // get a random pixel from the list
    int j = random(NUM_LEDS);
    int k = random(NUM_CP_LEDS);
    //Serial.print("Lighting up "); Serial.println(j); 
    
    // now we will 'fade' it in 5 steps
    for (int x=0; x < 5; x++) {
      col = CHSV(hue, 255, x*50 );
      rtLEDS[j] = col;
      ltLEDS[j] = col;
      cpLEDS[k] = col;
      FastLED.show();
      delay(wait);
      
    }
    // & fade out in 5 steps
    for (int x=5; x >= 0; x--) {
      col = CHSV(hue, 255, x*50);
      rtLEDS[j] = col;
      ltLEDS[j] = col;
      cpLEDS[k] = col;
      FastLED.show();
      delay(wait);
    }
  }
  // LEDs will be off when done (they are faded to 0)
}

uint8_t fadeIndex = 0;
void fadeLeftRight() {
//TBD!!!!
  fillLedsFromPalette(ltLEDS, NUM_LEDS, 255, fadeIndex);
  fillLedsFromPalette(rtLEDS, NUM_LEDS, 255, fadeIndex + 128);
  CRGB col1 = ColorFromPalette(currentPalette, fadeIndex, 16, currentBlending);
  CRGB col2 = ColorFromPalette(currentPalette, fadeIndex + 128, 16, currentBlending);
  fill_solid(cpLEDS, NUM_CP_LEDS/2, col1); 
  fill_solid(&cpLEDS[NUM_CP_LEDS/2], NUM_CP_LEDS/2, col2);
  fadeIndex++;
  FastLED.show();  
}

void loop() {
  
  boolean leftButtonPressed = CircuitPlayground.leftButton();    // Left button changes pattern
  boolean rightButtonPressed = CircuitPlayground.rightButton();  // Right button changes palette (TBD
  boolean switchLeft = CircuitPlayground.slideSwitch();         // Switch left => mode change

  boolean justPressedLeft = leftButtonPressed && !leftButtonState;
  boolean justPressedRight = rightButtonPressed && !rightButtonState;
  // mode change
  if (switchLeft) {
    if (justPressedLeft) {    
      ledMode = (ledMode + 1) % NUM_MODES;
      clearPixels();
      delay(200);
    } 
    if (justPressedRight) {
      paletteIndex = (paletteIndex + 1) % nPalettes;
      currentPalette = paletteList[paletteIndex];
    }
  } 
  
  // Store current button state
  leftButtonState = leftButtonPressed;
  rightButtonState = rightButtonPressed;

  switch(ledMode) {
    case MODE_MOTION:
      motion();
      break;
   case MODE_SOUND_REACT:
      soundreactive();
      break;
   case MODE_FADE:
      fadeLeftRight();
      break;
   case MODE_HEARTBEAT:
      if (!switchLeft && justPressedLeft) {
        heartbeatsPerMin += heartIncrement;
        heartbeatsPerMin = constrain(heartbeatsPerMin, heartMin, heartMax);
      }
      if (!switchLeft && justPressedRight) {
        heartbeatsPerMin -= heartIncrement;
        heartbeatsPerMin = constrain(heartbeatsPerMin, heartMin, heartMax);
      }
      EVERY_N_MILLISECONDS(20) {
        heartbeat(true);
      }
      break;
    case MODE_RAINBOW_BEAT:
      EVERY_N_MILLISECONDS(20) {
        rainbow_beat();
      }
      break;
    case MODE_SINELON:
      if (!switchLeft && justPressedLeft) { 
        sinelonBPM += bpmIncrement;
        sinelonBPM = constrain(sinelonBPM, bpmMin, bpmMax);     
      } 
      if (!switchLeft && justPressedRight) {
        sinelonBPM -= bpmIncrement;
        sinelonBPM = constrain(sinelonBPM, bpmMin, bpmMax);
      } 
      EVERY_N_MILLISECONDS(20) {
        sinelon();
      }
      break;
    default:
      break;
  }
  


} // loop()

   
