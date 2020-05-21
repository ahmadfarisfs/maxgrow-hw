/**************************************************************
 *
 * This sketch connects to a website and downloads a page.
 * It can be used to perform HTTP/RESTful API calls.
 *
 * TinyGSM Getting Started guide:
 *   https://tiny.cc/tinygsm-readme
 *
 **************************************************************/
#include <HttpClient.h>
#include "Esp.h"
#include "esp_system.h"
const int wdtTimeout = 3000; //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;
// Your GPRS credentials (leave empty, if missing)
const char apn[] = "3gprs";      // Your APN
const char gprsUser[] = "3gprs"; // User
const char gprsPass[] = "3gprs"; // Password
const char simPIN[] = "";        // SIM card PIN code, if any

// TTGO T-Call pin definitions
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to the module)
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

// Define the serial console for debug prints, if needed
//#define TINY_GSM_DEBUG SerialMon
//#define DUMP_AT_COMMANDS
#include "HX711.h"
#include <Wire.h>
#include <TinyGsmClient.h>
//#include "utilities.h"

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

bool setPowerBoostKeepOn(int en)
{
    Wire.beginTransmission(IP5306_ADDR);
    Wire.write(IP5306_REG_SYS_CTL0);
    if (en)
    {
        Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
    }
    else
    {
        Wire.write(0x35); // 0x37 is default reg value
    }
    return Wire.endTransmission() == 0;
}
// Server details
const char server[] = "api.thingspeak.com";
const char resource[] = "/update?api_key=C6VTEL02YIAII3VM&field1=90.5";
const int LOADCELL_DOUT_PIN = 14;
const int LOADCELL_SCK_PIN = 12;
TinyGsmClient client(modem);
const int port = 80;
HttpClient http(client, server, 80);
HX711 scale;
void setup()
{
    // Set console baud rate
    SerialMon.begin(115200);
    delay(10);

    // Keep power when running from battery
    Wire.begin(I2C_SDA, I2C_SCL);
    bool isOk = setPowerBoostKeepOn(1);
    SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    //setup hx711
    scale.set_scale(2280.f);                      // this value is obtained by calibrating the scale with known weights; see the README for details
    scale.tare();				        // reset the scale to 0
    //test hx711
    while (true)
    {
        if (scale.is_ready())
        {
           long reading = scale.read();
            SerialMon.print("HX711 reading: ");
           // SerialMon.println(scale.get_units(10), 1);
        }
        else
        {
            SerialMon.println("HX711 not found.");
        }

        delay(1000);
    }
    // Set-up modem reset, enable, power pins
    pinMode(MODEM_PWKEY, OUTPUT);
    pinMode(MODEM_RST, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);

    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    /*  SerialMon.println("Initializing modem...");
    modem.restart();
    // Or, use modem.init() if you don't need the complete restart

    String modemInfo = modem.getModemInfo();
    SerialMon.print("Modem: ");
    SerialMon.println(modemInfo);

    // Unlock your SIM card with a PIN if needed
    if (strlen(simPIN) && modem.getSimStatus() != 3)
    {
        modem.simUnlock(simPIN);
    }*/
}

void loop()
{
bailout:
    //if something happen, restart modem weh, network ini rada aneh behaviournya
    digitalWrite(MODEM_PWKEY, LOW);
    digitalWrite(MODEM_RST, HIGH);
    digitalWrite(MODEM_POWER_ON, HIGH);
    http.stop();
    // Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000);
    SerialMon.println("Initializing modem...");
    modem.restart();
    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork(240000L))
    {
        SerialMon.println(" fail");
        delay(10000);
        goto bailout;
    }
    SerialMon.println(" OK");

    if (modem.isNetworkConnected())
    {
        SerialMon.println("Network connected");
    }

    SerialMon.print(F("Connecting to APN: "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass))
    {
        SerialMon.println(" fail");
        delay(10000);
        goto bailout;
    }
    SerialMon.println(" OK");

    while (true)
    {
        SerialMon.print(F("Performing HTTP GET request... "));

        http.connectionKeepAlive();
        int err = http.get(resource);
        if (err != 0)
        {
            SerialMon.print(F("failed to connect "));
            SerialMon.println(err);
            delay(500);
            ESP.restart();

            break;
        }
        int status = http.responseStatusCode();
        SerialMon.print(F("Response status code: "));
        SerialMon.println(status);
        if (status < 0)
        {
            SerialMon.println("status is nono");
            ESP.restart();
            delay(500);
            break;
        }

        SerialMon.println(F("Response Headers:"));
        while (http.headerAvailable())
        {
            String headerName = http.readHeaderName();
            String headerValue = http.readHeaderValue();
            SerialMon.println("    " + headerName + " : " + headerValue);
        }

        int length = http.contentLength();
        if (length >= 0)
        {
            SerialMon.print(F("Content length is: "));
            SerialMon.println(length);
        }
        if (http.isResponseChunked())
        {
            SerialMon.println(F("The response is chunked"));
        }

        String body = http.responseBody();
        SerialMon.println(F("Response:"));
        SerialMon.println(body);

        SerialMon.print(F("Body length is: "));
        SerialMon.println(body.length());

        // Shutdown
        http.stop();
        //    SerialMon.println(F("Server disconnected"));
        //      modem.gprsDisconnect();

        //        SerialMon.println(F("GPRS disconnected"));
        //  while (true) {
        //semenit detk sekali
        delay(60000);
        //}
    }
}
