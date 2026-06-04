# 🌱 OrbitalAgro — Estação Terrestre de Validação Climática

> Sistema IoT de monitoramento de umidade do solo e irrigação inteligente, com telemetria em tempo real via MQTT e dashboard web de controle remoto.


Emanuel Italo Leal Trindade Soares (RM 561337)
Paulo Henrique Alves Estalise (RM 563811)
Gabriel Bebe (RM 562012)




**FIAP · 2TDS · Global Solution 2026/1 — Economia Espacial e Agronegócio · ODS 2 (Fome Zero e Agricultura Sustentável)**

---

## 📖 Sobre o Projeto

O **OrbitalAgro** simula uma estação terrestre de validação climática voltada ao agronegócio. Um nó IoT baseado em **ESP32** mede continuamente a umidade do solo, classifica o risco de seca em três níveis e aciona um sistema de irrigação — de forma manual, física ou remota. Todos os dados são publicados em tempo real em um broker MQTT público e consumidos por um **dashboard web** com tema de "rede orbital", que também envia comandos de volta ao dispositivo.

A proposta conecta-se ao **ODS 2 da ONU**, ao apoiar a tomada de decisão na agricultura através do uso racional da água e da prevenção de perdas por seca.

---

## ✨ Funcionalidades

- 📊 **Monitoramento contínuo** da umidade do solo via sensor analógico (ADC 0–4095).
- 🚦 **Classificação automática de risco** em três níveis: Seco (crítico), Moderado e Ideal.
- 💡 **Indicação visual local** por LEDs (vermelho/amarelo/verde) e display **LCD 16x2 I2C**.
- 🔘 **Controle físico** por dois botões: alternância de modo e acionamento da bomba.
- 📡 **Telemetria em tempo real** via MQTT (status, sensor e alertas).
- 🖥️ **Dashboard web** com medidor de risco, animação de reservatório e histórico de eventos.
- 🛰️ **Comando remoto** pelo dashboard: alternar modo, forçar irrigação e resetar contadores.
- 📋 **Histórico circular** dos últimos 5 eventos críticos registrados.

---

## 🏗️ Arquitetura

```
┌──────────────────────┐         MQTT          ┌──────────────────────┐
│   ESP32 (Wokwi)      │  ───── publish ─────► │   broker.hivemq.com  │
│                      │                       │                      │
│  • Sensor de umidade │  ◄──── subscribe ──── │  (broker público)    │
│  • LEDs + LCD        │                       └──────────┬───────────┘
│  • 2 Botões          │                                  │ WebSocket (wss)
│  • Wi-Fi + MQTT      │                                  ▼
└──────────────────────┘                       ┌──────────────────────┐
                                                │   Dashboard Web      │
                                                │  (HTML + mqtt.js)    │
                                                │  • Telemetria        │
                                                │  • Comandos remotos  │
                                                └──────────────────────┘
```

O ESP32 publica via TCP na porta `1883`. O dashboard, por ser um navegador, conecta via **WebSocket Seguro** na porta `8884` (`wss://broker.hivemq.com:8884/mqtt`). Ambos compartilham os mesmos tópicos lógicos.

---

## 🔧 Hardware

Projeto desenvolvido e simulado no **[Wokwi](https://wokwi.com/)**.

| Componente                       | Pino ESP32 | Tipo    | Função                                    |
| -------------------------------- | ---------- | ------- | ----------------------------------------- |
| LED Verde                        | 17         | OUTPUT  | Indica umidade ideal                      |
| LED Amarelo                      | 4          | OUTPUT  | Indica umidade moderada                   |
| LED Vermelho                     | 2          | OUTPUT  | Indica solo seco / bomba ativa (piscando) |
| Botão Preto                      | 18         | INPUT_PULLUP | Liga/desliga a bomba d'água          |
| Botão Branco                     | 19         | INPUT_PULLUP | Alterna modo AUTO / MANUAL            |
| Potenciômetro (sensor de solo)   | 33         | ANALOG  | Leitura de umidade do solo                |
| LCD 16x2 — SDA                   | 21         | I2C     | Display de status (endereço `0x27`)       |
| LCD 16x2 — SCL                   | 22         | I2C     | Display de status                         |

> O potenciômetro é usado para **simular** o sensor de umidade do solo, permitindo variar a leitura manualmente na simulação.

---

## 💻 Software e Bibliotecas

Firmware escrito em **C++ (Arduino Framework)**. Bibliotecas necessárias:

| Biblioteca            | Uso                                      |
| --------------------- | ---------------------------------------- |
| `WiFi.h`              | Conexão Wi-Fi (nativa do ESP32)          |
| `PubSubClient`        | Cliente MQTT                             |
| `ArduinoJson`         | Serialização dos payloads JSON           |
| `LiquidCrystal_I2C`   | Controle do display LCD 16x2             |

### Credenciais e configuração padrão

```cpp
SSID        = "Wokwi-GUEST";          // rede do simulador
PASSWORD    = "";                     // sem senha
BROKER_MQTT = "broker.hivemq.com";
BROKER_PORT = 1883;
ID_MQTT     = "orbital_agro_rm561337";
```

---

## 🧠 Lógica de Funcionamento

### Classificação de umidade

A leitura analógica (0–4095) é convertida em três níveis de risco:

| Leitura ADC      | Nível            | Constante         | LED      | Descrição           |
| ---------------- | ---------------- | ----------------- | -------- | ------------------- |
| `0 – 1365`       | Seco (crítico)   | `STATUS_SECO` (0) | 🔴 Vermelho | Risco de seca       |
| `1366 – 2730`    | Moderado         | `STATUS_MODERADO` (1) | 🟡 Amarelo | Atenção             |
| `2731 – 4095`    | Ideal            | `STATUS_UMIDO` (2) | 🟢 Verde   | Saturação adequada  |

Sempre que o solo **transita** para o estado seco, o contador de alertas é incrementado e um evento do tipo `AUTOMATICO_CRITICO` é registrado no histórico.

### Modos de operação

- **AUTO** (`modoAutomatico = true`): rótulo de irrigação automática; os LEDs refletem a umidade do solo.
- **MANUAL**: controle direto da bomba pelo operador.

A bomba d'água (`alertaManual`) é acionada pelo **botão preto** (físico) ou por **comando remoto** do dashboard. Enquanto ativa, o LED vermelho pisca em alta prioridade, sobrepondo a indicação de umidade.

### Display LCD

```
Linha 1:  Umid:<valor>      [AUTO]/[MANU]
Linha 2:  ALERTA: SECO! / SOLO: MODERADO / SOLO: IDEAL / BOMBA ATIVADA!
```

### Publicação

A telemetria é publicada a cada **2000 ms** (`INTERVALO_PUBLICACAO`). As conexões Wi-Fi e MQTT são verificadas de forma não-bloqueante a cada ciclo do `loop()`.

---

## 📡 Comunicação MQTT

Todos os tópicos seguem o padrão `orbitalsense/fiap2tds/<DEVICE_ID>/...`, onde `<DEVICE_ID>` é o RM (ex.: `RM561337`).

### Tópicos publicados pelo ESP32

| Tópico                                       | Conteúdo                                |
| -------------------------------------------- | --------------------------------------- |
| `orbitalsense/fiap2tds/RM561337/status`      | Estado geral do nó                      |
| `orbitalsense/fiap2tds/RM561337/sensor`      | Telemetria do sensor de umidade         |
| `orbitalsense/fiap2tds/RM561337/alertas`     | Contador e histórico de eventos         |

### Tópicos assinados (comandos recebidos)

| Tópico                                         | Ação                                    |
| ---------------------------------------------- | --------------------------------------- |
| `orbitalsense/fiap2tds/RM561337/reset`         | Zera contadores e histórico             |
| `orbitalsense/fiap2tds/RM561337/cmd/modo`      | Alterna entre modo AUTO e MANUAL        |
| `orbitalsense/fiap2tds/RM561337/cmd/bomba`     | Liga/desliga a bomba d'água             |

### Exemplos de payload

**`/status`**
```json
{
  "sistema": "OrbitalAgro",
  "uptime_ms": 124500,
  "modo_automatico": false,
  "alerta_manual_ativo": false,
  "total_alertas": 3,
  "nivel_atual": "ATENCAO"
}
```

**`/sensor`**
```json
{
  "sensor": "umidade_solo",
  "valor_adc": 2048,
  "percentual": 50.0,
  "nivel_risco": 1,
  "nivel_descricao": "ATENCAO",
  "leds": { "verde": false, "amarelo": true, "vermelho": false }
}
```

**`/alertas`**
```json
{
  "total_alertas": 3,
  "historico": [
    { "timestamp_ms": 84210, "valor_adc": 980, "tipo": "AUTOMATICO_CRITICO" },
    { "timestamp_ms": 91500, "valor_adc": 1100, "tipo": "MANUAL_BOTAO_PRETO" }
  ]
}
```

### Tipos de evento no histórico

| Tipo                  | Significado                                  |
| --------------------- | -------------------------------------------- |
| `AUTOMATICO_CRITICO`  | Solo detectado como seco automaticamente     |
| `MANUAL_BOTAO_PRETO`  | Bomba acionada fisicamente pelo botão        |
| `ACIONAMENTO_REMOTO`  | Bomba acionada pelo dashboard                 |

---

## 🖥️ Dashboard Web

Página única em HTML/CSS/JS que usa a biblioteca **[MQTT.js](https://github.com/mqttjs/MQTT.js)** (carregada via CDN) para comunicação em tempo real.

### Recursos do painel

- **Medidor de umidade (ADC)** com gauge circular e barra de saturação.
- **Card de risco** com cor e descrição dinâmicas conforme o nível.
- **Contador de alertas de seca** com botão de reset.
- **Animação de reservatório** que enche durante a irrigação e exibe mensagem de conclusão.
- **Histórico** dos últimos 5 eventos com badges de origem (clima/satélite ou bomba manual).
- **Comandos remotos**: alternar modo, forçar irrigação e resetar estação.
- **Campo "Nó Terrestre ID"**: permite trocar o `DEVICE_ID` monitorado em tempo real (botão *Sincronizar*).

### Como usar

1. Abra o arquivo `dashboard.html` em um navegador moderno.
2. Confirme o ID do nó (padrão `RM561337`) e clique em **Sincronizar**, se necessário.
3. Aguarde o status mudar para **REDE ORBITAL ATIVA**.
4. Acompanhe a telemetria e utilize os botões de comando remoto.

---

## 🚀 Como Executar

### Firmware (ESP32 no Wokwi)

1. Abra o projeto no **[Wokwi](https://wokwi.com/)** com um ESP32.
2. Monte o circuito conforme a tabela de **Hardware** acima (LEDs, botões, potenciômetro e LCD I2C).
3. Instale as bibliotecas `PubSubClient`, `ArduinoJson` e `LiquidCrystal_I2C`.
4. Cole o código do firmware no `sketch.ino`.
5. Inicie a simulação. O ESP32 conecta à rede `Wokwi-GUEST` e ao broker HiveMQ automaticamente.
6. Ajuste o potenciômetro para simular diferentes níveis de umidade do solo.

### Dashboard

1. Garanta que o firmware esteja rodando e publicando no broker.
2. Abra o `dashboard.html` (basta clicar duas vezes ou servir localmente).
3. O ID do dispositivo no dashboard deve corresponder ao RM usado nos tópicos do firmware.

> ⚠️ **Importante:** o `DEVICE_ID` do dashboard deve bater com o RM usado nos tópicos do firmware. No código atual os tópicos estão fixos em `RM561337` — para usar outro RM, atualize **ambos** (firmware e campo do dashboard).

---

## 📁 Estrutura de Arquivos

```
OrbitalAgro/
├── sketch.ino          # Firmware do ESP32 (leitura, MQTT, LCD, LEDs)
├── dashboard.html      # Painel web de telemetria e comando remoto
└── README.md           # Este arquivo
```

---

## 🔭 Observações e Possíveis Melhorias

- **Modo automático:** atualmente o modo `AUTO` serve como rótulo e controla a indicação dos LEDs, mas a bomba não é acionada automaticamente ao detectar seca — o acionamento é sempre manual/remoto. Uma evolução natural seria disparar a irrigação automaticamente quando `modoAutomatico == true` e `nivelRisco == STATUS_SECO`.
- **Broker público:** o `broker.hivemq.com` é aberto e sem autenticação. Para uso real, recomenda-se um broker dedicado com TLS e credenciais.
- **API REST:** a seção de endpoints no dashboard (`/status`, `/sensor`, `/reset`) é documentação simulada de uma futura integração com backend; o projeto usa MQTT como transporte real.
- **Segurança:** considerar identificadores de cliente MQTT únicos e tópicos por dispositivo para múltiplos nós.

---

## 👥 Autoria

Projeto acadêmico desenvolvido para a **Global Solution 2026/1 — FIAP**, turma **2TDS**, na disciplina de **IoT**, com foco no **ODS 2**.

Emanuel Italo Leal Trindade Soares (RM 561337)
Paulo Henrique Alves Estalise (RM 563811)
Gabriel Bebe (RM 562012)