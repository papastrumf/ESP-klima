/*
   ver 0.7.2

   Sources:
   http://www.jerome-bernard.com/blog/2015/10/04/wifi-temperature-sensor-with-nodemcu-esp8266/
   https://github.com/esp8266/Arduino/issues/1381
   http://www.esp8266.com/wiki/doku.php?id=esp8266_gpio_pin_allocations
   http://cholla.mmto.org/esp8266/pins/
   https://github.com/mozgy/esp8266/blob/master/WiFi_OLED/Mozz_WiFi_OLED
*/

#include <ESP8266WiFi.h>
//#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include <ESP_SSD1306.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
extern "C" {
#include "gpio.h"
#include "user_interface.h"
  //  uint32_t readvdd33(void);
  //ADC_MODE(ADC_VCC);
}

#define VERS                    0x0007021c
#define D0                      16              // GPIO16
#define D1                      5               // GPIO5
#define D2                      4               // GPIO4
#define D3                      0               // GPIO0
#define D4                      2               // GPIO2
#define D5                      14              // GPIO14
#define D6                      12              // GPIO12
#define D7                      13              // GPIO13
#define D8                      15              // GPIO15
#define OLED_MOSI               13              // HMOSI
#define OLED_CLK                14              // HCLK
#define OLED_DC                 5               // D1
#define OLED_CS                 15              // HCS
#define OLED_RST                16              // D0
#define RLY_T                   3               // D9
#define RLY_E                   1               // D10
#define STBUF                   120
#define NOCFGSTR                9
#define DEEPSLEEPVOLT           2674
#define OTA_MINVOLT             2750
#define STAT_SLEEP              0x01
#define STAT_HEATING            0x02            // grijanje radi
#define STAT_TEMP1_ADJ          0x04            // temperatura postimana na gumb
#define STAT_MENU               0x08
#define STAT_BTN1_ON            0x10
#define STAT_BTN2_ON            0x20
#define STAT_BTN_PLUS           0x40
#define DISP_SLEEP              7500            // za 7,5 sek. gasi display i aj spat
#define MENUSIZ                 5
#define MENU_EXIT               0
#define MENU_INFO               1
#define MENU_SETUP              2
#define MENU_ERASE_FLASH        3
#define MENU_LOG_FILE           4
#define ERASE_FLASH             false
#define DEEPSLPCNT_THR          3
#define ENRG_MODE_BATT          false           // Nrg mode
#define ENRG_GRID_DELAY         120000          // ms


ADC_MODE(ADC_VCC);
char WiFssid[2][15], WiFpsk[2][20];
const char* greetmsg = "\r\nKlima za po doma,  ver. %s (c) kek0 2018.\r\nEnergy mode %s\r\n";
int i, j, chipID, Vcc;
unsigned int localPort = 1245, httpPort = 80, syslogPort = 514, dlyc1, DScnt = 0;
char webSrv[] = "oblak.kek0.net", cfg_dat[] = "/config";
char st1[STBUF + 29], paramURI[STBUF],  WiFiSSID[15], WiFipswd[25];
char st2[NOCFGSTR][8] = { "Date: ", "UpdTi: ", "Temp1: ", "T1CR: ", "TMZC: ", "HSRZ: ", "WUIM: ", "SERM: ", "FWVR: " };
// T1CR - Temp1 correction            [def: 0]
// TMZC - timezone (client side)      [CET]
// HSRZ - histeresis (temp1)          [0.5]
// TCIV - temperature check interval  [2min = 120s]
// WUIM - web update interval multiplier
//        (120 sec * WUIM = 8 min)    [4]
// SERM - serial mode setup           [0]
// FWVR - firmware version            [0x0005070b = 0.5.7(b)]
int parmWUIM = 4, parmSERM, parmFWVR = 0x0005070b;
float parmTemp0, parmTemp1, parmT1CR, parmHSRZ;
char parmTMZC[5];
static const unsigned char PROGMEM logo32_vatra1[] = {
  B00000000, B00000001, B10000000, B00000000,
  B00000000, B00000011, B10000000, B00000000,
  B00000000, B00000111, B00000000, B00000000,
  B00000000, B00000111, B00000000, B00000000,
  B00000000, B00001111, B00000000, B00000000,
  B00000000, B00001111, B00000000, B00000000,
  B00000000, B00001111, B00000000, B00000000,
  B00000000, B00011111, B00000000, B00000000,
  B00000000, B00011111, B00000001, B00000000,
  B00000000, B00011111, B10000011, B00000000,
  B00000000, B10011111, B10000111, B00000000,
  B00000000, B11011111, B11001111, B00000000,
  B00000000, B11101111, B11001111, B00000000,
  B00000000, B01101111, B11101111, B00000000,
  B00000000, B11101111, B11111111, B00000000,
  B00000000, B11111111, B11111111, B00000000,
  B00000000, B11111111, B11111111, B10000000,
  B00000000, B11111111, B11111111, B10000000,
  B00000001, B11111111, B11111111, B11000000,
  B00000001, B11111111, B11111111, B11000000,
  B00000011, B11111111, B10111111, B11000000,
  B00000011, B11111111, B10111111, B11000000,
  B00000011, B11111111, B01111111, B11000000,
  B00000011, B11111010, B01111111, B11000000,
  B00000011, B11111000, B01111111, B11000000,
  B00000011, B11110000, B01101111, B11000000,
  B00000011, B11110000, B00101111, B11000000,
  B00000001, B11110000, B00001111, B10000000,
  B00000001, B11110000, B00000111, B10000000,
  B00000000, B11110000, B00000111, B00000000,
  B00000000, B01111000, B00001110, B00000000,
  B00000000, B00011100, B00011000, B00000000
};
static const unsigned char PROGMEM logo32_vatra0[] = {
  B00000000, B00000001, B10000000, B00000000,
  B00000000, B00000011, B10000000, B00000000,
  B00000000, B00000111, B00000000, B00000000,
  B00000000, B00000111, B00000000, B00000000,
  B00000000, B00001111, B00000000, B00000000,
  B00000000, B00001111, B00000000, B00000000,
  B00000000, B00001111, B00000000, B00000000,
  B00000000, B00011111, B00000000, B00000000,
  B00000000, B00011111, B00000001, B00000000,
  B00000000, B00011111, B10000011, B00000000,
  B00000000, B10011111, B10000110, B00000000,
  B00000000, B11011111, B11001100, B00000000,
  B00000000, B11101111, B11001000, B00000000,
  B00000000, B01101111, B11000000, B00000000,
  B00000000, B11101111, B11000000, B00000000,
  B00000000, B11111111, B10000001, B00000000,
  B00000000, B11111111, B00000011, B10000000,
  B00000000, B11111110, B00000111, B10000000,
  B00000001, B11111100, B00001111, B11000000,
  B00000001, B11111000, B00011111, B11000000,
  B00000011, B11110000, B00111111, B11000000,
  B00000011, B11100000, B10111111, B11000000,
  B00000011, B11000001, B01111111, B11000000,
  B00000011, B10000010, B01111111, B11000000,
  B00000011, B00000000, B01111111, B11000000,
  B00000000, B00000000, B01101111, B11000000,
  B00000000, B00010000, B00101111, B11000000,
  B00000000, B00110000, B00001111, B10000000,
  B00000000, B01110000, B00000111, B10000000,
  B00000000, B11110000, B00000111, B00000000,
  B00000000, B01111000, B00001110, B00000000,
  B00000000, B00011100, B00011000, B00000000
};
static const unsigned char PROGMEM thermomtr[] = {
  B00010000,
  B00101000,
  B00101000,
  B00101000,
  B00101000,
  B00111000,
  B00111000,
  B00111000,
  B00111000,
  B01011100,
  B01011100,
  B00111000
};
static const unsigned char PROGMEM kruzic[] = {
  B0110,
  B1001,
  B1001,
  B0110,
};
bool serialMsgEn = false;
struct _strk {
  char st1[40];
  char st2[8];
  int stfnd;
  int stfin;
  int st2len;
} Parms[NOCFGSTR];

OneWire oneWire(D4);
DallasTemperature DS18B20(&oneWire);
char temperatureString[6], swVer[15];
WiFiClient client;
ESP_SSD1306 display(OLED_DC, OLED_RST, OLED_CS);
WiFiUDP udp;
IPAddress syslogIPaddress(10, 27, 49, 5);       // log server
volatile int SysStatus = 0;
long m1s4slp;                                   // milisekundi kad pocinje sleep
int menuLvl = 0,  menuIdx = 0;
char menuItm[MENUSIZ][20] = { "izlaz", "info", "WiFi postav.", "brisi flash", "log dato." };


void log1(const char *lmsg) {
  File fi = SPIFFS.open("/log.0", "a+");
  if (fi) {
    fi.seek(0, SeekEnd);
    fi.printf("[%d] %s\n", millis() / 1000, lmsg);
    fi.close();
  }
}

float getTemperature() {

  if (serialMsgEn)
    Serial.println("Requesting DS18B20 temperature:");
  i = 0;
  do {
    DS18B20.requestTemperatures();
    parmTemp0 = DS18B20.getTempCByIndex(0);
    delay(100);
    if (serialMsgEn)
      Serial.printf(" T: %d.%02d;", (int)(parmTemp0 + parmT1CR), (int)((parmTemp0 + parmT1CR) * 100) % 100);
    i++;
  } while ((parmTemp0 == 85.0 || parmTemp0 == (-127.0)) && i < 12);
  if (serialMsgEn)
    Serial.println("");
  return parmTemp0 + parmT1CR;
}

bool setupWiFi() {
  int k, notchos = 1;
  char prgrs[6] = "-\\|/";

  i = WiFi.scanNetworks();
  if (i == 0) {
    if (serialMsgEn)
      Serial.println("no networks found");
  }
  else {
    for (j = 0; j < 2; j++) {
      for (k = 0; k < i; k++) {
        if (!strcmp(WiFssid[j], WiFi.SSID(k).c_str()) && notchos) {
          strcpy(WiFiSSID, WiFssid[j]);
          strcpy(WiFipswd, WiFpsk[j]);
          notchos = 0;
        }
      }
    }
    display.fillRect(32, 56, 95, 8, BLACK);
    if (notchos) {
      display.setCursor(42, 56);
      display.print("nema wifi!!");
      display.display();
      log1("No WiFi!");
      return (false);
    }
    if (serialMsgEn)
      Serial.printf("\r\nConnecting to %s", WiFiSSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFipswd);
    display.setCursor(36, 56);
    strncpy(st1, WiFiSSID, 9);
    st1[9] = '\0';
    display.printf("%s ", st1);
    display.display();
    j = 0;
    while (WiFi.status() != WL_CONNECTED && j++ < 20) {
      delay(500);
      if (serialMsgEn)
        Serial.print ( "." );
      display.setCursor(42 + 6 * strlen(st1), 56);
      display.setTextColor(BLACK);
      display.write(prgrs[(j + 3) % 4]);
      display.display();
      display.setCursor(42 + 6 * strlen(st1), 56);
      display.setTextColor(WHITE);
      display.write(prgrs[(j) % 4]);
      display.display();
    }
    if (j == 21) {
      if (serialMsgEn)
        Serial.printf("\r\nCould not connect to %s", WiFiSSID);
      if (i >= 1) {
        log1("No WiFi!");
        return (false);
      }
    } else {
      if (serialMsgEn)
        Serial.println("  OK");
      //Serial.print("  IP: ");
      //Serial.println(WiFi.localIP());
      //Serial.setDebugOutput(true);
      //WiFi.printDiag(Serial);
    }
  }
  return (true);
}

void readLog1() {
  String line;
  int li = 0;

  File fi = SPIFFS.open("/log.0", "r");
  if (fi) {
    if (serialMsgEn)
      Serial.println("===== Log =====");
    else {
      display.setCursor(0, 56);
      display.printf("l0");
      display.display();
      if (WiFi.status() != WL_CONNECTED) {
        if (serialMsgEn)
          Serial.println("WiFi reconnect!");
        WiFi.disconnect();
        delay(1200);
        setupWiFi();
      }
      udp.begin(localPort);
    }

    while (fi.available()) {
      line = fi.readStringUntil('\n');
      if (serialMsgEn)
        Serial.println(line);
      else {
        udp.beginPacket(syslogIPaddress, syslogPort);
        udp.write(line.c_str(), strlen(line.c_str()));
        udp.endPacket();
      }
      display.fillRect(12, 56, 12, 8, BLACK);
      display.printf("%2d", li);
      li++;
    }
    if (serialMsgEn)
      Serial.println(" ===== ===== ");
    else {
      display.fillRect(0, 56, 24, 8, BLACK);
      display.setCursor(0, 56);
      display.printf("l.");
      display.display();
    }
    fi.close();
  }
}

void parmStr2Var(int vi) {
  if (Parms[vi].stfin == 1) {
    switch (vi) {
      case 2:         // Temp1
        parmTemp1 = (float)atof(Parms[vi].st1);
        break;
      case 3:         // T1CR
        parmT1CR = (float)atof(Parms[vi].st1);
        break;
      case 4:         // TMZC
        strcpy(parmTMZC, Parms[vi].st1);
        break;
      case 5:         // HSRZ
        parmHSRZ = (float)atof(Parms[vi].st1);
        break;
      case 6:         // WUIM
        parmWUIM = atoi(Parms[vi].st1);
        if(parmWUIM <= 0)
          parmWUIM = 4; 
        break;
      case 7:         // SERM
        parmSERM = atoi(Parms[vi].st1);
        break;
      case 8:         // FWVR
        parmFWVR = atoi(Parms[vi].st1);
        break;
    }
  }
}

int getParms() {
  char in[STBUF];

  // Read all the lines of the reply from server
  j = 0;
  while (client.available()) {
    in[j] = client.read();
    //Serial.printf("{%c:", in[j]);
    //Serial.printf("%c", in[j]);
    for (i = 0; i < NOCFGSTR; i++) {
      //Serial.printf("%c%%", st2[i][j]);
      if (j < Parms[i].st2len) {
        if (in[j] == st2[i][j]) {
          //Serial.print("\\");
          Parms[i].stfnd++;
        }
      } else if (Parms[i].stfnd == Parms[i].st2len) {
        //Serial.printf("%d#", i);
        Parms[i].st1[j - Parms[i].st2len] = in[j];
      }
    }
    //Serial.println("");

    if (in[j] == '\r' || in[j] == '\n' || j == STBUF - 1) {
      for (i = 0; i < NOCFGSTR; i++) {
        if ((Parms[i].stfnd == Parms[i].st2len) && (Parms[i].stfin == 0)) {
          Parms[i].st1[j - Parms[i].st2len] = '\0';
          //Serial.printf(" %%P%d %% %s: %s;\r\n", i, Parms[i].st2, Parms[i].st1);
          Parms[i].stfin = 1;
        }
        Parms[i].stfnd = 0;
      }
      //Serial.println("\r\n");
      j = 0;
    } else {
      j++;
    }
  }
  client.stop();
  delay(100);
  for (i = 2; i < NOCFGSTR; i++) {
    //Serial.printf(" L%d (%s): %s *\r\n", i, Parms[i].st2, Parms[i].st1);
    parmStr2Var(i);
  }
  return 0;
}

int readConfig() {
  int ver, parmc, ver_parmc;
  File f1;
  String s1, s2;
  char** param;
  char** val;
  String line;
  char c;

  i = 0;
  f1 = SPIFFS.open(cfg_dat, "r");
  if (f1) {
    //f1.fscanf(f1, "KVPM=%2x\n", &ver_parmc);
    line = f1.readStringUntil('\n');
    //if(serialMsgEn)
    //  Serial.printf("li: %s;\r\n", line.c_str());
    //sscanf(line, "KVPM=%2x", &ver_parmc);
    s1 = String("KVPM");    // ver, parm
    s2 = line.substring(s1.length() + 1);
    ver_parmc = s2.substring(0, 1).toInt() << 4;
    c = s2.substring(1, 2).c_str()[0];
    if (c >= 'a' && c <= 'f')
      ver_parmc += ((int)c - (int)'a' + 10);
    else
      ver_parmc += ((int)c - (int)'0');
    //Serial.printf("s2: %s [%x - %c];\r\n", s2.c_str(), ver_parmc, c);
    ver = ((ver_parmc >> 5) & 0x07);
    parmc = (ver_parmc & 0x1f);
    if (serialMsgEn)
      Serial.printf("v:%d; PRMC:%d\r\n", ver, parmc);
    if (ver != 1 || parmc <= 0)
      return 0;
    param = (char **)malloc(sizeof(char *) * parmc);
    val = (char **)malloc(sizeof(char *) * parmc);
    while (f1.available() && i < parmc) {
      param[i] = (char *)malloc(8);
      val[i] = (char *)malloc(25);
      //f1.scanf(f1, "%4s=%s\n", param[i], val[i]);
      line = f1.readStringUntil('\n');
      if (serialMsgEn)
        Serial.printf("li[%d]: %s;\r\n", i, line.c_str());
      //sscanf(line, "%4s=%s\n", param[i], val[i]);
      if (strlen(line.c_str()) > 0) {     // izbjeg. prazne linije
        strcpy(param[i], line.substring(0, line.indexOf("=")).c_str());
        strcpy(val[i], line.substring(strlen(param[i]) + 1).c_str());
        i++;
      }
    }
    f1.close();
    paramURI[0] = '\0';
    for (i = 0; i < parmc; i++) {
      if (!strcmp(param[i], "SSID"))
        strcpy(WiFssid[1], val[i]);
      else if (!strcmp(param[i], "WPSK"))
        strcpy(WiFpsk[1], val[i]);
      else if (!strcmp(param[i], "Temp1"))
        strcpy(Parms[2].st1, val[i]);
      else {
        for (j = 3; j < NOCFGSTR; j++) {
          if (!strncmp(Parms[j].st2, param[i], 4)) {
            strcpy(Parms[j].st1, val[i]);
            sprintf(paramURI, "%s&%s=%s", paramURI, param[i], val[i]);
          }
        }
      }
      //Serial.printf("%d: %s => %s\r\n", i, param[i], val[i]);
    }
    for (i = 0; i < parmc; i++) {
      free(param[i]);
      free(val[i]);
    }
    free(param);
    free(val);
  } else {
    f1 = SPIFFS.open(cfg_dat, "a");
    f1.printf("KVPM=%x\n", 0x26);
    f1.printf("Temp1=19\n");
    f1.printf("T1CR=0\n");
    f1.printf("TMZC=CET\n");
    f1.printf("HSRZ=0.5\n");
    f1.printf("WUIM=4\n");
    f1.printf("SERM=0\n");
    f1.close();
  }
  return (0);
}

int saveConfig() {
  char parmNam[7];
  int ver_parmc;
  File f1;

  j = 0;
  strcpy(st1, "");
  // prvo Temp1, ime dulje za 1 char (5)
  if (strlen(Parms[2].st1) > 0) {
    strncpy(parmNam, Parms[2].st2, 5);
    parmNam[5] = '\0';              // strncpy ne stavlja '\0' na kraj
    sprintf(st1, "%s\n%s=%s", st1, parmNam, Parms[2].st1);
    j++;
  }
  for (i = 3; i < NOCFGSTR - 1; i++) { // ne upisuj zadnji param (FWVR)
    if (strlen(Parms[i].st1) > 0) {
      strncpy(parmNam, Parms[i].st2, 4);
      parmNam[4] = '\0';              // strncpy ne stavlja '\0' na kraj
      sprintf(st1, "%s\n%s=%s", st1, parmNam, Parms[i].st1);
      j++;
    }
  }
  if (strlen(WiFssid[1]) > 0 && strlen(WiFpsk[1]) > 0)  j += 2;
  //sprintf(st1, "\nSSID=%s\nWPSK=%s%s", WiFssid[1], WiFpsk[1], st1);   ovo ne radi dobro!
  ver_parmc = 0x20 + j;       // b001 xxxxx
  f1 = SPIFFS.open(cfg_dat, "w");
  f1.printf("KVPM=%x", ver_parmc);
  if (strlen(WiFssid[1]) > 0 && strlen(WiFpsk[1]) > 0)
    f1.printf("\nSSID=%s\nWPSK=%s", WiFssid[1], WiFpsk[1]);
  f1.print(st1);
  f1.close();
}

int drvaIugljen() {
  String line;
  char URI1[12] = "", PlGs[5] = "";

  i = 0;
  while (!client.connect(webSrv, httpPort) && i < 5) {
    if (serialMsgEn)
      Serial.printf("> %d ", i);
    delay(200);
    i++;
  }
  if (i == 5) {
    if (serialMsgEn)
      Serial.printf("connection to server failed.\r\n");
    display.printf("\nserver nedostup.\n");
    display.display();
  } else {
    if (parmTemp1 == 0) {
      strcpy(URI1, "&pm=1");
    } else if (SysStatus & STAT_TEMP1_ADJ) {
      sprintf(Parms[2].st1, "%d.%02d", (int)parmTemp1, (int)(parmTemp1 * 100) % 100);
      sprintf(URI1, "&t1=%s", Parms[2].st1);
    }
    if (SysStatus & STAT_HEATING) {
      strcpy(PlGs, "PaLi");
    } else {
      strcpy(PlGs, "GaSi");
    }
    sprintf(st1, "GET /vatra/ajde/?ci=%08x&t0=%s&xi=Vcc:%d.%03d,%s,v.%s,wkUp:%d%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
            chipID, temperatureString, Vcc / 1000, Vcc % 1000, PlGs, swVer, dlyc1 / parmWUIM, URI1, webSrv);
    //Serial.printf("ajde strlen: %d;\r\n", strlen(st1));
    client.print(st1);
    delay(100);
    getParms();
    j = 0;
    for (i = 2; i < NOCFGSTR; i++) {
      if (Parms[i].stfin != 0) {
        j++;
        if (serialMsgEn)
          Serial.printf(" %%%d (%s): %s *\r\n", i, st2[i], Parms[i].st1);
        parmStr2Var(i);
        Parms[i].stfin = 0;
      }
    }
    if (j > 0 || SysStatus & STAT_TEMP1_ADJ) {
      saveConfig();
      SysStatus = SysStatus & ~STAT_TEMP1_ADJ;
    }
    return 0;
  }
  return -1;
}

bool ajdeGrijanje() {
  if (parmTemp0 > 0 && parmTemp0 < 50) {
    if ((parmTemp0 + parmT1CR > parmTemp1 + parmHSRZ / 2) && (SysStatus & STAT_HEATING)) {
      if (!serialMsgEn)
        digitalWrite(RLY_T, 0);
      else
        Serial.println("Gasi grijanje");
      SysStatus = SysStatus & ~STAT_HEATING;
      display.fillRect(0, 12, 32, 32, BLACK);
      display.drawBitmap(0, 12, logo32_vatra0, 32, 32, 1);
      display.display();
    } else if ((parmTemp0 + parmT1CR < parmTemp1 - parmHSRZ / 2) && !(SysStatus & STAT_HEATING)) {
      if (!serialMsgEn)
        digitalWrite(RLY_T, 1);
      else
        Serial.println("Pali grijanje");
      SysStatus = SysStatus | STAT_HEATING;
      display.fillRect(0, 12, 32, 32, BLACK);
      display.drawBitmap(0, 12, logo32_vatra1, 32, 32, 1);
      display.display();
    }
    return true;
  } else  return false;
}

void crtajStatus() {
  display.fillRect(90, 0, 24, 8, BLACK);
  display.setCursor(90, 0);
  display.printf("[%02x]", SysStatus);
  display.display();
}

void crtajMenu() {
  for (i = 0; i < MENUSIZ; i++) {
    display.fillRect(12, 8 + 8 * i, 102, 8, BLACK);
    display.setCursor(12, 8 + 8 * i);
    if (i == menuIdx)  display.printf("\x10 %-12s \x11", menuItm[i]);
    else          display.printf("  %s  ", menuItm[i]);
  }
  display.display();
}

void kazIinfo() {
  char parmNam[5];

  display.printf("ver. %s\n", swVer);
  if (strlen(WiFiSSID) > 14) {
    strncpy(st1, WiFiSSID, 14);
    st1[14] = '\0';
  } else {
    strcpy(st1, WiFiSSID);
  }
  display.printf("SSID:%s\n", st1);
  display.display();
  j = 1;
  for (i = 2; i < NOCFGSTR - 1; i++) {  // osim zadnjeg parametra (FWVR)
    if (serialMsgEn)
      Serial.printf(" *parm[%s]: %s;\r\n", Parms[i].st2, Parms[i].st1);
    else {
      if (strlen(Parms[i].st1) > 0) {
        strncpy(parmNam, Parms[i].st2, 4);
        parmNam[4] = '\0';              // strncpy ne stavlja '\0' na kraj
        display.printf("%-4s:%-4s ", parmNam, Parms[i].st1);
        if (j % 2 == 0)  display.printf("\n");
        display.display();
        j++;
      }
    }
  }
}

void crtajE() {
  if (SysStatus & STAT_HEATING) {
    display.drawBitmap(0, 12,  logo32_vatra1, 32, 32, 1);
  } else {
    display.drawBitmap(0, 12,  logo32_vatra0, 32, 32, 1);
  }
  display.setFont(&FreeSans12pt7b);
  display.setCursor(45, 42);
  display.printf("%d.%d C", (int)parmTemp1, (int)(parmTemp1 * 10) % 10);
  //display.drawBitmap(90, 46, kruzic, 4, 4, 1);
  display.setCursor(52, 12);
  display.setFont(&FreeSans9pt7b);
  display.printf("%s C", temperatureString);
  display.drawBitmap(42, 0, thermomtr, 8, 12, 1);
  display.setFont(NULL);
  if(ENRG_MODE_BATT) {
    display.setCursor(48, 56);
    display.printf("%d.%03dV", Vcc / 1000, Vcc % 1000);
  }
  display.setCursor(90, 56);
  display.printf("(%d)", dlyc1);
  display.display();
}

void menuAkc() {
  int m1s;

  if (menuLvl == 0) {
    switch (menuIdx) {
      case MENU_EXIT:         // izlaz iz menu-a
        SysStatus = SysStatus & ~STAT_MENU;
        display.fillRect(12, 8, 96, 55, BLACK);
        display.display();
        crtajE();
        break;
      case MENU_SETUP:
        strcpy(Parms[7].st1, "1");
        saveConfig();
        display.fillRect(12, 8, 96, 8 * MENUSIZ, BLACK);
        display.setCursor(12, 8);
        display.printf("Odspoji relay-e i\nrebootaj. Dalje ide\nserijskom vezom.");
        display.display();
        m1s = millis() + 2500;
        while (millis() < m1s)  ;
        break;
      case MENU_INFO:
        display.fillRect(0, 8, 108, 8 * MENUSIZ, BLACK);
        display.setCursor(0, 8);
        kazIinfo();
        m1s = millis() + 5000;
        while (millis() < m1s)  ;
        display.fillRect(0, 0, 120, 64, BLACK);
        display.display();
        crtajMenu();
        break;
      case MENU_ERASE_FLASH:
        SPIFFS.format();
        display.fillRect(12, 8, 96, 8 * MENUSIZ, BLACK);
        display.setCursor(12, 8);
        display.printf("Flash obrisan!");
        display.display();
        m1s = millis() + 2500;
        while (millis() < m1s)  ;
        break;
      case MENU_LOG_FILE:
        display.fillRect(12, 8, 96, 8 * MENUSIZ, BLACK);
        display.setCursor(12, 8);
        display.printf("Citam log dato.");
        display.display();
        readLog1();
        m1s = millis() + 1000;
        while (millis() < m1s)  ;
        break;
      default:
        display.fillRect(12, 8, 96, 8 * MENUSIZ, BLACK);
        display.setCursor(12, 8);
        display.printf(" ** menu [%d] ** ", menuIdx);
        display.display();
        m1s = millis() + 500;
        while (millis() < m1s)  ;
    }
  }
}

void pin_ISR1() {
  int m1s;

  if (SysStatus & STAT_SLEEP)  return;
  m1s4slp = millis() + DISP_SLEEP;
  m1s = millis() + 200;
  while (millis() < m1s)  ;
  SysStatus = SysStatus & ~STAT_SLEEP;
  SysStatus = SysStatus | STAT_BTN1_ON;
  //crtajStatus();
  if (SysStatus & STAT_BTN2_ON)  return;
  if (SysStatus & STAT_MENU) {
    menuIdx = ++menuIdx % MENUSIZ;
    crtajMenu();
    m1s = millis() + 500;
    while (millis() < m1s)  ;
  } else {
    parmTemp1 += 0.5;
    SysStatus = SysStatus | STAT_TEMP1_ADJ;
    SysStatus = SysStatus | STAT_BTN_PLUS;
    ajdeGrijanje();

    display.fillRect(45, 24, 80, 24, BLACK);
    display.setFont(&FreeSans12pt7b);
    display.setCursor(45, 42);
    display.printf("%d.%d C", (int)parmTemp1, (int)(parmTemp1 * 10) % 10);
    display.setFont(NULL);
    display.display();
  }
}

void pin_ISR2() {
  int m1s;

  if (SysStatus & STAT_SLEEP)  return;
  m1s4slp = millis() + DISP_SLEEP;
  m1s = millis() + 200;
  while (millis() < m1s)  ;
  SysStatus = SysStatus & ~STAT_SLEEP;
  SysStatus = SysStatus | STAT_BTN2_ON;
  //crtajStatus();
  if (SysStatus & STAT_BTN1_ON)  return;
  if (SysStatus & STAT_MENU) {
    crtajMenu();
    m1s = millis() + 500;
    while (millis() < m1s)  ;
    menuAkc();
  } else {
    parmTemp1 -= 0.5;
    SysStatus = SysStatus | STAT_TEMP1_ADJ;
    SysStatus = SysStatus & ~STAT_BTN_PLUS;
    ajdeGrijanje();

    display.fillRect(45, 24, 80, 24, BLACK);
    display.setFont(&FreeSans12pt7b);
    display.setCursor(45, 42);
    display.printf("%d.%d C", (int)parmTemp1, (int)(parmTemp1 * 10) % 10);
    display.setFont(NULL);
    display.display();
  }
}

void OTAupd() {
  if (WiFi.status() == WL_CONNECTED) {
    sprintf(st1, "http://%s/vatra/fwupd/%08x", webSrv, chipID);
    //    sprintf(st1, "http://10.27.49.5/klima.bin");
    if (serialMsgEn)
      Serial.printf(" update link: %s\r\n", st1);
    t_httpUpdate_return ret = ESPhttpUpdate.update(st1);
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        display.fillRect(0, 32, 42, 8, BLACK);
        display.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        display.display();
        if (serialMsgEn)
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\r\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        display.fillRect(0, 32, 42, 8, BLACK);
        display.printf("HTTP_UPDATE_NO_UPDATES\n");
        display.display();
        break;
      case HTTP_UPDATE_OK:
        display.fillRect(0, 32, 42, 8, BLACK);
        display.printf("HTTP_UPDATE_OK\n");
        display.display();
        break;
    }
  }
}

void serialWiFiSetup() {
  int len, wifparm, m1s;
  bool stric, tmout = false;
  char serch[25];

  Serial.print("\n\n\r-- Konfiguracija WiFi postavki --");
  m1s = millis() + 45000;
  for (wifparm = 0; wifparm < 2; wifparm++) {
    j = 0;
    st1[0] = '\0';
    switch (wifparm) {
      case 0:
        Serial.print("\n\rDaj ime SSID-a: ");
        break;
      case 1:
        Serial.print("\n\rDaj lozinku: ");
        break;
    }
    stric = true;
    while (stric && !tmout) {
      if (len = Serial.available() > 0) {
        if (len > 24)  len = 24;
        Serial.readBytes(serch, len);
        for (i = 0; i < len; i++) {
          if (serch[i] == '\r' || serch[i] == '\n') {
            st1[j] = '\0';
            //Serial.printf("\n\rwifparm[%d]: %s.", wifparm, st1);
            stric = false;
            m1s += 45000;
          } else {
            if (wifparm == 0)  Serial.print(serch[i]);
            else    Serial.print("*");
            st1[j] = serch[i];
            j++;
          }
        }
      }
      if (millis() > m1s)  tmout = true;
    }
    switch (wifparm) {
      case 0:
        if (!tmout)
          strcpy(WiFssid[1], st1);
        break;
      case 1:
        if (!tmout)
          strcpy(WiFpsk[1], st1);
        break;
    }
  }
  strcpy(Parms[7].st1, "0");
  saveConfig();
  Serial.print("\n\n\r-- Konfiguracija gotova, spoji relay-e i rebootaj. --");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(12, 8);
  display.printf("Postavke gotove.\nSpoji relay-e i\nrebootaj.\n");
  display.display();
  while (true)  delay(2000);
}

void handleDelay() {
  int m1s;

  while (millis() < m1s4slp) {
    if (digitalRead(D2))  SysStatus = SysStatus & ~STAT_BTN1_ON;
    else  SysStatus = SysStatus | STAT_BTN1_ON;
    if (digitalRead(D3))  SysStatus = SysStatus & ~STAT_BTN2_ON;
    else  SysStatus = SysStatus | STAT_BTN2_ON;

    //crtajStatus();
    if ((SysStatus & STAT_BTN1_ON) && (SysStatus & STAT_BTN2_ON)) {
      if (SysStatus & STAT_BTN_PLUS) {
        parmTemp1 -= 0.5;                              //
        SysStatus = SysStatus & ~STAT_BTN_PLUS;
      }
      else  parmTemp1 += 0.5;                          // vrati promjenu T1
      SysStatus |= STAT_MENU;
      display.fillRect(0, 32, 60, 8, BLACK);
      display.setCursor(32, 56);
      display.printf("-- menu --");
      display.display();
      m1s = millis() + 500;
      while (millis() < m1s)  ;
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      crtajMenu();
    }
    delay(500);
  }
}

void setup() {
  int dlyc2 = 0;
  char parmNam[5];
  bool wif5;

  delay(100);
  display.begin(SSD1306_SWITCHCAPVCC);  // Switch OLED
  display.display();
  delay(500);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  for (i = 0; i < 168; i++) {
    if (i == '\n') continue;
    display.write(i);
    if ((i > 0) && (i % 21 == 0))
      display.println();
  }
  display.display();
  sprintf(swVer, "%d.%d.%d(%x)", (VERS & 0xFF000000) >> 24,
    (VERS & 0x00FF0000) >> 16, (VERS & 0x0000FF00) >> 8, VERS & 0X000000FF);
  if (serialMsgEn) {
    Serial.begin(57600);
    Serial.printf(greetmsg, swVer, (ENRG_MODE_BATT ? "BATT" : "GRID"));
  } else {
    delay(500);
    pinMode(RLY_T, OUTPUT);
    pinMode(RLY_E, OUTPUT);
    digitalWrite(RLY_E, 0);
    digitalWrite(RLY_T, 0);
    delay(250);
    digitalWrite(RLY_E, 1);
    delay(250);
    digitalWrite(RLY_T, 1);
    delay(250);
    digitalWrite(RLY_T, 0);
    delay(250);
    digitalWrite(RLY_E, 0);
  }
 
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(18, 0);
  DS18B20.begin();
  chipID = ESP.getChipId();
  if (serialMsgEn)
    Serial.printf("ESP8266 Chip id = %08x\r\n", chipID);
  display.printf("\x10  Klima  \x11\nchipID %08x\nEnrg mode %s\n", chipID, (ENRG_MODE_BATT ? "BATT" : "GRID"));
  display.display();

  //  uint32_t realSize = ESP.getFlashChipRealSize();
  //  uint32_t ideSize = ESP.getFlashChipSize();
  //  FlashMode_t ideMode = ESP.getFlashChipMode();
  //  Serial.printf("Flash real id:   %08X\r\n", ESP.getFlashChipId());
  //  Serial.printf("Flash real size: %u\n\r\n", realSize);
  //  Serial.printf("Flash ide  size: %u\r\n", ideSize);
  //  Serial.printf("Flash ide speed: %u\r\n", ESP.getFlashChipSpeed());
  //  Serial.printf("Flash ide mode:  %s\r\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
  //  if(ideSize != realSize) {
  //    Serial.println("Flash Chip configuration wrong!\r\n");
  //  } else {
  //    Serial.println("Flash Chip configuration ok.\r\n");
  //  }

  if (!SPIFFS.begin() || ERASE_FLASH) {
    if (serialMsgEn)
      Serial.println("SPIFFS!");
    SPIFFS.format();
    delay(2500);
  }
  Dir dir = SPIFFS.openDir("/");
  if (serialMsgEn)
    Serial.println("dir:");
  while (dir.next()) {
    if (serialMsgEn) {
      Serial.print(dir.fileName());
      Serial.print("\t");
      Serial.println(dir.fileSize());
    }
    if (dir.fileName() == "/log.0" && dir.fileSize() >= 32700) {
      if (SPIFFS.exists("/log.1"))
        SPIFFS.remove("/log.1");
      SPIFFS.rename("/log.0", "/log.1");
    }
  }

  strcpy(WiFssid[0], "kek0");
  strcpy(WiFpsk[0], "k1ju4wIf");
  strcpy(WiFssid[1], "");
  strcpy(WiFpsk[1], "");
  for (i = 0; i < NOCFGSTR; i++) {
    Parms[i].stfnd = 0;      // broj poklapanih znakova
    Parms[i].stfin = 0;      // nadjeno poklapanje
    Parms[i].st2len = strlen(st2[i]);
    Parms[i].st1[0] = '\0';
    strcpy(Parms[i].st2, st2[i]);
  }

  readConfig();
  for (i = 2; i < NOCFGSTR - 1; i++) {
    Parms[i].stfin = 1;
    parmStr2Var(i);
    Parms[i].stfin = 0;
  }
  strcpy(WiFiSSID, WiFssid[1]);
  kazIinfo();
  if (parmSERM) {
    display.printf("** Serial setup **");
    display.display();
    serialMsgEn = true;
    Serial.begin(57600);
    serialWiFiSetup();
  }

  dlyc1 = 0;
  display.setCursor(0, 56);
  display.printf("WiFi: ");
  display.display();
  while (!(wif5 = setupWiFi()) && ++dlyc2 >= 5) {
    display.printf(" -%c", dlyc2);
    display.display();
    //      pinMode(0, OUTPUT);
    //      digitalWrite(0, 1);
    //      pinMode(2, OUTPUT);
    //      digitalWrite(2, 1);
    //      ESP.restart();        // ne radi!
  }
  display.printf(".\n");
  display.display();

  delay(500);
  if (wif5) {
    j = 0;
    while (!client.connect(webSrv, httpPort) && j < 5) {
      if (serialMsgEn)
        Serial.printf("> %d ", j);
      delay(200);
      j++;
    }
    if (j == 5) {
      if (serialMsgEn)
        Serial.printf("connection to server failed [p]\r\n");
      display.printf("server nedostup.\n");
      display.display();
    } else {
      sprintf(st1, "GET /vatra/pali?ci=%08x%s HTTP/1.1\r\n", chipID, paramURI);
      //Serial.printf("pali strlen: %d;\r\n", strlen(st1));
      client.print(st1);
      sprintf(st1, "Host: %s\r\nConnection: close\r\n\r\n", webSrv);
      client.print(st1);
      delay(100);
      getParms();
      for (i = 0; i < NOCFGSTR; i++) {
        if (Parms[i].stfin == 1) {
          if (serialMsgEn)
            Serial.printf(" *%d (%s): %s *\r\n", i, Parms[i].st2, Parms[i].st1);
          delay(100);
        }
      }
    }
    log1(Parms[0].st1);
    saveConfig();
    for (i = 0; i < NOCFGSTR; i++) {
      Parms[i].stfin = 0;
    }
  }
  else  log1("No WiFi! *5");

  pinMode(D2, INPUT);
  pinMode(D3, INPUT);
  attachInterrupt(D2, pin_ISR1, FALLING);
  attachInterrupt(D3, pin_ISR2, FALLING);
  //gpio_init(); // Initilise GPIO pins  - NE jer smrda interrupt attachove
  //wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  //wifi_set_sleep_type(LIGHT_SLEEP_T);

  if (!serialMsgEn) {
    Vcc = ESP.getVcc();
    if (Vcc > OTA_MINVOLT) {
      digitalWrite(RLY_E, 1);
    }
  }
}

void loop() {
  float temperature;
  int upH, upM, upS, m1s;
  char grije[5] = "\0";

  if (SysStatus & STAT_MENU) {
    display.display();
    crtajMenu();
    delay(1000);

    if (digitalRead(D2))  SysStatus = SysStatus & ~STAT_BTN1_ON;
    else  SysStatus = SysStatus | STAT_BTN1_ON;
    if (digitalRead(D3))  SysStatus = SysStatus & ~STAT_BTN2_ON;
    else  SysStatus = SysStatus | STAT_BTN2_ON;
    return;
  }
  delay(500);
  temperature = getTemperature();
  // convert temperature to a string with two digits before the comma and 2 digits for precision
  dtostrf(temperature, 2, 2, temperatureString);
  if (serialMsgEn)
    Serial.printf("Acquired temperature: %s C\r\n", temperatureString);
  Vcc = ESP.getVcc();
  if (serialMsgEn)
    Serial.printf("ESP VCC: %d.%03dV\r\n", Vcc / 1000, Vcc % 1000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  if ((int)dlyc1 % parmWUIM == 0) {
    display.setCursor(0, 56);
    display.printf(" wifi + srvr");
    crtajE();
    if (WiFi.status() != WL_CONNECTED) {
      if (serialMsgEn)
        Serial.println("WiFi reconnect!");
      WiFi.disconnect();
      delay(2500);
      setupWiFi();
    }
    drvaIugljen();
  }
  dlyc1++;
  if (!ajdeGrijanje())
    strcpy(grije, "*T!*");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 56);
  display.printf("-%04x-", chipID & 0xFFFF);
  display.setCursor(90, 0);
  //display.printf("[%02x]", SysStatus);
  crtajE();
  m1s4slp = millis();

  if (strlen(grije)) {
    display.fillRect(0, 56, 36, 8, BLACK);
    display.setCursor(0, 56);
    display.printf("{%s}", grije);
    display.display();
  }

  if (parmFWVR > VERS && ESP.getVcc() >= OTA_MINVOLT) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(12, 8);
    display.printf("* novi FW *\n  v:%08x\n\n  Cekaj!", parmFWVR);
    display.setCursor(0, 32);
    display.display();
    if (serialMsgEn)
      Serial.printf("novi firmware spreman (%08x > %08x)\r\n", parmFWVR, VERS);
    OTAupd();
    delay(2500);
  }

  if (Vcc < DEEPSLEEPVOLT) {
    delay(1250);
    Vcc = ESP.getVcc();
    if (Vcc < DEEPSLEEPVOLT)
      DScnt += 1;
  } else {
    if(DScnt > 0)
      DScnt -= 1;
  }
  if(DScnt < DEEPSLPCNT_THR) {
    m1s4slp += DISP_SLEEP;
    handleDelay();
    if(ENRG_MODE_BATT) {
      display.clearDisplay();
      display.display();
      wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
      SysStatus = SysStatus | STAT_SLEEP;
      delay(200);
      gpio_pin_wakeup_enable(GPIO_ID_PIN(D2), GPIO_PIN_INTR_LOLEVEL);   // ne radi sa NEGEDGE!
      gpio_pin_wakeup_enable(GPIO_ID_PIN(D3), GPIO_PIN_INTR_LOLEVEL);
    /*      GPIO_PIN_INTR_DISABLE = 0,
            GPIO_PIN_INTR_POSEDGE = 1,
            GPIO_PIN_INTR_NEGEDGE = 2,
            GPIO_PIN_INTR_ANYEDGE = 3,
            GPIO_PIN_INTR_LOLEVEL = 4,
            GPIO_PIN_INTR_HILEVEL = 5       */
      wifi_fpm_open();
      wifi_fpm_do_sleep(0xFFFFFFF);
      //delay(100);
      //display.setCursor(90, 0);
      //display.printf("[%02x]", SysStatus);
      //display.display();

    //    gpio_pin_wakeup_disable();  NE SMIJE! onemogucuje intr na  tom pinu!
      SysStatus = SysStatus & ~STAT_SLEEP;
      attachInterrupt(D2, pin_ISR1, FALLING);
      attachInterrupt(D3, pin_ISR2, FALLING);
    /*      LOW to trigger the interrupt whenever the pin is low,
            CHANGE to trigger the interrupt whenever the pin changes value
            RISING to trigger when the pin goes from low to high,
            FALLING for when the pin goes from high to low.
            HIGH to trigger the interrupt whenever the pin is high.       */
    } else {
      m1s4slp += ENRG_GRID_DELAY;
      handleDelay();
    }
  } else {
    display.clearDisplay();
    display.setCursor(15, 24);
    display.printf("* DS * Vcc: %d.%03dV", Vcc / 1000, Vcc % 1000);
    display.display();
    if (!serialMsgEn)  digitalWrite(RLY_E, 0);
    delay(100);
    ESP.deepSleep(900000000, WAKE_RF_DEFAULT);
  }
  if (serialMsgEn && ENRG_MODE_BATT)
    Serial.println("");
}

