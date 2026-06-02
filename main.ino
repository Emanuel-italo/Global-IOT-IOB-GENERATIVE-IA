/*
 * ============================================================
 * GLOBAL SOLUTION 2026/1 — FIAP 2TDS
 * Disciplina: Disruptive Architectures: IoT, IoB & Generative IA
 *
 * PROJETO: OrbitalSense — Estação de Monitoramento Ambiental Terrestre
 *
 * TEMA: Economia Espacial aplicada à Terra
 * CONTEXTO: Satélites coletam dados ambientais e os transmitem para
 * estações terrestres IoT. Este protótipo simula uma estação receptora
 * que monitora temperatura/radiação (potenciômetro), emite alertas
 * visuais (LEDs) e disponibiliza os dados via WebServer + API REST.
 *
 * HARDWARE (Wokwi):
 *   OUTPUT:
 *     - Pino 17: LED VERDE  (nível NORMAL)
 *     - Pino  4: LED AMARELO (nível ATENÇÃO)
 *     - Pino  2: LED VERMELHO (nível CRÍTICO / ALERTA)
 *   INPUT DIGITAL:
 *     - Pino 18: BOTÃO PRETO  (confirma leitura / aciona alerta manual)
 *     - Pino 19: BOTÃO BRANCO (ativa/desativa modo automático de varredura)
 *   INPUT ANALÓGICO:
 *     - Pino 33: POTENCIÔMETRO (simula sensor de temperatura/radiação)
 *
 * ENDPOINTS REST (3 documentados):
 *   GET /status   → JSON com estado geral do sistema
 *   GET /sensor   → JSON com leitura atual do sensor + nível de risco
 *   GET /alertas  → JSON com histórico dos últimos alertas registrados
 *   POST /reset   → Reseta contador de alertas (via parâmetro URL)
 *
 * PROTOCOLO: HTTP/WebServer nativo do ESP32
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ─── Credenciais Wi-Fi ────────────────────────────────────────
// No Wokwi, use a rede virtual simulada
const char* SSID     = "Wokwi-GUEST";
const char* PASSWORD = "";

// ─── Mapeamento de Pinos ──────────────────────────────────────
#define BT_PRETO   18
#define BT_BRANCO  19
#define LED_GRE    17
#define LED_YEL     4
#define LED_RED     2
#define POT        33

// ─── Servidor Web na porta 80 ─────────────────────────────────
WebServer server(80);

// ─── Variáveis de Estado ──────────────────────────────────────
int  valorAnalog      = 0;    // leitura ADC (0–4095)
int  nivelRisco       = 0;    // 0=normal, 1=atenção, 2=crítico
bool modoAutomatico   = false;// controlado pelo botão branco
int  contadorAlertas  = 0;    // total de alertas críticos detectados
bool flagBranco       = LOW;  // detecção de borda botão branco
bool flagPreto        = LOW;  // detecção de borda botão preto
bool alertaManual     = false;// ativado por pulso no botão preto

// Histórico simples dos últimos 5 alertas
struct Alerta {
  unsigned long timestamp;
  int valor;
  String tipo;
};
Alerta historico[5];
int idxHistorico = 0;

// ─── Protótipos ───────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  // Configuração de pinos
  pinMode(BT_BRANCO, INPUT_PULLUP);
  pinMode(BT_PRETO,  INPUT_PULLUP);
  pinMode(LED_GRE,   OUTPUT);
  pinMode(LED_YEL,   OUTPUT);
  pinMode(LED_RED,   OUTPUT);

  // LEDs apagados na inicialização
  digitalWrite(LED_GRE, LOW);
  digitalWrite(LED_YEL, LOW);
  digitalWrite(LED_RED, LOW);

  // Inicializa histórico
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

  // ─── Rotas do WebServer ───────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();  // processa requisições HTTP
  processarBotoes();      // detecta pulsos nos botões
  lerSensor();            // lê potenciômetro e define nível
  atualizarLEDs();        // acende LED conforme nível
  delay(50);
}

// ─── Leitura do Sensor (Potenciômetro) ───────────────────────
void lerSensor() {
  valorAnalog = analogRead(POT);  // 0 a 4095

  int nivelAnterior = nivelRisco;

  // Faixas de risco baseadas na leitura ADC
  if (valorAnalog <= 1365) {
    nivelRisco = 0;  // NORMAL
  } else if (valorAnalog <= 2730) {
    nivelRisco = 1;  // ATENÇÃO
  } else {
    nivelRisco = 2;  // CRÍTICO
  }

  // Registra alerta quando entra em nível CRÍTICO
  if (nivelAnterior != 2 && nivelRisco == 2) {
    contadorAlertas++;
    registrarAlerta("AUTOMATICO_CRITICO");
    Serial.print("[ALERTA] Nivel CRITICO detectado! Valor ADC: ");
    Serial.println(valorAnalog);
  }
}

// ─── Atualização dos LEDs ─────────────────────────────────────
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

  // Modo automático: sequência verde → amarelo → vermelho
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

// ─── Registra alerta no histórico circular ───────────────────
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

// ═══════════════════════════════════════════════════════════════
//  HANDLERS DA API REST
// ═══════════════════════════════════════════════════════════════

/*
 * GET /status
 * Retorna o estado geral do sistema OrbitalSense.
 *
 * Resposta JSON:
 * {
 *   "sistema": "OrbitalSense",
 *   "versao": "1.0.0",
 *   "uptime_ms": 12345,
 *   "wifi_rssi": -65,
 *   "modo_automatico": false,
 *   "alerta_manual_ativo": false,
 *   "total_alertas": 3,
 *   "nivel_atual": "NORMAL"
 * }
 */
void handleStatus() {
  // Adiciona cabeçalho CORS para dashboard externo
  server.sendHeader("Access-Control-Allow-Origin", "*");

  StaticJsonDocument<256> doc;
  doc["sistema"]            = "OrbitalSense";
  doc["versao"]             = "1.0.0";
  doc["uptime_ms"]          = millis();
  doc["wifi_rssi"]          = WiFi.RSSI();
  doc["modo_automatico"]    = modoAutomatico;
  doc["alerta_manual_ativo"]= alertaManual;
  doc["total_alertas"]      = contadorAlertas;
  doc["nivel_atual"]        = nivelParaString(nivelRisco);

  String resposta;
  serializeJson(doc, resposta);
  server.send(200, "application/json", resposta);

  Serial.println("[API GET /status] Requisicao atendida.");
}

/*
 * GET /sensor
 * Retorna a leitura atual do sensor de radiação/temperatura simulado.
 *
 * Resposta JSON:
 * {
 *   "sensor": "radiacao_simulada",
 *   "valor_adc": 2048,
 *   "percentual": 50.0,
 *   "nivel_risco": 1,
 *   "nivel_descricao": "ATENCAO",
 *   "leds": {
 *     "verde": false,
 *     "amarelo": true,
 *     "vermelho": false
 *   }
 * }
 */
void handleSensor() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  float percentual = (valorAnalog / 4095.0) * 100.0;

  StaticJsonDocument<256> doc;
  doc["sensor"]          = "radiacao_simulada";
  doc["valor_adc"]       = valorAnalog;
  doc["percentual"]      = serialized(String(percentual, 1));
  doc["nivel_risco"]     = nivelRisco;
  doc["nivel_descricao"] = nivelParaString(nivelRisco);

  JsonObject leds = doc.createNestedObject("leds");
  leds["verde"]     = (nivelRisco == 0);
  leds["amarelo"]   = (nivelRisco == 1);
  leds["vermelho"]  = (nivelRisco == 2);

  String resposta;
  serializeJson(doc, resposta);
  server.send(200, "application/json", resposta);

  Serial.print("[API GET /sensor] ADC=");
  Serial.print(valorAnalog);
  Serial.print(" | Nivel: ");
  Serial.println(nivelParaString(nivelRisco));
}

/*
 * GET /alertas
 * Retorna o histórico dos últimos 5 alertas registrados.
 *
 * Resposta JSON:
 * {
 *   "total_alertas": 3,
 *   "historico": [
 *     { "timestamp_ms": 5200, "valor_adc": 3100, "tipo": "AUTOMATICO_CRITICO" },
 *     ...
 *   ]
 * }
 */
void handleAlertas() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  StaticJsonDocument<512> doc;
  doc["total_alertas"] = contadorAlertas;

  JsonArray arr = doc.createNestedArray("historico");
  for (int i = 0; i < 5; i++) {
    if (historico[i].timestamp > 0) {
      JsonObject item = arr.createNestedObject();
      item["timestamp_ms"] = historico[i].timestamp;
      item["valor_adc"]    = historico[i].valor;
      item["tipo"]         = historico[i].tipo;
    }
  }

  String resposta;
  serializeJson(doc, resposta);
  server.send(200, "application/json", resposta);

  Serial.println("[API GET /alertas] Historico enviado.");
}

/*
 * POST /reset
 * Zera o contador de alertas e limpa o histórico.
 *
 * Resposta JSON:
 * { "mensagem": "Contador de alertas resetado com sucesso.", "total_alertas": 0 }
 */
void handleReset() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  contadorAlertas = 0;
  idxHistorico    = 0;
  alertaManual    = false;
  for (int i = 0; i < 5; i++) {
    historico[i] = {0, 0, "vazio"};
  }

  StaticJsonDocument<128> doc;
  doc["mensagem"]      = "Contador de alertas resetado com sucesso.";
  doc["total_alertas"] = 0;

  String resposta;
  serializeJson(doc, resposta);
  server.send(200, "application/json", resposta);

  Serial.println("[API POST /reset] Sistema resetado.");
}

// Rota não encontrada
void handleNotFound() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  StaticJsonDocument<64> doc;
  doc["erro"] = "Endpoint nao encontrado.";
  String resposta;
  serializeJson(doc, resposta);
  server.send(404, "application/json", resposta);
}
