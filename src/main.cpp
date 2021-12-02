/******************************************************************************
 * IOT Temperature logger via Circus Of Things based on M5StickC
 * 
 * Use IOT to detect errors in the night setback function of your heating 
 * system and save costs through effective heating settings. Keep an eye
 * on your heating system to save money!
 * For more information check the project page at Hackster.io
 * https://www.hackster.io/hague/how-to-save-money-with-iot-16b177
 * 
 * Hague Nusseck @ electricidea
 * v0.5 08.Mar.2020
 * 
 * 
 * 
 * https://circusofthings.com/
 * for more information about Circus-of-Things:
 * https://www.hackster.io/jaume_miralles/monitoring-temperature-and-humidity-with-esp32-f53465
 *  
 * Distributed as-is; no warranty is given.
 ******************************************************************************/
#include <Arduino.h>

// M5StickC library:
#include <M5StickC.h>
// Install for PlatformIO:
// pio lib install "M5StickC"

// MCP9808 sensor library:
#include "Adafruit_MCP9808.h"
// Install for PlatformIO:
// pio lib install "Adafruit MCP9808 Library"

// MCP9808 sensor object:
Adafruit_MCP9808 tempsensor1 = Adafruit_MCP9808();

// MCP9808 I2C Adress
//  A2 A1 A0 address
//  0  0  0   0x18  this is the default address
//  0  0  1   0x19
//  0  1  0   0x1A
//  0  1  1   0x1B
//  1  0  0   0x1C
//  1  0  1   0x1D
//  1  1  0   0x1E
//  1  1  1   0x1F
#define MCP9808_I2CADDR 0x18

// MCP9808 Resolution:
// Mode Resolution SampleTime
//  0    0.5°C       30 ms
//  1    0.25°C      65 ms
//  2    0.125°C     130 ms
//  3    0.0625°C    250 ms
#define MCP9808_RES 3

// Required for all Adafruit Unified Sensor based libraries.
#include <Adafruit_Sensor.h>
// Install for PlatformIO:
//pio lib install "Adafruit Unified Sensor"

// TMP006 sensor library:
#include "Adafruit_TMP006.h"
// Install for PlatformIO:
// pio lib install "Adafruit TMP006"

// The default address of the TMP006 is 0x40. 
// By connecting the address pins as in the following table, 
// you can generate any address between 0x40 and 0x47
//  A2 A1 A0 address
//  0  0  0   0x40  this is the default address
//  0  0  1   0x41
//  0  1  0   0x42
//  0  1  1   0x43
//  1  0  0   0x44
//  1  0  1   0x45
//  1  1  0   0x46
//  1  1  1   0x47
#define TMP006_I2CADDR 0x40

// TMP006 sensor object:
Adafruit_TMP006 tempsensor2(TMP006_I2CADDR); 

// Time library;
#include <TimeLib.h>
// pio lib install "Time"
// lib_deps = Time

// Buffer values for Seconds and minutes
// to perform a test every minute
uint8_t last_second;
int second_count;

// WIFI and https client librarys:
#include "WiFi.h"
#include <WiFiClientSecure.h>

// WiFi network configuration:
const char* ssid     = "YourWiFi";
const char* password = "YourPassword";

// circusofthings.com configuration
const char* Circus_hostname = "circusofthings.com";
const char* Circus_key1 = "Your_key1";
const char* Circus_key2 = "Your_key2";
const char* Circus_token = "Your_token";


// Library for a simple text buffer scrolling display
#include "tb_display.h"
// https://www.hackster.io/hague/m5stickc-textbuffer-scrolling-display-fb6428
// https://github.com/electricidea/M5StickC-TB_Display

// Display brightness level
// possible values: 7 - 15
uint8_t screen_brightness = 10; 

// scren Rotation values:
// 1 = Button right
// 2 = Button above
// 3 = Button left
// 4 = Button below
int screen_orientation = 1;

// verbose level (serial output):
// TRUE = lot of data
// FALSE = only time and speed data (table format)
#define verbose_output false

// function forward declaration
boolean connect_Wifi();
int Circus_write(const char* Circus_key, double value);
void scan_I2C_bus();

void setup() {
  // initialize the M5Stack object
  m5.begin();
  // initialize I2C for the M5Stick HAT-Pins
  Wire.begin(0, 26);
  // set screen brightness
  M5.Axp.ScreenBreath(screen_brightness);

  // print a welcome message over serial porta
	Serial.println("===================");
	Serial.println("     M5StickC");
	Serial.println("Temperature logger");
	Serial.println(" 08.03.2020 v0.5");
	Serial.println("===================");

  // init the text buffer display and print welcome text on the display
  tb_display_init(screen_orientation);
  tb_display_print_String("        M5StickC\n\n   Temperature logger\n");
  delay(2000);
  // scan I2C Bus to detect the sensors
  scan_I2C_bus();
  delay(2000);

  // init MCP9808 Temp Sensor
  if (tempsensor1.begin(MCP9808_I2CADDR)) {
    Serial.println("[OK] MCP9808 found");
    tb_display_print_String("[OK] MCP9808 found\n");
  } else {
    Serial.println("[ERR] Couldn't find MCP9808!");
    tb_display_print_String("[ERR] Couldn't find MCP9808!\n");
  }
  tempsensor1.setResolution(MCP9808_RES);

  // init TMP006 Temp Sensor
  // you can also use tmp006.begin(TMP006_CFG_1SAMPLE) or 2SAMPLE/4SAMPLE/8SAMPLE to have
  // lower precision, higher rate sampling. default is TMP006_CFG_16SAMPLE which takes
  // 4 seconds per reading (16 samples)
  if (tempsensor2.begin()) {
    Serial.println("[OK] TMP006 found");
    tb_display_print_String("[OK] TMP006 found\n");
  } else {
    Serial.println("[ERR] Couldn't find TMP006!");
    tb_display_print_String("[ERR] Couldn't find TMP006!\n");
  }
  // wake up MCP9808 and TMP006
  Serial.println("wake up MCP9808.... "); 
  tempsensor1.wake();  
  Serial.println("wake up TMP006.... ");
  tempsensor2.wake();

  // Set WiFi to station mode and disconnect
  // from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  // connect to the configured AP
  connect_Wifi();
  // first measurement in 30 seconds
  last_second = second(now());
  second_count = 600 - 30;
}

void loop() {
  M5.update();
  // get actual time
  time_t t=now();

  // call every second:
  if (second(t) != last_second) {
    last_second = second(t);
    // increase to count 600 seconds = 10 minutes
    second_count++;
  }
  // call every 600 seconds:
  if (second_count >= 600) {    
    // check for Wire errors
    Serial.print("\nWire State: ");
    Serial.println(Wire.getErrorText(Wire.lastError()));

    float temperature1 = tempsensor1.readTempC();
    Serial.printf("Temp1: %2.1f *C\n", temperature1);

    float objtemp2 = tempsensor2.readObjTempC();
    Serial.print("Object Temperature: "); Serial.print(objtemp2); Serial.println("*C");
    float dietemp2 = tempsensor2.readDieTempC();
    Serial.print("Die Temperature: "); Serial.print(dietemp2); Serial.println("*C");

    char String_buffer[128]; 
    snprintf(String_buffer, sizeof(String_buffer), "Temp: %2.1f / %2.1f *C\n", temperature1, objtemp2);
    tb_display_print_String(String_buffer);
    // check if WIFI is still connected
    // if the WIFI ist not connected (anymore)
    // a reconnect is triggert
    wl_status_t wifi_Status = WiFi.status();
    if(wifi_Status != WL_CONNECTED){
      // reconnect if the connection get lost
      Serial.println("[ERR] Lost WiFi connection, reconnecting...");
      tb_display_print_String("[ERR] Lost WiFi!\n");
      if(connect_Wifi()){
        Serial.println("[OK] WiFi reconnected");
        tb_display_print_String("[OK] reconnected\n");
      } else {
        Serial.println("[ERR] unable to reconnect");
        tb_display_print_String("[ERR] unable to reconnect\n");
      }
    }
    // check if WIFI is connected
    // needed because of the above mentioned reconnection attempt
    wifi_Status = WiFi.status();
    if(wifi_Status == WL_CONNECTED){
      if(verbose_output) {
        Serial.print("[OK] WiFi connected / "); 
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.println("-----------------------");
      }
      // Write the measured speed to circusofthings.com
      Circus_write(Circus_key1, temperature1);
      Circus_write(Circus_key2, objtemp2);
    }
    // prepare the next call in one minute
    t=now();
    last_second = second(t);
    second_count = 0;
  }
}


// =============================================================
// connect_Wifi()
// connect to configured Wifi Access point
// returns true if the connection was successful otherwise false
// =============================================================
boolean connect_Wifi(){
  // Establish connection to the specified network until success.
  // Important to disconnect in case that there is a valid connection
  WiFi.disconnect();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  char String_buffer[128]; 
  snprintf(String_buffer, sizeof(String_buffer), "\nConnecting to:\n  %s\n",ssid);
  tb_display_print_String(String_buffer);
  delay(1500);
  //Start connecting (done by the ESP in the background)
  WiFi.begin(ssid, password);
  // read wifi Status
  wl_status_t wifi_Status = WiFi.status();
  int n_trials = 0;
  // loop while Wifi is not connected
  // run only for 20 trials.
  while (wifi_Status != WL_CONNECTED && n_trials < 20) {
    // Check periodicaly the connection status using WiFi.status()
    // Keep checking until ESP has successfuly connected
    wifi_Status = WiFi.status();
    n_trials++;
    switch(wifi_Status){
      case WL_NO_SSID_AVAIL:
          Serial.println("[ERR] WIFI SSID not available");
          tb_display_print_String("[ERR] SSID not available\n");
          break;
      case WL_CONNECT_FAILED:
          Serial.println("[ERR] WIFI Connection failed");
          tb_display_print_String("[ERR] Connection failed\n");
          break;
      case WL_CONNECTION_LOST:
          Serial.println("[ERR] WIFI Connection lost");
          tb_display_print_String("[ERR] Connection lost\n");
          break;
      case WL_DISCONNECTED:
          Serial.println("[STATE] WiFi disconnected");
          tb_display_print_String("[STATE] disconnected\n");
          break;
      case WL_IDLE_STATUS:
          Serial.println("[STATE] WiFi idle status");
          tb_display_print_String("[STATE] WiFi idle\n");
          break;
      case WL_SCAN_COMPLETED:
          Serial.println("[OK] WiFi scan completed");
          tb_display_print_String("[OK] WiFi scan completed\n");
          break;
      case WL_CONNECTED:
          Serial.println("[OK] WiFi connected");
          tb_display_print_String("[OK] WiFi connected\n");
          break;
      default:
          Serial.println("[ERR] WIFI unknown Status");
          tb_display_print_String("[ERR] WIFI unknown Status\n");
          break;
    }
    delay(500);
  }
  if(wifi_Status == WL_CONNECTED){
    // if connected
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    snprintf(String_buffer, sizeof(String_buffer), "IP: %s\n",WiFi.localIP().toString().c_str());
    tb_display_print_String(String_buffer);
    return true;
  } else {
    // if not connected
    Serial.println("[ERR] unable to connect Wifi");
    tb_display_print_String("[ERR] unable to connect\n");
    return false;
  }
}


// =============================================================
// Circus_write(double value)
// send value to circusofthings.com website
// the hostname, token and key need to be configured
// The function returns the number of received lines
// =============================================================
int Circus_write(const char* Circus_key, double value) {
  // Use WiFiClient class to create TCP connection
  WiFiClientSecure client;
  // Needs to be set Insecure, otherwise sometimes errors will occur
  client.setInsecure();
  const int httpsPort = 443;
  if(verbose_output) Serial.printf("--> connect to: %s:%i\r\n",Circus_hostname,httpsPort);
  int n_lines = 0;
  // check the connection
  if (!client.connect(Circus_hostname, httpsPort)) {
    Serial.println("[ERR] Circus Connection failed!!!");
    return 0;
  } else {
    char value_char[15];
    dtostrf(value,1,4,value_char);
    // We now create a URI for the request
    char url_char[250];
    sprintf_P(url_char, PSTR("/WriteValue?Key=%s&Value=%s&Token=%s\r\n"), Circus_key, value_char, Circus_token);
    String url_str = url_char;
    if(verbose_output) {
      Serial.print("Requesting URL: ");
      Serial.println(Circus_hostname+url_str);
    }
    // We need to manually create the HTTP GET message
    // client.print() will send the request to the server
    client.print(String("GET ") + url_str + " HTTP/1.1\r\n" +
                 "Host: " + Circus_hostname + "\r\n" +
                 "Connection: close\r\n\r\n");
    //Wait until a response is available
    int n_trials = 500;
    while(!client.available() && n_trials > 0){
      n_trials--;
      delay(10);
    }
    // Read all the lines of the reply from server
    // force timeout to 1000ms (ESP8266 = 5000ms default value)
    client.setTimeout(1000);
    while(client.available()){
      String line = client.readStringUntil('\r');
      n_lines++;
      if(verbose_output) Serial.print(line);
    }
    if(verbose_output) Serial.printf("\n[OK] %i lines received\n",n_lines);
  }
  return n_lines;
}


//==============================================================
// Scanning I2C adresses from 1 to 127
// and display all available devices
void scan_I2C_bus(){
  int address;
  int error;
  char String_buffer[128]; 
  tb_display_print_String("Scan I2C Bus:\n");
  for(address = 1; address < 128; address++) 
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if(error==0)
    {
      snprintf(String_buffer, sizeof(String_buffer), " %.2X ",address);
      tb_display_print_String(String_buffer);
    }
    else 
      // no device found at this address
      tb_display_print_char('.');
    delay(25);
  }
  tb_display_print_char('\n');
}

