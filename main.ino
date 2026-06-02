#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h> // Passo A: Biblioteca do Display adicionada

// ─── Mapeamento de Pinos (Padrão Clean Code) ──────────────────
#define PIN_BT_PRETO   18
#define PIN_BT_BRANCO  19
#define PIN_LED_G      17 
#define PIN_LED_Y       4
#define PIN_LED_R       2
#define PIN_POT        33

// ─── Configurações do Sistema ─────────────────────────────────
#define INTERVALO_PUBLICACAO 2000 

#define STATUS_SECO     0
#define STATUS_MODERADO 1
#define STATUS_UMIDO    2

// ─── Credenciais Wi-Fi e MQTT ─────────────────────────────────
const char* SSID        = "Wokwi-GUEST";
const char* PASSWORD    = "";
const char* BROKER_MQTT = "broker.hivemq.com";
const int   BROKER_PORT = 1883;
const char* ID_MQTT     = "orbital_agro_rm561337";