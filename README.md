# 🌦️ Estação Meteorológica CYD — PU4SYI

Estação meteorológica baseada em **ESP32 CYD** (Cheap Yellow Display) que combina dados meteorológicos online da API **OpenWeather** com leituras locais de um sensor **BME280**, exibindo tudo num display TFT colorido de 2.8".

Desenvolvida por **PU4SYI**.

---

## 📋 Funcionalidades

- **Clima atual** da sua cidade via OpenWeather (temperatura, sensação térmica, umidade, vento, pressão, nascer/pôr do sol)
- **Previsão de 5 dias** com agregação diária de mín/máx, umidade e vento
- **Sensor local BME280** medindo temperatura, umidade e pressão do ambiente interno
- **Tela comparativa** Interno × Externo, com cálculo de ponto de orvalho e índice de conforto
- **Ícones de clima desenhados por código** (sem necessidade de bitmaps ou SPIFFS)
- **Navegação por toque** — toque na tela para alternar entre as 3 telas
- **Sincronização de horário via NTP**
- Indicador de status de WiFi em tempo real

---

## 🖥️ Telas

### Tela 1 — Clima Atual
Mostra o clima externo da cidade com ícone, temperatura grande, descrição e sensação térmica. Inclui faixa com leitura interna do BME280 e painel com mín/máx, umidade, vento e pressão. Rodapé com nascer/pôr do sol e relógio.

### Tela 2 — Previsão 5 Dias
Cinco cards lado a lado, cada um com dia da semana, ícone do clima, temperatura máxima/mínima, umidade média e vento médio.

### Tela 3 — Interno × Externo
Tabela comparativa entre os dados do sensor local (BME280) e os dados externos (OpenWeather), com cálculo de delta colorido, ponto de orvalho e classificação de conforto ambiental.

---

## 🔧 Hardware

| Componente | Descrição |
|---|---|
| **CYD ESP32-2432S028R** | ESP32 com display TFT 2.8" 320×240, touch resistivo (ILI9341 + XPT2046) |
| **BME280** | Sensor de temperatura, umidade e pressão (I2C, 4 pinos) |
| **Fonte USB** | 5V / 1A para alimentação |

### Conexão do BME280

O BME280 é conectado via **I2C** utilizando os pinos do conector "SPI" da CYD (no ESP32, qualquer GPIO pode operar como I2C por software). A alimentação vem do conector P3.

| BME280 | CYD | GPIO |
|---|---|---|
| VCC | Conector P3 (3.3V) | — |
| GND | Conector P3 (GND) | — |
| SDA | Conector SPI (IO23) | GPIO 23 |
| SCL | Conector SPI (IO27) | GPIO 27 |

> ⚠️ **Alimente o BME280 com 3.3V**, não 5V. O chip BME280 opera entre 1.71V e 3.6V.

---

## 📚 Bibliotecas necessárias

Instale via **Arduino IDE → Library Manager**:

- `TFT_eSPI` (Bodmer)
- `ArduinoJson` (Benoit Blanchon) — versão 7.x ou superior
- `XPT2046_Touchscreen` (Paul Stoffregen)
- `Adafruit BME280 Library` (Adafruit)
- `Adafruit Unified Sensor` (Adafruit)

---

## ⚙️ Configuração

### 1. Core do ESP32

Use o **core ESP32 versão 3.0.7** no Boards Manager. Versões 3.3.x apresentam incompatibilidade com a TFT_eSPI (erros de `gpio_dev_t`).

### 2. Configuração do TFT_eSPI

Edite o arquivo `User_Setup.h` na pasta da biblioteca `TFT_eSPI` com os pinos da CYD:

```cpp
#define USER_SETUP_INFO "CYD ESP32-2432S028R"
#define ILI9341_2_DRIVER   // para CYD v2 (USB-C), use ST7789_DRIVER

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       55000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
```

### 3. Chave da API OpenWeather

Crie uma conta gratuita em [openweathermap.org/api](https://openweathermap.org/api) e gere sua API key. A ativação pode levar de 10 a 30 minutos após o cadastro. O plano gratuito permite 60 chamadas/min e 1.000.000/mês.

### 4. Edite as constantes no topo do sketch

```cpp
const char* WIFI_SSID     = "SUA_REDE_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA";
const char* OPENWEATHER_API_KEY = "SUA_API_KEY_AQUI";

const float LATITUDE   = -15.7942;     // sua latitude
const float LONGITUDE  = -47.8822;     // sua longitude
const char* CITY_NAME  = "Brasilia";   // exibido caso a API falhe
```

> O ESP32 conecta apenas em redes **2.4 GHz**. Pegue as coordenadas no Google Maps (clique direito no local → copiar coordenadas).

---

## 🚀 Upload

Configurações no Arduino IDE (**Tools**):

| Opção | Valor |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |
| CPU Frequency | 240MHz |

Se o upload falhar com `Failed to connect`, mantenha o botão **BOOT** pressionado durante o processo.

---

## 🔍 Solução de problemas

| Problema | Causa provável | Solução |
|---|---|---|
| Tela branca | Driver errado no `User_Setup.h` | Trocar ILI9341 por ST7789 (CYD v2) |
| Cores invertidas | RGB order | Adicionar `#define TFT_RGB_ORDER TFT_BGR` |
| `gpio_dev_t` não declarado | Core ESP32 3.3.x | Fazer downgrade para 3.0.7 |
| `Adafruit_Sensor.h not found` | Falta biblioteca | Instalar Adafruit Unified Sensor |
| `BME280: NAO ENCONTRADO` | SDA/SCL invertidos ou endereço | Verificar fiação; código tenta 0x76 e 0x77 |
| HTTP 401 | API key inválida/inativa | Aguardar ativação (até 30 min) |
| Touch deslocado | Calibração | Ajustar `TOUCH_MIN/MAX` via Serial Monitor |

---

## 📊 Consumo de API

- 1 chamada `/weather` + 1 chamada `/forecast` a cada 10 minutos
- ~288 chamadas/dia (~0,9% do limite mensal gratuito)
- Leitura do BME280 local a cada 30 segundos (sem consumo de API)

---

## 🛠️ Detalhes técnicos

- **Ícones por código:** desenhados com primitivas gráficas (círculos, linhas, triângulos) e escaláveis, eliminando a necessidade de assets externos.
- **Otimização de RAM:** o parse da previsão usa `DeserializationOption::Filter` do ArduinoJson para descartar campos não utilizados durante o parse, reduzindo o uso de heap.
- **Ponto de orvalho:** calculado pela aproximação de Magnus a partir da temperatura e umidade do BME280.
- **Agregação da previsão:** as 40 leituras de 3h em 3h são agrupadas por dia local, escolhendo o ícone mais próximo das 13h como representativo do dia.

---

## 📡 Sobre

Projeto desenvolvido por **PU4SYI** como parte de experimentos com ESP32, IoT e radioamadorismo.

73! 📻

---

## 📄 Licença

Sinta-se livre para usar, modificar e distribuir. Considere creditar o autor original.
