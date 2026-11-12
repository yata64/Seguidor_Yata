# Seguidor de Linha Yata (ESP32)

Robô seguidor de linha com ESP32 que funciona como **servidor de rede**: ele recebe comandos, entrega a leitura dos sensores e mantém um log do sistema por **HTTP (porta 80)** e por **socket TCP (porta 3333)**.

Projeto do TDE de **Conectividade de Sistemas Ciberfísicos** (PUCPR, Engenharia de Software).

## Hardware

| Componente | Função |
|---|---|
| ESP32 DevKit | Controle e servidor Wi-Fi |
| 2x QRE1113 (analógico) | **Sensores** de refletância (linha) |
| Ponte H L298N + 2x motor TT | **Atuadores** (tração diferencial) |
| 2x 18650 (paralelo) + TP4056 | Bateria e carga |
| XL6009 (step-up, 8 V) | Alimentação dos motores |
| MP1584 (step-down, 5 V) | Alimentação do ESP32 (VIN) |

### Ligações

| Sinal | GPIO |
|---|---|
| QRE esquerdo (AO) | 34 |
| QRE direito (AO) | 35 |
| L298N IN1 / IN2 / ENA (motor esquerdo) | 26 / 27 / 13 |
| L298N IN3 / IN4 / ENB (motor direito) | 32 / 33 / 25 |
| LED de status | 2 |

## Arquitetura de rede

```
 Cliente (navegador / curl / telnet / app)
            |   TCP/IP sobre Wi-Fi
            v
 +----------------------- ESP32 (servidor) -----------------------+
 |  Wi-Fi AP+STA                                                  |
 |   STA: rede externa (DHCP, reconexão automática, log de queda) |
 |   AP : rede própria "Seguidor" -> 192.168.4.1 (reserva)        |
 |                                                                |
 |  Porta 80   HTTP  -> página web + API JSON                     |
 |  Porta 3333 TCP   -> comandos em texto (telnet / nc)           |
 |  NTP (UDP 123)    -> data e hora do log                        |
 |  mDNS             -> http://seguidor.local                     |
 |                                                                |
 |  Núcleo 1: controle PID dos motores (500 Hz)                   |
 |  Núcleo 0: servidores HTTP/TCP, Wi-Fi e gravação do log        |
 +----------------------------------------------------------------+
```

### Rotas HTTP

| Método | Rota | Descrição |
|---|---|---|
| GET | `/` | Página de controle |
| GET | `/status` | Sensores e motores em tempo real (JSON) |
| GET | `/info` | IP, MAC, rede, RSSI, hora, quedas do Wi-Fi (JSON) |
| GET | `/log` | Log recente (RAM) |
| GET | `/log/arquivo` | Log completo gravado na flash |
| POST | `/start`, `/stop`, `/calibrar` | Comandos do atuador |
| GET/POST | `/param` | Ler/alterar PID e velocidades |
| POST | `/salvar`, `/padrao` | Salvar / restaurar ajustes |
| POST | `/wifi` | Trocar a rede Wi-Fi externa (`ssid`, `senha`) |
| POST | `/hora` | Acertar o relógio sem internet (`t` = epoch) |
| POST | `/log/apagar` | Apagar o log da flash |

### Comandos do socket TCP (porta 3333)

`INFO`, `SENSORES`, `STATUS`, `LIGAR`, `DESLIGAR`, `CALIBRAR`, `STREAM` (envio contínuo dos sensores, 5x/s), `LOG [n]`, `AJUDA`, `SAIR`.

### Log persistente

Arquivo `/log.txt` na flash (LittleFS), com rotação em 64 kB. Cada linha tem data/hora e nível:

```
12/11/2026 14:03:10 INFO  Wi-Fi conectado em "MinhaRede" | IP 172.20.10.4 | ...
12/11/2026 14:05:42 AVISO Wi-Fi CAIU (motivo 200: sinal perdido). Reconectando...
12/11/2026 14:05:49 INFO  Wi-Fi RECONECTADO apos 7 s (3 tentativas) | IP 172.20.10.4
12/11/2026 14:06:01 ERRO  Calibracao FALHOU: contraste E=80 D=95 (minimo 150).
```

## Como compilar

1. Arduino IDE com o pacote **esp32** (Espressif) instalado. Placa: **ESP32 Dev Module**.
2. Em **Ferramentas → Partition Scheme**, escolha **Default 4MB with spiffs** (o log usa essa partição).
3. Copie `segredos_exemplo.h` como `segredos.h` e preencha com o nome e a senha da rede Wi-Fi. Sem esse arquivo o robô só cria a rede própria, e a rede externa pode ser configurada pela página.
4. Carregue o sketch `Seguidor_Yata.ino`.

## Como usar

1. Ligue o robô. O IP aparece no Monitor Serial (115200) e na seção **Rede** da página.
2. Abra `http://IP` (ou `http://192.168.4.1` conectado na rede `Seguidor`, senha `seguidor123`).
3. **CALIBRAR** com o robô sobre a linha, depois **LIGAR** / **DESLIGAR**.

## Testes de conectividade

```bash
ping IP
curl http://IP/status
curl http://IP/info
curl -X POST http://IP/start
curl -X POST http://IP/stop
curl http://IP/log/arquivo
telnet IP 3333
```

No Wireshark, filtre por `ip.addr == IP` e `tcp.port == 80 || tcp.port == 3333` para ver o handshake TCP e as mensagens.
