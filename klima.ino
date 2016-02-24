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
#include <WiFiUdp.h>
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


#define SLEEP_DELAY_IN_SECONDS  40
#define ONE_WIRE_BUS            4               // DS18B20 pin
#define STBUF                   120
#define NOCFGSTR                8

char ssid[2][8];
char password[2][20];
const char* greetmsg = "\rKlima za po doma,  ver. 0.4 (c) kek0 2016.";
int led = 2, i, j, chipID, Vcc;
unsigned int localPort = 1245, httpPort = 80;
char webSrv[] = "oblak.kek0.net", cfg_dat[] = "/config";
char st1[STBUF], st2[NOCFGSTR][8] = { "Date: ", "UpdTi: ", "Temp1: ", "T1CR: ", "TMZC: ", "HSRZ: ", "INVL: ", "WUIV: " };

struct _strk {
  char st1[STBUF];
  char st2[15];
  int stfnd;
  int stfin;
  int st2len;
} strk[NOCFGSTR];

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
  char WiFiSSID[15];
  char WiFipswd[25];

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
      Serial.printf("\r\nCould not connect to %s", ssid[i]);
      if(i==1) {
        log1("No WiFi!");
        return(false);
      }
    } else {
      Serial.println("  OK");
      //Serial.print("  IP: ");
      //Serial.println(WiFi.localIP());
      //Serial.setDebugOutput(true);
      //WiFi.printDiag(Serial);
    }
  }
  return(true);
}

int getParms() {
  char in[STBUF];
  char _st1[25][NOCFGSTR];
  char _st2[15][NOCFGSTR];
  int _stfnd[NOCFGSTR];
  int _stfin[NOCFGSTR];
  int _st2len[NOCFGSTR];

  //Serial.printf("getParms %d\r\n", sizeof(strk));
  for(i=0; i<NOCFGSTR; i++) {
    _stfnd[i]=0;        // broj poklapanih znakova
    _stfin[i]=0;        // nadjeno poklapanje
    _st2len[i] = strlen(st2[i]);
    _st1[0][i] = '\0';
//    strcpy(_st2[i], st2[i]);
  }

  delay(150);
  if (!client.connect(webSrv, httpPort)) {
    Serial.println("connection to server failed [p]");
    return 0;
  }
  sprintf(st1, "GET /vatra/pali?ci=%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", chipID, webSrv);
  client.print(st1);
  delay(150);

  Serial.printf("getParms:1\r\n");
  // Read all the lines of the reply from server
  j=0;
  while(client.available()){
    in[j] = client.read();
    for(i=0; i<NOCFGSTR; i++) {
      if(j < _st2len[i]) {
        if(in[j] == _st2[i][j]) {
          _stfnd[i]++;
        }
      } else if(_stfnd[i] == _st2len[i]) {
        _st1[i][j- _st2len[i]] = in[j];
      }
    }

    if(in[j] == '\r' || in[j] == '\n' || j == STBUF-1) {
      for(i=0; i<NOCFGSTR; i++) {
//        printf("[%d,%d]: %s;\n", j, strk[i].stfnd[i], strk[i].st1[i]);
        if(_stfnd[i] == _st2len[i] && !_stfin[i]) {
          _st1[i][j- _st2len[i]] = '\0';
//          printf("\n%%%d K: %s;\n", i, strk[i].st1);
          _stfin[i] = 1;
        } else {
          _stfnd[i]=0;
        }
      }
      j=0;
    } else {
      j++;
    }
  }
  return 0;
}

int drvaIugljen() {
  String line;

  if (!client.connect(webSrv, httpPort)) {
    Serial.println("connection to server failed.");
    return -1;
  }
  digitalWrite(led, 0);
  sprintf(st1, "GET /vatra/ajde/?ci=%08x&t0=%s&xi=Vcc:%d.%03d HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                      chipID, temperatureString, Vcc/1000, Vcc%1000, webSrv);
  client.print(st1);
  delay(250);
  while(client.available()){
    line = client.readStringUntil('\n');
    Serial.println(line);
  }
  digitalWrite(led, 1);
//  http.begin(webSrv, 80, "/vatra/ajde/?ci=%08x&t0=%stest.html"); //HTTP
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
    while(f1.available()) {
      param[i] = (char *)malloc(8);
      val[i] = (char *)malloc(15);
      //f1.scanf(f1, "%4s=%s\n", param[i], val[i]);
      line = f1.readStringUntil('\n');
      //Serial.printf("li[%d]: %s;\r\n", i, line.c_str());
      //sscanf(line, "%4s=%s\n", param[i], val[i]);
      strcpy(param[i], line.substring(0, line.indexOf("=")).c_str());
      strcpy(val[i], line.substring(strlen(param[i]) +1).c_str());
      i++;
    }
    f1.close();
    for(i=0; i<parmc; i++) {
      if(!strcmp(param[i], "SSID"))
        strcpy(ssid[1], val[i]);
      if(!strcmp(param[i], "WPSW"))
        strcpy(password[1], val[i]);
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
    f1.printf("KVPM=%x\n", 0x23);
    f1.printf("SSID=IDA\n");
    f1.printf("WPSW=gU2dR!c4\n");
    f1.printf("TMZC=CEST\n");
    f1.close();
  }
  return(0);
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

  strcpy(ssid[0], "kek0");
  strcpy(password[0], "k1ju4wIf");
  readConfig();
  setupWiFi();

  delay(1250);
  getParms();
  for(i=0; i<NOCFGSTR; i++) {
    if(strk[i].stfin != 0) {
      Serial.printf(" *%d (%s): %s *\n", i, strk[i].st2, strk[i].st1);
    }
  }
  log1(strk[0].st1);

  wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
  char msgbuf[80];
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
//  if(getParms()) {
//    Serial.printf("Vri: %s;\r\n", st1);
//  }
  drvaIugljen();

  Serial.printf("Delaying for %d seconds...\r\n", SLEEP_DELAY_IN_SECONDS);
  i=SLEEP_DELAY_IN_SECONDS;
  while(i-- > 0) {
    delay(1000);
  }
//  WiFi.forceSleepBegin();
//  delay(10000 * SLEEP_DELAY_IN_SECONDS);
  Serial.println("");
}

