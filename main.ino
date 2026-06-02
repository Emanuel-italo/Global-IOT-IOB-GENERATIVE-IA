#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>


const char* SSID     = "Wokwi-GUEST";
const char* PASSWORD = "";

#define BT_PRETO   18
#define BT_BRANCO  19
#define LED_GRE    17
#define LED_YEL     4
#define LED_RED     2
#define POT        33