/*
 * ver 0.4.8
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
//#include <ESP8266HTTPClient.h>
//#include <gpio.h>
//#include <user_interface.h>
extern "C" {
  #include "gpio.h"
  #include "user_interface.h"
//  uint32_t readvdd33(void);
  ADC_MODE(ADC_VCC);
}

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
#define RLY_1                   0               // D3
#define RLY_2                   4               // D2
#define STBUF                   120
#define NOCFGSTR                8
#define DEEPSLEEPVOLT           2579
#define SERIALMSGEN             true

char ssid[2][8];
char password[2][20];
const char* greetmsg = "\rKlima za po doma,  ver. %s (c) kek0 2016.\r\n";
const char* vers = "0.4.8";
int led = D4, i, j, chipID, Vcc, dlyc1 = 0;
unsigned int localPort = 1245, httpPort = 80;
char webSrv[] = "oblak.kek0.net", cfg_dat[] = "/config";
char st1[STBUF], paramURI[STBUF],  WiFiSSID[15], WiFipswd[25];
char st2[NOCFGSTR][8] = { "Date: ", "UpdTi: ", "Temp1: ", "T1CR: ", "TMZC: ", "HSRZ: ", "TCIV: ", "WUIM: " };
                        // T1CR - Temp1 correction            [def: 0]
                        // TMZC - timezone (client side)      [CET]
                        // HSRZ - histeresis (temp1)          [0.5]
                        // TCIV - temperature check interval  [2min = 120s]
                        // WUIM - web update interval multiplier
                        //        (TCIV * WUIM = 8 min)       [4]
int parmTCIV, parmWUIM;
float parmTemp0, parmTemp1, parmT1CR, parmHSRZ;
char parmTMZC[5];
static const unsigned char PROGMEM logo16_glcd_bmp[] = {
  B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };
bool grijanjeRadi = false;

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
    if(SERIALMSGEN)
      Serial.println("===== Log =====");
    while(fi.available()) {
      line=fi.readStringUntil('\n');
      if(SERIALMSGEN)
        Serial.println(line);
    }
    if(SERIALMSGEN)
      Serial.println("=====");
    fi.close();
  }
}

float getTemperature() {

  if(SERIALMSGEN)
    Serial.println("Requesting DS18B20 temperature:");
  i = 0;
  do {
    DS18B20.requestTemperatures(); 
    parmTemp0 = DS18B20.getTempCByIndex(0);
    delay(100);
    if(SERIALMSGEN)
      Serial.printf("T: %d.%02d\r", (int)parmTemp0, (int)(parmTemp0*100)%100);
    i++;
  } while ((parmTemp0 == 85.0 || parmTemp0 == (-127.0)) && i <50);
  if(SERIALMSGEN)
    Serial.println("");
  return parmTemp0;
}

bool setupWiFi() {
  int k, notchos=1;
  char prgrs[6] = "-\\|/";

  i = WiFi.scanNetworks();
  if (i == 0) {
    if(SERIALMSGEN)
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
    if(SERIALMSGEN)
      Serial.printf("\r\nConnecting to %s", WiFiSSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFipswd);
    display.setCursor(36, 8);
    display.printf("%s ", WiFiSSID);
    display.display();
    j = 0;
    while (WiFi.status() != WL_CONNECTED && j++ < 20) {
      delay(500);
      if(SERIALMSGEN)
        Serial.print ( "." );
      display.setCursor(42+ 6*strlen(WiFiSSID), 8);
      display.setTextColor(BLACK);
      display.write(prgrs[(j+3)%4]);
      display.display();
      display.setCursor(42+ 6*strlen(WiFiSSID), 8);
//      display.printf("WiFi: %s %c\r", WiFiSSID, prgrs[j%4]);
      display.setTextColor(WHITE);
      display.write(prgrs[j%4]);
      display.display();
    }
    if(j == 21){
      if(SERIALMSGEN)
        Serial.printf("\r\nCould not connect to %s", WiFiSSID);
      if(i>=1) {
        log1("No WiFi!");
        return(false);
      }
    } else {
      if(SERIALMSGEN)
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
      case 6:         // TCIV
        parmTCIV = atoi(Parms[vi].st1);
        break;
      case 7:         // WUIM
        parmWUIM = atoi(Parms[vi].st1);
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
    //if(SERIALMSGEN)
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
      if(SERIALMSGEN)
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
    f1.printf("SSID=IDA\n");
    f1.printf("WPSW=gU2dR!c4\n");
    f1.printf("Temp1=19\n");
    f1.printf("T1CR=0\n");
    f1.printf("TMZC=CET\n");
    f1.printf("HSRZ=0.5\n");
    f1.printf("TCIV=120\n");
    f1.printf("WUIM=4\n");
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
  for(i=3; i<NOCFGSTR; i++) {
    if(strlen(Parms[i].st1) > 0) {
      strncpy(parmNam, Parms[i].st2, 4);
      parmNam[4] = '\0';              // strncpy ne stavlja '\0' na kraj
      sprintf(st1, "%s\n%s=%s", st1, parmNam, Parms[i].st1);
      j++;
    }
  }
  ver_parmc = 0x20 + 2 + j;
  f1 = SPIFFS.open(cfg_dat, "w");
  f1.printf("KVPM=%x\n", ver_parmc);
  f1.printf("SSID=%s\nWPSW=%s", WiFiSSID, WiFipswd);
  f1.print(st1);
  f1.close();
}

int drvaIugljen() {
  String line;
  char URI1[7]="";

  i=0;
  while(!client.connect(webSrv, httpPort) && i<5) {
    if(SERIALMSGEN)
      Serial.printf("> %d ", i);
    delay(200);
    i++;
  }
  if(i==5) {
    if(SERIALMSGEN)
      Serial.printf("connection to server failed.\r\n");
    return -1;
  }
  if(parmTemp1 == 0) {
    strcpy(URI1, "&pm=1");
  }
//  digitalWrite(led, 0);
  sprintf(st1, "GET /vatra/ajde/?ci=%08x&t0=%s&xi=Vcc:%d.%03d%%20Up:%d%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                      chipID, temperatureString, Vcc/1000, Vcc%1000, millis()/1000, URI1, webSrv);
  //Serial.printf("ajde strlen: %d;\r\n", strlen(st1));
  client.print(st1);
  delay(100);
//  while(client.available()){
//    line = client.readStringUntil('\n');
//    Serial.println(line);
//    delay(25);
//  }
  getParms();
  j=0;
  for(i=2; i<NOCFGSTR; i++) {
    if(Parms[i].stfin != 0) {
      j++;
      if(SERIALMSGEN)
        Serial.printf(" %%%d (%s): %s *\r\n", i, st2[i], Parms[i].st1);
      parmStr2Var(i);
      Parms[i].stfin=0;
    }
  }
  if(j > 0) {
    saveConfig();
  }

//  client.stop();
//  digitalWrite(led, 1);
//  http.begin(webSrv, 80, "/vatra/ajde/?ci=%08x&t0=%stest.html"); //HTTP
  return 0;
}

bool ajdeGrijanje() {
  if(parmTemp0 > 0 && parmTemp0 < 50) {
    if((parmTemp0 + parmT1CR > parmTemp1 + parmHSRZ /2) && grijanjeRadi) {
      if(SERIALMSGEN)
        Serial.println("Gasi grijanje");
      grijanjeRadi = false;
    } else if((parmTemp0 + parmT1CR < parmTemp1 - parmHSRZ /2) && !grijanjeRadi) {
      if(SERIALMSGEN)
        Serial.println("Pali grijanje");
      grijanjeRadi = true;
    }
    return true;
  } else {
    return false;
  }
}

void setup() {
  delay(100);
  display.begin(SSD1306_SWITCHCAPVCC);  // Switch OLED
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  for (i=0; i < 168; i++) {
    if (i == '\n') continue;
    display.write(i);
    if ((i > 0) && (i % 21 == 0))
      display.println();
  }
  display.display();
  if(SERIALMSGEN) {
    Serial.begin(115200);
    Serial.printf(greetmsg, vers);
  }
  delay(1500);
//  pinMode(led, OUTPUT);
//  digitalWrite(led, 1);
  pinMode(RLY_1, OUTPUT);
  digitalWrite(RLY_1, 1);
  delay(250);
  digitalWrite(RLY_1, 0);
  pinMode(RLY_2, OUTPUT);
  digitalWrite(RLY_2, 1);
  delay(250);
  digitalWrite(RLY_2, 0);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.printf("ver %s\n", vers);
  display.display();
  // setup OneWire bus
  DS18B20.begin();
  chipID = ESP.getChipId();
  if(SERIALMSGEN)
    Serial.printf("ESP8266 Chip id = %08X\r\n", chipID);

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
//  Serial.printf("*bla 1;\r\n");
 
  if(!SPIFFS.begin()) {
    if(SERIALMSGEN)
      Serial.println("SPIFFS!");
    SPIFFS.format();
    delay(2500);
  }
  Dir dir = SPIFFS.openDir("/");
  if(SERIALMSGEN)
    Serial.println("dir:");
  while (dir.next()) {
    if(SERIALMSGEN) {
      Serial.print(dir.fileName());
      Serial.print("\t");
      Serial.println(dir.fileSize());
    }
    if(dir.fileName() == "/log.0" && dir.fileSize() >= 32700)
      SPIFFS.rename("/log.0", "/log.1");
  }
  //readLog1();

  strcpy(ssid[0], "kek0");
  strcpy(password[0], "k1ju4wIf");
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
    //Serial.printf(" *parm[%s]: %s;\r\n", Parms[i].st2, Parms[i].st1);
    parmStr2Var(i);
    Parms[i].stfin =0;
  }
  dlyc1 = parmWUIM;
  display.printf("WiFi: ");
  display.display();
  while(!setupWiFi()) {
    display.printf(" --");
    display.display();
    delay(2500);
  }
  display.printf(" OK\n");
  display.display();

  delay(500);
  j=0;
  while(!client.connect(webSrv, httpPort) && i<5) {

    if(SERIALMSGEN)
      Serial.printf("> %d ", i);
    delay(200);
    j++;
  }
  if(j==5) {
    if(SERIALMSGEN)
      Serial.printf("connection to server failed [p]\r\n");
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
        if(SERIALMSGEN)
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

  //wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
//  char msgbuf[80];
  float temperature;
  int upH, upM, upS;
  char grije[5];

  //if (!client.connected()) {
  //  reconnect();
  //}
  //client.loop();

  temperature = getTemperature();
  // convert temperature to a string with two digits before the comma and 2 digits for precision
  dtostrf(temperature, 2, 2, temperatureString);
  // send temperature to the serial console
  if(SERIALMSGEN)
    Serial.printf("Acquired temperature: %s C\r\n", temperatureString);
  Vcc = ESP.getVcc();
  //Vcc = readvdd33();
  if(SERIALMSGEN)
    Serial.printf("ESP VCC: %d.%03dV\r\n", Vcc/1000, Vcc%1000);

  if(dlyc1 == parmWUIM) {
    if(WiFi.status() != WL_CONNECTED) {
      setupWiFi();
    }
    drvaIugljen();
    dlyc1 = 0;
  }
  dlyc1++;
  if(ajdeGrijanje()) {
    if(grijanjeRadi)  strcpy(grije, "upal");
    else  strcpy(grije, "ugas");
  } else  strcpy(grije, "*T!*");
  if(dlyc1 % 2)  digitalWrite(RLY_1, 0);
  else  digitalWrite(RLY_1, 1);
//  if(dlyc1 % 2)  Serial.println("Bla: gasi");
//  else  Serial.println("Bla: pali");
  if(dlyc1 % 2)  digitalWrite(RLY_2, 1);
  else  digitalWrite(RLY_2, 0);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.printf(" -Ola!  *%08x", chipID);
  display.drawBitmap(8, 24,  logo16_glcd_bmp, 16, 16, 1);
  display.setCursor(35, 16);
  display.printf("  T0: %sC", temperatureString);
  display.setCursor(35, 28);
  display.printf("  T1: %d.%02dC", (int)parmTemp1, (int)(parmTemp1*100) %100);
  display.setCursor(35, 40);
  display.printf(" Vcc: %d.%03dV", Vcc/1000, Vcc%1000);
  //display.println(0xDEADBEEF, HEX);
  upS = millis() /1000;
  upH = upS /3600;
  upM = (upS - upH * 3600) /60;
  upS = upS % 60;
  display.setCursor(35, 52);
  display.printf("  Up: %d:%02d:%02d", upH, upM, upS);
  display.setCursor(0, 52);
  display.printf("{%s}", grije);
  display.display();

  if(Vcc > DEEPSLEEPVOLT) {
    if(SERIALMSGEN)
      Serial.printf("Delaying for %d seconds...\r\n", parmTCIV);
    i=parmTCIV / 10;
//    i=2;
    while(i-- > 0) {
/*
//                  light sleep (pin wakeup ?)
      delay(100);
//      display.clearDisplay();
      display.fillRect(0, 8, 11, 15, BLACK);
      display.setCursor(0, 8);
      display.print("LS");
      display.display();
      wifi_station_disconnect();
      wifi_set_opmode(NULL_MODE);
      wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
//      wifi_set_sleep_type(LIGHT_SLEEP_T);
      wifi_fpm_open();
      gpio_pin_wakeup_enable(GPIO_ID_PIN(RLY_1), GPIO_PIN_INTR_HILEVEL);
      wifi_fpm_do_sleep(0xFFFFFFF);
//      wifi_fpm_do_sleep(0x2625A00);
      delay(10000);

      gpio_pin_wakeup_disable();
      display.fillRect(0, 8, 11, 15, BLACK);
      display.setCursor(0, 8);
      display.print("WU");
      display.display();
      wifi_fpm_close();
      wifi_set_opmode(STATION_MODE);
      wifi_station_connect();
      delay(400);
*/
      WiFi.forceSleepBegin();
      delay(10000);
    }
  } else {
    display.clearDisplay();
    display.setCursor(15, 24);
    display.printf("* DS * Vcc: %d.%03dV", Vcc/1000, Vcc%1000);
    display.display();
    //delay(5000);
    //display.clearDisplay();
    //display.display();
    ESP.deepSleep(900000000, WAKE_RF_DEFAULT);
  }
  if(SERIALMSGEN)
    Serial.println("");
}

