



// CODIGO TUDO FUNCIONANDO, DEPOIS VER A QUESTÃO DO SENSOR DE CHUVA

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <time.h>

// CONFIGURAÇÃO GERAL
const char* WIFI_SSID     = "SEU SSID";
const char* WIFI_PASSWORD = "SUA SENHA";
const char* OPENWEATHER_API_KEY = "SUA API";
const float LATITUDE   = -19.925819066245538;
const float LONGITUDE  = -44.02441085767081;
const char* CITY_NAME  = "Contagem";
const char* LANG  = "pt_br";
const char* UNITS = "metric";
const unsigned long UPDATE_INTERVAL  = 10UL * 60UL * 1000UL;
const unsigned long INTERVALO_BME280 = 30UL * 1000UL;
const long  GMT_OFFSET_SEC = -3 * 3600;
const char* NTP_SERVER     = "pool.ntp.org";

// SETAR PINOS
#define I2C_SDA  23
#define I2C_SCL  27
#define BME280_ADDR 0x76
Adafruit_BME280 bme;
bool bmeOk = false;

// CONFIGURAÇÃO DO TOUCH
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3700
#define TOUCH_MIN_Y 240
#define TOUCH_MAX_Y 3800


TFT_eSPI tft = TFT_eSPI();
#define COR_FUNDO       0x0841
#define COR_PAINEL      0x18E3
#define COR_TEXTO       TFT_WHITE
#define COR_DESTAQUE    0xFFE0
#define COR_SECUNDARIA  0x8C71
#define COR_VERMELHO    0xF800
#define COR_VERDE       0x07E0
#define COR_AZUL        0x05BF
#define COR_LARANJA     0xFC00
#define COR_CIANO       0x07FF


struct WeatherData {
  String cidade, descricao, iconCode;
  float temp, sensacao, tempMin, tempMax, ventoVel;
  int umidade, pressao, ventoDir, nuvens;
  long nascerSol, porSol, timestamp;
  bool valido;
} clima;

struct ForecastDay {
  long dataDia;
  float tempMin, tempMax, ventoMedio;
  String iconCode, descricao;
  int umidadeMedia, leituras;
  bool valido;
};

struct DadosInternos {
  float temp, umidade, pressao, pontoOrvalho;
  bool valido;
} interno;

ForecastDay previsao[5];
bool previsaoValida = false;

enum Tela { TELA_ATUAL, TELA_PREVISAO, TELA_COMPARATIVO };
Tela telaAtual = TELA_ATUAL;
const int TOTAL_TELAS = 3;

unsigned long ultimaAtualizacao = 0;
unsigned long ultimaLeituraBme  = 0;
unsigned long ultimoToque       = 0;


void conectarWiFi();
bool buscarClima();
bool buscarPrevisao();
void lerBME280();
float calcularPontoOrvalho(float t, float h);
String classificarConforto(float t, float h);
void desenharTela();
void desenharTelaAtual();
void desenharTelaPrevisao();
void desenharTelaComparativo();
void desenharCabecalho(const char* titulo);
void desenharTempPrincipal();
void desenharIconeClima(int x, int y, const String& code, float escala = 1.0);
void desenharFaixaInterna();
void desenharDetalhes();
void desenharRodape();
void desenharCardPrevisao(int x, int y, int w, int h, const ForecastDay& d);
void desenharBotoesNav();
void desenharLinhaComparativa(int y, const char* label, float vIn, float vEx, const char* unidade);
void mostrarErro(const String& msg);
void verificarToque();
String direcaoVento(int graus);
String formatarHora(long timestamp);
String diaSemana(long timestamp);

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== CASA ===");

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COR_FUNDO);
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  
  Wire.begin(I2C_SDA, I2C_SCL);
  bmeOk = bme.begin(BME280_ADDR, &Wire);
  if (!bmeOk) bmeOk = bme.begin(0x77, &Wire);
  Serial.printf("BME280: %s\n", bmeOk ? "OK" : "NAO ENCONTRADO");

  if (bmeOk) {
    bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::SAMPLING_X1,
                    Adafruit_BME280::FILTER_X16,
                    Adafruit_BME280::STANDBY_MS_500);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setTextSize(2);
  tft.drawString("SALA Weather", 160, 75);
  tft.setTextSize(1);
  tft.drawString(bmeOk ? "BME280: OK" : "BME280: nao detectado", 160, 110);
  tft.drawString("Iniciando...", 160, 135);

  conectarWiFi();
  configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);

  lerBME280();
  bool ok = buscarClima();
  buscarPrevisao();
  if (ok) desenharTela();
  else mostrarErro("Falha ao buscar dados");
  ultimaAtualizacao = millis();
}

// ===== LOOP =====
void loop() {
  if (millis() - ultimaAtualizacao >= UPDATE_INTERVAL) {
    if (WiFi.status() != WL_CONNECTED) conectarWiFi();
    buscarClima();
    buscarPrevisao();
    desenharTela();
    ultimaAtualizacao = millis();
  }

  if (millis() - ultimaLeituraBme >= INTERVALO_BME280) {
    lerBME280();
    if (telaAtual == TELA_ATUAL) desenharFaixaInterna();
    else if (telaAtual == TELA_COMPARATIVO) desenharTelaComparativo();
    ultimaLeituraBme = millis();
  }

  verificarToque();

  static unsigned long ultRelogio = 0;
  if (telaAtual == TELA_ATUAL && millis() - ultRelogio >= 60000) {
    desenharRodape();
    ultRelogio = millis();
  }
  delay(50);
}

// ===== TOUCH =====
void verificarToque() {
  if (!ts.tirqTouched() || !ts.touched()) return;
  if (millis() - ultimoToque < 300) return;
  TS_Point p = ts.getPoint();
  ultimoToque = millis();
  telaAtual = (Tela)(((int)telaAtual + 1) % TOTAL_TELAS);
  desenharTela();
}

// ===== WIFI =====
void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 30) { delay(500); t++; }
  Serial.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "OK" : "FAIL");
}

// ===== BME280 =====
void lerBME280() {
  if (!bmeOk) { interno.valido = false; return; }
  interno.temp = bme.readTemperature();
  interno.umidade = bme.readHumidity();
  interno.pressao = bme.readPressure() / 100.0f;
  interno.pontoOrvalho = calcularPontoOrvalho(interno.temp, interno.umidade);
  interno.valido = !isnan(interno.temp) && !isnan(interno.umidade);
  Serial.printf("BME: %.1fC %.0f%% %.1fhPa\n", interno.temp, interno.umidade, interno.pressao);
}

float calcularPontoOrvalho(float t, float h) {
  if (h <= 0 || h > 100) return 0;
  const float a = 17.62f, b = 243.12f;
  float gamma = (a * t) / (b + t) + log(h / 100.0f);
  return (b * gamma) / (a - gamma);
}

String classificarConforto(float t, float h) {
  if (t < 16) return "FRIO";
  if (t < 20 && h > 70) return "FRIO UMIDO";
  if (t >= 20 && t <= 26 && h >= 40 && h <= 60) return "AGRADAVEL";
  if (t > 26 && t <= 30 && h < 70) return "MORNO";
  if (t > 30 && h >= 60) return "ABAFADO";
  if (t > 32) return "QUENTE";
  if (h < 30) return "AR SECO";
  if (h > 80) return "AR UMIDO";
  return "OK";
}

// APIO DO OPENWEATHER
bool buscarClima() {
  if (WiFi.status() != WL_CONNECTED) return false;
  String url = "https://api.openweathermap.org/data/2.5/weather?lat=" + String(LATITUDE,4) +
               "&lon=" + String(LONGITUDE,4) + "&appid=" + String(OPENWEATHER_API_KEY) +
               "&units=" + String(UNITS) + "&lang=" + String(LANG);
  HTTPClient http;
  http.begin(url); http.setTimeout(10000);
  int code = http.GET();
  if (code != 200) { http.end(); clima.valido = false; return false; }
  String payload = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, payload)) { clima.valido = false; return false; }
  clima.cidade     = doc["name"] | CITY_NAME;
  clima.descricao  = doc["weather"][0]["description"] | "";
  clima.iconCode   = doc["weather"][0]["icon"] | "01d";
  clima.temp       = doc["main"]["temp"]       | 0.0f;
  clima.sensacao   = doc["main"]["feels_like"] | 0.0f;
  clima.tempMin    = doc["main"]["temp_min"]   | 0.0f;
  clima.tempMax    = doc["main"]["temp_max"]   | 0.0f;
  clima.umidade    = doc["main"]["humidity"]   | 0;
  clima.pressao    = doc["main"]["pressure"]   | 0;
  clima.ventoVel   = doc["wind"]["speed"]      | 0.0f;
  clima.ventoDir   = doc["wind"]["deg"]        | 0;
  clima.nascerSol  = doc["sys"]["sunrise"]     | 0L;
  clima.porSol     = doc["sys"]["sunset"]      | 0L;
  clima.timestamp  = doc["dt"]                 | 0L;
  clima.valido = true;
  if (clima.descricao.length() > 0) clima.descricao[0] = toupper(clima.descricao[0]);
  return true;
}

bool buscarPrevisao() {
  if (WiFi.status() != WL_CONNECTED) return false;
  String url = "https://api.openweathermap.org/data/2.5/forecast?lat=" + String(LATITUDE,4) +
               "&lon=" + String(LONGITUDE,4) + "&appid=" + String(OPENWEATHER_API_KEY) +
               "&units=" + String(UNITS) + "&lang=" + String(LANG) + "&cnt=40";
  HTTPClient http;
  http.begin(url); http.setTimeout(15000);
  int code = http.GET();
  if (code != 200) { http.end(); previsaoValida = false; return false; }

  JsonDocument filter;
  filter["list"][0]["dt"] = true;
  filter["list"][0]["main"]["temp_min"] = true;
  filter["list"][0]["main"]["temp_max"] = true;
  filter["list"][0]["main"]["humidity"] = true;
  filter["list"][0]["weather"][0]["icon"] = true;
  filter["list"][0]["weather"][0]["description"] = true;
  filter["list"][0]["wind"]["speed"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { previsaoValida = false; return false; }
  JsonArray list = doc["list"].as<JsonArray>();
  if (list.isNull() || list.size() == 0) { previsaoValida = false; return false; }

  for (int i = 0; i < 5; i++) {
    previsao[i].valido = false;
    previsao[i].tempMin = 999; previsao[i].tempMax = -999;
    previsao[i].umidadeMedia = 0; previsao[i].ventoMedio = 0;
    previsao[i].leituras = 0; previsao[i].iconCode = "01d";
    previsao[i].descricao = "";
  }

  long agora = clima.timestamp > 0 ? clima.timestamp : time(nullptr);
  long diaHoje = (agora + GMT_OFFSET_SEC) / 86400L;
  String iconesDia[5][8]; String descsDia[5][8];
  int horasDia[5][8]; int contIcones[5] = {0,0,0,0,0};

  for (JsonObject item : list) {
    long dt = item["dt"] | 0L;
    if (dt == 0) continue;
    long diaItem = (dt + GMT_OFFSET_SEC) / 86400L;
    int slot = (int)(diaItem - diaHoje) - 1;
    if (slot < 0 || slot > 4) continue;
    float tmin = item["main"]["temp_min"] | 0.0f;
    float tmax = item["main"]["temp_max"] | 0.0f;
    int hum = item["main"]["humidity"] | 0;
    float vel = item["wind"]["speed"] | 0.0f;
    String ic = item["weather"][0]["icon"] | "01d";
    String ds = item["weather"][0]["description"] | "";
    if (tmin < previsao[slot].tempMin) previsao[slot].tempMin = tmin;
    if (tmax > previsao[slot].tempMax) previsao[slot].tempMax = tmax;
    previsao[slot].umidadeMedia += hum;
    previsao[slot].ventoMedio += vel;
    previsao[slot].leituras++;
    previsao[slot].dataDia = diaItem * 86400L - GMT_OFFSET_SEC;
    previsao[slot].valido = true;
    int idx = contIcones[slot];
    if (idx < 8) {
      time_t t = dt + GMT_OFFSET_SEC;
      struct tm* ti = gmtime(&t);
      iconesDia[slot][idx] = ic;
      descsDia[slot][idx] = ds;
      horasDia[slot][idx] = ti->tm_hour;
      contIcones[slot]++;
    }
  }

  for (int i = 0; i < 5; i++) {
    if (!previsao[i].valido || previsao[i].leituras == 0) continue;
    previsao[i].umidadeMedia /= previsao[i].leituras;
    previsao[i].ventoMedio /= previsao[i].leituras;
    int melhor = 0, menorDist = 99;
    for (int j = 0; j < contIcones[i]; j++) {
      int dist = abs(horasDia[i][j] - 13);
      if (dist < menorDist) { menorDist = dist; melhor = j; }
    }
    previsao[i].iconCode = iconesDia[i][melhor];
    previsao[i].descricao = descsDia[i][melhor];
    if (previsao[i].descricao.length() > 0)
      previsao[i].descricao[0] = toupper(previsao[i].descricao[0]);
  }
  previsaoValida = true;
  return true;
}


void desenharTela() {
  if (telaAtual == TELA_ATUAL) desenharTelaAtual();
  else if (telaAtual == TELA_PREVISAO) desenharTelaPrevisao();
  else desenharTelaComparativo();
}

void desenharTelaAtual() {
  tft.fillScreen(COR_FUNDO);
  String titulo = "SALA " + clima.cidade;
  desenharCabecalho(titulo.c_str());
  desenharTempPrincipal();
  desenharFaixaInterna();
  desenharDetalhes();
  desenharRodape();
  desenharBotoesNav();
}

void desenharCabecalho(const char* titulo) {
  tft.fillRect(0, 0, 320, 30, COR_PAINEL);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COR_TEXTO, COR_PAINEL);
  tft.setTextSize(2);
  tft.drawString(titulo, 8, 15);
  tft.setTextDatum(MR_DATUM);
  tft.setTextSize(1);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(COR_VERDE, COR_PAINEL);
    tft.drawString("WiFi OK", 312, 15);
  } else {
    tft.setTextColor(COR_VERMELHO, COR_PAINEL);
    tft.drawString("WiFi OFF", 312, 15);
  }
}

void desenharTempPrincipal() {
  desenharIconeClima(60, 80, clima.iconCode, 1.0);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COR_DESTAQUE, COR_FUNDO);
  tft.setFreeFont(&FreeSansBold24pt7b);
  String tempStr = String((int)round(clima.temp));
  tft.drawString(tempStr, 130, 70);
  int tw = tft.textWidth(tempStr);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.drawString("C", 130 + tw + 18, 60);
  tft.drawCircle(130 + tw + 8, 55, 4, COR_DESTAQUE);
  tft.setFreeFont(NULL);
  tft.setTextSize(1);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.setTextSize(2);
  tft.drawString(clima.descricao, 130, 105);
  tft.setTextSize(1);
  tft.setTextColor(COR_SECUNDARIA, COR_FUNDO);
  tft.drawString("Sensacao: " + String(clima.sensacao, 1) + " C", 130, 125);
}

void desenharFaixaInterna() {
  tft.fillRect(0, 138, 320, 18, 0x10A2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(COR_CIANO, 0x10A2);
  tft.drawString("INTERNO:", 8, 147);
  tft.setTextColor(COR_TEXTO, 0x10A2);
  if (interno.valido) {
    char buf[64];
    sprintf(buf, "%.1f C  %.0f%%  %.0f hPa",
            interno.temp, interno.umidade, interno.pressao);
    tft.drawString(buf, 65, 147);
  } else {
    tft.setTextColor(COR_VERMELHO, 0x10A2);
    tft.drawString("Sensor offline", 65, 147);
  }
}

void desenharDetalhes() {
  tft.fillRect(0, 158, 320, 58, COR_PAINEL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(COR_SECUNDARIA, COR_PAINEL);
  tft.drawString("MIN/MAX", 10, 162);
  tft.setTextColor(COR_TEXTO, COR_PAINEL);
  tft.setTextSize(2);
  tft.drawString(String((int)round(clima.tempMin)) + "/" + String((int)round(clima.tempMax)), 10, 178);
  tft.setTextSize(1);
  tft.drawString("C", 10, 200);

  tft.setTextColor(COR_SECUNDARIA, COR_PAINEL);
  tft.drawString("UMIDADE", 90, 162);
  tft.setTextColor(COR_AZUL, COR_PAINEL);
  tft.setTextSize(2);
  tft.drawString(String(clima.umidade) + "%", 90, 178);
  tft.setTextSize(1);

  tft.setTextColor(COR_SECUNDARIA, COR_PAINEL);
  tft.drawString("VENTO", 175, 162);
  tft.setTextColor(COR_VERDE, COR_PAINEL);
  tft.setTextSize(2);
  tft.drawString(String(clima.ventoVel, 1), 175, 178);
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_PAINEL);
  tft.drawString("m/s " + direcaoVento(clima.ventoDir), 175, 200);

  tft.setTextColor(COR_SECUNDARIA, COR_PAINEL);
  tft.drawString("PRESSAO", 250, 162);
  tft.setTextColor(COR_LARANJA, COR_PAINEL);
  tft.setTextSize(2);
  tft.drawString(String(clima.pressao), 250, 178);
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_PAINEL);
  tft.drawString("hPa", 250, 200);
}

void desenharRodape() {
  tft.fillRect(0, 218, 240, 22, COR_FUNDO);
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(COR_DESTAQUE, COR_FUNDO);
  tft.drawString("Nasc " + formatarHora(clima.nascerSol) +
                 "  Por " + formatarHora(clima.porSol), 8, 230);
  struct tm ti;
  if (getLocalTime(&ti, 100)) {
    char buf[16];
    sprintf(buf, "%02d:%02d", ti.tm_hour, ti.tm_min);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(COR_TEXTO, COR_FUNDO);
    tft.drawString(buf, 235, 230);
  }
}

void desenharTelaPrevisao() {
  tft.fillScreen(COR_FUNDO);
  desenharCabecalho("SALA Previsao 5d");
  if (!previsaoValida) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COR_VERMELHO, COR_FUNDO);
    tft.setTextSize(2);
    tft.drawString("Sem dados", 160, 110);
    desenharBotoesNav();
    return;
  }
  int margem = 4, gap = 2;
  int largura = (320 - margem * 2 - gap * 4) / 5;
  int altura = 175, y = 35;
  for (int i = 0; i < 5; i++) {
    int x = margem + i * (largura + gap);
    desenharCardPrevisao(x, y, largura, altura, previsao[i]);
  }
  desenharBotoesNav();
}

void desenharCardPrevisao(int x, int y, int w, int h, const ForecastDay& d) {
  tft.fillRoundRect(x, y, w, h, 4, COR_PAINEL);
  if (!d.valido) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COR_SECUNDARIA, COR_PAINEL);
    tft.drawString("--", x + w/2, y + h/2);
    return;
  }
  int cx = x + w / 2;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COR_DESTAQUE, COR_PAINEL);
  tft.setTextSize(1);
  tft.drawString(diaSemana(d.dataDia), cx, y + 12);
  desenharIconeClima(cx, y + 50, d.iconCode, 0.55);
  tft.setTextColor(COR_VERMELHO, COR_PAINEL);
  tft.setTextSize(2);
  tft.drawString(String((int)round(d.tempMax)), cx, y + 92);
  tft.setTextColor(COR_AZUL, COR_PAINEL);
  tft.drawString(String((int)round(d.tempMin)), cx, y + 115);
  tft.drawFastHLine(x + 6, y + 132, w - 12, COR_SECUNDARIA);
  tft.setTextColor(COR_TEXTO, COR_PAINEL);
  tft.setTextSize(1);
  tft.drawString(String(d.umidadeMedia) + "%", cx, y + 145);
  tft.setTextColor(COR_VERDE, COR_PAINEL);
  tft.drawString(String(d.ventoMedio, 1), cx, y + 162);
}


void desenharTelaComparativo() {
  tft.fillScreen(COR_FUNDO);
  desenharCabecalho("SALA Int x Ext");
  if (!interno.valido || !clima.valido) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COR_VERMELHO, COR_FUNDO);
    tft.setTextSize(2);
    tft.drawString(interno.valido ? "Sem dados externos" : "Sensor offline", 160, 110);
    desenharBotoesNav();
    return;
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(COR_SECUNDARIA, COR_FUNDO);
  tft.drawString("INTERNO",  90,  40);
  tft.drawString("EXTERNO", 175,  40);
  tft.drawString("DELTA",   260,  40);
  tft.drawFastHLine(8, 52, 304, COR_SECUNDARIA);

  desenharLinhaComparativa(60,  "TEMP",  interno.temp,    clima.temp,           "C");
  desenharLinhaComparativa(85,  "UMID",  interno.umidade, (float)clima.umidade, "%");
  desenharLinhaComparativa(110, "PRESS", interno.pressao, (float)clima.pressao, "hPa");

  tft.drawFastHLine(8, 138, 304, COR_SECUNDARIA);
  tft.fillRoundRect(8, 145, 304, 70, 5, COR_PAINEL);

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(COR_SECUNDARIA, COR_PAINEL);
  tft.drawString("PONTO DE ORVALHO INT.", 16, 153);
  tft.setTextColor(COR_CIANO, COR_PAINEL);
  tft.setTextSize(2);
  tft.drawString(String(interno.pontoOrvalho, 1) + " C", 16, 168);
  tft.setTextSize(1);

  tft.setTextColor(COR_SECUNDARIA, COR_PAINEL);
  tft.drawString("CONFORTO", 16, 192);
  String conforto = classificarConforto(interno.temp, interno.umidade);
  uint16_t corConf = COR_VERDE;
  if (conforto == "QUENTE" || conforto == "ABAFADO") corConf = COR_VERMELHO;
  else if (conforto == "FRIO" || conforto == "FRIO UMIDO") corConf = COR_AZUL;
  else if (conforto == "AR SECO" || conforto == "AR UMIDO" || conforto == "MORNO") corConf = COR_LARANJA;
  tft.setTextColor(corConf, COR_PAINEL);
  tft.setTextSize(2);
  tft.drawString(conforto, 95, 192);
  tft.setTextSize(1);

  desenharBotoesNav();
}

void desenharLinhaComparativa(int y, const char* label, float vIn, float vEx, const char* unidade) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COR_DESTAQUE, COR_FUNDO);
  tft.setTextSize(2);
  tft.drawString(label, 10, y);
  bool isPress = (strcmp(unidade, "hPa") == 0);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.drawString(String(vIn, isPress ? 0 : 1), 90, y);
  tft.drawString(String(vEx, isPress ? 0 : 1), 175, y);
  float delta = vIn - vEx;
  uint16_t corDelta = COR_VERDE;
  if (fabs(delta) > 5) corDelta = COR_VERMELHO;
  else if (fabs(delta) > 2) corDelta = COR_LARANJA;
  tft.setTextColor(corDelta, COR_FUNDO);
  String sd = (delta >= 0 ? "+" : "") + String(delta, isPress ? 0 : 1);
  tft.drawString(sd, 260, y);
  tft.setTextSize(1);
  tft.setTextColor(COR_SECUNDARIA, COR_FUNDO);
  tft.drawString(unidade, 295, y + 7);
}


void desenharBotoesNav() {
  tft.fillRect(150, 215, 170, 25, COR_FUNDO);
  for (int i = 0; i < TOTAL_TELAS; i++) {
    uint16_t c = (i == (int)telaAtual) ? COR_DESTAQUE : COR_SECUNDARIA;
    tft.fillCircle(160 + i * 14, 228, 3, c);
  }
  tft.fillRoundRect(248, 218, 32, 20, 3, COR_PAINEL);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COR_TEXTO, COR_PAINEL);
  tft.setTextSize(2);
  tft.drawString("<", 264, 228);
  tft.fillRoundRect(285, 218, 32, 20, 3, COR_PAINEL);
  tft.drawString(">", 301, 228);
  tft.setTextSize(1);
}


void desenharIconeClima(int x, int y, const String& code, float escala) {
  bool dia = code.endsWith("d");
  uint16_t corSol  = dia ? COR_DESTAQUE : 0xC618;
  uint16_t corNuv  = 0xCE79;
  uint16_t corChuva = COR_AZUL;
  int r1 = max(3, (int)(22 * escala));
  int r2 = max(2, (int)(14 * escala));
  int r3 = max(2, (int)(16 * escala));
  int r4 = max(2, (int)(12 * escala));
  int off = max(1, (int)(8 * escala));

  if (code.startsWith("01")) {
    tft.fillCircle(x, y, r1, corSol);
    if (dia) {
      for (int i = 0; i < 8; i++) {
        float ang = i * PI / 4;
        int x1 = x + cos(ang) * (r1 + 6 * escala);
        int y1 = y + sin(ang) * (r1 + 6 * escala);
        int x2 = x + cos(ang) * (r1 + 14 * escala);
        int y2 = y + sin(ang) * (r1 + 14 * escala);
        tft.drawLine(x1, y1, x2, y2, corSol);
      }
    } else tft.fillCircle(x + off, y - off/2, r1 - 2, COR_FUNDO);
    return;
  }
  if (code.startsWith("02") || code.startsWith("03") || code.startsWith("04")) {
    if (code.startsWith("02")) tft.fillCircle(x - off, y - off, r2, corSol);
    tft.fillCircle(x - 10*escala, y + 5*escala, r2, corNuv);
    tft.fillCircle(x + 6*escala,  y + 2*escala, r3, corNuv);
    tft.fillCircle(x + 18*escala, y + 8*escala, r4, corNuv);
    tft.fillRect(x - 18*escala, y + 8*escala, 42*escala, 12*escala, corNuv);
    return;
  }
  if (code.startsWith("09") || code.startsWith("10")) {
    if (code.startsWith("10") && dia)
      tft.fillCircle(x - 14*escala, y - 12*escala, 10*escala, corSol);
    tft.fillCircle(x - 10*escala, y - 4*escala, r2, corNuv);
    tft.fillCircle(x + 8*escala,  y - 6*escala, r2, corNuv);
    tft.fillRect(x - 18*escala, y - 4*escala, 32*escala, 10*escala, corNuv);
    for (int i = -12; i <= 14; i += 8)
      tft.drawLine(x + i*escala, y + 10*escala, x + (i-3)*escala, y + 22*escala, corChuva);
    return;
  }
  if (code.startsWith("11")) {
    tft.fillCircle(x - 10*escala, y - 4*escala, r2, corNuv);
    tft.fillCircle(x + 8*escala,  y - 6*escala, r2, corNuv);
    tft.fillRect(x - 18*escala, y - 4*escala, 32*escala, 10*escala, corNuv);
    tft.fillTriangle(x - 4*escala, y + 8*escala, x + 6*escala, y + 8*escala, x, y + 18*escala, corSol);
    return;
  }
  if (code.startsWith("13")) {
    tft.fillCircle(x - 10*escala, y - 4*escala, r2, corNuv);
    tft.fillCircle(x + 8*escala,  y - 6*escala, r2, corNuv);
    tft.fillRect(x - 18*escala, y - 4*escala, 32*escala, 10*escala, corNuv);
    for (int i = -12; i <= 14; i += 8)
      tft.drawCircle(x + i*escala, y + 16*escala, max(1, (int)(2*escala)), COR_TEXTO);
    return;
  }
  if (code.startsWith("50")) {
    int linhas = max(3, (int)(5 * escala));
    for (int i = 0; i < linhas; i++)
      tft.drawFastHLine(x - 22*escala, y - 14*escala + i * 7*escala, 44*escala, corNuv);
    return;
  }
  tft.fillCircle(x, y, r1 - 4, corNuv);
}


String direcaoVento(int graus) {
  const char* dirs[] = {"N","NE","L","SE","S","SO","O","NO"};
  return String(dirs[(int)((graus + 22.5) / 45.0) % 8]);
}

String formatarHora(long timestamp) {
  if (timestamp == 0) return "--:--";
  time_t t = timestamp + GMT_OFFSET_SEC;
  struct tm* ti = gmtime(&t);
  char buf[8];
  sprintf(buf, "%02d:%02d", ti->tm_hour, ti->tm_min);
  return String(buf);
}

String diaSemana(long timestamp) {
  time_t t = timestamp + GMT_OFFSET_SEC;
  struct tm* ti = gmtime(&t);
  const char* dias[] = {"DOM","SEG","TER","QUA","QUI","SEX","SAB"};
  return String(dias[ti->tm_wday]);
}

void mostrarErro(const String& msg) {
  tft.fillScreen(COR_FUNDO);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COR_VERMELHO, COR_FUNDO);
  tft.setTextSize(2);
  tft.drawString("ERRO", 160, 100);
  tft.setTextSize(1);
  tft.setTextColor(COR_TEXTO, COR_FUNDO);
  tft.drawString(msg, 160, 130);
}
