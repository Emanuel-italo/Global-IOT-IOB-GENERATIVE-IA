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

// Tópicos de Comunicação
const char* TOPIC_PUB_STATUS = "orbitalsense/fiap2tds/RM561337/status";
const char* TOPIC_PUB_SENSOR = "orbitalsense/fiap2tds/RM561337/sensor";
const char* TOPIC_PUB_ALERTA = "orbitalsense/fiap2tds/RM561337/alertas";

const char* TOPIC_SUB_RESET  = "orbitalsense/fiap2tds/RM561337/reset";
const char* TOPIC_SUB_MODO   = "orbitalsense/fiap2tds/RM561337/cmd/modo";
const char* TOPIC_SUB_BOMBA  = "orbitalsense/fiap2tds/RM561337/cmd/bomba";

WiFiClient   espClient;
PubSubClient MQTT(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long ultimaPublicacao = 0;

int  valorAnalog      = 0;
int  nivelRisco       = STATUS_SECO;
bool modoAutomatico   = false;
int  contadorAlertas  = 0;
bool flagBranco       = LOW;
bool flagPreto        = LOW;
bool alertaManual     = false;

struct Alerta {
  unsigned long timestamp;
  int valor;
  String tipo;
};
Alerta historico[5];
int idxHistorico = 0;

void initWiFi();
void initMQTT();
void reconectaMQTT();
void verificaConexoes();
void callbackMQTT(char* topic, byte* payload, unsigned int length);

void lerSensor();
void processarBotoes();
void atualizarLEDs();
void atualizarLCD();
void registrarAlerta(String tipo);
String nivelParaString(int nivel);

void publicarTudo();
String jsonStatus();
String jsonSensor();
String jsonAlertas();

void setup() {
  Serial.begin(115200);

  // Passo B: Inicialização da tela nas primeiras linhas do setup
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
   lcd.print("OrbitalAgro IoT");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

   // Configuração dos Pinos
  pinMode(PIN_BT_BRANCO, INPUT_PULLUP);
  pinMode(PIN_BT_PRETO,  INPUT_PULLUP);
  pinMode(PIN_LED_G,     OUTPUT);
  pinMode(PIN_LED_Y,     OUTPUT);
  pinMode(PIN_LED_R,     OUTPUT);

    digitalWrite(PIN_LED_G, LOW);
    digitalWrite(PIN_LED_Y, LOW);
    digitalWrite(PIN_LED_R, LOW);

      // Limpa histórico
  for (int i = 0; i < 5; i++) historico[i] = {0, 0, "vazio"};

  initWiFi();
  initMQTT();
  delay(1000);
  lcd.clear(); // Limpa a mensagem de inicialização
}

void loop() {
  // Verifica conexões de forma não-bloqueante (Padrão Clean Code)
  verificaConexoes();
  MQTT.loop();
processarBotoes();
  lerSensor();
  atualizarLEDs();
  atualizarLCD(); // Passo C: Função de atualização da tela chamada no loop

    if (millis() - ultimaPublicacao >= INTERVALO_PUBLICACAO) {
    ultimaPublicacao = millis();
    publicarTudo();
  }
  
  delay(50);
}

void initWiFi() {
  Serial.print("[WiFi] Conectando a ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD, 6);

    while (WiFi.status() != WL_CONNECTED) { 
    delay(200); 
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Conectado!");
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(callbackMQTT);
}

void reconectaMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!MQTT.connected()) {
    Serial.print("[MQTT] Conectando ao Broker... ");
    String clientId = String(ID_MQTT) + "-" + String(random(0xffff), HEX);

        if (MQTT.connect(clientId.c_str())) {
      Serial.println("OK!");
      MQTT.subscribe(TOPIC_SUB_RESET);
      MQTT.subscribe(TOPIC_SUB_MODO);
      MQTT.subscribe(TOPIC_SUB_BOMBA);
      publicarTudo();
    } else {
      Serial.print("Falha, rc=");
      Serial.print(MQTT.state());
      Serial.println(" Nova tentativa automatica...");
    }
  }
}

void verificaConexoes() {
  if (WiFi.status() != WL_CONNECTED) initWiFi();
  if (!MQTT.connected()) reconectaMQTT();
}

void lerSensor() {
  valorAnalog = analogRead(PIN_POT);
  int nivelAnterior = nivelRisco;

  if      (valorAnalog <= 1365) nivelRisco = STATUS_SECO;
  else if (valorAnalog <= 2730) nivelRisco = STATUS_MODERADO;
  else                          nivelRisco = STATUS_UMIDO;

  
  if (nivelAnterior != STATUS_SECO && nivelRisco == STATUS_SECO) {
    contadorAlertas++;
    registrarAlerta("AUTOMATICO_CRITICO");
    Serial.printf("[ALERTA] Solo extremamente SECO! ADC: %d\n", valorAnalog);
  }
}

void processarBotoes() {
  bool statusBranco = digitalRead(PIN_BT_BRANCO);
  if (statusBranco == LOW  && flagBranco == LOW)  flagBranco = HIGH;
  if (statusBranco == HIGH && flagBranco == HIGH) {
    flagBranco = LOW;
    modoAutomatico = !modoAutomatico;
    alertaManual   = false;
    Serial.println(modoAutomatico ? "[SISTEMA] Irrigação AUTO Ativada" : "[SISTEMA] Controle MANUAL");
  }


  
  bool statusPreto = digitalRead(PIN_BT_PRETO);
  if (statusPreto == LOW  && flagPreto == LOW)  flagPreto = HIGH;
  if (statusPreto == HIGH && flagPreto == HIGH) {
    flagPreto    = LOW;
    alertaManual = !alertaManual;
  