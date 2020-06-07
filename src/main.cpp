#include <HttpClient.h>
#include "Esp.h"
#include "esp_system.h"
#include "HX711.h"
#include <Wire.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "EEPROM.h"

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
//const char apn[] = "3gprs";      // Your APN
//const char gprsUser[] = "3gprs"; // User
//const char gprsPass[] = "3gprs"; // Password
const char simPIN[] = ""; // SIM card PIN code, if any

// Server details
//const char server[] = "34.70.87.0";
const char server[] = "feeder.maxgrow.id";
//const char resource[] = "/update?api_key=C6VTEL02YIAII3VM&field1=90.5";

//const int wdtTimeout = 3000; //time in ms to trigger the watchdog
//hw_timer_t *timer = NULL;

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
HttpClient http(client, server, 80);

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
struct APNSetting
{
    char apn[20];
    char user[20];
    char pass[20];
};
HX711 scales[4];

AsyncWebServer serverX(80);
const char *lcoffset[] = {"lcoff0", "lcoff1", "lcoff2", "lcoff3"};
const char *lcscale[] = {"lcscale0", "lcscale1", "lcscale2", "lcscale3"};

const char *apnSet[] = {"apn", "user", "pass"};
const char *ssid = "MAXGROW-IoT";
const char *password = "testpassword";
const char *totaloff = "totaloff";
const char *samplingPeriod = "sampp";

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

struct Setting
{
    float scale[4];
    float offset[4];
    float totalOffset;
    long samplingPeriod;
};

Setting mySetting;
APNSetting myInternet;
void setup()
{
    // Set console baud rate
    SerialMon.begin(115200);
    delay(1000);
    if (!EEPROM.begin(1000))
    {
        Serial.println("Failed to initialise EEPROM");
        Serial.println("Restarting...");

        ESP.restart();
    }
    EEPROM.get(2, mySetting);
    EEPROM.get(100, myInternet);
    Serial.println("Read config from eeprom");
    for (int i = 0; i < 4; i++)
    {
        Serial.print("Scale:");
        Serial.println(mySetting.scale[i]);
        Serial.print("Offset:");
        Serial.println(mySetting.offset[i]);
    }
    Serial.print("TotalOffset:");
    Serial.println(mySetting.totalOffset);
    delay(10);
    WiFi.softAP(ssid, password);
    Serial.println("Wait 100 ms for AP_START...");
    delay(100);

    Serial.println("Set softAPConfig");
    IPAddress Ip(192, 168, 1, 1);
    IPAddress NMask(255, 255, 255, 0);
    WiFi.softAPConfig(Ip, Ip, NMask);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
    serverX.on("/set_apn", HTTP_GET, [](AsyncWebServerRequest *request) {
        String message;
        bool valid = true;
        for (int i = 0; i < 3; i++)
        {
            if (request->hasParam(apnSet[i]))
            {
                message = request->getParam(apnSet[i])->value();
                switch (i)
                {
                case 0:
                    message.toCharArray(myInternet.apn, 20);
                    break;
                case 1:
                    message.toCharArray(myInternet.user, 20);
                    break;
                case 2:
                    message.toCharArray(myInternet.pass, 20);
                    break;
                }
            }
            else
            {
                valid = false;
                message = "No message sent";
            }
        }
        if (valid)
        {
            EEPROM.put(100, myInternet);
            EEPROM.commit();
            request->send(200, "text/plain", "APN Set! Please restart");
        }
        else
        {
            request->send(400, "text/plain", "Data is not complete");
        }
    });
    // Send a GET request to <IP>/get?message=<message>
    serverX.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request) {
        String message;
        bool valid = true;
        //parse offset
        for (int i = 0; i < 4; i++)
        {
            if (request->hasParam(lcoffset[i]))
            {
                message = request->getParam(lcoffset[i])->value();
                if (message != ""){
                      mySetting.offset[i] = message.toFloat();
    
                }
            }
            else
            {
                message = "No message sent";
            }
            //parse scale
            if (request->hasParam(lcscale[i]))
            {
                message = request->getParam(lcscale[i])->value();
               
                 if (message != ""){
                mySetting.scale[i] = message.toFloat();
            }}
            else
            {
                //  valid = false;
                message = "No message sent";
            }
        }
        //parse total
        if (request->hasParam(totaloff))
        {
            message = request->getParam(totaloff)->value();
            if (message != ""){
            mySetting.totalOffset = message.toFloat();
        }}
        else
        {
            //  valid = false;
            message = "No message sent";
        }
        //prse sampling periode
        if (request->hasParam(samplingPeriod))
        {
            message = request->getParam(samplingPeriod)->value();
              if (message != ""){
            mySetting.samplingPeriod = message.toInt();
        }}
        else
        {
          //  valid = false;
            message = "No message sent";
        }
        if (valid)
        {
            EEPROM.put(2, mySetting);
            EEPROM.commit();
            Serial.println("Write config to eeprom");
            for (int i = 0; i < 4; i++)
            {
                Serial.print("Scale:");
                Serial.println(mySetting.scale[i]);
                Serial.print("Offset:");
                Serial.println(mySetting.offset[i]);
            }
            Serial.print("TotalOffset:");
            Serial.println(mySetting.totalOffset);
            request->send(200, "text/plain", "Hello, Calibrated !");
        }
        else
        {
            request->send(400, "text/plain", "Data is not complete!");
        }
    });
    // Send a GET request to <IP>/get?message=<message>
    serverX.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
        String message;
        //parse offset
        double sum = 0.0;
        double res;
        for (int i = 0; i < 4; i++)
        {
            if (scales[i].is_ready())
            {
                double reading = scales[i].get_value();

                res = reading * mySetting.scale[i] + mySetting.offset[i];
                sum += res;
                //            SerialMon.print(i);
                //          SerialMon.print(" HX711 reading: ");
                //        SerialMon.println(reading);
                message += "loadcell" + String(i) + "kg: " + String(res, 2) + ",raw:" + String(reading, 2) + ",scale:" + String(mySetting.scale[i], 2) + ",offset:" + String(mySetting.offset[i], 2) + "\r\n";
            }
            else
            {
                message += "loadcell" + String(i) + "kg:NaN,raw:NaN,scale:" + String(mySetting.scale[i], 2) + ",offset:" + String(mySetting.offset[i], 2) + "\r\n";
                SerialMon.print(i);
                SerialMon.println("HX711 not found.");
            }
        }
        message += "total offset: " + String(mySetting.totalOffset, 2) + "\r\n";
        message += "calculated total: " + String(sum + mySetting.totalOffset)+ "\r\n";
     message += "sampling periode: " + String(mySetting.samplingPeriod);

        //  EEPROM.put(2, mySetting);
        request->send(200, "text/plain", "Value:\r\n" + message);
    });

    serverX.onNotFound(notFound);
    serverX.begin();
    // Keep power when running from battery
    Wire.begin(I2C_SDA, I2C_SCL);
    bool isOk = setPowerBoostKeepOn(1);
    SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

    //setup hx711
    scales[0].begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scales[1].begin(34, 13);
    scales[2].begin(33, 25);
    scales[3].begin(35, 32);
    delay(100);
 
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
    if (!modem.waitForNetwork(60000L))
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
    SerialMon.print(myInternet.apn);
    //try 5 timres
    if (!modem.gprsConnect(myInternet.apn, myInternet.user, myInternet.pass))
    {
        SerialMon.println(" fail");
        delay(10000);
        goto bailout;
    }
    SerialMon.println(" OK");

    while (true)
    {
        SerialMon.println(F("Getting measurement from load cells"));

        for (int i = 0; i < 4; i++)
        {
            if (scales[i].is_ready())
            {
                double reading = scales[i].get_value();

                a[i] = reading * mySetting.scale[i] + mySetting.offset[i];
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

        double sumoff = a[0] + a[1] + a[2] + a[3] + mySetting.totalOffset;
        //   sprintf(buffer, "/post?id=1&lc0=%.02f&lc1=%.02f&lc2=%.02f&lc3=%.02f", a[0], a[1], a[2], a[3]);
        sprintf(buffer, "/api/payload?device_id=1&payload=%.02f", sumoff);

        SerialMon.print(F("Performing HTTP GET request... "));

        http.connectionKeepAlive();
        int err = http.post(buffer);
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

        // Shutdown
        http.stop();
        //    SerialMon.println(F("Server disconnected"));
        //      modem.gprsDisconnect();

        //        SerialMon.println(F("GPRS disconnected"));
        //  while (true) {
        //semenit detk sekali
        delay(mySetting.samplingPeriod);
        //}
    }
}
