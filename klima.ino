/*
 * ver 0.5.4
 * 
 * Sources:
 * http://www.jerome-bernard.com/blog/2015/10/04/wifi-temperature-sensor-with-nodemcu-esp8266/
 * https://github.com/esp8266/Arduino/issues/1381
 * http://www.esp8266.com/wiki/doku.php?id=esp8266_gpio_pin_allocations
 * http://cholla.mmto.org/esp8266/pins/
 * https://github.com/mozgy/esp8266/blob/master/WiFi_OLED/Mozz_WiFi_OLED
 */

#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include <ESP_SSD1306.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
extern "C" {
  #include "gpio.h"
  #include "user_interface.h"
//  uint32_t readvdd33(void);
  ADC_MODE(ADC_VCC);
}

#define VERS                    0x00050400      // 0.5.4
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
#define RLY_1                   3               // D9
#define RLY_2                   1               // D10
#define STBUF                   120
#define NOCFGSTR                8
#define DEEPSLEEPVOLT           2579
#define STAT_SLEEP              0x01
#define STAT_HEATING            0x02            // grijanje radi
#define STAT_TEMP1_ADJ          0x04            // temperatura postimana na gumb
#define STAT_MENU               0x08
#define STAT_BTN1_ON            0x10
#define STAT_BTN2_ON            0x20
#define STAT_BTN_PLUS           0x40
#define DISP_SLEEP              7500            // za 7,5 sek. gasi display i aj spat
#define MENUSIZ                 3
#define MENU_EXIT               0
#define MENU_INFO               1
#define MENU_SETUP              2


char ssid[2][8], password[2][20];
const char* greetmsg = "\rKlima za po doma,  ver. %d.%d.%d (c) kek0 2016.\r\n";
int i, j, chipID, Vcc;
unsigned int localPort = 1245, httpPort = 80, syslogPort = 514, dlyc1;
char webSrv[] = "oblak.kek0.net", cfg_dat[] = "/config";
char st1[STBUF], paramURI[STBUF],  WiFiSSID[15], WiFipswd[25];
char st2[NOCFGSTR][8] = { "Date: ", "UpdTi: ", "Temp1: ", "T1CR: ", "TMZC: ", "HSRZ: ", "WUIM: ", "SERM: " };
                        // T1CR - Temp1 correction            [def: 0]
                        // TMZC - timezone (client side)      [CET]
                        // HSRZ - histeresis (temp1)          [0.5]
                        // TCIV - temperature check interval  [2min = 120s]
                        // WUIM - web update interval multiplier
                        //        (120 sec * WUIM = 8 min)    [4]
                        // SERM - serial mode setup           [0]
int parmWUIM = 4, parmSERM;
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
  B00000000, B00011100, B00011000, B00000000 };
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
  B00000000, B01101111, B11100000, B00000000, 
  B00000000, B11101111, B11100001, B00000000, 
  B00000000, B11111111, B11000011, B00000000, 
  B00000000, B11111111, B10000111, B10000000, 
  B00000000, B11111111, B00001111, B10000000, 
  B00000001, B11111110, B00011111, B11000000, 
  B00000001, B11111100, B00111111, B11000000, 
  B00000011, B11111000, B00111111, B11000000, 
  B00000011, B11110000, B10111111, B11000000, 
  B00000011, B11100001, B01111111, B11000000, 
  B00000011, B11000010, B01111111, B11000000, 
  B00000011, B10000000, B01111111, B11000000, 
  B00000011, B00000000, B01101111, B11000000, 
  B00000010, B00010000, B00101111, B11000000, 
  B00000000, B00110000, B00001111, B10000000, 
  B00000000, B01110000, B00000111, B10000000, 
  B00000000, B11110000, B00000111, B00000000, 
  B00000000, B01111000, B00001110, B00000000, 
  B00000000, B00011100, B00011000, B00000000 };
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
char temperatureString[6];
//ADC_MODE(ADC_VCC);
WiFiClient client;
ESP_SSD1306 display(OLED_DC, OLED_RST, OLED_CS);
WiFiUDP udp;
IPAddress syslogIPaddress(10, 27, 49, 5);       // log server
volatile int SysStatus = 0;
long m1s4slp;                                   // milisekundi kad pocinje sleep
int menuLvl = 0,  menuIdx = 0;
char menuItm[MENUSIZ][20] = { "izlaz", "info", "WiFi postav." };


void log1(const char *lmsg) {
  File fi = SPIFFS.open("/log.0", "a+");
  if(fi) {
    fi.seek(0, SeekEnd);
    fi.printf("[%d] %s\n", millis()/1000, lmsg);
    fi.close();
  }
}

void readLog1() {
  String line;
  
  File fi = SPIFFS.open("/log.0", "r");
  if(fi) {
    if(serialMsgEn)
      Serial.println("===== Log =====");
    else
      udp.begin(localPort);

    while(fi.available()) {
      line=fi.readStringUntil('\n');
      if(serialMsgEn)
        Serial.println(line);
      else {
        udp.beginPacket(syslogIPaddress, syslogPort);
        udp.write(line.c_str(), strlen(line.c_str()));
        udp.endPacket();
      }
    }
    if(serialMsgEn)
      Serial.println(" ===== ===== ");
    fi.close();
  }
}

float getTemperature() {

  if(serialMsgEn)
    Serial.println("Requesting DS18B20 temperature:");
  i = 0;
  do {
    DS18B20.requestTemperatures(); 
    parmTemp0 = DS18B20.getTempCByIndex(0);
    delay(100);
    if(serialMsgEn)
      Serial.printf("T: %d.%02d\r", (int)(parmTemp0 + parmT1CR), (int)((parmTemp0 + parmT1CR) *100) %100);
    i++;
  } while ((parmTemp0 == 85.0 || parmTemp0 == (-127.0)) && i <50);
  if(serialMsgEn)
    Serial.println("");
  return parmTemp0 + parmT1CR;
}

bool setupWiFi() {
  int k, notchos=1;
  char prgrs[6] = "-\\|/";

  i = WiFi.scanNetworks();
  if (i == 0) {
    if(serialMsgEn)
      Serial.println("no networks found");
  }
  else {
    for(j=0; j<2; j++) {
      for(k=0; k<i; k++) {
        if(!strcmp(ssid[j], WiFi.SSID(k).c_str()) && notchos) {
          strcpy(WiFiSSID, ssid[j]);
          strcpy(WiFipswd, password[j]);
          notchos = 0;
        }
      }
    }
    if(notchos) {
      display.setCursor(42, 40);
      display.print("nema wifi!!");
      display.display();
      log1("No WiFi!");
      return(false);
    }
    if(serialMsgEn)
      Serial.printf("\r\nConnecting to %s", WiFiSSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFipswd);
    display.setCursor(36, 40);
    display.printf("%s ", WiFiSSID);
    display.display();
    j = 0;
    while (WiFi.status() != WL_CONNECTED && j++ < 20) {
      delay(500);
      if(serialMsgEn)
        Serial.print ( "." );
      display.setCursor(42+ 6*strlen(WiFiSSID), 40);
      display.setTextColor(BLACK);
      display.write(prgrs[(j+3)%4]);
      display.display();
      display.setCursor(42+ 6*strlen(WiFiSSID), 40);
//      display.printf("WiFi: %s %c\r", WiFiSSID, prgrs[j%4]);
      display.setTextColor(WHITE);
      display.write(prgrs[j%4]);
      display.display();
    }
    if(j == 21){
      if(serialMsgEn)
        Serial.printf("\r\nCould not connect to %s", WiFiSSID);
      if(i>=1) {
        log1("No WiFi!");
        return(false);
      }
    } else {
      if(serialMsgEn)
        Serial.println("  OK");
      //Serial.print("  IP: ");
      //Serial.println(WiFi.localIP());
      //Serial.setDebugOutput(true);
      //WiFi.printDiag(Serial);
    }
  }
  return(true);
}

void parmStr2Var(int vi) {
  if(Parms[vi].stfin == 1) {
    switch(vi) {
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
        break;
      case 7:         // SERM
        parmSERM = atoi(Parms[vi].st1);
        break;
    }
  }
}

int getParms() {
  char in[STBUF];

  // Read all the lines of the reply from server
  j=0;
  while(client.available()) {
    in[j] = client.read();
    //Serial.printf("{%c:", in[j]);
    //Serial.printf("%c", in[j]);
    for(i=0; i<NOCFGSTR; i++) {
      //Serial.printf("%c%%", st2[i][j]);
      if(j < Parms[i].st2len) {
        if(in[j] == st2[i][j]) {
          //Serial.print("\\");
          Parms[i].stfnd++;
        }
      } else if(Parms[i].stfnd == Parms[i].st2len) {
        //Serial.printf("%d#", i);
        Parms[i].st1[j- Parms[i].st2len] = in[j];
      }
    }
    //Serial.println("");

    if(in[j] == '\r' || in[j] == '\n' || j == STBUF-1) {
      for(i=0; i<NOCFGSTR; i++) {
        if((Parms[i].stfnd == Parms[i].st2len) && (Parms[i].stfin == 0)) {
          Parms[i].st1[j- Parms[i].st2len] = '\0';
          //Serial.printf(" %%P%d %% %s: %s;\r\n", i, Parms[i].st2, Parms[i].st1);
          Parms[i].stfin = 1;
        }
        Parms[i].stfnd=0;
      }
      //Serial.println("\r\n");
      j=0;
    } else {
      j++;
    }
  }
  client.stop();
  delay(100);
  for(i=2; i<NOCFGSTR; i++) {
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

  i=0;
  f1 = SPIFFS.open(cfg_dat, "r");
  if(f1) {
    //f1.fscanf(f1, "KVPM=%2x\n", &ver_parmc);
    line = f1.readStringUntil('\n');
    //Serial.printf("li: %s;\r\n", line.c_str());
    //sscanf(line, "KVPM=%2x", &ver_parmc);
    s1 = String("KVPM");
    s2 = line.substring(s1.length() + 1);
    ver_parmc = s2.substring(0, 1).toInt() << 4;
    c = s2.substring(1, 2).c_str()[0];
    if(c >= 'a' && c <= 'f')
      ver_parmc += ((int)c - (int)'a' + 10);
    else
      ver_parmc += ((int)c - (int)'0');
    //Serial.printf("s2: %s [%x - %c];\r\n", s2.c_str(), ver_parmc, c);
    ver = ((ver_parmc >> 5) & 0x07);
    parmc = (ver_parmc & 0x1f);
    //if(serialMsgEn)
    //  Serial.printf("v:%d; PRMC:%d\r\n", ver, parmc);
    if(ver != 1 || parmc <=0)
      return 0;
    param = (char **)malloc(sizeof(char *) * parmc);
    val = (char **)malloc(sizeof(char *) * parmc);
    while(f1.available() && i < parmc) {
      param[i] = (char *)malloc(8);
      val[i] = (char *)malloc(15);
      //f1.scanf(f1, "%4s=%s\n", param[i], val[i]);
      line = f1.readStringUntil('\n');
      if(serialMsgEn)
        Serial.printf("li[%d]: %s;\r\n", i, line.c_str());
      //sscanf(line, "%4s=%s\n", param[i], val[i]);
      if(strlen(line.c_str()) >0) {       // izbjeg. prazne linije
        strcpy(param[i], line.substring(0, line.indexOf("=")).c_str());
        strcpy(val[i], line.substring(strlen(param[i]) +1).c_str());
        i++;
      }
    }
    f1.close();
    paramURI[0] = '\0';
    for(i=0; i<parmc; i++) {
      if(!strcmp(param[i], "SSID"))
        strcpy(ssid[1], val[i]);
      else if(!strcmp(param[i], "WPSW"))
        strcpy(password[1], val[i]);
      else if(!strcmp(param[i], "Temp1"))
        strcpy(Parms[2].st1, val[i]);
      else {
        for(j=3; j<NOCFGSTR; j++) {
          if(!strncmp(Parms[j].st2, param[i], 4)) {
            strcpy(Parms[j].st1, val[i]);
            sprintf(paramURI, "%s&%s=%s", paramURI, param[i], val[i]);
          }
        }
      }
      //Serial.printf("%d: %s => %s\r\n", i, param[i], val[i]);
    }
    for(i=0; i<parmc; i++) {
      free(param[i]);
      free(val[i]);
    }
    free(param);
    free(val);
  } else {
    f1 = SPIFFS.open(cfg_dat, "a");
    f1.printf("KVPM=%x\n", 0x28);
    f1.printf("Temp1=19\n");
    f1.printf("T1CR=0\n");
    f1.printf("TMZC=CET\n");
    f1.printf("HSRZ=0.5\n");
    f1.printf("WUIM=4\n");
    f1.printf("SERM=1\n");
    f1.close();
  }
  return(0);
}

int saveConfig() {
  char parmNam[5];
  int ver_parmc;
  File f1;
  
  j=0;
  strcpy(st1, "");
  // prvo Temp1, ime dulje za 1 char (5)
  if(strlen(Parms[2].st1) > 0) {
    strncpy(parmNam, Parms[2].st2, 5);
    parmNam[5] = '\0';              // strncpy ne stavlja '\0' na kraj
    sprintf(st1, "%s\n%s=%s", st1, parmNam, Parms[2].st1);
    j++;
  }
  for(i=3; i<NOCFGSTR; i++) {
    if(strlen(Parms[i].st1) > 0) {
      strncpy(parmNam, Parms[i].st2, 4);
      parmNam[4] = '\0';              // strncpy ne stavlja '\0' na kraj
      sprintf(st1, "%s\n%s=%s", st1, parmNam, Parms[i].st1);
      j++;
    }
  }
  ver_parmc = 0x20 + 2 + j;       // b001 xxxxx
  f1 = SPIFFS.open(cfg_dat, "w");
  f1.printf("KVPM=%x\n", ver_parmc);
  f1.printf("SSID=%s\nWPSW=%s", WiFiSSID, WiFipswd);
  f1.print(st1);
  f1.close();
}

int drvaIugljen() {
  String line;
  char URI1[12]="", PlGs[4]="";

  i=0;
  while(!client.connect(webSrv, httpPort) && i<5) {
    if(serialMsgEn)
      Serial.printf("> %d ", i);
    delay(200);
    i++;
  }
  if(i==5) {
    if(serialMsgEn)
      Serial.printf("connection to server failed.\r\n");
    display.printf("\nserver nedostup.\n");
    display.display();
    //return -1;
  } else {
    if(parmTemp1 == 0) {
      strcpy(URI1, "&pm=1");
    } else if(SysStatus & STAT_TEMP1_ADJ) {
      sprintf(URI1, "&t1=%d.%02d", (int)parmTemp1, (int)(parmTemp1*100) %100);
    }
    if(SysStatus & STAT_HEATING) {
      strcpy(PlGs, "pl");
    } else {
      strcpy(PlGs, "gs");
    }
    sprintf(st1, "GET /vatra/ajde/?ci=%08x&t0=%s&xi=Vcc:%d.%03d,Stu:%s%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                        chipID, temperatureString, Vcc/1000, Vcc%1000, PlGs, URI1, webSrv);
    //Serial.printf("ajde strlen: %d;\r\n", strlen(st1));
    client.print(st1);
    delay(100);
    getParms();
    j=0;
    for(i=2; i<NOCFGSTR; i++) {
      if(Parms[i].stfin != 0) {
        j++;
        if(serialMsgEn)
          Serial.printf(" %%%d (%s): %s *\r\n", i, st2[i], Parms[i].st1);
        parmStr2Var(i);
        Parms[i].stfin=0;
      }
    }
    if(j > 0 || SysStatus & STAT_TEMP1_ADJ) {
      saveConfig();
      SysStatus = SysStatus & ~STAT_TEMP1_ADJ;
    }
    return 0;
  }
  return -1;
}

bool ajdeGrijanje() {
  if(parmTemp0 > 0 && parmTemp0 < 50) {
    if((parmTemp0 + parmT1CR > parmTemp1 + parmHSRZ /2) && (SysStatus & STAT_HEATING)) {
      if(serialMsgEn)
        Serial.println("Gasi grijanje");
      SysStatus = SysStatus & ~STAT_HEATING;
    } else if((parmTemp0 + parmT1CR < parmTemp1 - parmHSRZ /2) && !(SysStatus & STAT_HEATING)) {
      if(serialMsgEn)
        Serial.println("Pali grijanje");
      SysStatus = SysStatus | STAT_HEATING;
    }
    return true;
  } else {
    return false;
  }
}

void crtajStatus() {
  display.fillRect(78, 0, 24, 8, BLACK);
  display.setCursor(78, 0);
  display.printf("[%02x]", SysStatus);
  display.display();
}

void crtajMenu() {
  for(i=0; i<MENUSIZ; i++) {
    display.fillRect(12, 8 + 8 * i, 112, 8, BLACK);
    display.setCursor(12, 8 + 8 * i);
    if(i == menuIdx)  display.printf("\x10 %s \x11", menuItm[i]);
        else          display.printf("  %s  ", menuItm[i]);
  }
  display.display();
}

void menuAkc() {
  int m1s;

  if(menuLvl == 0) {
    switch(menuIdx) {
      case MENU_EXIT:         // izlaz iz menu-a
        SysStatus = SysStatus & ~STAT_MENU;
        display.fillRect(12, 8, 90, 8 * MENUSIZ, BLACK);
        display.display();
        break;
      case MENU_SETUP:
        strcpy(Parms[7].st1, "1");
        saveConfig();
        display.fillRect(12, 8, 90, 8 * MENUSIZ, BLACK);
        display.setCursor(12, 8);
        display.printf("Odspoji tipke i\nrebootaj. Dalje ide\nserijskom vezom.");
        display.display();
        m1s = millis() + 2500;
        while(millis() < m1s)  ;
        break;
      default:
        display.setCursor(12, 8);
        display.printf(" ** ** ", menuIdx);
        display.display();
        m1s = millis() + 500;
        while(millis() < m1s)  ;
    }
  }
}

void pin_ISR1() {
  int m1s;

  if(SysStatus & STAT_SLEEP)  return;
  m1s4slp = millis() + DISP_SLEEP;
  m1s = millis() + 200;
  while(millis() < m1s)  ;
  SysStatus = SysStatus & ~STAT_SLEEP;
  SysStatus = SysStatus | STAT_BTN1_ON;
  crtajStatus();
  if(SysStatus & STAT_BTN2_ON)  return;
  if(SysStatus & STAT_MENU) {
    menuIdx = ++menuIdx % MENUSIZ;
    crtajMenu();
    m1s = millis() + 500;
    while(millis() < m1s)  ;
  } else {
    parmTemp1 += 0.5;
    SysStatus = SysStatus | STAT_TEMP1_ADJ;
    SysStatus = SysStatus | STAT_BTN_PLUS;

    display.fillRect(71, 28, 24, 8, BLACK);
    display.setCursor(35, 28);
    display.printf("  T1: %d.%02dC", (int)parmTemp1, (int)(parmTemp1*100) %100);
    m1s = millis() + 200;
    display.setCursor(0, 8);
    display.print("B1");
    display.display();
    while(millis() < m1s)  ;
    display.fillRect(0, 8, 12, 8, BLACK);
    display.display();
  }
}

void pin_ISR2() {
  int m1s;

  if(SysStatus & STAT_SLEEP)  return;
  m1s4slp = millis() + DISP_SLEEP;
  m1s = millis() + 200;
  while(millis() < m1s)  ;
  SysStatus = SysStatus & ~STAT_SLEEP;
  SysStatus = SysStatus | STAT_BTN2_ON;
  crtajStatus();
  if(SysStatus & STAT_BTN1_ON)  return;
  if(SysStatus & STAT_MENU) {
    crtajMenu();
    m1s = millis() + 500;
    while(millis() < m1s)  ;
//    display.fillRect(46, 16, 36, 8, BLACK);
    menuAkc();
  } else {
    parmTemp1 -= 0.5;
    SysStatus = SysStatus | STAT_TEMP1_ADJ;
    SysStatus = SysStatus & ~STAT_BTN_PLUS;

    display.fillRect(71, 28, 24, 8, BLACK);
    display.setCursor(35, 28);
    display.printf("  T1: %d.%02dC", (int)parmTemp1, (int)(parmTemp1*100) %100);
    m1s = millis() + 200;
    display.setCursor(0, 8);
    display.print("B2");
    display.display();
    while(millis() < m1s)  ;
    display.fillRect(0, 8, 12, 8, BLACK);
    display.display();
  }
}

void OTAupd() {
  if(WiFi.status() == WL_CONNECTED) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    t_httpUpdate_return ret = ESPhttpUpdate.update("http://server/file.bin");
    switch(ret) {
      case HTTP_UPDATE_FAILED:
        display.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        display.display();
        break;
      case HTTP_UPDATE_NO_UPDATES:
        display.printf("HTTP_UPDATE_NO_UPDATES\n");
        display.display();
        break;
      case HTTP_UPDATE_OK:
        display.printf("HTTP_UPDATE_OK\n");
        display.display();
        break;
    }
  }
}

void serialWiFiSetup() {
  int stric;

  j=0;
  Serial.begin(115200);
  Serial.print("\n\n-- Konfiguracija WiFi postavki --\n");
  Serial.print("Daj ime SSID-a: ");
  stric = 1;
  while(stric) {
    if(j = Serial.available() > STBUF) { j = STBUF; }
    Serial.readBytes(st1, j);
    for(i = 0; i < j; i++){
      if(st1[i] == '\r' || st1[i] == '\n'){
        st1[i] = '\0';
        display.println(st1);
        display.display();
        stric = 0;
      } else {
        Serial.print(st1[j]);
        j++;
      }
    }
  }
  j=0;
  Serial.print("Daj lozinku: ");
  stric = 1;
  while(stric) {
    if(j = Serial.available() > STBUF) { j = STBUF; }
    Serial.readBytes(st1, j);
    for(i = 0; i < j; i++){
      if(st1[i] == '\r' || st1[i] == '\n'){
        st1[i] = '\0';
        display.println(st1);
        display.display();
        stric = 0;
      } else {
        j++;
      }
    }
  }

  strcpy(Parms[7].st1, "0");
  saveConfig();
  while(true)  delay(2000);
}

void setup() {
  int dlyc2 = 0;
  char parmNam[5];

  delay(100);
  display.begin(SSD1306_SWITCHCAPVCC);  // Switch OLED
  display.display();
  delay(500);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  for (i=0; i < 168; i++) {
    if (i == '\n') continue;
    display.write(i);
    if ((i > 0) && (i % 21 == 0))
      display.println();
  }
  display.display();
  if(serialMsgEn) {
    Serial.begin(115200);
    Serial.printf(greetmsg, (VERS & 0xFF000000) >> 24, (VERS & 0x00FF0000) >> 16, (VERS & 0x0000FF00) >> 8);
  } else {
    delay(1000);
    pinMode(RLY_1, OUTPUT);
    digitalWrite(RLY_1, 1);
    delay(250);
    digitalWrite(RLY_1, 0);
    pinMode(RLY_2, OUTPUT);
    digitalWrite(RLY_2, 1);
    delay(250);
    digitalWrite(RLY_2, 0);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.printf("ver %d.%d.%d\n", (VERS & 0xFF000000) >> 24, (VERS & 0x00FF0000) >> 16, (VERS & 0x0000FF00) >> 8);
  display.display();
  // setup OneWire bus
  DS18B20.begin();
  chipID = ESP.getChipId();
  if(serialMsgEn)
    Serial.printf("ESP8266 Chip id = %08x\r\n", chipID);
  display.printf("chipID %08x\n", chipID);
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
 
  if(!SPIFFS.begin()) {
    if(serialMsgEn)
      Serial.println("SPIFFS!");
    SPIFFS.format();
    delay(2500);
  }
  Dir dir = SPIFFS.openDir("/");
  if(serialMsgEn)
    Serial.println("dir:");
  while (dir.next()) {
    if(serialMsgEn) {
      Serial.print(dir.fileName());
      Serial.print("\t");
      Serial.println(dir.fileSize());
    }
    if(dir.fileName() == "/log.0" && dir.fileSize() >= 32700)
      SPIFFS.rename("/log.0", "/log.1");
  }
  //readLog1();
  //SPIFFS.remove("/config");

  strcpy(ssid[0], "kek0");
  strcpy(password[0], "k1ju4wIf");
  strcpy(ssid[1], "--");
  strcpy(password[1], "!1");
  for(i=0; i<NOCFGSTR; i++) {
    Parms[i].stfnd=0;        // broj poklapanih znakova
    Parms[i].stfin=0;        // nadjeno poklapanje
    Parms[i].st2len = strlen(st2[i]);
    Parms[i].st1[0] = '\0';
    strcpy(Parms[i].st2, st2[i]);
  }

  readConfig();
  for(i=2; i<NOCFGSTR; i++) {
    Parms[i].stfin =1;
    if(serialMsgEn)
      Serial.printf(" *parm[%s]: %s;\r\n", Parms[i].st2, Parms[i].st1);
    else {
      if(strlen(Parms[i].st1) > 0) {
        strncpy(parmNam, Parms[i].st2, 4);
        parmNam[4] = '\0';              // strncpy ne stavlja '\0' na kraj
        display.printf("%s:%s  ", parmNam, Parms[i].st1);
        if(i%2 > 0)  display.printf("\n");
      }
      display.display();
    }
    parmStr2Var(i);
    Parms[i].stfin =0;
  }
  if(parmSERM) {
    display.printf("** Serial setup **");
    display.display();
    serialWiFiSetup();
    delay(2500);
  }

  dlyc1 = 0;
  display.printf("WiFi: ");
  display.display();
  while(!setupWiFi()) {
    display.printf(" --");
    display.display();
    if(++dlyc2 >= 5) {
//      pinMode(0, OUTPUT);
//      digitalWrite(0, 1);
//      pinMode(2, OUTPUT);
//      digitalWrite(2, 1);
//      ESP.restart();        // ne radi!
      break;
    }
  }
  display.printf(".\n");
  display.display();

  delay(500);
  j=0;
  while(!client.connect(webSrv, httpPort) && j<5) {
    if(serialMsgEn)
      Serial.printf("> %d ", j);
    delay(200);
    j++;
  }
  if(j==5) {
    if(serialMsgEn)
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
    for(i=0; i<NOCFGSTR; i++) {
      if(Parms[i].stfin == 1) {
        if(serialMsgEn)
          Serial.printf(" *%d (%s): %s *\r\n", i, Parms[i].st2, Parms[i].st1);
        delay(100);
      }
    }
  }
  log1(Parms[0].st1);
  saveConfig();
  for(i=0; i<NOCFGSTR; i++) {
    Parms[i].stfin=0;
  }

  pinMode(D2, INPUT);
  pinMode(D3, INPUT);
  attachInterrupt(D2, pin_ISR1, FALLING);
  attachInterrupt(D3, pin_ISR2, FALLING);
  //gpio_init(); // Initilise GPIO pins  - NE jer smrda interrupt attachove
  //wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  //wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
//  char msgbuf[80];
  float temperature;
  int upH, upM, upS, m1s;
  char grije[5];

  if(SysStatus & STAT_MENU) {
    display.display();
    crtajMenu();
    delay(1000);

    if(digitalRead(D2))  SysStatus = SysStatus & ~STAT_BTN1_ON;
    else  SysStatus = SysStatus | STAT_BTN1_ON;
    if(digitalRead(D3))  SysStatus = SysStatus & ~STAT_BTN2_ON;
    else  SysStatus = SysStatus | STAT_BTN2_ON;
    return;
  }
  temperature = getTemperature();
  // convert temperature to a string with two digits before the comma and 2 digits for precision
  dtostrf(temperature, 2, 2, temperatureString);
  if(serialMsgEn)
    Serial.printf("Acquired temperature: %s C\r\n", temperatureString);
  Vcc = ESP.getVcc();
  if(serialMsgEn)
    Serial.printf("ESP VCC: %d.%03dV\r\n", Vcc/1000, Vcc%1000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  if(dlyc1 % parmWUIM == 0) {
    display.setCursor(0,0);
    display.printf(" wifi + srvr", chipID);
    display.drawBitmap(0, 16,  logo32_vatra1, 32, 32, 1);
    display.setCursor(35, 16);
    display.printf("  T0: %sC", temperatureString);
    display.setCursor(35, 28);
    display.printf("  T1: %d.%02dC", (int)parmTemp1, (int)(parmTemp1*100) %100);
    display.setCursor(35, 40);
    display.printf(" Vcc: %d.%03dV", Vcc/1000, Vcc%1000);
    display.setCursor(35, 52);
    display.printf(" WkU: %d", dlyc1);
    display.display();
    if(WiFi.status() != WL_CONNECTED) {
      if(serialMsgEn)
        Serial.println("WiFi reconnect!");
      WiFi.disconnect();
      delay(2500);
      setupWiFi();
    }
    drvaIugljen();
//    dlyc1 = 0;
  }
  dlyc1++;
  if(ajdeGrijanje()) {
    if(SysStatus & STAT_HEATING) {
      strcpy(grije, "upal");
      if(!serialMsgEn)  digitalWrite(RLY_2, 1);
    }
    else {
      strcpy(grije, "ugas");
      if(!serialMsgEn)  digitalWrite(RLY_2, 0);
    }
  } else  strcpy(grije, "*T!*");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  if(SysStatus & STAT_HEATING) {
    display.drawBitmap(0, 16,  logo32_vatra1, 32, 32, 1);
  } else {
    display.drawBitmap(0, 16,  logo32_vatra0, 32, 32, 1);
  }
  display.setCursor(0, 0);
  display.printf("-%08x-   [%02x]", chipID, SysStatus);
  display.setCursor(35, 16);
  display.printf("  T0: %sC", temperatureString);
  display.setCursor(35, 28);
  display.printf("  T1: %d.%02dC", (int)parmTemp1, (int)(parmTemp1*100) %100);
  display.setCursor(35, 40);
  display.printf(" Vcc: %d.%03dV", Vcc/1000, Vcc%1000);
  display.setCursor(35, 52);
  display.printf(" WkU: %d", dlyc1);
  //display.println(0xDEADBEEF, HEX);
  m1s4slp = millis();
  m1s4slp += 7500;

  display.setCursor(0, 52);
  display.printf("{%s}", grije);
  display.display();

  if(Vcc > DEEPSLEEPVOLT) {
    while(millis() < m1s4slp) {
      if(digitalRead(D2))  SysStatus = SysStatus & ~STAT_BTN1_ON;
      else  SysStatus = SysStatus | STAT_BTN1_ON;
      if(digitalRead(D3))  SysStatus = SysStatus & ~STAT_BTN2_ON;
      else  SysStatus = SysStatus | STAT_BTN2_ON;

      crtajStatus();
      if((SysStatus & STAT_BTN1_ON) && (SysStatus & STAT_BTN2_ON)) {
        if(SysStatus & STAT_BTN_PLUS)  parmTemp1 -= 0.5;  //
        else  parmTemp1 += 0.5;                           // vrati promjenu T1
        SysStatus |= STAT_MENU;
        display.setCursor(35, 52);
        display.printf(" -- menu -- ");
        display.display();
        m1s = millis() + 500;
        while(millis() < m1s)  ;
//        display.fillRect(35, 52, 72, 8, BLACK);
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        crtajMenu();
      }
      delay(500);
    }
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
    delay(100);
    display.setCursor(78, 0);
    display.printf("[%02x]", SysStatus);
    display.display();

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
    display.clearDisplay();
    display.setCursor(15, 24);
    display.printf("* DS * Vcc: %d.%03dV", Vcc/1000, Vcc%1000);
    display.display();
    ESP.deepSleep(900000000, WAKE_RF_DEFAULT);
  }
  if(serialMsgEn)
    Serial.println("");
}

