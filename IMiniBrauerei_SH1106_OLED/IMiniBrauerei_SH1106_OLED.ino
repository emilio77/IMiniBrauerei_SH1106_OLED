// iMiniBrauerei  für ESPs mit eingebautem 0.96" OLED von emilio  
// D3 : DS18B20 Sensor
// D4 : Onboard LED
// D5 : Heizung
// D6 : Rührwerk
// D7 : Pumpe/Kühlung
// D8 : Alarm

#include <ESP8266WiFi.h>                             //https://github.com/esp8266/Arduino
#include <WiFiClient.h>                              //https://github.com/esp8266/Arduino
#include <EEPROM.h>                                  
#include <WiFiManager.h>                             //https://github.com/kentaylor/WiFiManager
#include <ESP8266WebServer.h>                        //http://www.wemos.cc/tutorial/get_started_in_arduino.html
#include <DoubleResetDetector.h>                     //https://github.com/datacute/DoubleResetDetector
#include <OneWire.h>                                 //http://www.pjrc.com/teensy/td_libs_OneWire.html
// #include "SSD1306Wire.h"                             //https://github.com/squix78/esp8266-oled-ssd1306 
#include "SH1106Wire.h"                             //https://github.com/squix78/esp8266-oled-ssd1306 

#define Version "1.1.0_ESP+OLED"

#define deltaMeldungMillis 5000                      // Sendeintervall an die Brauerei in Millisekunden
#define DRD_TIMEOUT 10                               // Number of seconds after reset during which a subseqent reset will be considered a double reset.
#define DRD_ADDRESS 0                                // RTC Memory Address for the DoubleResetDetector to use
#define RESOLUTION 12                                // OneWire - 12bit resolution == 750ms update rate
#define OWinterval (800 / (1 << (12 - RESOLUTION))) 
#define OLED_RESET 0

// SSD1306Wire  display(0x3c, D1, D2);
SH1106Wire  display(0x3c, D1, D2);

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

IPAddress UDPip(192,168,178,255);                     // IP-Adresse an welche UDP-Nachrichten geschickt werden xx.xx.xx.255 = Alle Netzwerkteilnehmer die am Port horchen.
IPAddress myIP(192,168,4,1);                          // IP-Adresse als HotSpot
unsigned int answerPort = 5003;                       // Port auf den Temperaturen geschickt werden
unsigned int localPort = 5010;                        // Port auf dem gelesen wird
ESP8266WebServer server(80);                          // Webserver initialisieren auf Port 80
WiFiUDP Udp;

OneWire ds(D3); 

const char *ssid = "IMiniBrauerei";
const char *password = "IMiniBrauerei";

const int PIN_LED = D4;                                // Controls the onboard LED.

const int Heizung = D5;                               // Im folgenden sind die Pins der Sensoren und Aktoren festgelegt
const int Ruehrwerk = D6;
const int Pumpe = D7;
const int Summer = D8;

char charVal[8];
char packetBuffer[24];                                 // buffer to hold incoming packet,
char temprec[24] = "";
char relais[5] = "";
char state[3] = "";

bool HLowActive, RLowActive, PLowActive, ALowActive, HotSpot = true;

bool initialConfig = false;                             // Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
unsigned long jetztMillis = 0, letzteInMillis = 0, letzteOfflineMillis = 0, letzteTempMillis = 0, displayMillis=0;

float Temp = 0.0;
int solltemp = 0;

String str = "";

const unsigned char officon[] =
{
  0b01100000, 0b00000000, //          ##     
  0b11100000, 0b00000000, //         ###     
  0b11000000, 0b00000001, //        ###      
  0b11000000, 0b00000001, //        ###      
  0b11100110, 0b00000001, //        ####  ## 
  0b11111110, 0b00000011, //       ######### 
  0b11111100, 0b00000111, //      #########  
  0b11111000, 0b00001111, //     #########   
  0b11000000, 0b00011111, //    #######      
  0b10000000, 0b00111111, //   #######       
  0b00000000, 0b01111111, //  #######        
  0b00000000, 0b01111110, //  ######         
  0b00000000, 0b01111100, //  #####          
  0b00000000, 0b00111000, //   ###           
  0b00000000, 0b00000000, //    
  0b00000000, 0b00000000, // 
};

const unsigned char playicon[] =
{
  0b00000000, 0b00000000, //   
  0b00000000, 0b00000000, //     
  0b01000000, 0b00000000, //      # 
  0b11000000, 0b00000001, //      ###  
  0b11000000, 0b00000111, //      #####   
  0b11000000, 0b00011111, //      #######    
  0b11000000, 0b00111111, //      ########     
  0b11000000, 0b00011111, //      ####### 
  0b11000000, 0b00000111, //      #####   
  0b11000000, 0b00000001, //      ###  
  0b01000000, 0b00000000, //      # 
  0b00000000, 0b00000000, //        
  0b00000000, 0b00000000, //   
  0b00000000, 0b00000000, //         
  0b00000000, 0b00000000, //                 
  0b00000000, 0b00000000, //  
};

const unsigned char pauseicon[] =
{
  0b00000000, 0b00000000, // 
  0b00000000, 0b00000000, //   
  0b10011100, 0b00000011, //       ###  ### 
  0b10011100, 0b00000011, //       ###  ### 
  0b10011100, 0b00000011, //       ###  ### 
  0b10011100, 0b00000011, //       ###  ### 
  0b10011100, 0b00000011, //       ###  ### 
  0b10011100, 0b00000011, //       ###  ### 
  0b10011100, 0b00000011, //       ###  ### 
  0b10011100, 0b00000011, //       ###  ### 
  0b00000000, 0b00000000, //
  0b00000000, 0b00000000, //   
  0b00000000, 0b00000000, //                 
  0b00000000, 0b00000000, //                 
  0b00000000, 0b00000000, //         
  0b00000000, 0b00000000, //                 
};

const unsigned char stopicon[] =
{
  0b00000000, 0b00000000, //                 
  0b00000000, 0b00000000, // 
  0b11111100, 0b00000011, //       ######## 
  0b11111100, 0b00000011, //       ######## 
  0b11111100, 0b00000011, //       ######## 
  0b11111100, 0b00000011, //       ######## 
  0b11111100, 0b00000011, //       ######## 
  0b11111100, 0b00000011, //       ######## 
  0b11111100, 0b00000011, //       ######## 
  0b11111100, 0b00000011, //       ######## 
  0b00000000, 0b00000000, // 
  0b00000000, 0b00000000, //   
  0b00000000, 0b00000000, //   
  0b00000000, 0b00000000, //         
  0b00000000, 0b00000000, //         
  0b00000000, 0b00000000, //                 
};

unsigned char tapicon[] =
{
  0b00000000, 0b00000000, //              
  0b11100000, 0b00000111, //      ######     
  0b10000000, 0b00000001, //        ##       
  0b11110000, 0b00001111, //     ########    
  0b11111110, 0b01111111, //  ############## 
  0b11111111, 0b01111111, //  ###############
  0b11111111, 0b01111111, //  ###############
  0b11111111, 0b01111111, //  ###############
  0b00001111, 0b00000000, //             ####
  0b00001111, 0b00000000, //             ####
  0b00000000, 0b00000000, //                 
  0b00001100, 0b00000000, //             ##  
  0b00001100, 0b00000000, //             ##  
  0b00000000, 0b00000000, //                 
  0b00001100, 0b00000000, //             ##  
  0b00001100, 0b00000000, //             ##  
};

unsigned char sirenicon[] =
{
  0b00000000, 0b00000000, // 
  0b10000000, 0b00000000, //         #      
  0b10000010, 0b00100000, //   #     #     # 
  0b10000100, 0b00010000, //    #    #    #  
  0b11101000, 0b00001011, //     # ##### #   
  0b11110000, 0b00000111, //      #######    
  0b11111000, 0b00001111, //     #########   
  0b10111000, 0b00001111, //     ##### ###   
  0b10011100, 0b00011111, //    ######  ###  
  0b10011100, 0b00011111, //    ######  ###  
  0b10001100, 0b00011111, //    ######   ##  
  0b10001100, 0b00011111, //    ######   ## 
  0b10001100, 0b00011111, //    ######   ## 
  0b11111100, 0b00011111, //    ###########
  0b11111100, 0b00011111, //    ###########
  0b00000000, 0b00000000, // 
};

unsigned char fireicon[] =
{
  0b00000000, 0b00000001, //        #
  0b10000000, 0b00000001, //        ## 
  0b11000000, 0b00000011, //       ####  
  0b11000000, 0b00000011, //       ####     
  0b11101000, 0b00000111, //      ###### #    
  0b11111100, 0b00001111, //     ##########   
  0b11111100, 0b00011111, //    ###########
  0b01111110, 0b00111111, //   ###### ######
  0b00111110, 0b00111111, //   ######  #####
  0b00011110, 0b01111101, //  ##### #   ####  
  0b00011110, 0b01111100, //  #####     ####
  0b10011110, 0b01111000, //  ####   #  ####  
  0b10011100, 0b01111001, //  ####  ##  ###  
  0b00011100, 0b00111001, //   ###  #   ###  
  0b00111000, 0b00011100, //    ###    ###
  0b00110000, 0b00001100, //     ##    ##            
};

unsigned char ruehricon[] =
{
  0b10000000, 0b00000001, //        ##
  0b10000000, 0b00000111, //      #### 
  0b10000000, 0b00001101, //     ## ##  
  0b10000000, 0b00001101, //     ## ##  
  0b10111100, 0b00001101, //     ## ## #### 
  0b11111110, 0b00001101, //     ## ######## 
  0b11000010, 0b00000111, //      #####    # 
  0b01111111, 0b11111110, // #######  #######
  0b01111111, 0b11111110, // #######  #######
  0b11100000, 0b01000011, //  #    ##### 
  0b10110000, 0b01111111, //  ######## ##
  0b10110000, 0b00111101, //   #### ## ##  
  0b10110000, 0b00000001, //        ## ##  
  0b10110000, 0b00000001, //        ## ##  
  0b11100000, 0b00000001, //        ####
  0b10000000, 0b00000001, //        ##              
};

void Hauptseite()
{
  char dummy[8];
  String Antwort = "";
  Antwort += "<meta http-equiv='refresh' content='5'/>";
  Antwort += "<font face=";
  Antwort += char(34);
  Antwort += "Courier New";
  Antwort += char(34);
  Antwort += ">";
   
  Antwort += "<b>Aktuelle Temperatur: </b>\n</br>";
  
  dtostrf(Temp, 5, 1, dummy);  
  Antwort += dummy;
  Antwort += " ";
  Antwort += char(176);
  Antwort += "C\n</br>";

  Antwort += "\n</br><b>Schaltstatus: </b>\n</br>Heizung:&nbsp;&nbsp;";
  if (relais[1] == 'H') { Antwort +="Ein\n</br>"; } else { Antwort +="Aus\n</br>"; }
  Antwort +="R"; Antwort +=char(252); Antwort +="hrwerk:&nbsp;";
  if (relais[2] == 'R') { Antwort +="Ein\n</br>"; } else { Antwort +="Aus\n</br>"; }
  Antwort += "Pumpe:&nbsp;&nbsp;&nbsp;&nbsp;";
  if (relais[3] == 'P') { Antwort +="Ein\n</br>"; } else { Antwort +="Aus\n</br>"; }
  Antwort += "Alarm:&nbsp;&nbsp;&nbsp;&nbsp;";
  if (relais[4] == 'A') { Antwort +="Ein\n</br>"; } else { Antwort +="Aus\n</br>"; }
  Antwort +="\n</br><b>Brauereistatus: </b>\n</br>";
  if (state[1]=='o') { Antwort +="OFFLINE "; }        
  else if (state[1]=='x') { Antwort +="INAKTIV"; }
  else if (state[1]=='y') { Antwort +="AKTIV"; }
  else if (state[1]=='z') { Antwort +="PAUSIERT"; }
  Antwort +="\n</br>";      
  Antwort +="</br>Verbunden mit: ";
  Antwort +=WiFi.SSID(); 
  Antwort +="</br>Signalstaerke: ";
  Antwort +=WiFi.RSSI(); 
  Antwort +="dBm  </br>";
  Antwort +="</br>IP-Adresse: ";
  IPAddress IP = WiFi.localIP();
  Antwort += IP[0];
  Antwort += ".";
  Antwort += IP[1];
  Antwort += ".";
  Antwort += IP[2];
  Antwort += ".";
  Antwort += IP[3];
  Antwort +="</br>";
  Antwort +="</br>UDP-IN port: ";
  Antwort +=localPort; 
  Antwort +="</br>UDP-OUT port: ";
  Antwort +=answerPort; 
  Antwort +="</br></br>";
  Antwort += "</font>";
  server.send ( 300, "text/html", Antwort );
}

void packetAuswertung()
{
  int temp = 0;
  int temp2 = 0;
  if ((temprec[0]=='C') && (temprec[18]=='c'))             // Begin der Decodierung des seriellen Strings  
  { 
    temp=(int)temprec[1];
    if ( temp < 0 ) { temp = 256 + temp; }
    if ( temp > 7) {relais[4]='A';temp=temp-8;} else {relais[4]='a';} 
    if ( temp > 3) {relais[3]='P';temp=temp-4;} else {relais[3]='p';} 
    if ( temp > 1) {relais[2]='R';temp=temp-2;} else {relais[2]='r';}
    if ( temp > 0) {relais[1]='H';temp=temp-1;} else {relais[1]='h';}   

    temp=(int)temprec[2];
    if ( temp < 0 ) { temp = 256 + temp; }
    if ( temp > 127) {temp=temp-128;}  
    if ( temp > 63) {temp=temp-64;}
    if ( temp > 31) {temp=temp-32;}    
    if ( temp > 15) {temp=temp-16;}  
    if ( temp > 7) {temp=temp-8;}  
    if ( temp > 3) {state[1]='x';temp=temp-4;} 
    else if ( temp > 1) {state[1]='z';temp=temp-2;}  
    else if ( temp > 0) {state[1]='y';temp=temp-1;}    

    temp=(int)temprec[3];
    if ( temp < 0 ) { temp = 256 + temp; }
    solltemp=temp;
  }
  SerialOut();
}
 
void DisplayOut() {
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setColor(WHITE);

  if (state[1]=='o') { display.drawXbm(111, 0, 16, 16, officon);}        
  else if (state[1]=='x') { display.drawXbm(111, 0, 16, 16, stopicon);}
  else if (state[1]=='y') { display.drawXbm(111, 0, 16, 16, playicon);}
  else if (state[1]=='z') { display.drawXbm(111, 0, 16, 16, pauseicon);}

  if (relais[1] == 'H') { display.drawXbm(0, 0, 16, 16, fireicon);}
  if (relais[2] == 'R') { display.drawXbm(22, 0, 16, 16, ruehricon);}
  if (relais[3] == 'P') { display.drawXbm(44, 0, 16, 16, tapicon);}
  if (relais[4] == 'A') { display.drawXbm(66, 0, 16, 16, sirenicon);}

  display.drawHorizontalLine(0, 22, 128);
  display.drawVerticalLine(63, 22, 63);
  
  display.drawString(0, 26, "Ist:");
  dtostrf(Temp, 3, 1, charVal); 
  if (Temp<100) {str=charVal;} else {str="100";};  
  display.drawString(0, 44, str + " °C"); 
  
  display.drawString(69, 26, "Soll:");
  dtostrf(solltemp, 3, 1, charVal); 
  if (solltemp<100) {str=charVal;} else {str="100";};  
  display.drawString(69, 44, str + " °C"); 

  display.display();
}

void UDPOut() {
  dtostrf(Temp, 3, 1, charVal);
  Udp.beginPacket(UDPip, answerPort);
  Udp.write('T');
  if (Temp<10) {Udp.write(' ');}
  Udp.write(charVal);
  Udp.write('t');
  Udp.println();
  Udp.endPacket();
}

void UDPRead()
{
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    for (int schleife = 0; schleife < 23; schleife++) { temprec[schleife] = ' '; }
    // read the packet into packetBufffer
    Udp.read(packetBuffer, packetSize);
    for (int schleife = 0; schleife < 23; schleife++) { temprec[schleife] = packetBuffer[schleife]; }
    letzteInMillis = millis();
    Serial.print("Udp Packet received"); 
    packetAuswertung();
  }
}    

void SerialOut()
{
  Serial.print(" Reilaistatus: ");
  Serial.print(digitalRead(Heizung)); 
  Serial.print(digitalRead(Ruehrwerk));
  Serial.print(digitalRead(Pumpe));
  Serial.print(digitalRead(Summer));
  Serial.print(" S: ");
  Serial.print(solltemp);
  Serial.print(" I: ");
  Serial.print(Temp,1);
}

void OfflineCheck()
{
  if (jetztMillis > letzteInMillis+10000) 
  {
    if (jetztMillis > letzteOfflineMillis+1000) {Serial.print("offline"); SerialOut(); letzteOfflineMillis=jetztMillis; } 
    relais[1]='h';      
    relais[2]='r';               
    relais[3]='p'; 
    relais[4]='a';       
    state[1]='o';
  }
}

void RelaisOut()
{
  if (relais[1] == 'H') { digitalWrite(Heizung,!HLowActive); } else { digitalWrite(Heizung,HLowActive); }
  if (relais[2] == 'R') { digitalWrite(Ruehrwerk,!RLowActive); } else { digitalWrite(Ruehrwerk,RLowActive); }
  if (relais[3] == 'P') { digitalWrite(Pumpe,!PLowActive); } else { digitalWrite(Pumpe,PLowActive); }
  if (relais[4] == 'A') { digitalWrite(Summer,!ALowActive); } else { digitalWrite(Summer,ALowActive); }
}

float DS18B20lesen()
{
  int TReading, SignBit;
  byte i, present = 0, data[12], addr[8];
  if ( !ds.search(addr))  { ds.search(addr); }        // Wenn keine weitere Adresse vorhanden, von vorne anfangen
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);                                  // start Konvertierung, mit power-on am Ende
  delay(750);                                         // 750ms sollten ausreichen
  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);                                     // Wert lesen
  for ( i = 0; i < 9; i++) { data[i] = ds.read(); }
  TReading = (data[1] << 8) + data[0];
  SignBit = TReading & 0x8000;                        // test most sig bit
  if (SignBit) {TReading = (TReading ^ 0xffff) + 1;}  // 2's comp
  Temp = TReading * 0.0625;                           // Für DS18S20  temperatur = TReading*0.5
  if (SignBit) {Temp = Temp * -1;}
  return Temp;
}

void ReadSettings() {
  EEPROM.begin(512);
  EEPROM.get(0, localPort);
  EEPROM.get(20, answerPort);
  EEPROM.get(40, HLowActive);
  EEPROM.get(50, RLowActive);
  EEPROM.get(60, PLowActive);
  EEPROM.get(70, ALowActive);
  EEPROM.get(90, HotSpot);
  EEPROM.commit();
  EEPROM.end();
}  

void WriteSettings() {
  EEPROM.begin(512);
  EEPROM.put(0, localPort);
  EEPROM.put(20, answerPort);
  EEPROM.put(40, HLowActive);
  EEPROM.put(50, RLowActive);
  EEPROM.put(60, PLowActive);
  EEPROM.put(70, ALowActive);
  EEPROM.put(90, HotSpot);
  EEPROM.end();    
}

void setup() {
  pinMode(PIN_LED, OUTPUT);       // Im folgenden werden die Pins als I/O definiert
  pinMode(Heizung, OUTPUT);
  pinMode(Summer, OUTPUT);
  pinMode(Ruehrwerk, OUTPUT);
  pinMode(Pumpe, OUTPUT);
 
  Serial.begin(19200);
  Serial.println("\n");
  Serial.println("Starting");
  Serial.print("iMiniBrauerei V");  
  Serial.println(Version);

  // Initialising the UI will init the display too.
  display.init();
  // display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  ReadSettings();

  WiFi.printDiag(Serial);            //Remove this line if you do not want to see WiFi password printed

  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    initialConfig = true;
  }
  
  if (initialConfig) {
    Serial.println("Starting configuration portal.");
    digitalWrite(PIN_LED, LOW); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
    char convertedValue[5];

    char hotspot[24] = "type=\"checkbox\"";
    if (HotSpot) {
      strcat(hotspot, " checked");
    }
    WiFiManagerParameter p_hotspot("HotSpot", "Create HotSpot", "T", 2, hotspot, WFM_LABEL_AFTER);

    sprintf(convertedValue, "%d", answerPort);
    WiFiManagerParameter p_answerPort("answerPort", "send temperature on UDP Port", convertedValue, 5);
    
    sprintf(convertedValue, "%d", localPort);
    WiFiManagerParameter p_localPort("localPort", "receive relais state on UDP Port", convertedValue, 5);

    char hlowactive[24] = "type=\"checkbox\"";
    if (HLowActive) {
      strcat(hlowactive, " checked");
    }
    WiFiManagerParameter p_hlowactive("HLowActive", "Heizung (D5) Low Active", "T", 2, hlowactive, WFM_LABEL_AFTER);

    char rlowactive[24] = "type=\"checkbox\"";
    if (RLowActive) {
      strcat(rlowactive, " checked");
    }
    WiFiManagerParameter p_rlowactive("RLowAactive", "Ruehrwerk (D6) Low Active", "T", 2, rlowactive, WFM_LABEL_AFTER);

    char plowactive[24] = "type=\"checkbox\"";
    if (PLowActive) {
      strcat(plowactive, " checked");
    }
    WiFiManagerParameter p_plowactive("PLowActive", "Pumpe/Kuehlung (D7) Low Active", "T", 2, plowactive, WFM_LABEL_AFTER);

    char alowactive[24] = "type=\"checkbox\"";
    if (ALowActive) {
      strcat(alowactive, " checked");
    }
    WiFiManagerParameter p_alowactive("ALowActive", "Alarm (D8) Low Active", "T", 2, alowactive, WFM_LABEL_AFTER);

    WiFiManager wifiManager;
    wifiManager.setBreakAfterConfig(true);
    wifiManager.addParameter(&p_hotspot);
    wifiManager.addParameter(&p_answerPort);
    wifiManager.addParameter(&p_localPort);
    wifiManager.addParameter(&p_hlowactive);
    wifiManager.addParameter(&p_rlowactive);
    wifiManager.addParameter(&p_plowactive);
    wifiManager.addParameter(&p_alowactive);
    wifiManager.setConfigPortalTimeout(300);

    if (!wifiManager.startConfigPortal()) {
      Serial.println("Not connected to WiFi but continuing anyway.");
    } else {
      //if you get here you have connected to the WiFi
      Serial.println("connected...yeey :)");
    }
 
    HotSpot = (strncmp(p_hotspot.getValue(), "T", 1) == 0);
    answerPort = atoi(p_answerPort.getValue());
    localPort = atoi(p_localPort.getValue());
    HLowActive = (strncmp(p_hlowactive.getValue(), "T", 1) == 0);
    RLowActive = (strncmp(p_rlowactive.getValue(), "T", 1) == 0);
    PLowActive = (strncmp(p_plowactive.getValue(), "T", 1) == 0);
    ALowActive = (strncmp(p_alowactive.getValue(), "T", 1) == 0);

    WriteSettings();
  }

  if (!HotSpot) {
    WiFi.setOutputPower(20.5);
    WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
  } else { 
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_AP); // Force to acess point mode.
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();
  }
   
  digitalWrite(PIN_LED, HIGH); // Turn led off as we are not in configuration mode.

  if (!HotSpot) {
    unsigned long startedAt = millis();
    Serial.print("After waiting ");
    int connRes = WiFi.waitForConnectResult();
    float waited = (millis()- startedAt);  
    Serial.print(waited/1000);
    Serial.print(" secs in setup() connection result is ");
    Serial.println(connRes);
    if (WiFi.status()!=WL_CONNECTED){
      Serial.println("failed to connect, finishing setup anyway");
    } else{
      Serial.print("local ip: ");
      Serial.println(WiFi.localIP());
      Udp.begin(localPort);
      Serial.print("UDP-IN port: ");
      Serial.println(localPort);
      UDPip=WiFi.localIP();
      display.clear();
      display.setFont(ArialMT_Plain_16);
      display.setColor(WHITE);
      display.drawString(0, 0, "IP-Adresse:");
      display.drawHorizontalLine(0, 28, 128);
      str=String(UDPip[0])+"."+String(UDPip[1])+"."+String(UDPip[2])+"."+String(UDPip[3]);
      display.drawString(0, 40, str); 
      display.display();
      UDPip[3]=255;
      Serial.print("UDP-OUT port: ");
      Serial.println(answerPort); 
      delay(8000);  
      server.on("/", Hauptseite);
      server.begin();                          // HTTP-Server starten
    }
  } else {
    Serial.print("local ip: ");
    Serial.println(myIP);
    Udp.begin(localPort);
    Serial.print("UDP-IN port: ");
    Serial.println(localPort);
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setColor(WHITE);
    display.drawString(0, 0, "IP-Adresse:    AP");
    display.drawHorizontalLine(0, 28, 128);
    str=String(myIP[0])+"."+String(myIP[1])+"."+String(myIP[2])+"."+String(myIP[3]);
    display.drawString(0, 40, str); 
    display.display();
    UDPip = myIP;
    UDPip[3]=255;
    Serial.print("UDP-OUT port: ");
    Serial.println(answerPort); 
    delay(8000);  
    server.on("/", Hauptseite);
    server.begin();                          // HTTP-Server starten
  }
}

void loop() {

  drd.loop();
    
  jetztMillis = millis();

  server.handleClient(); // auf HTTP-Anfragen warten
 
  if ((WiFi.status()!=WL_CONNECTED) and (!HotSpot)) {
    WiFi.reconnect();
    Serial.println("lost connection");
    delay(5000);
    Udp.begin(localPort);
    server.on("/", Hauptseite);
    server.begin();            // HTTP-Server starten
  } else{
    if(!deltaMeldungMillis == 0 && jetztMillis - letzteTempMillis > deltaMeldungMillis)
    {
      digitalWrite(PIN_LED, LOW);
      Temp = DS18B20lesen();
      UDPOut();
      letzteTempMillis = jetztMillis;
      digitalWrite(PIN_LED, HIGH);
    }
    UDPRead();
    OfflineCheck();
    RelaisOut();
    DisplayOut();
    Hauptseite();
    if (jetztMillis < 100000000) {wdt_reset();}             // WatchDog Reset  
  }  
}


   
 
