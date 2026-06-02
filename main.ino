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

    if (valorAnalog <= 1365) {
    nivelRisco = 0;  // NORMAL
  } else if (valorAnalog <= 2730) {
    nivelRisco = 1;  // ATENÇÃO
  } else {
    nivelRisco = 2;  // CRÍTICO
  }

    if (nivelAnterior != 2 && nivelRisco == 2) {
    contadorAlertas++;
    registrarAlerta("AUTOMATICO_CRITICO");
    Serial.print("[ALERTA] Nivel CRITICO detectado! Valor ADC: ");
    Serial.println(valorAnalog);
  }
}

void atualizarLEDs() {
  // Alerta manual tem prioridade: pisca o LED vermelho
  if (alertaManual) {
        digitalWrite(LED_GRE, LOW);
    digitalWrite(LED_YEL, LOW);
    digitalWrite(LED_RED, HIGH);
    delay(150);
    digitalWrite(LED_RED, LOW);
    delay(150);
    return;
  
}

if (modoAutomatico) {
    digitalWrite(LED_GRE, HIGH);
    delay(400);
        digitalWrite(LED_GRE, LOW);
    digitalWrite(LED_YEL, HIGH);
        delay(400);
    digitalWrite(LED_YEL, LOW);
    digitalWrite(LED_RED, HIGH);

        delay(400);
    digitalWrite(LED_RED, LOW);
    return;
  }

    // Modo normal: LED fixo conforme nível de risco
  digitalWrite(LED_GRE, nivelRisco == 0 ? HIGH : LOW);
  digitalWrite(LED_YEL, nivelRisco == 1 ? HIGH : LOW);
    digitalWrite(LED_RED, nivelRisco == 2 ? HIGH : LOW);
}

// ─── Processamento dos Botões (detecção de borda) ─────────────
void processarBotoes() {
  // ── Botão BRANCO: ativa/desativa modo automático ───────────
  bool statusBranco = digitalRead(BT_BRANCO);

    if (statusBranco == LOW && flagBranco == LOW) {
    // Borda de descida: botão pressionado
    flagBranco = HIGH;
  }
  if (statusBranco == HIGH && flagBranco == HIGH) {
    // Borda de subida: ação ao soltar
    flagBranco = LOW;
    modoAutomatico = !modoAutomatico;
    alertaManual   = false; // cancela alerta manual ao trocar modo
    Serial.print("[BOTAO BRANCO] Modo automatico: ");
    Serial.println(modoAutomatico ? "ATIVADO" : "DESATIVADO");
  }

    // ── Botão PRETO: aciona alerta manual + registra na Serial ─
  bool statusPreto = digitalRead(BT_PRETO);

  if (statusPreto == LOW && flagPreto == LOW) {
    flagPreto = HIGH;
  }

    if (statusPreto == HIGH && flagPreto == HIGH) {
    flagPreto    = LOW;
    alertaManual = !alertaManual;

        if (alertaManual) {
      contadorAlertas++;
      registrarAlerta("MANUAL_BOTAO_PRETO");
      Serial.println("[BOTAO PRETO] ALERTA MANUAL ATIVADO!");
    } else {
      Serial.println("[BOTAO PRETO] Alerta manual desativado.");
    }
  }
}

void registrarAlerta(String tipo) {
  historico[idxHistorico] = {millis(), valorAnalog, tipo};
    idxHistorico = (idxHistorico + 1) % 5;
}

// ─── Converte nível numérico para string descritiva ──────────
String nivelParaString(int nivel) {
  switch (nivel) {
     case 0: return "NORMAL";
    case 1: return "ATENCAO";
    case 2: return "CRITICO";
    default: return "DESCONHECIDO";
  }
}

void handleStatus() {
  // Adiciona cabeçalho CORS para dashboard externo
  server.sendHeader("Access-Control-Allow-Origin", "*");
