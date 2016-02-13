/*
 * ver 0.2
 * 
 * Source:
 * http://www.jerome-bernard.com/blog/2015/10/04/wifi-temperature-sensor-with-nodemcu-esp8266/
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

#define SLEEP_DELAY_IN_SECONDS  449
#define ONE_WIRE_BUS            4      // DS18B20 pin
#define STBUF                   120

char ssid[2][8];
char password[2][20];
const char* greetmsg = "\rKlima za po doma,  ver. 0.2 (c) kek0 2016.";
int led = 2, i, j, chipID, Vcc;
unsigned int localPort = 1245;
unsigned int syslogPort = 514, httpPort = 80;
char webSrv[] = "oblak.kek0.net";
char st1[STBUF];

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
char temperatureString[6];
ADC_MODE(ADC_VCC);
WiFiServer server(23);
//WiFiUDP udp;
//IPAddress syslogIPaddress(192, 0, 2, 1);
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
//  digitalWrite(led, 0);
  do {
    DS18B20.requestTemperatures(); 
    temp = DS18B20.getTempCByIndex(0);
    delay(100);
    Serial.printf("T: %d.%02d\r", (int)temp, (int)(temp*100)%100);
    i++;
  } while ((temp == 85.0 || temp == (-127.0)) && i <50);
//  digitalWrite(led, 1);
  Serial.println("");
  return temp;
}

bool setupWiFi() {
  for(i=0; i<2; i++) {
    Serial.printf("\r\nConnecting to %s", ssid[i]);
    WiFi.begin(ssid[i], password[i]);
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
      Serial.println("OK");
      break;
    }
  }
  return(true);
}

int getDTime() {
  char st2[]="Date: ", in[STBUF];
  int sfound, st2len=strlen(st2);

  //st1=new char(STBUF);
  if (!client.connect(webSrv, httpPort)) {
    Serial.println("connection to web server failed");
    return 0;
  }
  sprintf(st1, "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", webSrv);
  client.print(st1);
  delay(250);
  
  // Read all the lines of the reply from server and print them to Serial
  j=0;
  sfound=0;
  st1[0] = '\0';
  while(client.available()){
    in[j] = client.read();
    //Serial.print(in[j]);
    if(j < st2len) {
      if(in[j] == st2[j]) {
        sfound++;
      }
    } else {
//      Serial.printf("A%cB%dC%dD  ", in[j], j, j-st2len);
      st1[j- st2len] = in[j];
    }
    if(in[j] == '\r' || in[j] == '\n' || j == STBUF-1) {
      //Serial.printf("[%d,%d]: %s;\r\n", j, sfound, st1);
      if(sfound == st2len) {
        st1[j- st2len] = '\0';
//        Serial.printf("\r\n%%K: %s;\n\r\n", st1);
        return strlen(st1);
      } else {
        j=0;
        sfound=0;
      }
    } else {
      j++;
//      delay(10);
    }
  }
  //free(st1);
  return 0;
}

int drvaIugljen() {
  String line;
  //HTTPClient http;

  if (!client.connect(webSrv, httpPort)) {
    Serial.println("connection to web server failed");
    return -1;
  }
  sprintf(st1, "GET /vatra/ajde/?ci=%08x&t0=%s&xi=Vcc:%d.%d HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                      chipID, temperatureString, Vcc/1000, Vcc%1000, webSrv);
  client.print(st1);
  delay(250);
  while(client.available()){
    line = client.readStringUntil('\n');
    Serial.println(line);
  }
//  http.begin(webSrv, 80, "/vatra/ajde/?ci=%08x&t0=%stest.html"); //HTTP
  return 0;
}

void setup() {
//  char *st1;
  
  delay(2500);
  Serial.begin(115200);
  Serial.println ( greetmsg );
  delay(1500);
//  pinMode(led, OUTPUT);

  // setup OneWire bus
  DS18B20.begin();
  chipID = ESP.getChipId();
  Serial.printf("ESP8266 Chip id = %08X\r\n", chipID);
 
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
  }

  strcpy(ssid[0], "kek0");
  strcpy(password[0], "k1ju4wIf");
  strcpy(ssid[1], "IDA");
  strcpy(password[1], "gU2dR!c4");
  setupWiFi();
//  udp.begin(localPort);

  getDTime();
  log1(st1);
}

void loop() {
  char msgbuf[80];
  //if (!client.connected()) {
  //  reconnect();
  //}
  //client.loop();

  float temperature = getTemperature();
  // convert temperature to a string with two digits before the comma and 2 digits for precision
  dtostrf(temperature, 2, 2, temperatureString);
  // send temperature to the serial console
  Serial.printf("Acquired temperature: %s C\r\n", temperatureString);
  Vcc = ESP.getVcc();
  Serial.printf("ESP VCC: %d.%dV\r\n", Vcc/1000, Vcc%1000);

  if(WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }
  //udp.beginPacket(syslogIPaddress, syslogPort);
  //sprintf(msgbuf, "<134> Temp: %s%cC; Vcc: %d.%03dV; Up: %ds\r\n",
  //  temperatureString, 0xb0, Vcc/1000, Vcc%1000, millis()/1000);
  //udp.write(msgbuf, strlen(msgbuf));
  //udp.endPacket();
  if(getDTime()) {
    Serial.printf("Vri: %s;\r\n", st1);
  }
  drvaIugljen();

  Serial.printf("Delaying for %d  seconds...\r\n", SLEEP_DELAY_IN_SECONDS);
  i=SLEEP_DELAY_IN_SECONDS;
  while(i-- > 0) {
    delay(1000);
  }
  Serial.println("");
}

