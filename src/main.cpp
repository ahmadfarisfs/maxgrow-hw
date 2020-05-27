#include <HttpClient.h>
#include "Esp.h"
#include "esp_system.h"
#include "HX711.h"
#include <Wire.h>

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#include <TinyGsmClient.h>

// TTGO T-Call pin definitions
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22
#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

//Load cell config
#define LOADCELL_DOUT_PIN 14
#define LOADCELL_SCK_PIN 12

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial
#define SerialAT Serial1

// GPRS credentials (leave empty, if missing)
const char apn[] = "3gprs";      // Your APN
const char gprsUser[] = "3gprs"; // User
const char gprsPass[] = "3gprs"; // Password
const char simPIN[] = "";        // SIM card PIN code, if any

// Server details
const char server[] = "34.70.87.0";
//const char resource[] = "/update?api_key=C6VTEL02YIAII3VM&field1=90.5";

//const int wdtTimeout = 3000; //time in ms to trigger the watchdog
//hw_timer_t *timer = NULL;

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
HttpClient http(client, server, 8080);

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

HX711 scales[4];
void setup()
{
    // Set console baud rate
    SerialMon.begin(115200);
    delay(10);

    // Keep power when running from battery
    Wire.begin(I2C_SDA, I2C_SCL);
    bool isOk = setPowerBoostKeepOn(1);
    SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

    //setup hx711
    scales[0].begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scales[1].begin(34, 13);//kayaknya 26 nih maslah , kudu ganti     
    scales[2].begin(33, 25);
    scales[3].begin(35, 32);
    //scale.set_scale(2280.f);
    delay(100);
//    scales[0].tare();
  //  scales[1].tare();
  //  scales[2].tare();
  //  scales[3].tare();
    
    //test hx711
 //SerialMon.print("fasf");
   /*
    while (true)
    {
        for (int i = 0; i < 4; i++)
        {
            if (scales[i].is_ready())
            {
                double reading = scales[i].get_value();

                a[i] = reading;
                //            SerialMon.print(i);
                //          SerialMon.print(" HX711 reading: ");
                //        SerialMon.println(reading);
            }
            else
            {
                SerialMon.print(i);
                SerialMon.println("HX711 not found.");
            }
        }
        SerialMon.print(a[0]);
        SerialMon.print(" ");
        SerialMon.print(a[1]);
        SerialMon.print(" ");
        SerialMon.print(a[2]);
        SerialMon.print(" ");
        SerialMon.println(a[3]);
        //.//SerialMon.print(" ");

        delay(1000);
    }*/
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
double a[4];
char buffer[100];
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
        SerialMon.println("fail");
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
  SerialMon.print(F("Getting measurement from load cells"));

 for (int i = 0; i < 4; i++)
        {
            if (scales[i].is_ready())
            {
                double reading = scales[i].get_value();

                a[i] = reading;
                //            SerialMon.print(i);
                //          SerialMon.print(" HX711 reading: ");
                //        SerialMon.println(reading);
            }
            else
            {
                SerialMon.print(i);
                SerialMon.println("HX711 not found.");
            }
        }
        SerialMon.print(a[0]);
        SerialMon.print(" ");
        SerialMon.print(a[1]);
        SerialMon.print(" ");
        SerialMon.print(a[2]);
        SerialMon.print(" ");
        SerialMon.println(a[3]);

        sprintf(buffer, "/post?id=1&lc0=%.02f&lc1=%.02f&lc2=%.02f&lc3=%.02f", a[0],a[1],a[2],a[3]);

        SerialMon.print(F("Performing HTTP GET request... "));

        http.connectionKeepAlive();
        int err = http.get(buffer);
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
        delay(30000);
        //}
    }
}
