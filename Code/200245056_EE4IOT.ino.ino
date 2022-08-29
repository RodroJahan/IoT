#include <RFID.h>
#include <SPI.h>
#include "rgb_lcd.h"
#include "SimpleDHT.h"
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <WiFiUdp.h>

#define USERNAME        "Rodro_Jahan" // my username
#define SlaveSelect_PIN 0
#define Reset_PIN       2
#define LEDPIN          15
#define TEMPPIN         16
#define AnalogPin       A0
#define ssid            "Rodro's iPhone" // your hotspot SSID or name
#define password        "password"             // your hotspot password
#define TASK1L 100
#define TIMEOUTVAL 250
#define LINES 8
#define LEN 150
#define SOCKET 8888
#define BUFFERLEN 255

#define NOPUBLISH      // comment this out once publishing at less than 10 second intervals
#define ADASERVER     "io.adafruit.com"     // do not change this
#define ADAPORT       8883                  // Encrypted Port
#define ADAUSERNAME   "rodro307"               // ADD YOUR username here between the qoutation marks
#define ADAKEY        "aio_XUxc39Gz1lEVQ0pIFJ9nsPlDQq59" // ADD YOUR Adafruit key here betwwen marks

int PresentCard = 0;
int FinalCard = 0;
int count = 0;
int temporarycount = 0;
int flag = 0;
unsigned long TASK1LC = 0;
char Number_RFID[] = {"\n"};
char Location[] = "Dhaka Max group front entrance";
char store_number [13]; // This variable stores uid(12 char) number including a null character at the end 
String uid[12];
char inputbuf[LINES][LEN] = {};
char inBUFFER[BUFFERLEN];
char outBUFFER[BUFFERLEN];
float Temp;
float TempArr[10];
float Mintemp = 36.5;
float Maxtemp = 38;


WiFiClientSecure client;   
WiFiServer SERVER(80);
WiFiClient Client;
rgb_lcd lcd;
WiFiUDP UDP;
RFID myrfid(SlaveSelect_PIN, Reset_PIN);         
static const char *fingerprint PROGMEM = "59 3C 48 0A B1 8B 39 4E 0D 58 50 47 9A 13 55 60 CC A0 1D AF";
Adafruit_MQTT_Client MQTT(&client, ADASERVER, ADAPORT, ADAUSERNAME, ADAKEY);


//Feeds we publish to
Adafruit_MQTT_Publish TEMP = Adafruit_MQTT_Publish(&MQTT, ADAUSERNAME "/feeds/temperature");
Adafruit_MQTT_Publish Devloc = Adafruit_MQTT_Publish(&MQTT, ADAUSERNAME "/feeds/location");
Adafruit_MQTT_Publish UID = Adafruit_MQTT_Publish(&MQTT, ADAUSERNAME "/feeds/uid");
Adafruit_MQTT_Publish CHANGE_Mintemp_LMT = Adafruit_MQTT_Publish(&MQTT, ADAUSERNAME "/feeds/lower-limit");
Adafruit_MQTT_Publish CHANGE_Maxtemp_LMT = Adafruit_MQTT_Publish(&MQTT, ADAUSERNAME "/feeds/upper-limit");
Adafruit_MQTT_Subscribe Mintemp_LMT = Adafruit_MQTT_Subscribe(&MQTT, ADAUSERNAME "/feeds/lower-limit");
Adafruit_MQTT_Subscribe Maxtemp_LMT = Adafruit_MQTT_Subscribe(&MQTT, ADAUSERNAME "/feeds/upper-limit");



void MQTTconnect ( void );


void setup() {                 // On initialization, the setup function is called once.
  Serial.begin(115200);       // Set up serial contact with the host computer.
  delay(500);
  Serial.print("Trying to connect");    // Acknowledge of attempt to connect
  Serial.print(ssid);                           // print the ssid over serial

  WiFi.begin(ssid, password);                   // trying to connect to SSID with the password

  while (WiFi.status() != WL_CONNECTED)         // until we got connected
  {
    delay(500);                                 // wait for 0.5 seconds (500ms)
    Serial.print(".");                          // print a . to acknowledge
  }
  Serial.print("\n");                           // print a blank line

  Serial.println("Connection Successful");      // Inform that the device is successfully connected 
  Serial.print("IP:  ");                        // display the IP address
  Serial.println(WiFi.localIP());               // All four IP address are displayig by printIn function
  Serial.print("DNS: ");                        // display DNS IP address
  Serial.println(WiFi.dnsIP());
  Serial.println(""); Serial.println(""); Serial.println(""); // display three blank lines to differentiate ASCII chars created while starting
  pinMode(LEDPIN, OUTPUT); // pin 15 set as output pin
  lcd.begin(16, 2);
  lcd.setRGB(0, 255, 0);
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());                    // renew display with local board IP address
  client.setFingerprint(fingerprint);
  MQTT.subscribe(&Mintemp_LMT);
  MQTT.subscribe(&Maxtemp_LMT);
  SPI.begin();
  myrfid.init();
  MQTTconnect();
  SERVER.begin();
  Serial.println("HTTP linked");
  if (UDP.begin(SOCKET))
  {
    Serial.println("Congratulations!, port opened");
  }
  else
    ;
}


void loop()
{
  count = temporarycount;
  count = count % 10;
  temporarycount = count;
  lcd.setCursor(0, 0);
  lcd.print("Scan your RFID Card");
  unsigned long current_millis = millis();
  Adafruit_MQTT_Subscribe *subscription;                    // create a subscriber object instance
  while ( subscription = MQTT.readSubscription(500) )      // Read a subscription and wait for max of 500 milliseconds.
  {
    if (subscription == &Mintemp_LMT)                               // if the subscription we have receieved matches the one we are after
    {
      Mintemp = atof((char *)Mintemp_LMT.lastread);
    }
    if (subscription == &Maxtemp_LMT)                               // if the subscription we have receieved matches the one we are after
    {
      Maxtemp = atof((char *)Maxtemp_LMT.lastread);
    }
  }
  if ((current_millis - TASK1LC) >= TASK1L)
  {
    TASK1LC = millis();
    if (checkRFID())
    {
      temporarycount++;
      TEMP.publish(Temp);
      Devloc.publish(Location);
      UID.publish(store_number);


#ifdef NOPUBLISH
      if ( !MQTT.ping() )
      {
        MQTT.disconnect();
      }
#endif
    }
  }


  int timecounter = 0;
  int i;
  Client = SERVER.available();
  if (Client)
  {
    //Serial.println("Client detected, reloading data");
    while (!Client.available())
    {
      delay(1);
      timecounter++;
      if (timecounter >= TIMEOUTVAL)
      {
        return;
      }
    }
    //Serial.println("Request updating");
    for (i = 0; i < LINES; i++)
    {
      Client.readStringUntil('\r').toCharArray(&inputbuf[i][0], LEN);
    // Serial.println(inputbuf[i]);
    }

    if (!strcmp(inputbuf[0], "GET /?Maxtemp++ HTTP/1.1"))
    {
      Maxtemp += 0.5;
      CHANGE_Maxtemp_LMT.publish(Maxtemp);
    }
    if (!strcmp(inputbuf[0], "GET /?Maxtemp-- HTTP/1.1"))
    {
      Maxtemp -= 0.5;
      CHANGE_Maxtemp_LMT.publish(Maxtemp);
    }
    if (!strcmp(inputbuf[0], "GET /?Mintemp-- HTTP/1.1"))
    {
      Mintemp -= 0.5;
      CHANGE_Mintemp_LMT.publish(Mintemp);
    }
    if (!strcmp(inputbuf[0], "GET /?Mintemp++ HTTP/1.1"))
    {
      Mintemp += 0.5;
      CHANGE_Mintemp_LMT.publish(Mintemp);
    }

    Client.flush();
    servepage();
    Client.stop();
  }

  int packetsize = 0;
  packetsize = UDP.parsePacket();

  if (packetsize)
  {
    UDP.read(inBUFFER, BUFFERLEN);
    inBUFFER[packetsize] = '\0';
    Serial.print("Obtained ");
    Serial.print(packetsize);
    Serial.println(" bytes");

    Serial.println("Contents");
    Serial.println(inBUFFER);
    Serial.print("IP:  ");
    Serial.println(UDP.remoteIP());
    Serial.print("Port: ");
    Serial.println(UDP.remotePort());
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort()); 
    strcpy(outBUFFER, WiFi.localIP().toString().c_str());
    UDP.write(outBUFFER);
    UDP.endPacket();
  }
}

void servepage ( void ){
  String reply;

  reply += "<!DOCTYPE HTML>";                                  // Beginning of HTML 

  reply += "<html>";                                           
  reply += "<head>";                                           
  reply += "<meta href=http://10.0.2.21/=""refresh"" content=""2"">";
  reply += "<title> Covid Test</title>";                  // Page title
  reply += "</head>";
  reply += "<br>";
  reply += "<body>";                                           
  reply += "<h1> Covid Test</h1>";  
  reply += "<br>";
  reply += "Device Devloc:  ";
  reply += Location;
  reply += "<br>";
  reply += "<br>";
  reply += "<br>";
  for (count = 0; count < 10 ; count++)
  {
    reply += count + 1;
    reply += ")  ";
    reply += "Temperature: ";
    reply += TempArr[count];
    reply += "&emsp;&emsp;";
    reply += "UID: ";
    reply += uid[count];
    reply += "<br>";
  }
  reply += "<br>";
  reply += "<a href=http://10.0.2.21/>refresh</a>";
  reply += "<br>";
  reply += "<a href=http://10.0.2.21/?Maxtemp++>Maxtemp++</a>";
  reply += "<br>";
  reply += "<a href=http://10.0.2.21/?Maxtemp-->Maxtemp--</a>";
  reply += "<br>";
  reply += "<br>";
  reply += "<a href=http://10.0.2.21/?Mintemp++>Mintemp++</a>";
  reply += "<br>";
  reply += "<a href=http://10.0.2.21/?Mintemp-->Mintemp--</a>";
  reply += "<br>";
  reply += "</body>";                                          
  reply += "</html>";                                          // HTML finish
  Client.printf("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %u\r\n\r\n%s", reply.length(), reply.c_str());
}
void MQTTconnect ( void ){
  unsigned char attempts = 0;
  // Exit if already occpied.
  if ( MQTT.connected() )
  {
    return;
  }

  Serial.print("Establishing connection to MQTT... ");

  while ( MQTT.connect() != 0 )                                   
  {
    Serial.println("Thank you! new attempt to connect within 5 seconds ");   
    MQTT.disconnect();                                             
    delay(5000);                                                   // 5secs time interval
    attempts++;
    if (attempts == 3)
    {
      Serial.println("Communication problem detected, asking WDT to reset");
      while (1)
      {
        ;    
      }
    }
  }
  Serial.println("Congratulations! MQTT connection successful");
}
int checkRFID() {
  PresentCard = myrfid.isCard();
  if (PresentCard && !FinalCard)
  {
    if (myrfid.readCardSerial())
    {
      for (int i = 0; i < 4; i++) 
      {
        Number_RFID[i] = myrfid.serNum[i];
      }
      sprintf(store_number, "%d%d%d%d", Number_RFID[0], Number_RFID[1], Number_RFID[2], Number_RFID[3]);
      uid[count] = {store_number};
      lcd.clear();
      Temp = analogRead(AnalogPin);
      Temp = (Temp / 51.2) + 25;
      TempArr[count] = Temp;
      lcd.print("Temp: ");
      lcd.setCursor(6, 0);
      lcd.print(Temp);
      lcd.setCursor(11, 0);
      lcd.print("*C");
      count++;
      if (Temp > Mintemp && Temp < Maxtemp)
      {
        digitalWrite(LEDPIN, HIGH);
        delay(1000);
        digitalWrite(LEDPIN, LOW );
        return 1;
      }
      else
      {
        lcd.setCursor(0, 1);
        lcd.print("Please, go back to your residence");
        delay(1000);
        lcd.clear();
        return 1;
      }
    }
  }
  FinalCard = PresentCard;
  myrfid.halt();
  return 0;
}
