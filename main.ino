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