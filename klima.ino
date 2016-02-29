/*
 * ver 0.4
 * 
 * Sources:
 * http://www.jerome-bernard.com/blog/2015/10/04/wifi-temperature-sensor-with-nodemcu-esp8266/
 * https://github.com/esp8266/Arduino/issues/1381
 */

#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include <SSD1306.h>
#include <SSD1306Ui.h>
#include <SPI.h>
#include <ESP8266HTTPClient.h>
extern "C" {
  #include "gpio.h"
}
extern "C" {
  #include "user_interface.h"
}


#define ONE_WIRE_BUS            4               // DS18B20 pin
#define STBUF                   120
#define NOCFGSTR                8

char ssid[2][8];
char password[2][20];
const char* greetmsg = "\rKlima za po doma,  ver. 0.4 (c) kek0 2016.";
int led = 2, i, j, chipID, Vcc;
unsigned int localPort = 1245, httpPort = 80;
char webSrv[] = "oblak.kek0.net", cfg_dat[] = "/config";
char st1[STBUF], paramURI[50],  WiFiSSID[15], WiFipswd[25];
char st2[NOCFGSTR][9] = { "Date: ", "UpdTi: ", "Temp1: ", "T1CR: ", "TMZC: ", "HSRZ: ", "TCIV: ", "WUIM: " };
                        // T1CR - Temp1 correction            [def: 0]
                        // TMZC - timezone (client side)      [CET]
                        // HSRZ - histeresis (temp1)          [0.5]
                        // TCIV - temperature check interval  [2min = 120s]
                        // WUIM - web update interval multiplier
                        //        (INVL * WUIM = 8 min)       [4]
int parmT1CR, parmTCIV, parmHSRZ, parmWUIM;
char parmTMZC[5];

struct _strk {
  char st1[25];
  char st2[8];
  int stfnd;
  int stfin;
  int st2len;
} Parms[NOCFGSTR];

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
char temperatureString[6];
ADC_MODE(ADC_VCC);
WiFiClient client;


void log1(const char *lmsg) {
  File fi = SPIFFS.open("/log.0", "a+");
  if(fi) {
    fi.seek(0, SeekEnd);
    fi.printf("[%d] %s", millis()/1000, lmsg);
    fi.close();
  }
}

void readLog1() {
  String line;
  
  File fi = SPIFFS.open("/log.0", "r");
  if(fi) {
  Serial.println("===== Log =====");
    while(fi.available()) {
      line=fi.readStringUntil('\n');
      Serial.println(line);
    }
    fi.close();
    Serial.println("=====");
  }
}

float getTemperature() {
  float temp;

  Serial.println("Requesting DS18B20 temperature:");
  i = 0;
  do {
    DS18B20.requestTemperatures(); 
    temp = DS18B20.getTempCByIndex(0);
    delay(100);
    Serial.printf("T: %d.%02d\r", (int)temp, (int)(temp*100)%100);
    i++;
  } while ((temp == 85.0 || temp == (-127.0)) && i <50);
  Serial.println("");
  return temp;
}

bool setupWiFi() {
  int k, notchos=1;

  i = WiFi.scanNetworks();
  if (i == 0)
    Serial.println("no networks found");
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
    Serial.printf("\r\nConnecting to %s", WiFiSSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFipswd);
    j = 0;
    while (WiFi.status() != WL_CONNECTED && j++ < 20) {
      delay(500);
      Serial.print ( "." );
    }
    if(j == 21){
      Serial.printf("\r\nCould not connect to %s", WiFiSSID);
      if(i==1) {
        log1("No WiFi!");
        return(false);
      }
    } else {
      Serial.println("  OK");
      //Serial.print("  IP: ");
      //Serial.println(WiFi.localIP());
      //Serial.setDebugOutput(true);
      //Serial.setDebugOutput(false);
      //WiFi.printDiag(Serial);
    }
  }
  return(true);
}

int getParms() {
  char in[STBUF];

//  Serial.printf("getParms:1\r\n");
  // Read all the lines of the reply from server
  j=0;
//  Serial.printf("\r\ngetParms:2\r\n");
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
//        Serial.printf("[%d,%d]: %s;\r\n", j, _Parms[i].stfnd, Parms[i].st1);
        if((Parms[i].stfnd == Parms[i].st2len) && (Parms[i].stfin == 0)) {
          Parms[i].st1[j- Parms[i].st2len] = '\0';
//          Serial.printf("\n%%%d K: %s;\r\n", i, Parms[i].st1);
          Parms[i].stfin = 1;
        }
        Parms[i].stfnd=0;
      }
      //Serial.println("\r\n");
      j=0;
    } else {
      j++;
    }
//    delay(10);
  }
  client.stop();
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
    Serial.printf("v:%d: PRMC:%d\r\n", ver, parmc);
    if(ver != 1 || parmc <=0)
      return 0;
    param = (char **)malloc(sizeof(char *) * parmc);
    val = (char **)malloc(sizeof(char *) * parmc);
    while(f1.available() && i < parmc) {
      param[i] = (char *)malloc(8);
      val[i] = (char *)malloc(15);
      //f1.scanf(f1, "%4s=%s\n", param[i], val[i]);
      line = f1.readStringUntil('\n');
      Serial.printf("li[%d]: %s;\r\n", i, line.c_str());
      //sscanf(line, "%4s=%s\n", param[i], val[i]);
      strcpy(param[i], line.substring(0, line.indexOf("=")).c_str());
      strcpy(val[i], line.substring(strlen(param[i]) +1).c_str());
      i++;
    }
    f1.close();
    paramURI[0] = '\0';
    for(i=0; i<parmc; i++) {
      if(!strcmp(param[i], "SSID"))
        strcpy(ssid[1], val[i]);
      else if(!strcmp(param[i], "WPSW"))
        strcpy(password[1], val[i]);
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
    f1.printf("KVPM=%x\n", 0x27);
    f1.printf("SSID=IDA\n");
    f1.printf("WPSW=gU2dR!c4\n");
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
//  Serial.println(st1);
  f1.close();
}

int drvaIugljen() {
  String line;

  i=0;
  while(!client.connect(webSrv, httpPort) && i<5) {
    Serial.printf(" > %d\t", i);
    delay(200);
    i++;
  }
  if(i==5) {
    Serial.printf("connection to server %s:%d failed.\r\n", webSrv, httpPort);
    return -1;
  }
  digitalWrite(led, 0);
  sprintf(st1, "GET /vatra/ajde/?ci=%08x&t0=%s&xi=Vcc:%d.%03d HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                      chipID, temperatureString, Vcc/1000, Vcc%1000, webSrv);
  client.print(st1);
  delay(100);
  while(client.available()){
    line = client.readStringUntil('\n');
    Serial.println(line);
    delay(25);
  }
  getParms();
  j=0;
  for(i=3; i<NOCFGSTR; i++) {
    if(Parms[i].stfin != 0) {
      j++;
      Serial.printf(" %%%d (%s): %s *\r\n", i, st2[i], Parms[i].st1);
      Parms[i].stfin=0;
    }
  }
  if(j > 0) {
    saveConfig();
  }

//  client.stop();
  digitalWrite(led, 1);
//  http.begin(webSrv, 80, "/vatra/ajde/?ci=%08x&t0=%stest.html"); //HTTP
  return 0;
}

void setup() {
  delay(2500);
  Serial.begin(115200);
  Serial.println ( greetmsg );
  delay(1500);
  pinMode(led, OUTPUT);
  digitalWrite(led, 1);

  // setup OneWire bus
  DS18B20.begin();
  chipID = ESP.getChipId();
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
 
  if(!SPIFFS.begin()) {
    Serial.println("SPIFFS!");
    SPIFFS.format();
  }
  Dir dir = SPIFFS.openDir("/");
  Serial.println("dir:");
  while (dir.next()) {
    Serial.print(dir.fileName());
    Serial.print("\t");
    Serial.println(dir.fileSize());
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
  setupWiFi();

  delay(250);
  i=0;
  while(!client.connect(webSrv, httpPort) && i<5) {
    Serial.printf(" > %d\t", i);
    delay(200);
    i++;
  }
  if(i==5) {
    Serial.printf("connection to server %s:%d failed [p]\r\n", webSrv, httpPort);
  } else {
    sprintf(st1, "GET /vatra/pali?ci=%08x%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", chipID, paramURI, webSrv);
    client.print(st1);
    delay(100);
    getParms();
    for(i=0; i<NOCFGSTR; i++) {
      if(Parms[i].stfin != 0) {
        Serial.printf(" *%d (%s): %s *\r\n", i, st2[i], Parms[i].st1);
      }
    }
  }
//  log1(Parms[0].st1);
  saveConfig();
  for(i=0; i<NOCFGSTR; i++) {
    Parms[i].stfin=0;        // nadjeno poklapanje
  }

  wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
//  char msgbuf[80];
  float temperature;

  //if (!client.connected()) {
  //  reconnect();
  //}
  //client.loop();

  temperature = getTemperature();
  // convert temperature to a string with two digits before the comma and 2 digits for precision
  dtostrf(temperature, 2, 2, temperatureString);
  // send temperature to the serial console
  Serial.printf("Acquired temperature: %s C\r\n", temperatureString);
  Vcc = ESP.getVcc();
  Serial.printf("ESP VCC: %d.%03dV\r\n", Vcc/1000, Vcc%1000);

  if(WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }
  drvaIugljen();

  Serial.printf("Delaying for %d seconds...\r\n", parmTCIV * parmWUIM);
  parmTCIV=120; parmWUIM=4;
  i=parmTCIV * parmWUIM;
  while(i-- > 0) {
    delay(1000);
  }
  Serial.println("");
}

