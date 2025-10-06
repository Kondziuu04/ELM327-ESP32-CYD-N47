#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <BluetoothSerial.h>
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

BluetoothSerial SerialBT;

uint8_t elmMAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // MAC address of the ELM327 module

void sendOBDCommand(const char* cmd) {
    SerialBT.print(cmd);
    SerialBT.print("\r");
}

String readELM327(uint32_t timeoutMs = 1200) { // wait for and read ELM327
    String resp = "";
    resp.reserve(64);// reserve memory
    unsigned long t0 = millis();

while (SerialBT.available()) {// clean Rx bufor
    SerialBT.read();
}

    while (millis() - t0 < timeoutMs) {
        while (SerialBT.available()) {
            const char c = SerialBT.read();
            if (c == '>') { // prompt = end of answer

                if (resp.indexOf("SEARCHING...") == -1) {// skip searching message
                return resp;
            }
            resp = "";// clear
            }
            resp += c;
        }
    }
    return resp;
}

String cleanResp(String resp) {// clean external characters
    resp.replace("\r", " ");
    resp.replace("\n", " ");
    resp.replace(">", " ");
    while (resp.indexOf("  ") != -1) resp.replace("  ", " ");
    resp.trim();
    return resp;
}

constexpr uint32_t DEFAULT_AT_TIMEOUT_MS = 2000;  // timeout for atCommands
constexpr uint32_t ATZ_TIMEOUT_MS = 5000;
bool sendAndVerify(const char* cmd, const char* expected, uint32_t timeoutMs = DEFAULT_AT_TIMEOUT_MS) {
    if (!SerialBT.connected()) {
        Serial.print("Not connected, cannot send: ");
        Serial.println(cmd);
        return false;
    }

    sendOBDCommand(cmd);
    const String resp = cleanResp(readELM327());

    // Check if AT command response
    const bool ok = resp.indexOf(expected) != -1;
    if (!ok) {
        Serial.print("Command failed: ");
        Serial.print(cmd);
        Serial.print(" | Response: ");
        Serial.println(resp);
    }
    return ok;
}

constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;
bool connectAndInit(const bool fullReset ) {// connect to ELM327

    const uint32_t t0 = millis();
    bool connected = false;

    while ((millis() - t0) < CONNECT_TIMEOUT_MS ) {
        Serial.println("Try to connect...");
        if (SerialBT.connect(elmMAC)) {
            connected = true;
            Serial.println("Connect!!!");
            break;
        }
        delay(500);
    }

    if (!connected) {
        Serial.println("Fail to connect to ELM327");
        return false;
    }

        delay(1200);

        while (SerialBT.available()) SerialBT.read();// clean RX before send atCommands

    if (!SerialBT.connected()) {
        Serial.println("Lost connection to ELM327");
        return false;
    }

    if (fullReset) {
        if (!sendAndVerify("ATZ", "ELM327", ATZ_TIMEOUT_MS)) {
            Serial.println("ATZ failed");
            return false;
        }
        delay(1200);
    }

    const char* atCommands[] = {"ATE0",
        "ATL0",
        "ATS0",
        "ATH0",
        "ATH1",
        "ATSP0",
        "ATSP6",
        "ATSH7E0"};

    for (const char* cmd : atCommands) {
        if (!sendAndVerify(cmd, "OK")) {
            Serial.println("Failed with atCommands");
            Serial.print(cmd);
            return false;
        }
        delay(200);
    }
    return true;
}

int parseCoolantTemp(const String& resp) {
    int idx = resp.indexOf("41 05");
    if (idx == -1) idx = resp.indexOf("4105"); // format for 41 05 XX or 4105XX
    if (idx != -1 && idx + 4 <= resp.length()) {

        String hexByte;
        if (resp[idx + 4] == ' ') hexByte = resp.substring(idx + 5, idx + 7);
        else hexByte = resp.substring(idx + 4, idx + 6);
        int val = strtol(hexByte.c_str(), nullptr, 16);
        return val - 40;// formula
    }
    return -1000;
}

bool isBMW = true;

    float parseOilTemp(const String& resp) {
        // for BMW
        if (resp.startsWith("7E8")) {
            String temp = resp;
            temp.remove(0, 4);  // remove "7E8 "
            temp.trim();

            if (temp.length() >= 4) {
                String hexHi = temp.substring(0, 2);
                String hexLo = temp.substring(2, 4);
                int hi = strtol(hexHi.c_str(), NULL, 16);
                int lo = strtol(hexLo.c_str(), NULL, 16);
                int raw = (hi << 8) | lo;
                isBMW = true;
                return static_cast<float>(raw) * 0.01f - 100.0f;// formula
            }
        }
        // for standard pids
        else if (resp.indexOf("41 5C") != -1 || resp.indexOf("415C") != -1) {
            int idx = resp.indexOf("41 5C");
            if (idx == -1) idx = resp.indexOf("415C");

            // format "41 5C XX" or "415CXX"
            String hexVal;
            if (resp[idx + 4] == ' ') {
                hexVal = resp.substring(idx + 5, idx + 7);
            } else {
                hexVal = resp.substring(idx + 4, idx + 6);
            }

            int temp = strtol(hexVal.c_str(), nullptr, 16);
            isBMW = false;
            return static_cast<float>(temp) - 40.0f;// formula
        }
        return -1000.0f;  // if error
    }

float parseVoltage(const String& resp) {
    int idx = resp.indexOf("41 42");
    if (idx == -1) idx = resp.indexOf("4142");
    if (idx == -1) return -1.0f;  // error

    // format "41 42 XX YY" or "4142XXYY"
    String hexByte1, hexByte2;
    if (resp[idx + 4] == ' ') {
        hexByte1 = resp.substring(idx + 5, idx + 7);
        hexByte2 = resp.substring(idx + 8, idx + 10);
    } else {
        hexByte1 = resp.substring(idx + 4, idx + 6);
        hexByte2 = resp.substring(idx + 6, idx + 8);
    }

    int A = strtol(hexByte1.c_str(), nullptr, 16);
    int B = strtol(hexByte2.c_str(), nullptr, 16);

    return (256.0f * static_cast<float>(A) + static_cast<float>(B)) / 1000.0f;
}

float parseMAP(const String& resp) {
    int idx = resp.indexOf("41 0B");
    if (idx == -1) idx = resp.indexOf("410B");
    if (idx == -1) return -1.0f;

    String hexByte;
    if (resp[idx + 4] == ' ') {
        hexByte = resp.substring(idx + 5, idx + 7);
    } else {
        hexByte = resp.substring(idx + 4, idx + 6);
    }

    return static_cast<float>(strtol(hexByte.c_str(), nullptr, 16));  // kPa
}

float parseBARO(const String& resp) {
    int idx = resp.indexOf("41 33");
    if (idx == -1) idx = resp.indexOf("4133");
    if (idx == -1) return -1.0f;

    String hexByte;
    if (resp[idx + 4] == ' ') {
        hexByte = resp.substring(idx + 5, idx + 7);
    } else {
        hexByte = resp.substring(idx + 4, idx + 6);
    }

    return static_cast<float>(strtol(hexByte.c_str(), nullptr, 16));  // kPa
}

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

constexpr unsigned long UI_UPDATE_INTERVAL = 500;  //  UI refreshing
unsigned long lastUIUpdate = 0;

    void setup() {
        // ----- TFT setup ----- //
        tft.begin();
        tft.setRotation(1);
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        tft.setCursor(0, 0);
        tft.drawRect(0, 0, 320, 240, TFT_WHITE);
        tft.drawLine(160, 0, 160, 240, TFT_WHITE);
        tft.drawLine(0, 120, 320, 120, TFT_WHITE);

        tft.setTextSize(1);
        tft.drawCentreString("Temp Coolant", 80, 20, 2);
        tft.drawCentreString("Temp Oil", 240, 20, 2);
        tft.drawCentreString("Boost", 80, 140, 2);
        tft.drawCentreString("Voltage", 240, 140, 2);

        // ----- ELM327 setup ----- //
        Serial.begin(115200);

        if (SerialBT.begin("obdDisplay", true)) {
            Serial.println("Start BT Classic success");
        }
        else {
            Serial.println("Start BT Classic fail");
        }

        esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 4, (uint8_t *)"1234");// eml327 password

        connectAndInit(true);
    }

int tempC = 0;
int tempOil = 0;
int voltage = 0;
float boost = 0.0f;

String prevTempC = "";
String prevTempOil = "";
String prevBoost = "";
String prevVoltage = "";

//refresh rate
constexpr unsigned long TEMP_UPDATE_INTERVAL = 10000;
constexpr const unsigned long BOOST_UPDATE_INTERVAL = 1000;
constexpr const unsigned long VOLTAGE_UPDATE_INTERVAL = 2000;

unsigned long lastTempUpdate = 0;
unsigned long lastBoostUpdate = 0;
unsigned long lastVoltageUpdate = 0;

void loop() {
    // ----- ELM327 loop ----- //
    if (SerialBT.connected()) {

            unsigned long currentTime = millis();
            unsigned long currentMillis = millis();

                if (currentTime - lastTempUpdate >= TEMP_UPDATE_INTERVAL) {//Slower refresh rate to optimize performance
                    lastTempUpdate = currentTime;

                    sendOBDCommand("0105");
                    String respCoolant = cleanResp(readELM327());
                    tempC = parseCoolantTemp(respCoolant);
                    //Serial.println("Temp Coolant: ");
                    //Serial.println(tempC);

                    if (tempC < -40 || tempC > 200) {
                        tempC = -1000;// Bad value
                    }

                    if (isBMW) {
                        sendOBDCommand("2C100458"); // BMW OBD2
                    } else {
                        sendOBDCommand("015C");  // Standard OBD2
                    }

                    String respOil = cleanResp(readELM327());
                    tempOil = static_cast<int>(parseOilTemp(respOil));
                    //Serial.println("Temp Oil: ");
                    //Serial.println(tempOil);

                    if (tempOil < -40 || tempOil > 200) {
                        tempOil = -1000;// Bad value
                    }
                }

                if (currentTime - lastVoltageUpdate >= VOLTAGE_UPDATE_INTERVAL) {
                    lastVoltageUpdate = currentTime;
                    sendOBDCommand("0142");
                    String respVoltage = cleanResp(readELM327());
                    voltage = static_cast<int>(parseVoltage(respVoltage));
                    //Serial.println("Voltage: ");
                    //Serial.println(voltage);

                    if (voltage < 0) {
                        voltage = -1;// Bad value
                    }
                }

                if (currentTime - lastBoostUpdate >= BOOST_UPDATE_INTERVAL) {
                    lastBoostUpdate = currentTime;
                    sendOBDCommand("010B");
                    String mapResp = cleanResp(readELM327());
                    sendOBDCommand("0133");
                    String baroResp = cleanResp(readELM327());
                    float map = parseMAP(mapResp);
                    float baro = parseBARO(baroResp);

                    if (map > 0 && baro > 0) {
                        boost = (map - baro) / 100.0f;  //formula
                    }
                    if (boost < 0 || boost > 10.0f) {
                        boost = -1.0f;  // Bad value
                    }
                }

                // ----- TFT display ----- // tft display is 320x240
            if (currentMillis - lastUIUpdate >= UI_UPDATE_INTERVAL) {
                lastUIUpdate = currentMillis;
                const String currentTempC = String(tempC) + " °C";
                const String currentTempOil = String(tempOil) + " °C";
                const String currentBoost = String(boost, 1) + " Bar";
                const String currentVoltage = String(voltage, 1) + " V";

                // refresh when value changed
                if (currentTempC != prevTempC) {
                    tft.fillRect(40, 60, 80, 30, TFT_BLACK);
                    tft.drawCentreString(currentTempC, 80, 60, 2);
                    prevTempC = currentTempC;
                }

                if (currentTempOil != prevTempOil) {
                    tft.fillRect(200, 60, 80, 30, TFT_BLACK);
                    tft.drawCentreString(currentTempOil, 240, 60, 2);
                    prevTempOil = currentTempOil;
                }

                if (currentBoost != prevBoost) {
                    tft.fillRect(40, 180, 80, 30, TFT_BLACK);
                    tft.drawCentreString(currentBoost, 80, 180, 2);
                    prevBoost = currentBoost;
                }

                if (currentVoltage != prevVoltage) {
                    tft.fillRect(200, 180, 80, 30, TFT_BLACK);
                    tft.drawCentreString(currentVoltage, 240, 180, 2);
                    prevVoltage = currentVoltage;
                }
            }
        }

    else {
            connectAndInit(false);
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 50);
            tft.println("No connection");
            tft.setCursor(10, 80);
            tft.println("with ELM327");
            delay(500);
            return;
        }
    delay(10);
    }
