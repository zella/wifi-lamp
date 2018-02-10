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

#define MIN_VAL 0
#define MAX_VAL 15

#define ROTARY_PIN1 D1
#define ROTARY_PIN2 D2

#define OUT_PIN1 D5
#define OUT_PIN2 D6
#define OUT_PIN3 D7
#define OUT_PIN4 D8

Ticker ticker;
Ticker encTicker;

char blynk_token[34] = "BLYNK_TOKEN";

bool shouldSaveConfig = false; //flag for saving data

SimpleTimer timer;

byte lampBright = 0;

// void printBits(byte myByte){
//  for(byte mask = 0x80; mask; mask >>= 1){
//    if(mask  & myByte)
//        Serial.print('1');
//    else
//        Serial.print('0');
//  }
// }

void setHardwareVal()
{
    digitalWrite(OUT_PIN1, HIGH && (lampBright & B00001000));
    digitalWrite(OUT_PIN2, HIGH && (lampBright & B00000100));
    digitalWrite(OUT_PIN3, HIGH && (lampBright & B00000010));
    digitalWrite(OUT_PIN4, HIGH && (lampBright & B00000001));

    if(Blynk.connected()){
        Blynk.virtualWrite(V3, lampBright);
    }

    Serial.println(lampBright);
}

BLYNK_WRITE(V3)
{
    lampBright = param.asInt(); // 0 .. 15
    Serial.print("From wifi:");
    Serial.println(lampBright);

    setHardwareVal();
}

void decLamp()
{
    if (lampBright > MIN_VAL)
    {
        lampBright = lampBright - 1;
        setHardwareVal();
    }
}

void incLamp()
{
    if (lampBright < MAX_VAL)
    {
        lampBright = lampBright + 1;
        setHardwareVal();
    }
}

byte lastState = 0;
byte steps = 0;
int dir = 0;
byte AState = 0;
byte BState = 0;
byte State = 0;

void encF()
{
    // read the input pin:
    AState = digitalRead(ROTARY_PIN1);
    BState = digitalRead(ROTARY_PIN2) << 1;
    State = AState | BState;

    bool isChanged = false;

    if (lastState != State)
    {
        switch (State)
        {
        case 0:
            if (lastState == 2)
            {
                steps++;
                dir = 1;
                isChanged = true;
            }
            else if (lastState == 1)
            {
                steps--;
                dir = -1;
                isChanged = true;
            }
            break;
        case 1:
            if (lastState == 0)
            {
                steps++;
                dir = 1;
                isChanged = true;
            }
            else if (lastState == 3)
            {
                steps--;
                dir = -1;
                isChanged = true;
            }
            break;
        case 2:
            if (lastState == 3)
            {
                steps++;
                dir = 1;
                isChanged = true;
            }
            else if (lastState == 0)
            {
                steps--;
                dir = -1;
                isChanged = true;
            }
            break;
        case 3:
            if (lastState == 1)
            {
                steps++;
                dir = 1;
                isChanged = true;
            }
            else if (lastState == 2)
            {
                steps--;
                dir = -1;
                isChanged = true;
            }
            break;
        }
    }

    lastState = State;
    if (isChanged)
    {
        // Serial.print(dir);
        // Serial.print("\t");
        // Serial.println(steps);
        if (dir == -1)
        {
            decLamp();
        }
        else
        {
            incLamp();
        }
    }
}

void tick()
{
    //toggle state
    int state = digitalRead(BUILTIN_LED); // get the current state of GPIO1 pin
    digitalWrite(BUILTIN_LED, !state);    // set pin to the opposite state
}

void encTick()
{
    // r.loop();
    encF();
}

void saveConfigCallback()
{ //callback notifying us of the need to save config
    Serial.println("Should save config");
    shouldSaveConfig = true;
    ticker.attach(0.2, tick); // led toggle faster
}

void setup()
{

    Serial.begin(9600);
    Serial.println();

    //set led pin as output
    pinMode(BUILTIN_LED, OUTPUT);
    pinMode(OUT_PIN1, OUTPUT);
    pinMode(OUT_PIN2, OUTPUT);
    pinMode(OUT_PIN3, OUTPUT);
    pinMode(OUT_PIN4, OUTPUT);
    pinMode(ROTARY_PIN1, INPUT_PULLUP);
    pinMode(ROTARY_PIN2, INPUT_PULLUP);

    setHardwareVal();
    // start ticker with 0.5 because we start in AP mode and try to connect
    ticker.attach(0.6, tick);
    encTicker.attach_ms(10, encTick);

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
    if (!wifiManager.autoConnect("CentralHeatingAP", "testtest"))
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
                 //r.loop(); using ticker
}