# README — OrbitalAgro

```markdown
# 🌱 OrbitalAgro — Estação Terrestre de Validação Climática

Projeto de IoT desenvolvido com ESP32 (simulado no Wokwi) para monitoramento de umidade do solo em tempo real, com integração MQTT, display LCD I2C e controle remoto via dashboard.

---

## 📋 Descrição

O **OrbitalAgro** simula uma estação de campo agrícola que lê continuamente a umidade do solo, classifica o risco de seca, aciona LEDs de alerta e publica os dados em tópicos MQTT no broker público HiveMQ. O sistema aceita comandos remotos para ligar/desligar a bomba d'água, alternar modos de operação e resetar contadores.

---

## 🏗️ Arquitetura do Sistema

```
[Potenciômetro/Sensor] --> [ESP32] --> [Broker MQTT: broker.hivemq.com]
                              |                    |
                         [LCD I2C]         [Dashboard Externo]
                         [LEDs RGB]
                         [Botões]
```

- Conexão MQTT **não-bloqueante** via `verificaConexoes()`
- Publicação temporizada a cada **2 segundos**
- Código organizado em módulos com responsabilidades separadas

---

## 🔌 Hardware (Wokwi)

| Tipo       | Componente                  | Pino ESP32 |
|------------|-----------------------------|------------|
| OUTPUT     | LED Verde                   | 17         |
| OUTPUT     | LED Amarelo                 | 4          |
| OUTPUT     | LED Vermelho                | 2          |
| INPUT      | Botão Branco (modo)         | 19         |
| INPUT      | Botão Preto (bomba)         | 18         |
| ANALOG IN  | Potenciômetro (umidade)     | 33         |
| I2C SDA    | Display LCD 16x2            | 21         |
| I2C SCL    | Display LCD 16x2            | 22         |

---

## 📡 Tópicos MQTT

### Publicação (ESP32 → Dashboard)

| Tópico | Conteúdo |
|--------|----------|
| `orbitalsense/fiap2tds/RM561337/status` | Estado geral do sistema |
| `orbitalsense/fiap2tds/RM561337/sensor` | Leitura do sensor de umidade |
| `orbitalsense/fiap2tds/RM561337/alertas` | Histórico dos últimos 5 alertas |

### Subscrição (Dashboard → ESP32)

| Tópico | Ação |
|--------|------|
| `orbitalsense/fiap2tds/RM561337/reset` | Reseta contadores e histórico |
| `orbitalsense/fiap2tds/RM561337/cmd/modo` | Alterna modo automático/manual |
| `orbitalsense/fiap2tds/RM561337/cmd/bomba` | Liga/desliga irrigação remotamente |

---

## 📦 Payloads JSON

### `/status`
```json
{
  "sistema": "OrbitalAgro",
  "uptime_ms": 12500,
  "modo_automatico": false,
  "alerta_manual_ativo": false,
  "total_alertas": 2,
  "nivel_atual": "NORMAL"
}
```

### `/sensor`
```json
{
  "sensor": "umidade_solo",
  "valor_adc": 2100,
  "percentual": "51.3",
  "nivel_risco": 1,
  "nivel_descricao": "ATENCAO",
  "leds": {
    "verde": false,
    "amarelo": true,
    "vermelho": false
  }
}
```

### `/alertas`
```json
{
  "total_alertas": 2,
  "historico": [
    {
      "timestamp_ms": 5300,
      "valor_adc": 800,
      "tipo": "AUTOMATICO_CRITICO"
    },
    {
      "timestamp_ms": 9100,
      "valor_adc": 0,
      "tipo": "MANUAL_BOTAO_PRETO"
    }
  ]
}
```

---

## 🚦 Lógica de Classificação

| Faixa ADC       | Status      | LED Ativo | Descrição MQTT |
|-----------------|-------------|-----------|----------------|
| 0 – 1365        | STATUS_SECO | Vermelho  | `CRITICO`      |
| 1366 – 2730     | STATUS_MODERADO | Amarelo | `ATENCAO`   |
| 2731 – 4095     | STATUS_UMIDO | Verde    | `NORMAL`       |

---

## 🖥️ Display LCD

**Linha 0:** Valor ADC atual + modo de operação (`[AUTO]` / `[MANU]`)  
**Linha 1:** Status da bomba ou condição do solo

Exemplos:
```
Umid:800   [MANU]
ALERTA: SECO!
```
```
Umid:2100  [AUTO]
SOLO: MODERADO
```

---

## 🔘 Botões Físicos

| Botão  | Ação ao soltar |
|--------|----------------|
| Branco (pino 19) | Alterna entre modo automático e manual |
| Preto  (pino 18) | Liga/desliga a bomba e registra alerta |

---

## 📚 Bibliotecas Necessárias

```
Arduino.h
WiFi.h
PubSubClient
ArduinoJson
LiquidCrystal_I2C
```

---

## ⚙️ Configuração

1. Abra o projeto no [Wokwi](https://wokwi.com) ou na IDE Arduino com ESP32 configurado.
2. As credenciais Wi-Fi já estão configuradas para a rede virtual do Wokwi:
   ```cpp
   SSID     = "Wokwi-GUEST"
   PASSWORD = ""
   ```
3. O broker MQTT público não requer autenticação:
   ```
   broker.hivemq.com : 1883
   ```
4. Compile e faça upload. O LCD exibirá `OrbitalAgro IoT / Iniciando...` durante a conexão.

---

## 👤 Autor

**RM561337** — FIAP 2TDS  
Projeto: OrbitalAgro / OrbitalSense
```