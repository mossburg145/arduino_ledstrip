#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include "FS.h"
#include <OTAHelper.h>

#include "SceneHelper.h"
#include "StripConfig.h"

OTAHelper otaHelper;
SceneHelper sceneHelper;
Adafruit_NeoPixel strip = Adafruit_NeoPixel();

NeoPixelAnimator animations(1);
RgbColor startColor(0, 0, 0);
RgbColor targetColor(0, 0, 0);

struct configurationStruct {
    struct wifiStruct {
        const char* ssid;
        const char* key;
    };
    struct deviceStruct {
        const char* name;
        int pixels;
    };
    struct animationStruct {
        int duration;
    };

    wifiStruct wifi;
    deviceStruct device;
    animationStruct animation;
};

configurationStruct configuration;

void setup() {
    Serial.begin(57600);

    SPIFFS.begin();

    if (!readConfigurationAndInit()) return;

    sceneHelper.onChange([&](uint8_t r, uint8_t g, uint8_t b) {
        Serial.printf(
            "\nScene changed, changing color of strip to rgb(%d,%d,%d).\n",
            r, g, b
        );

        // Our current color is the last animations target color.
        startColor = targetColor;
        targetColor = RgbColor(r, g, b);

        animations.StartAnimation(
            0,
            configuration.animation.duration,
            blendingAnimation
        );
    });
}

void loop() {
    otaHelper.handle();
    sceneHelper.handle();

    if (animations.IsAnimating())
    {
        animations.UpdateAnimations();
        strip.show();
    }
}

void blendingAnimation(const AnimationParam& param)
{
    RgbColor color = RgbColor::LinearBlend(
        startColor,
        targetColor,
        param.progress
    );

    for (uint16_t i = 0; i < configuration.device.pixels; i++) {
        strip.setPixelColor(i, strip.Color(color.R, color.G, color.B));
    }
}

bool readConfigurationAndInit()
{
    Serial.print("Reading configuration file.....");

    File configFile = SPIFFS.open("/config.json", "r");

    if (!configFile) {
        Serial.println("Failed to open config file");
        return false;
    }

    Serial.println(".. ok.");

    size_t size = configFile.size();
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);

    DynamicJsonBuffer jsonBuffer;
    JsonObject& config = jsonBuffer.parseObject(buf.get());

    configuration.wifi.ssid = config["wifi"]["ssid"];
    configuration.wifi.key = config["wifi"]["key"];
    configuration.device.name = config["device"]["name"];
    configuration.device.pixels = config["device"]["pixels"];
    configuration.animation.duration = config["animation"]["duration"];

    Serial.printf(
        "\nconfiguration.wifi.ssid = %s\n"
        "configuration.wifi.key = *********\n"
        "configuration.device.name = %s\n"
        "configuration.device.pixels = %d\n"
        "configuration.animation.duration = %d\n\n",
        configuration.wifi.ssid,
        configuration.device.name,
        configuration.device.pixels,
        configuration.animation.duration
    );

    connectWifi();

    otaHelper.setDeviceName(configuration.device.name);
    sceneHelper.setDeviceName(configuration.device.name);

    otaHelper.setup();

    prepareLedStrip();

    loadScenes(config["scenes"]);

    return true;
}

void connectWifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(
        configuration.wifi.ssid,
        configuration.wifi.key
    );

    Serial.print("Connecting to wifi..");

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.println(".. ok.\n");
}

void prepareLedStrip()
{
    strip.updateLength(configuration.device.pixels);
    strip.setPin(LED_STRIP_PIN);
    strip.updateType(LED_STRIP_TYPE);

    strip.begin();
    strip.clear();
    strip.show();

    Serial.println("Cleared led strip.\n");
}

void loadScenes(JsonArray& scenes)
{
    for (auto& sceneInfo : scenes) {
        char* name = strdup(sceneInfo["name"]);
        JsonArray& color = sceneInfo["rgb"];

        uint8_t r = color[0];
        uint8_t g = color[1];
        uint8_t b = color[2];

        bool isOffSwitch = sceneInfo["isOffSwitch"];

        Scene scene;
        scene.name = name;

        if (isOffSwitch) {
            scene.isOffSwitch = true;
        } else {
            scene.r = r;
            scene.g = g;
            scene.b = b;
        }

        sceneHelper.add(scene);
    }
}
