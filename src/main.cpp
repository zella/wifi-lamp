#include <FS.h> //this needs to be first, or it all crashes and burns...
//#define BLYNK_DEBUG           // Comment this out to disable debug and save space
#define BLYNK_PRINT Serial // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager

//for LED status
#include <Ticker.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>

#define PIN_OUT D1
#define PWM_PERIOD 20
#define MEASURE_DELTA 32
#define MEASURE_DELTA_PERIOD 100

Ticker ticker;
Ticker pwmTicker;

char blynk_token[34] = "BLYNK_TOKEN";

bool shouldSaveConfig = false; //flag for saving data

SimpleTimer timer;

int lastPwm = 0;

bool wifiSetted = false;

BLYNK_WRITE(V3)
{
    int pwmVal = param.asInt(); // assigning incoming value from pin V1 to a variable
    Serial.print("v3:");
    Serial.println(pwmVal);

    analogWrite(PIN_OUT, pwmVal);
    wifiSetted = true;
}

void tick()
{
    //toggle state
    int state = digitalRead(BUILTIN_LED); // get the current state of GPIO1 pin
    digitalWrite(BUILTIN_LED, !state);    // set pin to the opposite state
}

void pwmControlCallback()
{
    if (!wifiSetted)
    {
        int pwmVal = analogRead(A0);
        analogWrite(PIN_OUT, pwmVal);
    }
}

void deltaMeasureCallback()
{
    int pwmVal = analogRead(A0);

    if (abs(pwmVal - lastPwm) > MEASURE_DELTA)
    {
        wifiSetted = false;
    }
    lastPwm = pwmVal;

    Serial.print("measured:");
    Serial.println(pwmVal);
}

void saveConfigCallback()
{ //callback notifying us of the need to save config
    Serial.println("Should save config");
    shouldSaveConfig = true;
    ticker.attach(0.2, tick); // led toggle faster
}

void setup()
{
    pinMode(PIN_OUT, OUTPUT);
    // pinMode(PIN_DOWN, OUTPUT);
    // pinMode(PIN_UP, OUTPUT);

    Serial.begin(9600);
    Serial.println();

    //set led pin as output
    pinMode(BUILTIN_LED, OUTPUT);
    // start ticker with 0.5 because we start in AP mode and try to connect
    ticker.attach(0.6, tick);

    pwmTicker.attach_ms(PWM_PERIOD, pwmControlCallback);
    pwmTicker.attach_ms(MEASURE_DELTA_PERIOD, deltaMeasureCallback);
    //SPIFFS.format();    //clean FS, for testing
    Serial.println("Mounting FS..."); //read configuration from FS json

    if (SPIFFS.begin())
    {
        Serial.println("Mounted file system");
        if (SPIFFS.exists("/config.json"))
        {
            //file exists, reading and loading
            Serial.println("Reading config file");
            File configFile = SPIFFS.open("/config.json", "r");
            if (configFile)
            {
                Serial.println("Opened config file");
                size_t size = configFile.size();
                // Allocate a buffer to store contents of the file.
                std::unique_ptr<char[]> buf(new char[size]);

                configFile.readBytes(buf.get(), size);
                DynamicJsonBuffer jsonBuffer;
                JsonObject &json = jsonBuffer.parseObject(buf.get());
                json.printTo(Serial);
                if (json.success())
                {
                    Serial.println("\nparsed json");

                    strcpy(blynk_token, json["blynk_token"]);
                }
                else
                {
                    Serial.println("Failed to load json config");
                }
            }
        }
    }
    else
    {
        Serial.println("Failed to mount FS");
    }
    //end read

    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 33); // was 32 length

    Serial.println(blynk_token);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    wifiManager.setSaveConfigCallback(saveConfigCallback); //set config save notify callback

    //set static ip
    // this is for connecting to Office router not GargoyleTest but it can be changed in AP mode at 192.168.4.1
    //wifiManager.setSTAStaticIPConfig(IPAddress(192,168,10,111), IPAddress(192,168,10,90), IPAddress(255,255,255,0));

    wifiManager.addParameter(&custom_blynk_token); //add all your parameters here

    //wifiManager.resetSettings();  //reset settings - for testing

    //set minimu quality of signal so it ignores AP's under that quality
    //defaults to 8%
    //wifiManager.setMinimumSignalQuality();

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep, in seconds
    wifiManager.setTimeout(600); // 10 minutes to enter data and then Wemos resets to try again.

    //fetches ssid and pass and tries to connect, if it does not connect it starts an access point with the specified name
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect("CentralHeatingAP", "MY123PWD"))
    {
        Serial.println("Failed to connect and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        ESP.reset();
        delay(5000);
    }
    Serial.println("Connected Central Heating System :)"); //if you get here you have connected to the WiFi
    ticker.detach();
    //turn LED off
    digitalWrite(BUILTIN_LED, HIGH);

    strcpy(blynk_token, custom_blynk_token.getValue()); //read updated parameters

    if (shouldSaveConfig)
    { //save the custom parameters to FS
        Serial.println("saving config");
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.createObject();
        json["blynk_token"] = blynk_token;

        File configFile = SPIFFS.open("/config.json", "w");
        if (!configFile)
        {
            Serial.println("Failed to open config file for writing");
        }

        json.printTo(Serial);
        json.printTo(configFile);
        configFile.close();
        //end save
    }

    Serial.println("local ip");
    Serial.println(WiFi.localIP());

    Blynk.config(blynk_token);
    Blynk.connect();
}

void loop()
{
    Blynk.run(); // Initiates Blynk
    timer.run(); // Initiates SimpleTimer
}