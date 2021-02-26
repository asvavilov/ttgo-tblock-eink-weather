#include <Arduino.h>

#define LILYGO_WATCH_BLOCK

#define LILYGO_EINK_GDEH0154D67_TP

#include <LilyGoWatch.h>

#include <WiFi.h>
#include "time.h"
#include <DHTesp.h>
#include <BME280I2C.h>
#include <Wire.h>
#include <ThingSpeak.h>
