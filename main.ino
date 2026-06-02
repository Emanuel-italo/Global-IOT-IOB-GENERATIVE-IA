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

WebServer server(80);

// ─── Variáveis de Estado ──────────────────────────────────────
int  valorAnalog      = 0;    // leitura ADC (0–4095)
int  nivelRisco       = 0;    // 0=normal, 1=atenção, 2=crítico
bool modoAutomatico   = false;// controlado pelo botão branco
int  contadorAlertas  = 0;    // total de alertas críticos detectados
bool flagBranco       = LOW;  // detecção de borda botão branco
bool flagPreto        = LOW;  // detecção de borda botão preto
bool alertaManual     = false;// ativado por pulso no botão preto


struct Alerta {
  unsigned long timestamp;
  int valor;
  String tipo;
};
Alerta historico[5];
int idxHistorico = 0;

void atualizarLEDs();
void processarBotoes();
void lerSensor();
void handleStatus();
void handleSensor();
void handleAlertas();
void handleReset();
void handleNotFound();
void registrarAlerta(String tipo);
String nivelParaString(int nivel);

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BT_BRANCO, INPUT_PULLUP);
  pinMode(BT_PRETO,  INPUT_PULLUP);
  pinMode(LED_GRE,   OUTPUT);
  pinMode(LED_YEL,   OUTPUT);
  pinMode(LED_RED,   OUTPUT);

    digitalWrite(LED_GRE, LOW);
  digitalWrite(LED_YEL, LOW);
  digitalWrite(LED_RED, LOW);

    for (int i = 0; i < 5; i++) {
    historico[i] = {0, 0, "vazio"};
  }

Serial.println("==============================================");
  Serial.println(" OrbitalSense — Estacao IoT Inicializando...");
  Serial.println("==============================================");

    // ─── Conexão Wi-Fi ────────────────────────────────────────
  Serial.print("[WiFi] Conectando a: ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);

    int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

    if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] CONECTADO!");
    Serial.print("[WiFi] IP da Estacao: ");
        Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] FALHA NA CONEXAO — operando offline");
  }

server.on("/status",  HTTP_GET,  handleStatus);
server.on("/sensor",  HTTP_GET,  handleSensor);
  server.on("/alertas", HTTP_GET,  handleAlertas);
  server.on("/reset",   HTTP_POST, handleReset);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[HTTP] WebServer iniciado na porta 80");
  Serial.println("==============================================");
   Serial.println(" ENDPOINTS DISPONIVEIS:");
  Serial.println("   GET  /status   — estado geral do sistema");
  Serial.println("   GET  /sensor   — leitura atual + nivel de risco");
    Serial.println("   GET  /alertas  — historico de alertas");
  Serial.println("   POST /reset    — zera contador de alertas");
  Serial.println("==============================================\n");
}

void loop() {
  server.handleClient();  // processa requisições HTTP
  processarBotoes();      // detecta pulsos nos botões
  lerSensor();            // lê potenciômetro e define nível
  atualizarLEDs();        // acende LED conforme nível
  delay(50);
}

void lerSensor() {
  valorAnalog = analogRead(POT);  // 0 a 4095

  int nivelAnterior = nivelRisco;