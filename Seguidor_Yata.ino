/* =============================================================================
   SEGUIDOR DE LINHA - ESP32 + 2x QRE1113 (RoboCore) + 2x Motor TT (3-6 V)
   Driver: ponte H L298N  |  Controle de START/STOP por pagina web (Wi-Fi)
   -----------------------------------------------------------------------------
   LIGACOES (altere nos #define abaixo se usar outros pinos)

     QRE esquerdo  AO  -> GPIO 34   (somente entrada, ADC1)
     QRE direito   AO  -> GPIO 35   (somente entrada, ADC1)
     QRE VCC -> 3V3    QRE GND -> GND
     (Os sensores PRECISAM ficar em pinos do ADC1: o ADC2 nao funciona com o
      Wi-Fi ligado.)

     L298N  IN1 -> GPIO 26  |  OUT1/OUT2 -> motor ESQUERDO
     L298N  IN2 -> GPIO 27
     L298N  ENA -> GPIO 13  (PWM do motor esquerdo)
     L298N  IN3 -> GPIO 32  |  OUT3/OUT4 -> motor DIREITO
     L298N  IN4 -> GPIO 33
     L298N  ENB -> GPIO 25  (PWM do motor direito)
     L298N  GND -> GND do ESP32  (obrigatorio: terra comum)
     L298N  +12V (VMS) -> saida do step-up (8 V)

     LED de status -> GPIO 2 (LED da placa)
     Nao ha botao fisico: START, STOP e CALIBRAR sao feitos pela pagina web.

   ALIMENTACAO
     2x 18650 em suporte PARALELO (1S, 3,0-4,2 V)
       -> TP4056 USB-C com protecao (carrega pelo cabo magnetico e protege
          contra descarga excessiva e curto; substitui a BMS)
       -> chave KCD11 (no OUT+ do TP4056)
       -> step-up XL6009 ajustado em 8 V -> +12V do L298N
                                         -> step-down MP1584 em 5 V -> VIN

   -----------------------------------------------------------------------------
   CUIDADOS ESPECIFICOS DO L298N

   1) RETIRE os jumpers de ENA e ENB. Com eles no lugar o enable fica preso em
      5 V e o PWM nao tem efeito nenhum (o carrinho so anda a 100%).

   2) As entradas IN aceitam 3,3 V do ESP32 (o nivel alto do L298N comeca em
      2,3 V), entao nao precisa de conversor de nivel.

   3) O L298N "come" de 1,8 a 2,5 V entre a alimentacao e o motor. Por isso
      a bateria de 1S (3,7 V) passa por um step-up de 7,5-8 V: os motores TT
      recebem no maximo ~6 V, que e o limite deles. Ligar a bateria direto no
      +12V deixaria os motores com ~1,5 V (nao andam).

   4) O Wi-Fi do ESP32 puxa picos de 300-500 mA. Alimente o ESP32 por um
      step-down de 5 V (no VIN), com um capacitor de 470-1000 uF entre 5V e
      GND. Pelo regulador do L298N a placa tende a reiniciar quando os
      motores arrancam.

   5) A protecao do TP4056 corta a bateria acima de ~3 A. Se o robo
      reiniciar sozinho nas arrancadas, diminua vBase/vMax na pagina.

   -----------------------------------------------------------------------------
   ARQUITETURA DE REDE (cliente-servidor sobre TCP/IP)
     O ESP32 e o SERVIDOR: abre portas em LISTEN e fica esperando os
     clientes (celular, PC, curl, telnet). Ele nunca inicia a conversa.

       Wi-Fi em modo AP+STA:
         STA -> entra na rede externa (REDE_SSID), IP por DHCP, reconecta
                sozinho se cair e grava a queda/volta no log com data e hora.
         AP  -> rede propria "SeguidorYata" (192.168.4.1), sempre ligada como
                reserva.
       Porta 80   (TCP) -> servidor HTTP: pagina + API JSON
       Porta 3333 (TCP) -> socket de comandos em texto (telnet / nc)
       NTP (UDP 123)    -> acerta o relogio quando ha internet; sem internet,
                           a pagina manda a hora do celular.

     ROTAS HTTP (teste com curl; troque IP pelo IP mostrado na pagina)
       GET  /             pagina de controle
       GET  /status       sensores em tempo real (JSON)
       GET  /info         IP, MAC, rede, RSSI, hora, quedas do Wi-Fi (JSON)
       GET  /log          log recente (RAM)
       GET  /log/arquivo  log COMPLETO gravado na flash
       POST /start  /stop  /calibrar        atuador (motores)
       GET|POST /param  POST /salvar  POST /padrao   ajustes
       POST /wifi (ssid, senha)  POST /hora (t)  POST /log/apagar
       ex.: curl http://IP/status
            curl -X POST http://IP/start

     SOCKET TCP:  telnet IP 3333   (ou: nc IP 3333)
       INFO, SENSORES, STATUS, LIGAR, DESLIGAR, CALIBRAR, STREAM, LOG [n],
       AJUDA, SAIR

     LOG PERSISTENTE: /log.txt na flash (LittleFS), niveis INFO / AVISO /
       ERRO, com data e hora. Sobrevive a reinicios e a falta de energia.
       Use a particao padrao "Default 4MB with spiffs" (Ferramentas >
       Partition Scheme).

   COMO USAR
     1) Ligue a chave liga/desliga. O robo entra na rede externa e tambem
        cria a rede propria "SeguidorYata" (senha seguidor123).
     2) Abra a pagina: pelo IP mostrado na secao "Rede" (rede externa) ou
        em http://192.168.4.1 (rede propria). No PC: http://seguidor.local
     3) Coloque o robo sobre a linha e toque em CALIBRAR: ele gira sozinho
        ~4 s. Deixe os dois sensores passarem pelo preto e pelo branco.
        A calibracao fica salva na memoria (so precisa refazer se mudar a
        pista ou a iluminacao).
     4) Coloque na pista e toque em LIGAR. DESLIGAR para parar.
        DESLIGAR durante a calibracao cancela a calibracao.
     5) Em AJUSTES, mude PID e velocidades e toque em APLICAR (vale na hora,
        mesmo com o robo andando). SALVAR grava na memoria para o proximo
        boot; PADRAO volta aos valores de fabrica deste codigo.
     6) Para carregar: desligue a chave e encoste o cabo magnetico no
        TP4056. Luz vermelha = carregando, azul/verde = cheia.
     7) A pagina mostra os SENSORES ao vivo e um LOG com tudo o que o robo
        fez (start/stop, calibracao, linha perdida, motivo de reinicio).

   CHAO QUE NAO E BRANCO
     Nao precisa ser branco: precisa ter CONTRASTE com a linha. A calibracao
     aprende o valor do seu chao e o da sua linha. Calibre sempre no chao
     onde o robo vai andar. Se a linha for MAIS CLARA que o chao, desmarque
     "Linha mais escura que o chao" na pagina. O log avisa se o contraste
     ficou baixo.

   FORCA / VELOCIDADE DO MOTOR (na pagina, em Ajustes)
     Velocidade base -> forca normal nas retas (suba de 50 em 50)
     Velocidade maxima -> teto nas curvas (ate 1023)
     PWM minimo -> suba se as rodas nao arrancam em velocidade baixa
     O limite fisico e a tensao do step-up (8 V): nao passe disso.

   AJUSTE FINO PELA SERIAL (digite e ENTER)
     p 0.35   -> Kp          i 0.0005 -> Ki          d 3.5 -> Kd
     v 420    -> velocidade base (0..1023)
     m 900    -> velocidade maxima
     z 130    -> PWM minimo (zona morta do motor)
     r        -> inverte polaridade do sensor (linha branca em fundo preto)
     c        -> calibrar      s -> start/stop      t -> telemetria      ? -> parametros
============================================================================= */

#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <esp_system.h>

// ------------------------- REDE ----------------------------------------------
// 1) Rede Wi-Fi EXTERNA (roteador ou roteador do celular). O robo entra nela
//    como estacao (STA), pega um IP por DHCP e reconecta sozinho se cair.
//    Pode trocar depois pela pagina (secao "Rede"), sem gravar o codigo.
//    Use rede WPA2 comum (roteador do celular serve). Rede com login de
//    usuario (ex.: eduroam) NAO funciona no ESP32.
//    O nome e a senha ficam no arquivo segredos.h (fora do GitHub). Para
//    criar: copie segredos_exemplo.h como segredos.h e preencha. Sem ele o
//    robo so cria a rede propria, e a rede externa e configurada pela pagina.
#if __has_include("segredos.h")
#include "segredos.h"
#endif
#ifndef REDE_SSID
#define REDE_SSID    "NOME_DA_SUA_REDE"
#define REDE_SENHA   "SENHA_DA_REDE"
#endif

// 2) Rede PROPRIA do robo (Access Point), sempre ligada como reserva:
//    se a rede externa cair, ainda da para acessar em http://192.168.4.1
#define AP_SSID      "Seguidor"
#define AP_SENHA     "seguidor123"   // minimo 8 caracteres

#define HOSTNAME     "seguidor"      // http://seguidor.local (no PC)
#define HTTP_PORTA   80              // servidor web (HTTP sobre TCP)
#define TCP_PORTA    3333            // socket TCP de comandos (telnet/nc)

// Fuso horario de Brasilia (UTC-3, sem horario de verao) e servidores NTP
#define FUSO_TZ      "<-03>3"
#define NTP_1        "pool.ntp.org"
#define NTP_2        "a.st1.ntp.br"

// ------------------------- PINOS ---------------------------------------------
#define PIN_QRE_E   34
#define PIN_QRE_D   35

#define PIN_AIN1    26
#define PIN_AIN2    27
#define PIN_PWMA    13
#define PIN_BIN1    32
#define PIN_BIN2    33
#define PIN_PWMB    25

#define PIN_LED      2

// Se um motor girar ao contrario, troque false por true (nao precisa mexer fio)
#define INVERTE_MOTOR_E   false
#define INVERTE_MOTOR_D   false

// true  = trava a roda quando a velocidade e zero (curva mais seca)
// false = roda solta, carrinho desliza mais
#define FREIO_AO_PARAR    true

// ------------------------- PWM -----------------------------------------------
// ATENCAO: o L298N usa transistores Darlington, que chaveiam devagar.
// Acima de ~10 kHz ele perde torque e esquenta muito. 5 kHz e um bom meio termo
// (chia um pouco, mas o motor responde bem). Nao suba isso para 20 kHz.
#define PWM_FREQ    5000         // 5 kHz: limite saudavel para o L298N
#define PWM_RES     10           // 10 bits -> 0..1023
#define PWM_MAX     ((1 << PWM_RES) - 1)
#define CH_A        0
#define CH_B        1

// ------------------------- CONTROLE ------------------------------------------
#define DT_US       2000UL       // periodo de amostragem = 2 ms (500 Hz)
const float DT_S  = DT_US / 1000000.0f;

struct Param {
  float Kp  = 0.35f;     // proporcional
  float Ki  = 0.0005f;   // integral
  float Kd  = 3.5f;      // derivativo (diferenca por amostra, filtrada)
  int   vBase  = 420;    // velocidade de cruzeiro (0..1023)
  int   vMax   = 900;    // teto de PWM
  int   vBusca = 320;    // velocidade ao procurar a linha perdida
  int   pwmMin = 250;    // zona morta: motor TT + queda de ~2 V do L298N. Ajuste
                         // na pagina: o menor valor em que as rodas ainda giram
  bool  linhaEscura = true;  // true = linha mais escura que o chao (preta)
  bool  permiteRe   = true;  // deixa uma roda girar ao contrario em curva forte
} P;

// Copia de P compartilhada com o servidor web (outro nucleo). Protegida por
// muxP: o loop() so usa P; a pagina so le/escreve Pcompart.
Param Pcompart;
volatile bool pNovoWeb = false;          // a pagina mandou parametros novos
portMUX_TYPE muxP = portMUX_INITIALIZER_UNLOCKED;

#define LIMIAR_LINHA   200   // soma normalizada minima p/ considerar que ve a linha
#define ALFA_D         0.30f // filtro do termo derivativo (0..1)
#define LIM_INTEGRAL   8000.0f

// ------------------------- ESTADO --------------------------------------------
Preferences nvs;
int  minE = 4095, maxE = 0, minD = 4095, maxD = 0;
// "volatile": estas variaveis sao lidas pela tarefa do servidor web, que roda
// no outro nucleo do ESP32
volatile bool calibrado  = false;
volatile bool rodando    = false;
volatile bool calibrando = false;
bool telemetria = false;

float integral = 0, erroAnt = 0, dFiltrado = 0;
int   ultimoLado = 0;          // -1 = linha saiu pela esquerda, +1 = pela direita
bool  perdida = false, avisouPerda = false;   // para o log de "linha perdida"
unsigned long tPerdeu = 0;
unsigned long tUltimoCiclo = 0;

// Pedidos vindos da pagina web. O servidor so ANOTA o pedido; quem mexe nos
// motores e sempre o loop(), para o controle nunca ser interrompido no meio.
enum { PED_NADA = 0, PED_START, PED_STOP, PED_CALIBRAR };
volatile uint8_t pedidoWeb = PED_NADA;
portMUX_TYPE muxPed = portMUX_INITIALIZER_UNLOCKED;
void fazerPedido(uint8_t p) {           // chamado pela tarefa web (nucleo 0)
  portENTER_CRITICAL(&muxPed);
  pedidoWeb = p;
  portEXIT_CRITICAL(&muxPed);
}

WebServer  server(HTTP_PORTA);          // servidor HTTP (escuta na porta 80)
WiFiServer tcpServidor(TCP_PORTA);     // servidor de socket TCP (porta 3333)

// Valores ao vivo para a pagina (escritos pelo loop, lidos pelo servidor)
volatile int tBrE = 0, tBrD = 0;     // leitura bruta do ADC (0..4095)
volatile int tPE = 0, tPD = 0;       // normalizado (0 = chao, 1000 = linha)
volatile int tErro = 0, tVE = 0, tVD = 0;

// Estado da rede
String redeSsid, redeSenha;          // rede externa em uso (NVS ou #define)
volatile bool staConectado = false;  // conectado na rede externa?
volatile bool staJaConectou = false;
volatile uint32_t quedasWifi = 0;    // quantas vezes a rede externa caiu
volatile unsigned long tQuedaWifi = 0;
volatile uint32_t tentativasWifi = 0;
volatile bool pedidoReinicio = false;
uint32_t numBoot = 0;                // contador de inicializacoes (NVS)
bool fsOk = false;                   // LittleFS montado?

// ============================= LOG ===========================================
// Cada mensagem tem DATA/HORA e NIVEL (INFO, AVISO, ERRO) e vai para:
//   - a Serial;
//   - um buffer na RAM (as ultimas LOG_LINHAS, para a pagina ao vivo);
//   - o arquivo /log.txt na memoria FLASH (LittleFS), que sobrevive a
//     desligar e reiniciar. Quando passa de LOG_MAX_BYTES, o arquivo vira
//     /log_antigo.txt e um novo comeca.
// A escrita na flash e feita por uma tarefa separada (tarefaFlash), em lotes,
// para nao atrasar o controle dos motores.
#define LOG_LINHAS    40
#define LOG_TAM       160
#define LOG_FILA      32
#define ARQ_LOG       "/log.txt"
#define ARQ_LOG_ANT   "/log_antigo.txt"
#define LOG_MAX_BYTES (64 * 1024)

char logBuf[LOG_LINHAS][LOG_TAM];
volatile uint32_t logSeq = 0;        // total de mensagens ja escritas
volatile uint32_t logDescartadas = 0;
portMUX_TYPE muxLog = portMUX_INITIALIZER_UNLOCKED;
QueueHandle_t filaLog = NULL;        // linhas esperando para ir para a flash

bool horaValida() { return time(nullptr) > 1700000000; }   // ja sincronizou?

// "11/11/2026 22:30:15" ou, antes de saber a hora, "boot+00:01:23"
void carimbo(char *out, size_t n) {
  time_t t = time(nullptr);
  if (t > 1700000000) {
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(out, n, "%d/%m/%Y %H:%M:%S", &tmv);
  } else {
    unsigned long s = millis() / 1000;
    snprintf(out, n, "boot+%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  }
}

// niveis do log (uint8_t em vez de enum por causa dos prototipos que a IDE
// do Arduino gera automaticamente no topo do arquivo)
#define NV_INFO   0
#define NV_AVISO  1
#define NV_ERRO   2

void logV(uint8_t nivel, const char *fmt, va_list ap) {
  static const char *NOME[] = { "INFO ", "AVISO", "ERRO " };
  char linha[LOG_TAM];
  char quando[24];
  carimbo(quando, sizeof(quando));
  int n = snprintf(linha, sizeof(linha), "%s %s ", quando, NOME[nivel]);
  vsnprintf(linha + n, sizeof(linha) - n, fmt, ap);
  Serial.println(linha);

  portENTER_CRITICAL(&muxLog);
  memcpy(logBuf[logSeq % LOG_LINHAS], linha, LOG_TAM);
  logSeq = logSeq + 1;
  portEXIT_CRITICAL(&muxLog);

  if (filaLog && xQueueSend(filaLog, linha, 0) != pdTRUE) logDescartadas = logDescartadas + 1;
}

// Uso igual ao printf: logI("vBase=%d", P.vBase);
void logI(const char *fmt, ...) { va_list ap; va_start(ap, fmt); logV(NV_INFO,  fmt, ap); va_end(ap); }
void logA(const char *fmt, ...) { va_list ap; va_start(ap, fmt); logV(NV_AVISO, fmt, ap); va_end(ap); }
void logE(const char *fmt, ...) { va_list ap; va_start(ap, fmt); logV(NV_ERRO,  fmt, ap); va_end(ap); }

// Tarefa que grava o log na flash (nucleo 0, junto com o Wi-Fi)
void tarefaFlash(void *) {
  char linha[LOG_TAM];
  for (;;) {
    if (xQueueReceive(filaLog, linha, portMAX_DELAY) != pdTRUE) continue;
    vTaskDelay(pdMS_TO_TICKS(300));            // junta mais linhas num lote so
    File f = LittleFS.open(ARQ_LOG, FILE_APPEND);
    if (!f) continue;
    do {
      f.print(linha);
      f.print('\n');
    } while (xQueueReceive(filaLog, linha, 0) == pdTRUE);
    if (logDescartadas) {
      f.printf("(%lu linhas de log descartadas: fila cheia)\n", (unsigned long)logDescartadas);
      logDescartadas = 0;
    }
    size_t tam = f.size();
    f.close();
    if (tam > LOG_MAX_BYTES) {                 // rotacao do arquivo
      LittleFS.remove(ARQ_LOG_ANT);
      LittleFS.rename(ARQ_LOG, ARQ_LOG_ANT);
    }
  }
}

// ============================= PWM / MOTORES =================================
static inline void pwmSetup(uint8_t pino, uint8_t canal) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)canal;
  ledcAttach(pino, PWM_FREQ, PWM_RES);
#else
  ledcSetup(canal, PWM_FREQ, PWM_RES);
  ledcAttachPin(pino, canal);
#endif
}

static inline void pwmWrite(uint8_t pino, uint8_t canal, uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)canal;
  ledcWrite(pino, duty);
#else
  (void)pino;
  ledcWrite(canal, duty);
#endif
}

// vel: -1023 (re) .. 0 (freio) .. +1023 (frente)
void motor(bool esquerdo, int vel) {
  uint8_t in1 = esquerdo ? PIN_AIN1 : PIN_BIN1;
  uint8_t in2 = esquerdo ? PIN_AIN2 : PIN_BIN2;
  uint8_t pwm = esquerdo ? PIN_PWMA : PIN_PWMB;
  uint8_t ch  = esquerdo ? CH_A     : CH_B;
  bool inverte = esquerdo ? INVERTE_MOTOR_E : INVERTE_MOTOR_D;

  vel = constrain(vel, -PWM_MAX, PWM_MAX);
  if (inverte) vel = -vel;

  int mag = abs(vel);
  if (mag == 0) {
    // No L298N o freio ("fast motor stop") e IN1 = IN2 com o ENABLE ALTO.
    // Com o enable em 0 o motor fica livre (roda por inercia).
    digitalWrite(in1, HIGH);
    digitalWrite(in2, HIGH);
    pwmWrite(pwm, ch, FREIO_AO_PARAR ? PWM_MAX : 0);
    return;
  }
  // compensa a zona morta do motor + queda de tensao do L298N
  mag = map(mag, 1, PWM_MAX, P.pwmMin, PWM_MAX);

  if (vel > 0) { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); }
  else         { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
  pwmWrite(pwm, ch, mag);
}

void motores(int vE, int vD) { motor(true, vE); motor(false, vD); }
void pararMotores()          { motores(0, 0); }

// Corta o PWM sem frear: o motor desacelera solto, sem pico de corrente.
void soltarMotores() {
  pwmWrite(PIN_PWMA, CH_A, 0);
  pwmWrite(PIN_PWMB, CH_B, 0);
}

// liga/desliga a corrida (usado pela pagina web e pela Serial)
void setRodando(bool r) {
  if (r && !calibrado) {
    logA("Pedido de LIGAR recusado: calibre antes");
    return;
  }
  rodando = r;
  integral = 0; erroAnt = 0; dFiltrado = 0;
  if (!rodando) { pararMotores(); tErro = 0; tVE = 0; tVD = 0; }
  if (rodando) logI("Motores LIGADOS (base=%d max=%d)", P.vBase, P.vMax);
  else         logI("Motores DESLIGADOS");
}

// ============================= SENSORES ======================================
int lerBruto(uint8_t pino) {
  int s = 0;
  for (int i = 0; i < 4; i++) s += analogRead(pino);
  return s >> 2;
}

// devolve 0 (fora da linha) .. 1000 (em cima da linha)
int normaliza(int bruto, int mn, int mx) {
  if (mx - mn < 150) return 0;                 // calibracao ruim
  int v = map(constrain(bruto, mn, mx), mn, mx, 0, 1000);
  // QRE sobre preto reflete menos -> tensao MAIOR na saida analogica
  return P.linhaEscura ? v : (1000 - v);
}

void lerSensores(int &pE, int &pD) {
  int bE = lerBruto(PIN_QRE_E), bD = lerBruto(PIN_QRE_D);
  pE = normaliza(bE, minE, maxE);
  pD = normaliza(bD, minD, maxD);
  tBrE = bE; tBrD = bD; tPE = pE; tPD = pD;
}

// ============================= CALIBRACAO ====================================
void salvarCalibracao() {
  nvs.begin("seguidor", false);
  nvs.putInt("minE", minE); nvs.putInt("maxE", maxE);
  nvs.putInt("minD", minD); nvs.putInt("maxD", maxD);
  nvs.putBool("ok", true);
  nvs.end();
}

void carregarCalibracao() {
  nvs.begin("seguidor", true);
  calibrado = nvs.getBool("ok", false);
  if (calibrado) {
    minE = nvs.getInt("minE", 0); maxE = nvs.getInt("maxE", 4095);
    minD = nvs.getInt("minD", 0); maxD = nvs.getInt("maxD", 4095);
  }
  nvs.end();
}

void calibrar() {
  logI("CALIBRANDO: os sensores precisam passar pela linha e pelo chao...");
  calibrando = true;
  minE = minD = 4095; maxE = maxD = 0;

  unsigned long t0 = millis();
  while (millis() - t0 < 4000) {
    // DESLIGAR na pagina web cancela a calibracao na hora
    if (pedidoWeb == PED_STOP) {
      pedidoWeb = PED_NADA;
      pararMotores();
      digitalWrite(PIN_LED, LOW);
      carregarCalibracao();          // volta para a calibracao salva (se houver)
      calibrando = false;
      logA("Calibracao CANCELADA pelo usuario");
      return;
    }

    // varre girando no proprio eixo, alternando o sentido a cada 0,5 s.
    // Nos primeiros 120 ms de cada meia-volta o motor fica SOLTO: inverter
    // o sentido de uma vez puxa um pico de corrente que derruba a tensao
    // (e o Wi-Fi/ESP32 caiam durante a calibracao).
    unsigned long dt = millis() - t0;
    int sentido = ((dt / 500) % 2 == 0) ? 1 : -1;
    if (dt >= 500 && (dt % 500) < 120) soltarMotores();
    else                               motores(sentido * 300, -sentido * 300);

    int a = lerBruto(PIN_QRE_E), b = lerBruto(PIN_QRE_D);
    minE = min(minE, a); maxE = max(maxE, a);
    minD = min(minD, b); maxD = max(maxD, b);
    tBrE = a; tBrD = b;                // a pagina mostra ao vivo

    digitalWrite(PIN_LED, (millis() / 100) % 2);
    delay(5);
  }
  pararMotores();
  digitalWrite(PIN_LED, LOW);

  int cE = maxE - minE, cD = maxD - minD;   // contraste linha x chao
  bool ok = (cE > 150) && (cD > 150);
  if (ok) {
    calibrado = true;
    salvarCalibracao();
    logI("Calibracao OK  E[%d..%d]  D[%d..%d]  contraste E=%d D=%d",
         minE, maxE, minD, maxD, cE, cD);
    if (cE < 600 || cD < 600)
      logA("Contraste baixo. Abaixe os sensores (3-5 mm) ou use linha mais escura/chao mais claro.");
  } else {
    logE("Calibracao FALHOU: contraste E=%d D=%d (minimo 150).", cE, cD);
    logA("Os dois sensores passaram pela linha? Altura 3-5 mm do chao?");
    carregarCalibracao();          // volta para a calibracao salva (se houver)
    if (calibrado) logI("Mantida a calibracao anterior.");
  }
  calibrando = false;
}

// ============================= PARAMETROS ====================================
void mostrarParametros();
// Grava/le os parametros em um namespace separado da calibracao
void salvarParam(const Param &q) {
  Preferences pref;
  pref.begin("param", false);
  pref.putFloat("Kp", q.Kp);  pref.putFloat("Ki", q.Ki);  pref.putFloat("Kd", q.Kd);
  pref.putInt("vBase", q.vBase);   pref.putInt("vMax", q.vMax);
  pref.putInt("vBusca", q.vBusca); pref.putInt("pwmMin", q.pwmMin);
  pref.putBool("escura", q.linhaEscura); pref.putBool("re", q.permiteRe);
  pref.putBool("ok", true);
  pref.end();
}

void carregarParam() {
  Preferences pref;
  pref.begin("param", true);
  if (pref.getBool("ok", false)) {
    P.Kp = pref.getFloat("Kp", P.Kp);  P.Ki = pref.getFloat("Ki", P.Ki);  P.Kd = pref.getFloat("Kd", P.Kd);
    P.vBase  = pref.getInt("vBase", P.vBase);   P.vMax   = pref.getInt("vMax", P.vMax);
    P.vBusca = pref.getInt("vBusca", P.vBusca); P.pwmMin = pref.getInt("pwmMin", P.pwmMin);
    P.linhaEscura = pref.getBool("escura", P.linhaEscura);
    P.permiteRe   = pref.getBool("re", P.permiteRe);
    logI("Ajustes carregados da memoria.");
  }
  pref.end();
}

// loop() -> pagina: publica os valores atuais de P
void publicarParam() {
  portENTER_CRITICAL(&muxP);
  Pcompart = P;
  portEXIT_CRITICAL(&muxP);
}

// pagina -> loop(): aplica os valores que a pagina mandou
void aplicarParamWeb() {
  if (!pNovoWeb) return;
  portENTER_CRITICAL(&muxP);
  bool kiMudou = (Pcompart.Ki != P.Ki);
  P = Pcompart;
  pNovoWeb = false;
  portEXIT_CRITICAL(&muxP);
  if (kiMudou) integral = 0;
  logI("Ajustes recebidos pela rede e aplicados:");
  mostrarParametros();
}

// ============================= SERIAL ========================================
void mostrarParametros() {
  logI("Kp=%.3f Ki=%.5f Kd=%.2f base=%d max=%d busca=%d pwmMin=%d linhaEscura=%d calib=%d",
         P.Kp, P.Ki, P.Kd, P.vBase, P.vMax, P.vBusca, P.pwmMin, (int)P.linhaEscura, (int)calibrado);
}

void tratarSerial() {
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n');
  s.trim();
  if (!s.length()) return;
  char c = s.charAt(0);
  float v = s.substring(1).toFloat();

  switch (c) {
    case 'p': P.Kp = v; break;
    case 'i': P.Ki = v; integral = 0; break;
    case 'd': P.Kd = v; break;
    case 'v': P.vBase  = constrain((int)v, 0, PWM_MAX); break;
    case 'm': P.vMax   = constrain((int)v, 0, PWM_MAX); break;
    case 'z': P.pwmMin = constrain((int)v, 0, 400); break;
    case 'r': P.linhaEscura = !P.linhaEscura; break;
    case 'c': setRodando(false); calibrar(); return;
    case 's': setRodando(!rodando); return;
    case 't': telemetria = !telemetria; return;
    default: break;
  }
  publicarParam();      // a pagina web passa a mostrar os valores novos
  mostrarParametros();
}

// ============================= SERVIDOR WEB ==================================
const char PAGINA[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Seguidor Yata</title>
<style>
body{font-family:sans-serif;max-width:420px;margin:0 auto;padding:16px;background:#111;color:#eee;text-align:center}
h1{font-size:22px;font-weight:500}
#st{font-size:20px;padding:14px;border-radius:10px;background:#444;margin:16px 0}
button{width:100%;font-size:22px;padding:18px;margin:8px 0;border:0;border-radius:10px;color:#fff}
.on{background:#1d9e75}.off{background:#e24b4a}.cal{background:#534ab7}
#msg{min-height:1.4em;color:#fac775}
h2{font-size:18px;font-weight:500;margin:28px 0 8px;border-top:1px solid #333;padding-top:18px}
.row{display:flex;justify-content:space-between;align-items:center;margin:8px 0;text-align:left}
.row small{color:#999;display:block;font-size:12px}
input[type=number]{width:110px;font-size:18px;padding:8px;border-radius:8px;border:1px solid #555;background:#222;color:#eee}
input[type=checkbox]{width:24px;height:24px}
.aj button{font-size:18px;padding:13px}
.apl{background:#185fa5}.sec{background:#5f5e5a}
.sen{display:flex;align-items:center;gap:8px;margin:8px 0;text-align:left}
.sen span{width:18px}
.bar{flex:1;height:20px;background:#333;border-radius:10px;overflow:hidden}
.bar i{display:block;height:100%;width:0;background:#1d9e75;transition:width .2s}
.sen b{width:120px;text-align:right;font:13px monospace}
.mono{font:13px monospace;color:#bbb;margin:6px 0}
#log{text-align:left;background:#000;color:#9fe1cb;font:12px/1.4 monospace;height:280px;overflow:auto;padding:8px;border-radius:8px;word-break:break-word}
#log .la{color:#fac775}#log .le{color:#ff8a80;font-weight:bold}
.peq{font-size:15px;padding:10px}
a.bt{display:block;font-size:15px;padding:10px;margin:8px 0;border-radius:10px;color:#fff;background:#185fa5;text-decoration:none}
table{width:100%;border-collapse:collapse;font-size:14px;text-align:left}
td{padding:5px 4px;border-bottom:1px solid #2a2a2a}td:first-child{color:#999;width:38%}
td:last-child{font-family:monospace;word-break:break-all}
input[type=text],input[type=password]{width:100%;box-sizing:border-box;font-size:16px;padding:9px;margin:4px 0;border-radius:8px;border:1px solid #555;background:#222;color:#eee}
.ok{color:#5dcaa5}.ruim{color:#ff8a80}
</style></head><body>
<h1>Seguidor Yata</h1>
<div id="st">conectando...</div>
<button class="on"  onclick="cmd('start')">LIGAR</button>
<button class="off" onclick="cmd('stop')">DESLIGAR</button>
<button class="cal" onclick="cmd('calibrar')">CALIBRAR</button>
<div id="msg"></div>
<h2>Sensores ao vivo</h2>
<div class="sen"><span>E</span><div class="bar"><i id="bE"></i></div><b id="nE">-</b></div>
<div class="sen"><span>D</span><div class="bar"><i id="bD"></i></div><b id="nD">-</b></div>
<div class="mono" id="tl">-</div>
<div class="mono" id="cb">-</div>
<h2>Rede</h2>
<table>
<tr><td>Rede externa</td><td id="rSta">-</td></tr>
<tr><td>IP (rede externa)</td><td id="rIp">-</td></tr>
<tr><td>Sinal (RSSI)</td><td id="rRssi">-</td></tr>
<tr><td>MAC (estacao)</td><td id="rMac">-</td></tr>
<tr><td>Rede propria (AP)</td><td id="rAp">-</td></tr>
<tr><td>MAC (AP)</td><td id="rMacAp">-</td></tr>
<tr><td>Portas</td><td id="rPortas">-</td></tr>
<tr><td>Quedas do Wi-Fi</td><td id="rQuedas">-</td></tr>
<tr><td>Data e hora</td><td id="rHora">-</td></tr>
<tr><td>Ligado ha</td><td id="rUp">-</td></tr>
<tr><td>Inicializacao</td><td id="rBoot">-</td></tr>
<tr><td>Flash (log)</td><td id="rFs">-</td></tr>
</table>
<details style="text-align:left;margin-top:10px"><summary>Trocar a rede Wi-Fi externa</summary>
<input type="text" id="wS" placeholder="nome da rede (SSID)">
<input type="password" id="wP" placeholder="senha">
<button class="sec peq" onclick="salvarWifi()">SALVAR E REINICIAR</button>
</details>
<h2>Log do sistema</h2>
<div id="log"></div>
<button class="sec peq" onclick="$('log').innerHTML=''">LIMPAR TELA</button>
<a class="bt" href="/log/arquivo" download="log_seguidor.txt">BAIXAR LOG COMPLETO (FLASH)</a>
<button class="sec peq" onclick="apagarLog()">APAGAR LOG DA FLASH</button>
<h2>Ajustes</h2>
<div class="row"><label>Kp<small>proporcional</small></label><input type="number" id="Kp" step="any" min="0"></div>
<div class="row"><label>Ki<small>integral</small></label><input type="number" id="Ki" step="any" min="0"></div>
<div class="row"><label>Kd<small>derivativo</small></label><input type="number" id="Kd" step="any" min="0"></div>
<div class="row"><label>Velocidade base<small>forca nas retas, 0 a 1023</small></label><input type="number" id="vBase" min="0" max="1023"></div>
<div class="row"><label>Velocidade maxima<small>teto nas curvas, 0 a 1023</small></label><input type="number" id="vMax" min="0" max="1023"></div>
<div class="row"><label>Velocidade de busca<small>linha perdida</small></label><input type="number" id="vBusca" min="0" max="1023"></div>
<div class="row"><label>PWM minimo<small>suba se a roda nao arranca, 0 a 400</small></label><input type="number" id="pwmMin" min="0" max="400"></div>
<div class="row"><label for="esc">Linha mais escura que o chao<small>desmarque se a linha for mais clara</small></label><input type="checkbox" id="esc"></div>
<div class="row"><label for="re">Permitir re nas curvas</label><input type="checkbox" id="re"></div>
<div class="aj">
<button class="apl" onclick="aplicar()">APLICAR</button>
<button class="sec" onclick="salvar()">SALVAR NA MEMORIA</button>
<button class="sec" onclick="padrao()">RESTAURAR PADRAO</button>
</div>
<script>
const $=id=>document.getElementById(id);
const st=$('st'),msg=$('msg'),logEl=$('log');
function cmd(c){
  fetch('/'+c,{method:'POST'}).then(r=>r.text()).then(t=>{msg.textContent=t;atualiza();})
  .catch(()=>{msg.textContent='Sem conexao com o robo';});
}
let seq=0,lendoLog=false,ocupado=false;
function lerLog(){
  if(lendoLog)return;lendoLog=true;
  fetch('/log?d='+seq).then(r=>r.text()).then(t=>{
    const l=t.split('\n');const fim=+l.shift();
    const fundo=logEl.scrollTop+logEl.clientHeight>=logEl.scrollHeight-30;
    if(fim<seq)addLinha('--- robo reiniciou ---');
    l.filter(x=>x.length).forEach(addLinha);
    while(logEl.childNodes.length>300)logEl.removeChild(logEl.firstChild);
    if(fundo)logEl.scrollTop=logEl.scrollHeight;
    seq=fim;
  }).catch(()=>{}).finally(()=>{lendoLog=false;});
}
function addLinha(x){
  const d=document.createElement('div');d.textContent=x;
  if(x.indexOf(' ERRO ')>0)d.className='le';else if(x.indexOf(' AVISO ')>0)d.className='la';
  logEl.appendChild(d);
}
function apagarLog(){
  if(!confirm('Apagar todo o log gravado na flash?'))return;
  fetch('/log/apagar',{method:'POST'}).then(r=>r.text()).then(t=>msg.textContent=t).catch(erro);
}
function hms(s){const h=Math.floor(s/3600),m=Math.floor(s/60)%60;return h+'h '+m+'min '+(s%60)+'s';}
let horaEnviada=false;
function info(){
  fetch('/info').then(r=>r.json()).then(i=>{
    const rs=$('rSta');
    if(!i.sta_ssid){rs.textContent='nao configurada';}
    else{rs.innerHTML=i.sta_conectado?'<span class="ok">CONECTADO</span> em ':'<span class="ruim">DESCONECTADO</span> - ';
      rs.append(i.sta_ssid);}
    $('rIp').textContent=i.sta_conectado?i.sta_ip+'  (gw '+i.gateway+')':'-';
    $('rRssi').textContent=i.sta_conectado?i.rssi+' dBm':'-';
    $('rMac').textContent=i.mac_sta;$('rMacAp').textContent=i.mac_ap;
    $('rAp').textContent=i.ap_ssid+'  '+i.ap_ip+'  ('+i.ap_clientes+' cliente(s))';
    $('rPortas').textContent='HTTP '+i.http_porta+'  |  TCP '+i.tcp_porta+(i.tcp_cliente?' (cliente: '+i.tcp_cliente+')':'');
    $('rQuedas').textContent=i.quedas_wifi;
    $('rHora').textContent=i.hora+(i.hora_ok?'':'  (sem hora)');
    $('rUp').textContent=hms(i.uptime_s);
    $('rBoot').textContent='#'+i.boot+' - '+i.motivo_reinicio;
    $('rFs').textContent=i.fs_ok?(Math.round(i.fs_log/1024)+' kB de log | '+Math.round(i.fs_usado/1024)+'/'+Math.round(i.fs_total/1024)+' kB'):'ERRO na flash';
    if(!i.hora_ok&&!horaEnviada){horaEnviada=true;
      fetch('/hora',{method:'POST',body:new URLSearchParams({t:Math.floor(Date.now()/1000)})});}
  }).catch(()=>{});
}
setInterval(info,2000);info();
function salvarWifi(){
  const s=$('wS').value.trim();
  if(!s){msg.textContent='Digite o nome da rede';return;}
  if(!confirm('Salvar a rede "'+s+'" e reiniciar o robo?'))return;
  fetch('/wifi',{method:'POST',body:new URLSearchParams({ssid:s,senha:$('wP').value})})
  .then(r=>r.text()).then(t=>msg.textContent=t).catch(erro);
}
function atualiza(){
  if(ocupado)return;ocupado=true;
  fetch('/status').then(r=>r.json()).then(s=>{
    if(s.calibrando){st.textContent='CALIBRANDO...';st.style.background='#534ab7';}
    else if(s.rodando){st.textContent='RODANDO';st.style.background='#1d9e75';}
    else{st.textContent=s.calibrado?'PARADO':'PARADO (sem calibracao)';st.style.background='#444';}
    $('bE').style.width=(s.pE/10)+'%';$('bD').style.width=(s.pD/10)+'%';
    $('nE').textContent=s.pE+' ('+s.brE+')';$('nD').textContent=s.pD+' ('+s.brD+')';
    $('tl').textContent='erro '+s.erro+'  |  motor E '+s.vE+'  |  motor D '+s.vD;
    $('cb').textContent=s.calibrado?('calib E['+s.minE+'..'+s.maxE+']  D['+s.minD+'..'+s.maxD+']'):'sem calibracao';
    if(s.log!==seq)lerLog();
  }).catch(()=>{st.textContent='Sem conexao';st.style.background='#633806';})
  .finally(()=>{ocupado=false;});
}
setInterval(atualiza,400);atualiza();
const campos=['Kp','Ki','Kd','vBase','vMax','vBusca','pwmMin'];
function erro(){msg.textContent='Sem conexao com o robo';}
function carregaParam(){
  return fetch('/param').then(r=>r.json()).then(p=>{
    campos.forEach(k=>$(k).value=p[k]);$('esc').checked=p.linhaEscura;$('re').checked=p.permiteRe;
  });
}
function aplicar(){
  const b=new URLSearchParams();
  campos.forEach(k=>b.append(k,$(k).value));
  b.append('linhaEscura',$('esc').checked?1:0);b.append('permiteRe',$('re').checked?1:0);
  return fetch('/param',{method:'POST',body:b}).then(r=>r.text()).then(t=>{msg.textContent=t;return carregaParam();}).catch(erro);
}
function salvar(){
  aplicar().then(()=>fetch('/salvar',{method:'POST'})).then(r=>r.text()).then(t=>msg.textContent=t).catch(erro);
}
function padrao(){
  if(!confirm('Voltar todos os ajustes para o padrao?'))return;
  fetch('/padrao',{method:'POST'}).then(r=>r.text()).then(t=>{msg.textContent=t;carregaParam();}).catch(erro);
}
carregaParam().catch(erro);
</script></body></html>)rawliteral";

void webPagina() { server.send(200, "text/html", PAGINA); }

void webStatus() {
  char json[320];
  snprintf(json, sizeof(json),
           "{\"rodando\":%s,\"calibrado\":%s,\"calibrando\":%s,"
           "\"brE\":%d,\"brD\":%d,\"pE\":%d,\"pD\":%d,\"erro\":%d,\"vE\":%d,\"vD\":%d,"
           "\"minE\":%d,\"maxE\":%d,\"minD\":%d,\"maxD\":%d,\"log\":%lu}",
           rodando ? "true" : "false",
           calibrado ? "true" : "false",
           calibrando ? "true" : "false",
           (int)tBrE, (int)tBrD, (int)tPE, (int)tPD, (int)tErro, (int)tVE, (int)tVD,
           minE, maxE, minD, maxD, (unsigned long)logSeq);
  server.send(200, "application/json", json);
}

// Devolve as mensagens do log a partir da numero "d".
// 1a linha = total de mensagens (a pagina usa para pedir so as novas).
void webLog() {
  static char copia[LOG_LINHAS][LOG_TAM];   // so a tarefa web usa
  uint32_t desde = server.hasArg("d") ? strtoul(server.arg("d").c_str(), NULL, 10) : 0;
  uint32_t fim;
  portENTER_CRITICAL(&muxLog);
  fim = logSeq;
  memcpy(copia, logBuf, sizeof(logBuf));
  portEXIT_CRITICAL(&muxLog);

  uint32_t ini = (fim > LOG_LINHAS) ? fim - LOG_LINHAS : 0;   // mais antiga guardada
  if (desde > ini && desde <= fim) ini = desde;
  String r;
  r.reserve(16 + (fim - ini) * 64);
  r += String((unsigned long)fim);
  r += '\n';
  for (uint32_t i = ini; i < fim; i++) {
    r += copia[i % LOG_LINHAS];
    r += '\n';
  }
  server.send(200, "text/plain", r);
}

// ----- comandos do atuador (motores). Usados pelo HTTP e pelo socket TCP -----
// Devolvem o codigo HTTP (200 = aceito, 409 = recusado) e a mensagem.
int pedirLigar(const char **msg) {
  if (calibrando) { *msg = "Aguarde a calibracao terminar"; return 409; }
  if (!calibrado) { *msg = "Calibre primeiro";             return 409; }
  fazerPedido(PED_START);
  *msg = "Ligado";
  return 200;
}

int pedirDesligar(const char **msg) {
  *msg = calibrando ? "Calibracao cancelada" : "Desligado";
  fazerPedido(PED_STOP);
  return 200;
}

int pedirCalibrar(const char **msg) {
  if (calibrando) { *msg = "Ja esta calibrando"; return 409; }
  fazerPedido(PED_CALIBRAR);
  *msg = "Calibrando: deixe o robo sobre a linha";
  return 200;
}

// IP:porta de quem mandou a requisicao HTTP (para o log)
String quemHttp() {
  return server.client().remoteIP().toString();
}

void webStart() {
  const char *m; int c = pedirLigar(&m);
  logI("HTTP %s: comando LIGAR -> %s", quemHttp().c_str(), m);
  server.send(c, "text/plain", m);
}

void webStop() {
  const char *m; int c = pedirDesligar(&m);
  logI("HTTP %s: comando DESLIGAR -> %s", quemHttp().c_str(), m);
  server.send(c, "text/plain", m);
}

void webCalibrar() {
  const char *m; int c = pedirCalibrar(&m);
  logI("HTTP %s: comando CALIBRAR -> %s", quemHttp().c_str(), m);
  server.send(c, "text/plain", m);
}

// le um campo do formulario; mantem o valor atual se vier vazio ou invalido
static void lerCampo(const char *nome, float &dest, float lo, float hi) {
  if (!server.hasArg(nome)) return;
  String s = server.arg(nome);
  s.trim();
  s.replace(',', '.');
  if (!s.length()) return;
  float v = s.toFloat();
  if (isnan(v) || isinf(v)) return;
  dest = constrain(v, lo, hi);
}

static void lerCampo(const char *nome, int &dest, int lo, int hi) {
  float f = dest;
  lerCampo(nome, f, (float)lo, (float)hi);
  dest = (int)lroundf(f);
}

void webParamGet() {
  Param q;
  portENTER_CRITICAL(&muxP);
  q = Pcompart;
  portEXIT_CRITICAL(&muxP);

  char json[256];
  snprintf(json, sizeof(json),
           "{\"Kp\":%.4f,\"Ki\":%.6f,\"Kd\":%.4f,\"vBase\":%d,\"vMax\":%d,"
           "\"vBusca\":%d,\"pwmMin\":%d,\"linhaEscura\":%s,\"permiteRe\":%s}",
           q.Kp, q.Ki, q.Kd, q.vBase, q.vMax, q.vBusca, q.pwmMin,
           q.linhaEscura ? "true" : "false", q.permiteRe ? "true" : "false");
  server.send(200, "application/json", json);
}

void webParamPost() {
  Param q;
  portENTER_CRITICAL(&muxP);
  q = Pcompart;
  portEXIT_CRITICAL(&muxP);

  lerCampo("Kp",     q.Kp,     0.0f, 100.0f);
  lerCampo("Ki",     q.Ki,     0.0f, 10.0f);
  lerCampo("Kd",     q.Kd,     0.0f, 1000.0f);
  lerCampo("vBase",  q.vBase,  0, PWM_MAX);
  lerCampo("vMax",   q.vMax,   0, PWM_MAX);
  lerCampo("vBusca", q.vBusca, 0, PWM_MAX);
  lerCampo("pwmMin", q.pwmMin, 0, 400);
  if (server.hasArg("linhaEscura")) q.linhaEscura = (server.arg("linhaEscura") == "1");
  if (server.hasArg("permiteRe"))   q.permiteRe   = (server.arg("permiteRe") == "1");

  portENTER_CRITICAL(&muxP);
  Pcompart = q;
  pNovoWeb = true;
  portEXIT_CRITICAL(&muxP);
  server.send(200, "text/plain", "Ajustes aplicados (ainda nao salvos)");
}

void webSalvar() {
  Param q;
  portENTER_CRITICAL(&muxP);
  q = Pcompart;
  portEXIT_CRITICAL(&muxP);
  salvarParam(q);
  server.send(200, "text/plain", "Ajustes salvos na memoria");
}

void webPadrao() {
  Param q;                      // valores padrao definidos na struct Param
  portENTER_CRITICAL(&muxP);
  Pcompart = q;
  pNovoWeb = true;
  portEXIT_CRITICAL(&muxP);

  Preferences pref;             // apaga os ajustes salvos
  pref.begin("param", false);
  pref.clear();
  pref.end();
  server.send(200, "text/plain", "Padrao restaurado");
}

// ----- informacoes de rede: IP, MAC, status da conexao, hora -----
const char *motivoReinicio();
WiFiClient tcpCli;                       // cliente TCP atual (1 por vez)
String tcpCliNome;                       // "ip:porta" do cliente TCP

// copia texto para JSON trocando aspas/barras (nome de rede pode ter)
String jsonTexto(const String &s) {
  String r;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') r += '\\';
    if ((uint8_t)c >= 32) r += c;
  }
  return r;
}

void webInfo() {
  char hora[24];
  carimbo(hora, sizeof(hora));
  size_t tamLog = 0;
  if (fsOk) {
    File f = LittleFS.open(ARQ_LOG, FILE_READ);
    if (f) { tamLog = f.size(); f.close(); }
  }
  bool sta = staConectado;
  String j;
  j.reserve(700);
  j  = "{\"sta_ssid\":\"" + jsonTexto(redeSsid) + "\"";
  j += ",\"sta_conectado\":"; j += sta ? "true" : "false";
  j += ",\"sta_ip\":\"" + (sta ? WiFi.localIP().toString() : String("")) + "\"";
  j += ",\"gateway\":\"" + (sta ? WiFi.gatewayIP().toString() : String("")) + "\"";
  j += ",\"mascara\":\"" + (sta ? WiFi.subnetMask().toString() : String("")) + "\"";
  j += ",\"rssi\":" + String(sta ? WiFi.RSSI() : 0);
  j += ",\"mac_sta\":\"" + WiFi.macAddress() + "\"";
  j += ",\"ap_ssid\":\"" AP_SSID "\"";
  j += ",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
  j += ",\"mac_ap\":\"" + WiFi.softAPmacAddress() + "\"";
  j += ",\"ap_clientes\":" + String(WiFi.softAPgetStationNum());
  j += ",\"hostname\":\"" HOSTNAME ".local\"";
  j += ",\"http_porta\":" + String(HTTP_PORTA);
  j += ",\"tcp_porta\":" + String(TCP_PORTA);
  j += ",\"tcp_cliente\":\"" + (tcpCli.connected() ? tcpCliNome : String("")) + "\"";
  j += ",\"quedas_wifi\":" + String((unsigned long)quedasWifi);
  j += ",\"hora\":\"" + String(hora) + "\"";
  j += ",\"hora_ok\":"; j += horaValida() ? "true" : "false";
  j += ",\"uptime_s\":" + String(millis() / 1000);
  j += ",\"boot\":" + String((unsigned long)numBoot);
  j += ",\"motivo_reinicio\":\"" + String(motivoReinicio()) + "\"";
  j += ",\"fs_ok\":"; j += fsOk ? "true" : "false";
  j += ",\"fs_log\":" + String((unsigned long)tamLog);
  j += ",\"fs_usado\":" + String((unsigned long)(fsOk ? LittleFS.usedBytes() : 0));
  j += ",\"fs_total\":" + String((unsigned long)(fsOk ? LittleFS.totalBytes() : 0));
  j += "}";
  server.send(200, "application/json", j);
}

// O navegador manda a hora dele quando o robo nao tem internet para o NTP
void webHora() {
  if (!server.hasArg("t")) { server.send(400, "text/plain", "falta t"); return; }
  if (horaValida()) { server.send(200, "text/plain", "Hora ja estava certa"); return; }
  time_t t = (time_t)strtoul(server.arg("t").c_str(), NULL, 10);
  if (t < 1700000000) { server.send(400, "text/plain", "hora invalida"); return; }
  struct timeval tv = { t, 0 };
  settimeofday(&tv, NULL);
  logI("Hora ajustada pelo navegador de %s (sem NTP)", quemHttp().c_str());
  server.send(200, "text/plain", "Hora ajustada");
}

// Baixa o log completo da flash (arquivo antigo + atual)
void webLogArquivo() {
  if (!fsOk) { server.send(500, "text/plain", "Flash (LittleFS) indisponivel"); return; }
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain; charset=utf-8", "");
  const char *arqs[] = { ARQ_LOG_ANT, ARQ_LOG };
  uint8_t buf[512];
  for (const char *nome : arqs) {
    File f = LittleFS.open(nome, FILE_READ);
    if (!f) continue;
    size_t n;
    while ((n = f.read(buf, sizeof(buf) - 1)) > 0) {
      buf[n] = 0;
      server.sendContent((const char *)buf, n);
    }
    f.close();
  }
  server.sendContent("");                       // fim da resposta
}

void webLogApagar() {
  if (!fsOk) { server.send(500, "text/plain", "Flash indisponivel"); return; }
  LittleFS.remove(ARQ_LOG_ANT);
  LittleFS.remove(ARQ_LOG);
  logA("Log da flash APAGADO por %s", quemHttp().c_str());
  server.send(200, "text/plain", "Log da flash apagado");
}

// Troca a rede Wi-Fi externa (salva na NVS e reinicia)
void webWifi() {
  String s = server.hasArg("ssid") ? server.arg("ssid") : "";
  String p = server.hasArg("senha") ? server.arg("senha") : "";
  s.trim();
  if (!s.length() || s.length() > 32 || p.length() > 63) {
    server.send(400, "text/plain", "Nome ou senha invalidos"); return;
  }
  Preferences pref;
  pref.begin("wifi", false);
  pref.putString("ssid", s);
  pref.putString("senha", p);
  pref.end();
  logI("Rede Wi-Fi trocada para \"%s\" por %s. Reiniciando...", s.c_str(), quemHttp().c_str());
  server.send(200, "text/plain", "Salvo. O robo vai reiniciar e entrar na rede nova.");
  pedidoReinicio = true;
}

// ============================= SOCKET TCP (porta 3333) =======================
// Servidor TCP "cru": o cliente abre uma conexao (telnet, nc, PuTTY em modo
// Raw) e manda comandos de texto terminados em ENTER. Um cliente por vez.
String tcpLinha;
bool tcpStream = false;
unsigned long tcpTStream = 0;

void tcpAjuda() {
  tcpCli.print(
    "Comandos (digite e ENTER):\r\n"
    "  INFO      - IP, MAC, rede, sinal, hora\r\n"
    "  SENSORES  - leitura dos sensores agora\r\n"
    "  STATUS    - estado do robo\r\n"
    "  LIGAR / DESLIGAR / CALIBRAR - comandos dos motores\r\n"
    "  STREAM    - liga/desliga envio continuo dos sensores (5x por segundo)\r\n"
    "  LOG [n]   - ultimas n linhas do log (padrao 15)\r\n"
    "  AJUDA     - esta lista      SAIR - fecha a conexao\r\n");
}

void tcpSensores() {
  tcpCli.printf("SENSORES bruto_E=%d bruto_D=%d linha_E=%d linha_D=%d erro=%d motor_E=%d motor_D=%d\r\n",
                (int)tBrE, (int)tBrD, (int)tPE, (int)tPD, (int)tErro, (int)tVE, (int)tVD);
}

void tcpComando(String c) {
  c.trim();
  String arg;
  int sp = c.indexOf(' ');
  if (sp > 0) { arg = c.substring(sp + 1); c = c.substring(0, sp); }
  c.toUpperCase();
  const char *m;

  if (c == "AJUDA" || c == "HELP" || c == "?") tcpAjuda();
  else if (c == "INFO") {
    char hora[24]; carimbo(hora, sizeof(hora));
    tcpCli.printf("INFO rede=\"%s\" conectado=%s ip=%s gateway=%s rssi=%d mac=%s\r\n",
                  redeSsid.c_str(), staConectado ? "sim" : "nao",
                  WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
                  staConectado ? WiFi.RSSI() : 0, WiFi.macAddress().c_str());
    tcpCli.printf("INFO ap=\"%s\" ap_ip=%s mac_ap=%s portas: http=%d tcp=%d\r\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str(), WiFi.softAPmacAddress().c_str(),
                  HTTP_PORTA, TCP_PORTA);
    tcpCli.printf("INFO hora=%s ligado_ha=%lus quedas_wifi=%lu boot=#%lu\r\n",
                  hora, millis() / 1000, (unsigned long)quedasWifi, (unsigned long)numBoot);
  }
  else if (c == "SENSORES") tcpSensores();
  else if (c == "STATUS") {
    tcpCli.printf("STATUS %s calibrado=%s\r\n",
                  calibrando ? "CALIBRANDO" : (rodando ? "RODANDO" : "PARADO"),
                  calibrado ? "sim" : "nao");
  }
  else if (c == "LIGAR")    { int r = pedirLigar(&m);    tcpCli.printf("%s %s\r\n", r == 200 ? "OK" : "ERRO", m);
                              logI("TCP %s: comando LIGAR -> %s", tcpCliNome.c_str(), m); }
  else if (c == "DESLIGAR") { int r = pedirDesligar(&m); tcpCli.printf("%s %s\r\n", r == 200 ? "OK" : "ERRO", m);
                              logI("TCP %s: comando DESLIGAR -> %s", tcpCliNome.c_str(), m); }
  else if (c == "CALIBRAR") { int r = pedirCalibrar(&m); tcpCli.printf("%s %s\r\n", r == 200 ? "OK" : "ERRO", m);
                              logI("TCP %s: comando CALIBRAR -> %s", tcpCliNome.c_str(), m); }
  else if (c == "STREAM") {
    tcpStream = !tcpStream;
    tcpCli.printf("OK stream %s\r\n", tcpStream ? "ligado (digite STREAM para parar)" : "desligado");
  }
  else if (c == "LOG") {
    int n = arg.length() ? arg.toInt() : 15;
    n = constrain(n, 1, LOG_LINHAS);
    static char copia[LOG_TAM];
    uint32_t fim = logSeq;
    uint32_t ini = (fim > (uint32_t)n) ? fim - n : 0;
    if (fim > LOG_LINHAS && ini < fim - LOG_LINHAS) ini = fim - LOG_LINHAS;
    for (uint32_t i = ini; i < fim; i++) {
      portENTER_CRITICAL(&muxLog);
      memcpy(copia, logBuf[i % LOG_LINHAS], LOG_TAM);
      portEXIT_CRITICAL(&muxLog);
      tcpCli.print(copia); tcpCli.print("\r\n");
    }
  }
  else if (c == "SAIR" || c == "EXIT" || c == "QUIT") {
    tcpCli.print("Tchau!\r\n");
    logI("Cliente TCP %s saiu (SAIR)", tcpCliNome.c_str());
    tcpCli.stop();
    tcpCliNome = "";
    tcpStream = false;
  }
  else if (c.length()) tcpCli.printf("ERRO comando desconhecido: %s (digite AJUDA)\r\n", c.c_str());
}

void tratarTCP() {
  // 1) conexao nova?  (o servidor fica em LISTEN na porta TCP_PORTA)
  if (tcpServidor.hasClient()) {
    WiFiClient novo = tcpServidor.accept();
    if (tcpCli.connected()) {
      String quem = novo.remoteIP().toString();
      novo.print("Ocupado: ja existe um cliente conectado.\r\n");
      novo.stop();
      logA("Conexao TCP recusada de %s (ja ha um cliente)", quem.c_str());
    } else {
      tcpCli = novo;
      tcpCli.setNoDelay(true);
      tcpCliNome = tcpCli.remoteIP().toString() + ":" + String(tcpCli.remotePort());
      tcpLinha = "";
      tcpStream = false;
      logI("Cliente TCP conectado: %s -> porta %d", tcpCliNome.c_str(), TCP_PORTA);
      tcpCli.print("=== Seguidor Yata - servidor TCP ===\r\n");
      tcpAjuda();
    }
  }
  if (!tcpCliNome.length()) return;

  // 2) cliente caiu?
  if (!tcpCli.connected()) {
    logI("Cliente TCP desconectou: %s", tcpCliNome.c_str());
    tcpCli.stop();
    tcpCliNome = "";
    tcpStream = false;
    return;
  }

  // 3) le os bytes recebidos e monta a linha
  while (tcpCli.available()) {
    char ch = tcpCli.read();
    if (ch == '\r' || ch == '\n') {
      if (tcpLinha.length()) { tcpComando(tcpLinha); tcpLinha = ""; }
      if (!tcpCliNome.length()) return;          // cliente digitou SAIR
    } else if (ch == 8 || ch == 127) {
      if (tcpLinha.length()) tcpLinha.remove(tcpLinha.length() - 1);
    } else if ((uint8_t)ch >= 32 && tcpLinha.length() < 64) {
      tcpLinha += ch;
    }
  }

  // 4) envio continuo dos sensores
  if (tcpStream && millis() - tcpTStream >= 200) {
    tcpTStream = millis();
    tcpSensores();
  }
}

// ============================= WI-FI =========================================
const char *motivoDesconexao(uint8_t r) {
  switch (r) {
    case 2:   return "autenticacao expirou";
    case 8:   return "saiu da rede";
    case 15:  return "senha errada (handshake)";
    case 200: return "sinal perdido (beacon timeout)";
    case 201: return "rede nao encontrada";
    case 202: return "falha de autenticacao (senha?)";
    case 203: return "falha de associacao";
    case 204: return "falha no handshake (senha?)";
    case 205: return "falha de conexao";
    default:  return "outro";
  }
}

void eventoWifi(WiFiEvent_t ev, WiFiEventInfo_t info) {
  switch (ev) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      staConectado = true;
      if (staJaConectou && tQuedaWifi) {
        logI("Wi-Fi RECONECTADO em \"%s\" apos %lu s (%lu tentativas) | IP %s | RSSI %d dBm",
             redeSsid.c_str(), (millis() - tQuedaWifi) / 1000, (unsigned long)tentativasWifi,
             WiFi.localIP().toString().c_str(), WiFi.RSSI());
      } else {
        logI("Wi-Fi conectado em \"%s\" | IP %s | gateway %s | RSSI %d dBm | MAC %s",
             redeSsid.c_str(), WiFi.localIP().toString().c_str(),
             WiFi.gatewayIP().toString().c_str(), WiFi.RSSI(), WiFi.macAddress().c_str());
      }
      staJaConectou = true;
      tQuedaWifi = 0;
      tentativasWifi = 0;
      configTzTime(FUSO_TZ, NTP_1, NTP_2);     // pede a hora certa na internet
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      uint8_t r = info.wifi_sta_disconnected.reason;
      if (staConectado) {                        // estava conectado e caiu
        staConectado = false;
        quedasWifi = quedasWifi + 1;
        tQuedaWifi = millis();
        logA("Wi-Fi CAIU (motivo %u: %s). Reconectando...", r, motivoDesconexao(r));
      } else if (!staJaConectou && tentativasWifi == 0) {
        logE("Nao conectou em \"%s\" (motivo %u: %s). Tentando de novo...",
             redeSsid.c_str(), r, motivoDesconexao(r));
      }
      tentativasWifi = tentativasWifi + 1;
      break;
    }

    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      logI("Dispositivo entrou na rede propria (AP %s)", AP_SSID);
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      logI("Dispositivo saiu da rede propria (AP %s)", AP_SSID);
      break;
    default: break;
  }
}

// Se a reconexao automatica nao resolver, tenta de novo a cada 15 s (e a
// cada 60 s depois de muitas falhas: cada tentativa faz o radio varrer os
// canais, o que atrapalha quem esta conectado na rede propria do robo)
void vigiarWifi() {
  static unsigned long tUlt = 0;
  static bool avisouHora = false;
  if (!avisouHora && horaValida()) {
    char h[24]; carimbo(h, sizeof(h));
    logI("Relogio sincronizado: %s", h);
    avisouHora = true;
  }
  if (!redeSsid.length() || staConectado) return;
  if (millis() - tUlt < (tentativasWifi < 20 ? 15000UL : 60000UL)) return;
  tUlt = millis();
  if (tentativasWifi > 0 && tentativasWifi % 20 == 0)
    logE("Ainda sem conexao com \"%s\" (%lu tentativas)", redeSsid.c_str(), (unsigned long)tentativasWifi);
  WiFi.begin(redeSsid.c_str(), redeSenha.c_str());
}

// O servidor roda em uma tarefa propria no nucleo 0 (o mesmo do Wi-Fi), assim
// o loop() de controle no nucleo 1 continua com o periodo fixo de 2 ms.
void tarefaWeb(void *) {
  for (;;) {
    server.handleClient();     // HTTP (porta 80)
    tratarTCP();               // socket TCP (porta 3333)
    vigiarWifi();              // reconexao + aviso de hora
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void iniciarRede() {
  // rede externa: o que foi salvo pela pagina tem prioridade sobre o #define
  Preferences pref;
  pref.begin("wifi", true);
  redeSsid  = pref.getString("ssid", REDE_SSID);
  redeSenha = pref.getString("senha", REDE_SENHA);
  pref.end();
  if (redeSsid == "NOME_DA_SUA_REDE") redeSsid = "";       // nao configurada

  WiFi.onEvent(eventoWifi);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_AP_STA);                    // estacao + ponto de acesso juntos
  WiFi.softAP(AP_SSID, AP_SENHA);
  // Potencia menor = picos de corrente menores no Wi-Fi (menos quedas de
  // tensao quando os motores arrancam). Alcance continua de varios metros.
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.setAutoReconnect(true);
  setenv("TZ", FUSO_TZ, 1);
  tzset();

  if (redeSsid.length()) {
    logI("Conectando na rede externa \"%s\"...", redeSsid.c_str());
    WiFi.begin(redeSsid.c_str(), redeSenha.c_str());
  } else {
    logA("Rede externa NAO configurada: use a rede propria \"%s\" e configure na pagina", AP_SSID);
  }

  server.on("/",            HTTP_GET,  webPagina);
  server.on("/status",      HTTP_GET,  webStatus);       // sensores em tempo real
  server.on("/info",        HTTP_GET,  webInfo);         // IP, MAC, conexao
  server.on("/log",         HTTP_GET,  webLog);          // log recente (RAM)
  server.on("/log/arquivo", HTTP_GET,  webLogArquivo);   // log completo (flash)
  server.on("/log/apagar",  HTTP_POST, webLogApagar);
  server.on("/start",       HTTP_POST, webStart);        // atuador
  server.on("/stop",        HTTP_POST, webStop);
  server.on("/calibrar",    HTTP_POST, webCalibrar);
  server.on("/param",       HTTP_GET,  webParamGet);
  server.on("/param",       HTTP_POST, webParamPost);
  server.on("/salvar",      HTTP_POST, webSalvar);
  server.on("/padrao",      HTTP_POST, webPadrao);
  server.on("/hora",        HTTP_POST, webHora);
  server.on("/wifi",        HTTP_POST, webWifi);
  server.onNotFound([]() {
    logA("HTTP %s: rota inexistente %s", quemHttp().c_str(), server.uri().c_str());
    server.send(404, "text/plain", "Nao encontrado");
  });
  server.begin();                            // LISTEN na porta 80
  tcpServidor.begin();                       // LISTEN na porta 3333

  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", HTTP_PORTA);
    MDNS.addService("seguidor", "tcp", TCP_PORTA);
  }

  xTaskCreatePinnedToCore(tarefaWeb, "web", 8192, NULL, 1, NULL, 0);

  logI("Servidores em LISTEN: HTTP porta %d, TCP porta %d", HTTP_PORTA, TCP_PORTA);
  logI("Rede propria \"%s\" (senha \"%s\") -> http://%s | MAC AP %s",
       AP_SSID, AP_SENHA, WiFi.softAPIP().toString().c_str(), WiFi.softAPmacAddress().c_str());
}

// executa no loop() o que a pagina web pediu
void tratarPedidosWeb() {
  if (pedidoWeb == PED_NADA) return;
  // le e limpa de uma vez so: um DESLIGAR que chegue no meio nao se perde
  portENTER_CRITICAL(&muxPed);
  uint8_t ped = pedidoWeb;
  pedidoWeb = PED_NADA;
  portEXIT_CRITICAL(&muxPed);

  switch (ped) {
    case PED_START:    if (!rodando) setRodando(true);  break;
    case PED_STOP:     if (rodando)  setRodando(false); break;
    case PED_CALIBRAR: setRodando(false); calibrar();   break;
  }
}

// Por que o ESP32 ligou/reiniciou. "queda de tensao" nas arrancadas = bateria
// fraca ou a protecao do TP4056 cortando (veja a nota 5 no topo).
const char *motivoReinicio() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "ligou a energia";
    case ESP_RST_EXT:      return "botao EN (reset)";
    case ESP_RST_SW:       return "reinicio pelo programa";
    case ESP_RST_PANIC:    return "ERRO no programa (travou)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "watchdog (programa travou)";
    case ESP_RST_BROWNOUT: return "QUEDA DE TENSAO (bateria fraca ou motor puxando demais)";
    case ESP_RST_DEEPSLEEP:return "acordou do sono";
    default:               return "desconhecido";
  }
}

// reinicios que indicam problema viram ERRO no log
bool reinicioRuim() {
  esp_reset_reason_t r = esp_reset_reason();
  return r == ESP_RST_BROWNOUT || r == ESP_RST_PANIC || r == ESP_RST_INT_WDT ||
         r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
}

// ============================= SETUP =========================================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_AIN1, OUTPUT); pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT); pinMode(PIN_BIN2, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  pwmSetup(PIN_PWMA, CH_A);
  pwmSetup(PIN_PWMB, CH_B);
  pararMotores();

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_QRE_E, ADC_11db);
  analogSetPinAttenuation(PIN_QRE_D, ADC_11db);

  // log: fila para a flash + monta o LittleFS (formata na 1a vez)
  fsOk = LittleFS.begin(false);
  bool formatou = false;
  if (!fsOk) { formatou = true; fsOk = LittleFS.begin(true); }
  if (fsOk) {          // sem flash nao cria a fila (ela encheria e nunca esvaziaria)
    filaLog = xQueueCreate(LOG_FILA, LOG_TAM);
    xTaskCreatePinnedToCore(tarefaFlash, "flash", 4096, NULL, 1, NULL, 0);
  }

  // contador de inicializacoes (ajuda a separar cada boot no log da flash)
  Preferences pref;
  pref.begin("sist", false);
  numBoot = pref.getUInt("boots", 0) + 1;
  pref.putUInt("boots", numBoot);
  pref.end();

  logI("===== INICIO #%lu - Seguidor de Linha ESP32 (2x QRE + 2x motor TT) =====",
       (unsigned long)numBoot);
  if (reinicioRuim()) logE("Reinicio inesperado: %s", motivoReinicio());
  else                logI("Motivo do reinicio: %s", motivoReinicio());
  if (!fsOk)          logE("Falha ao montar a flash (LittleFS): log NAO sera gravado");
  else if (formatou)  logA("Flash formatada (primeiro uso do LittleFS)");

  carregarCalibracao();
  carregarParam();
  publicarParam();

  if (calibrado) logI("Calibracao carregada  E[%d..%d]  D[%d..%d]", minE, maxE, minD, maxD);
  else           logA("SEM calibracao: toque em CALIBRAR na pagina.");
  mostrarParametros();

  iniciarRede();

  tUltimoCiclo = micros();
}

// ============================= LOOP ==========================================
void loop() {
  tratarSerial();
  tratarPedidosWeb();
  aplicarParamWeb();

  // troca de rede pela pagina: para os motores, grava o log e reinicia
  if (pedidoReinicio) {
    setRodando(false);
    pararMotores();
    unsigned long t0 = millis();
    while (filaLog && uxQueueMessagesWaiting(filaLog) && millis() - t0 < 3000) delay(50);
    delay(800);                    // da tempo da resposta HTTP sair e o arquivo fechar
    ESP.restart();
  }

  // amostragem em periodo fixo
  if (micros() - tUltimoCiclo < DT_US) return;
  tUltimoCiclo += DT_US;

  int pE, pD;
  lerSensores(pE, pD);
  int soma = pE + pD;

  digitalWrite(PIN_LED, rodando);

  if (!rodando || !calibrado) {
    pararMotores();
    // parado: mostra o erro na pagina para conferir os sensores com a mao
    tErro = (soma >= LIMIAR_LINHA) ? (int)(1000.0f * (pE - pD) / soma) : 0;
    tVE = 0; tVD = 0;
    perdida = false;
    return;
  }

  // ----- linha perdida: gira para o lado em que ela sumiu -----
  if (soma < LIMIAR_LINHA) {
    integral = 0;
    int giro = (ultimoLado >= 0) ? P.vBusca : -P.vBusca;
    motores(giro, -giro);
    tErro = 0; tVE = giro; tVD = -giro;

    // registra no log so se ficar perdida mais de 150 ms (evita encher o log)
    if (!perdida) { perdida = true; avisouPerda = false; tPerdeu = millis(); }
    if (!avisouPerda && millis() - tPerdeu >= 150) {
      logA("Linha perdida: procurando para a %s", (giro > 0) ? "direita" : "esquerda");
      avisouPerda = true;
    }
    if (telemetria) Serial.println("buscando...");
    return;
  }
  if (perdida) {
    if (avisouPerda) logI("Linha encontrada de novo (%lu ms perdida)", millis() - tPerdeu);
    perdida = false;
  }

  // ----- erro: +1000 (linha toda na esquerda) .. -1000 (toda na direita) -----
  float erro = 1000.0f * (float)(pE - pD) / (float)soma;
  if (erro >  250) ultimoLado = -1;   // linha escapando pela esquerda
  if (erro < -250) ultimoLado =  1;   // escapando pela direita

  // ----- PID -----
  integral += erro * DT_S;
  integral = constrain(integral, -LIM_INTEGRAL, LIM_INTEGRAL);

  float deriv = erro - erroAnt;
  dFiltrado = ALFA_D * deriv + (1.0f - ALFA_D) * dFiltrado;
  erroAnt = erro;

  float correcao = P.Kp * erro + P.Ki * integral + P.Kd * dFiltrado;

  // ----- mistura diferencial -----
  // erro > 0 (linha a esquerda) -> freia a roda esquerda, acelera a direita
  int vE = P.vBase - (int)correcao;
  int vD = P.vBase + (int)correcao;

  int piso = P.permiteRe ? -P.vMax : 0;
  vE = constrain(vE, piso, P.vMax);
  vD = constrain(vD, piso, P.vMax);

  motores(vE, vD);
  tErro = (int)erro; tVE = vE; tVD = vD;

  if (telemetria) {
    static uint8_t n = 0;
    if (++n >= 25) {   // ~20 Hz no monitor/plotter serial
      n = 0;
      Serial.printf("E:%d D:%d erro:%.0f corr:%.0f vE:%d vD:%d\n",
                    pE, pD, erro, correcao, vE, vD);
    }
  }
}
