// minilight- an Arduino-powered LED-strip nightlight, with light sensor and on/off button
// Code written by Vince Petaccio
// version 09.02.2017
// Developed on MEGA 2560 Arduino board
// Uses FastLED library, which must be added to the Arduino IDE

// SETTINGS ---------------------------------------------

// Define the number of LEDs in the strip
#define NUM_LEDS 120

// Diagnostic variables (use 9600 baud for serial monitor/plotter)
// Turn on light measurement output- LIGHT WILL NOT TURN ON
bool lightMeasure = false;

// Turn on strip brightness knob value measurement- LIGHT WILL NOT TURN ON
bool stripBrightnessMeasure = false;

// Turn on the "new session" flag output when the button is in the off position
bool buttPrint = false;

// Set the brightness threshold for activation. Use the lightMeasure variable to find the measured brightness with lights on
// and lights off. Set this value to be between the two, so that the strip will activate when the lights are turned off.
int minBrightness = 150;

// Set the strip colors (See color reference below settings)
int firstHue = 200;
int secondHue = 30;
int firstSat = 255;
int secondSat = 255;

// Set the brightness difference between the two ends of the strip
int brightOffset = 90;

// Choose whether the colors will morph (any integer but 0) or not (0). 
int morph = 0;
// Set the amount of gradation between the two ends of the strip, in hue degrees
int morphOffset = 15;
// Set the morph speed- this is the amount that the hue changes in *each iteration of the loop*
double morphSpeed = .01;

// Set the duration of nightlight in ms (minutes*seconds*1000). Time will be *APPROXIMATE*, and does not include fade in/out.
unsigned long onTime = 1500000;

// Set the number of fade steps and delay between each step of the fade in/out. 100 steps at 8ms works out to roughly 5s total
int stepDelay = 8;
int fadeSteps = 100;

// Set the offset for brightness- avoids having to crawl under the bed and turn the pot
int brightnessOffset = 0;

// COLOR REFERENCES -------------------------------------
// Mountain Sunrise [default]
//int firstHue = 200;
//int secondHue = 30;
//int firstSat = 255;
//int secondSat = 255;

// Magenta Sunset:
// int firstHue = 5;
// int secondHue = -5;
// int firstSat = 180;
// int secondSat = 80;

// Sky Blue:
// int firstHue = 160;
// int secondHue = 180;
// int firstSat = 170;
// int secondSat = 50;

// Peach Slice:
// int firstHue = 270;
// int secondHue = 290;
// int firstSat = 200;
// int secondSat = 40;

// Blue Raspberry:
// int firstHue = 180;
// int secondHue = 210;
// int firstSat = 255;
// int secondSat = 120;

// Hue angles:
// Red = 0
// Yellow = 42
// Green = 85
// Aqua = 128
// Blue = 171
// Purple = 213

// ------------------------------------------------------

#include <bitswap.h>
#include <chipsets.h>
#include <color.h>
#include <colorpalettes.h>
#include <colorutils.h>
#include <controller.h>
#include <cpp_compat.h>
#include <dmx.h>
#include <FastLED.h>
#include <fastled_config.h>
#include <fastled_delay.h>
#include <fastled_progmem.h>
#include <fastpin.h>
#include <fastspi.h>
#include <fastspi_bitbang.h>
#include <fastspi_dma.h>
#include <fastspi_nop.h>
#include <fastspi_ref.h>
#include <fastspi_types.h>
#include <hsv2rgb.h>
#include <led_sysdefs.h>
#include <lib8tion.h>
#include <noise.h>
#include <pixelset.h>
#include <pixeltypes.h>
#include <platforms.h>
#include <power_mgt.h>

// ------------------------------------------------------
// Define input pins

// LED strip data pin (digital)
#define DATA_PIN 7
// Strip brightness potentiometer pin (analog)
#define BRIGHT_PIN 7
// On/Off button pin (digital)
#define BUTTON_PIN 34
// Light sensor data pin (analog)
#define PHOTO_PIN 0

// ------------------------------------------------------

// Define global utility variables
CRGB leds[NUM_LEDS];
int stripBrightness;
int knobSetting;
int newSession = 1;
unsigned long sessionStart;
int justEnding;

void setup() {
  
  // Serial monitoring for light measurement
  if (lightMeasure || stripBrightnessMeasure || buttPrint){
    Serial.begin(9600);
  }
  
  // Setup the LED strip
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  // Setup the button input
  pinMode(BUTTON_PIN, INPUT);

  // Turn off the LEDs if they were on at time of upload
  lightsOut();
}

void loop() {

  // Check the pot on the bright_pin to determine how bright the strip should be
  stripBrightness = brightnessRead() + brightnessOffset;

  // Print the strip brightess pot measurement if the flag is on
  while (stripBrightnessMeasure){
      Serial.println(brightnessRead());
  }

  // Print the light sensor measurement if the flag is on
  while (lightMeasure){
      lightCheck();
  }

  // Build a loop to run the LEDs as a slowly evolving gradient
  for (double index = 0; index < 256 ; index += morphSpeed) {
    if (newSession && buttCheck() && lightCheck()) {
      newSession = 0;
      justEnding = 1;
      sessionStart = millis();
      for(int j = 0; j < fadeSteps + 1; j++){
        fadeMake(j,index);
      }
    }
    // Steady-on portion
    if (!newSession && (millis() < sessionStart + onTime)) {
      // Serial.println("Steady on");
      showLight(index);
    } else {
      // Fade the lights out
      if (justEnding && !newSession) {
        // Serial.println("Fading out");
        justEnding = 0;
        for(int k = fadeSteps; k > 0; k--){
          fadeMake(k,index);
        }
        lightsOut();
      }
      else {
        buttCheck();
        lightCheck();
      }
    }
  }
}

void fadeMake(int stepVal, int index){
  if (buttCheck()) {
    if (morph) {
      fill_gradient(leds, 0, CHSV(round(firstHue+index),max(stepVal-(fadeSteps-firstSat),0),max(round((stripBrightness*stepVal)/fadeSteps),0)), NUM_LEDS-1, CHSV(round(secondHue+index),max(0,((secondSat-fadeSteps)+stepVal)),max(round(((stripBrightness-brightOffset)*stepVal)/fadeSteps),0)), SHORTEST_HUES);
    } else {
      fill_gradient(leds, 0, CHSV(firstHue,max(stepVal-(fadeSteps-firstSat),0),max(round((stripBrightness*stepVal)/fadeSteps),0)), NUM_LEDS-1, CHSV(secondHue,max(0,((secondSat-fadeSteps)+stepVal)),max(round(((stripBrightness-brightOffset)*stepVal)/fadeSteps),0)), SHORTEST_HUES);
    }
    FastLED.show();
  } else {
    lightsOut();
  }
  delay(stepDelay);
}

void showLight(int index){
  if (buttCheck()) {
    if (morph) {
      fill_gradient(leds, 0, CHSV(round(firstHue+index),firstSat,stripBrightness), NUM_LEDS-1, CHSV(round(secondHue+index+morphOffset),secondSat,min(stripBrightness+brightOffset,max((stripBrightness-brightOffset),0))), SHORTEST_HUES);
    } else {
      fill_gradient(leds, 0, CHSV(firstHue,firstSat,stripBrightness), NUM_LEDS-1, CHSV(secondHue,secondSat,max((stripBrightness-brightOffset),0)), SHORTEST_HUES);    
    }
    FastLED.show();
  } else {
    lightsOut();
  }
}

void lightsOut(){
  FastLED.clear();
  FastLED.show();
}

int buttCheck(){
  int buttSlap = digitalRead(BUTTON_PIN);  
  if (!buttSlap){
    if (buttPrint) {
      Serial.println("Button in off position- new session");
    }
    newSession = 1;
  }
  return buttSlap;
}

bool lightCheck(){
  // Check how bright the room is, smoothing the readings by taking a 25-point average
  int lightLevel = 0;
  int lightReading = 0;
  for(int i = 0; i < 25; i++) {
    lightReading = analogRead(PHOTO_PIN);
    lightLevel = lightLevel + lightReading;
    delay(1);
  }
  int lightSample = lightLevel/25;  
  if (lightMeasure){
    Serial.println(lightSample);
  }
  // Reset the session if the light gets turned back on
  if (lightSample >= minBrightness) {
    newSession = 1;
  }
  // Returns true or false- true if the room is DARK enough for the light to activate, false if it's TOO BRIGHT.
  return (lightSample < minBrightness);
}

int brightnessRead(){
  // Get the brightness knob value
  knobSetting = analogRead(BRIGHT_PIN);
  // Convert knob value from 0-1023 value to 0-255 value
  return round(knobSetting*0.2492);
}

