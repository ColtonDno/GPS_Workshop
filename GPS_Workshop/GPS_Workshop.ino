
//Select which board you're using
#define TINY_GSM_MODEM_SIM7000

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

#define TINY_GSM_RX_BUFFER 1024

//Set serial for SIM chip
#define SerialAT Serial1

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

//Special debugger
#define TINY_GSM_DEBUG SerialMon

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <SPI.h>
#include <Ticker.h>
#include "utilities.h"

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

const char server[]   = "www.coltondno.com";
String resource = "/post_lte.php";
String apiKeyValue = "rmf2dJYgD3F4Egvds5S42s";
String uptime_resource = "/device_startup.php";

char apn[] = "h2g2";
String device_id = "Ayo";

TinyGsmClient client(modem, 0);
HttpClient    http(client, server, 80);

struct GPSData
{
    float latitude;
    float longitude;
    float speed;
    float accuracy;
    float altitude;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int vsat;
    int usat;
} GPSData;

void enableGPS(void)
{
  // Set SIM7000G GPIO4 LOW ,turn on GPS power
  // CMD:AT+SGPIO=0,4,1,1
  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    DBG(" SGPIO=0,4,1,1 false ");
  SerialMon.println(modem.waitResponse(10000L));    
  }
  modem.enableGPS();
}

void disableGPS(void)
{
  // Set SIM7000G GPIO4 LOW ,turn off GPS power
  // CMD:AT+SGPIO=0,4,1,0
  modem.sendAT("+SGPIO=0,4,1,0");
  if (modem.waitResponse(10000L) != 1) {
    DBG(" SGPIO=0,4,1,0 false ");
  }
  modem.disableGPS();
}

void printGPS(struct GPSData data)
{
  char buffer[100];
  SerialMon.println("GPS Data:");
  sprintf(buffer,"%d/%d/%d %d:%d:%d",data.month,data.day,data.year,data.hour,data.minute,data.second);
  SerialMon.println(buffer);

  SerialMon.print("Latitude: ");Serial.println(data.latitude, 6);
  SerialMon.print("Longitude: ");Serial.println(data.longitude, 6);
  SerialMon.print("Altitude: ");Serial.println(data.altitude);
  SerialMon.print("Speed: ");Serial.println(data.speed);
  SerialMon.print("Accuracy: ");Serial.println(data.accuracy);

  return;
}

float timer = millis();

void setup() {
  SerialMon.begin(115200);
  delay(10);
  SerialMon.println("Starting;");
  
  //Parameters are defined in utilities
  SerialAT.begin(UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  //Initialize modem
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(1000);
  digitalWrite(MODEM_PWRKEY, LOW);

  SerialMon.println("Initializing modem...");
  if (!modem.init()) {
    modem.restart();
    delay(1000);
    Serial.println("Failed to restart modem, attempting to continue without restarting");
    return;
  }

  uint8_t ret = modem.setNetworkMode(2);
  DBG("setNetworkMode:", ret);

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

  delay(5000);  
  SerialMon.println("Finished Setup");
}

void loop() {
  SerialMon.println("Looping");

  if (!modem.gprsConnect("iot.truphone.com")) {
    SerialMon.println("Failed GRPS setup");
    delay(1000);
    return;
  }
  if (modem.isGprsConnected()) { SerialMon.println("GPRS connected"); }

  if (!modem.isNetworkConnected()) {
    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork()) {
      SerialMon.println(" failed");
      delay(1000);
      return;
    }
    SerialMon.println(" success");
    digitalWrite(LED_PIN, HIGH);
  }
  if (modem.isNetworkConnected()) { SerialMon.println("Network connected"); }

  SerialMon.println("Enabling GPS");
  enableGPS();
  
  String rawData = modem.getGPSraw();
  SerialMon.println(rawData);
  
  struct GPSData gpsData;
  while (!modem.getGPS(&gpsData.latitude, &gpsData.longitude, &gpsData.speed, &gpsData.altitude, &gpsData.vsat, &gpsData.usat, &gpsData.accuracy, &gpsData.year, &gpsData.month, &gpsData.day, &gpsData.hour, &gpsData.minute, &gpsData.second)) 
  {
    delay(2000);
  }

  printGPS(gpsData);

  disableGPS();

  delay(1000);

  String httpRequestData = "api_key=" + apiKeyValue + "&latitude=" + String(gpsData.latitude, 6) + "&longitude=" + String(gpsData.longitude,6) + "&speed=" + String(gpsData.speed,3) + "&accuracy=" + String(gpsData.accuracy,4) + "&device_id=" + device_id + "";
  SerialMon.println(httpRequestData);
  String contentType = "application/x-www-form-urlencoded";
  SerialMon.print(F("Performing HTTP POST request... "));
  int err = http.post(resource, contentType, httpRequestData);
  if (err != 0) {
    SerialMon.println(F("failed to connect"));
    SerialMon.println(err);
    delay(60000);
    return;
  }

  int status = http.responseStatusCode();
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status) {
    delay(60000);
    return;
  }

  SerialMon.println(F("Response Headers:"));
  while (http.headerAvailable()) {
    String headerName  = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    SerialMon.println("    " + headerName + " : " + headerValue);
  }

  int length = http.contentLength();
  if (length >= 0) {
    SerialMon.print(F("Content length is: "));
    SerialMon.println(length);
  }
  if (http.isResponseChunked()) {
    SerialMon.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);

  SerialMon.print(F("Body length is: "));
  SerialMon.println(body.length());
  //for(;;){delay(1000);}

  //Wait until at least a minute has passed before looping
  while (millis() - timer < 10000)
    delay(1000);
  timer = millis();
}