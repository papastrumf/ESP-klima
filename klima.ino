/*
 * ver 0.1
 * 
 * Source:
 * http://www.jerome-bernard.com/blog/2015/10/04/wifi-temperature-sensor-with-nodemcu-esp8266/
 */

#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include <WiFiUdp.h>

#define SLEEP_DELAY_IN_SECONDS  30
#define ONE_WIRE_BUS            4      // DS18B20 pin

const char* ssid = "kek0";
const char* password = "k1ju4wIf";
const char* greetmsg = "\rKlima za po doma,  ver. 0.1 (c) kek0 2016.";
int led = 0;
unsigned int localPort = 1245;
unsigned int syslogPort = 514;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
char temperatureString[6];
ADC_MODE(ADC_VCC);
WiFiServer server(23);
WiFiUDP udp;
IPAddress syslogIPaddress(192, 0, 2, 1);

void log1(const char *lmsg) {
  File fi = SPIFFS.open("/log.0", "a+");
  if(fi) {
    fi.seek(0, SeekEnd);
    fi.println(lmsg);
    fi.close();
  }
}

void setup() {
  delay(2500);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println ( greetmsg );
  delay(1500);
  pinMode(led, OUTPUT);

  // setup OneWire bus
  DS18B20.begin();
  Serial.printf(" ESP8266 Chip id = %08X\r\n", ESP.getChipId());
 
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

  // Wait for WiFi connection
  Serial.printf("\nConnecting to %s", ssid);
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {
    delay(500);
    Serial.print ( "." );
  }
  if(i == 21){
    Serial.printf("\r\nCould not connect to %s\r\n", ssid);
    log1("No WiFi!");
//    while(1) delay(500);
  }
  Serial.println("Ready!");
  udp.begin(localPort);
}

float getTemperature() {
  Serial.println("Requesting DS18B20 temperature:");
  float temp;
  digitalWrite(led, 1);
  do {
    DS18B20.requestTemperatures(); 
    temp = DS18B20.getTempCByIndex(0);
    delay(100);
    Serial.print("T: ");
    Serial.print(temp);
  } while (temp == 85.0 || temp == (-127.0));
  digitalWrite(led, 0);
  Serial.println("");
  return temp;
}

void loop() {
  uint8_t i;
  int Vcc;
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

//  Serial.print("Closing WiFi connection...");
//  WiFi.disconnect();
//  delay(100);

  Vcc = ESP.getVcc();
  Serial.printf("ESP VCC: %d.%03dV\r\n", Vcc/1000, Vcc%1000);
  udp.beginPacket(syslogIPaddress, syslogPort);
  sprintf(msgbuf, "<134> Temp: %s%cC; Vcc: %d.%03dV; Up: %ds\r\n",
    temperatureString, 0xb0, Vcc/1000, Vcc%1000, millis()/1000);
  udp.write(msgbuf, strlen(msgbuf));
  udp.endPacket();

//  Serial.print("\nEntering deep sleep mode for ");
  Serial.printf("Delaying for %d  seconds...\r\n", SLEEP_DELAY_IN_SECONDS);
  //ESP.deepSleep(SLEEP_DELAY_IN_SECONDS * 1000000, WAKE_RF_DEFAULT);
  //ESP.deepSleep(10 * 1000, WAKE_NO_RFCAL);
  //delay(500);   // wait for deep sleep to happen
  i=SLEEP_DELAY_IN_SECONDS;
  while(i-- > 0) {
    delay(1000);
  }
  Serial.println("");
}

