#include <stdio.h>

#include <TFT_eSPI.h>

#include <Button2.h>
#include <esp_adc_cal.h>

#include "compressor_monitor_logo.c"

// The configuration lives here

#include "config.h"

#if defined(WIFI_CLIENT_SSID) || defined(WIFI_AP_SSID)
#define USE_WIFI

#include <WiFi.h>
#include <SPIFFS.h>
#include <WebServer.h>
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

#if defined(WIFI_CLIENT_SSID) && !defined(WIFI_CLIENT_PASSWORD)
#define WIFI_CLIENT_PASSWORD NULL
#endif

#if defined(WIFI_AP_SSID)
#if !defined(WIFI_AP_PASSWORD)
#define WIFI_AP_PASSWORD NULL
#endif

#if !defined(WIFI_AP_IP)
#define WIFI_AP_IP 192, 168, 16, 1
#endif
const IPAddress apIp(WIFI_AP_IP);

#if !defined(WIFI_AP_NETMASK)
#define WIFI_AP_NETMASK 255, 255, 255, 0
#endif
#endif

WebServer webServer(80);

#define DATA_UPDATE_FREQUENCY_HZ 5

#define DATA_BUFFER_SIZE 512
DynamicJsonDocument dataJson(DATA_BUFFER_SIZE);
char getDataResponse[DATA_BUFFER_SIZE];

#endif // WIFI_x_SSID

#if !defined(SHOW_IGNITION)
#undef HAS_SWITCH
#endif

#define FREQUENCY_TO_MS(frequencyHz) (1000 / (frequencyHz))

#define LOOP_FREQUENCY_HZ 200
#define DISPLAY_UPDATE_FREQUENCY_HZ 5
#define BEEPER_UPDATE_FREQUENCY_HZ 100

#define STARTUP_DELAY_MS 5000

#define BUTTON_REPEAT_DELAY_MS 500
#define BUTTON_REPEAT_INTERVAL_MS 100

#define BEEPER_SEQUENCE_LENGTH 20

#define ORANGE 0xFBE0

typedef struct beeperSequence_s {
    uint8_t period;
    uint8_t offset;
    bool sequence[BEEPER_SEQUENCE_LENGTH];
} beeperSequence_t;

#if defined(SHOW_IGNITION)
typedef enum {
    IGNITION_STATE_OFF = 0,
    IGNITION_STATE_ON,
    IGNITION_STATE_CONFIRM,
    IGNITION_STATE_COUNT
} ignitionState_t;

const char *ignitionStateNames[] = { "OFF", "ON", "CONFIRM" };

const uint16_t ignitionStateColours[] = { TFT_RED, TFT_GREEN, TFT_YELLOW };
#endif

const beeperSequence_t beeperSequenceIgnitionOff = {
    .period = 15,
    .offset = 0,
    .sequence = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1 },
};

typedef enum {
    PRESSURE_STATE_FILLING = 0,
    PRESSURE_STATE_APPROACHING,
    PRESSURE_STATE_OVER,
#if defined(HAS_SWITCH)
    PRESSURE_STATE_SAFETY_STOPPED,
#endif
    PRESSURE_STATE_COUNT
} pressureState_t;

const char *pressureStateNames[] = {
    "FILL",
    "CLOSE",
    "OVER",
#if defined(HAS_SWITCH)
    "STOP",
#endif
};

const uint16_t pressureStateColours[] = {
    TFT_GREEN,
    TFT_YELLOW,
    ORANGE,
#if defined(HAS_SWITCH)
    TFT_RED,
#endif
};

const beeperSequence_t pressureStateBeeperSequences[PRESSURE_STATE_COUNT] = {
    {
        .period = 1,
        .offset = 0,
        .sequence = { 0 },
    }, {
        .period = 5,
        .offset = 0,
        .sequence = { 1, 1, 1, 1, 1 },
    }, {
        .period = 1,
        .offset = 0,
        .sequence = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
#if defined(HAS_SWITCH)
    }, {
        .period = 3,
        .offset = 0,
        .sequence = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
#endif
    },
};

typedef enum {
    BATTERY_STATE_OK = 0,
    BATTERY_STATE_LOW,
    BATTERY_STATE_COUNT
} batteryState_t;

const uint16_t batteryStateColours[] = { TFT_GREEN, TFT_RED };

const beeperSequence_t beeperSequenceBatteryLow = {
    .period = 15,
    .offset = 5,
    .sequence = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1 },
};

typedef enum {
    INPUT_STATE_PRESSURE_LIMIT = 0,
#if defined(SHOW_IGNITION)
    INPUT_STATE_IGNITION,
#if defined(HAS_SWITCH)
    INPUT_STATE_OVERRIDE,
#endif
#endif
    INPUT_STATE_PURGE,
    INPUT_STATE_COUNT
} inputState_t;

const char *inputStateNames[] = {
    "LIMIT",
#if defined(SHOW_IGNITION)
    "IGNITION",
#if defined(HAS_SWITCH)
    "OVERR",
#endif
#endif
    "PURGE",
};

#if defined(HAS_SWITCH)
const beeperSequence_t beeperSequenceOverrideActive = {
    .period = 1,
    .offset = 0,
    .sequence = { 1, 1, 1, 1, 1 },
};

const beeperSequence_t beeperSequenceOverrideEnding = {
    .period = 1,
    .offset = 0,
    .sequence = { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1 },
};
#endif

const beeperSequence_t beeperSequencePurgeNeeded = {
    .period = 15,
    .offset = 10,
    .sequence = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 },
};

typedef enum {
    SERVER_STATE_WIFI_DISCONNECTED,
    SERVER_STATE_WIFI_CONNECTING,
    SERVER_STATE_WIFI_CONNECTED,
    SERVER_STATE_WIFI_AP_SET_IP,
    SERVER_STATE_STARTING,
    SERVER_STATE_RUNNING,
} serverState_t;

const uint16_t serverStateColours[] = { TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_YELLOW, TFT_YELLOW, TFT_GREEN };

const char *serverStateNames[] = { "DIS", "CONN", "CTD", "IP", "START", "RUN" };

typedef enum {
    SERVER_TYPE_NONE,
    SERVER_TYPE_CLIENT,
    SERVER_TYPE_AP,
} serverType_t;

const char *serverTypeNames[] = { "?", "C", "A" };

typedef struct globalState_s {
    float pressureBar;
    float batteryV;
#if defined(HAS_SWITCH)
    uint64_t overrideCountdownStartedMs;
#endif
    uint8_t pressureLimitBar;
#if defined(SHOW_IGNITION)
    ignitionState_t ignitionState;
#endif
    pressureState_t pressureState;
    batteryState_t batteryState;
    inputState_t inputState;
    uint64_t runTimeMs;
    uint64_t lastPurgeRunTimeMs;
    uint64_t nextButtonRepeatEventMs;
    Button2 *repeatEventButton;
    serverState_t serverState;
    serverType_t serverType;
} globalState_t;

globalState_t state;

const char *webResources[] = {
    "manifest.json",
    "dive_tanks_512x512.png",
    "dive_tanks_192x192.png",
    "stylesheet.css",
    "gauge.min.js",
    "main.js",
    "serviceworker.js",
    "favicon.ico",
};

TFT_eSPI tft = TFT_eSPI(240, 320);

Button2 buttonUp(BUTTON_UP_PIN);
Button2 buttonDown(BUTTON_DOWN_PIN);
Button2 buttonCycle(BUTTON_CYCLE_PIN);

esp_adc_cal_characteristics_t adc_chars;

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
// Unfortunately this does not work with WiFi
/*
void espDelay(uint32_t us)
{
    esp_sleep_enable_timer_wakeup(us);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
    esp_light_sleep_start();
}
*/

void handlePressureLimitChange(Button2 &button)
{
    if (button == buttonUp) {
        if (state.pressureLimitBar < 300) {
            state.pressureLimitBar++;
        }
    } else {
        if (state.pressureLimitBar > 0) {
            state.pressureLimitBar--;
        }
    }
}

void handleUpDownButtonPressed(Button2 &button)
{
    switch (state.inputState) {
    case INPUT_STATE_PRESSURE_LIMIT:
        handlePressureLimitChange(button);

        break;
#if defined(SHOW_IGNITION)
    case INPUT_STATE_IGNITION:
        if (button == buttonUp) {
            state.ignitionState = IGNITION_STATE_ON;
        } else {
            if (state.ignitionState == IGNITION_STATE_ON) {
                state.ignitionState = IGNITION_STATE_CONFIRM;
            } else if (state.ignitionState == IGNITION_STATE_CONFIRM) {
                state.ignitionState = IGNITION_STATE_OFF;
            }
        }

        break;
#if defined(HAS_SWITCH)
    case INPUT_STATE_OVERRIDE:
        if (button == buttonUp) {
            state.overrideCountdownStartedMs = millis();
        } else {
            if (state.pressureState < PRESSURE_STATE_OVER) {
                state.overrideCountdownStartedMs = 0;
            }
        }

        break;
#endif
#endif
    case INPUT_STATE_PURGE:
        if (button == buttonUp) {
            state.lastPurgeRunTimeMs = state.runTimeMs;
        }

        break;
    default:

        break;
    }

    state.nextButtonRepeatEventMs = millis() + BUTTON_REPEAT_DELAY_MS;
    state.repeatEventButton = &button;
}

void handleUpDownButtonReleased(Button2 &button)
{
    state.nextButtonRepeatEventMs = 0;
}

void handleCycleButtonPressed(Button2 &button)
{
    state.inputState = (inputState_t)((state.inputState + 1) % INPUT_STATE_COUNT);
}

void setupButtons(void)
{
    buttonUp.setPressedHandler(handleUpDownButtonPressed);
    buttonUp.setReleasedHandler(handleUpDownButtonReleased);
    buttonDown.setPressedHandler(handleUpDownButtonPressed);
    buttonDown.setReleasedHandler(handleUpDownButtonReleased);
    buttonCycle.setPressedHandler(handleCycleButtonPressed);
}

#if defined(MEASURE_V_REF)
void measureVRef(void)
{
    esp_err_t status = adc2_vref_to_gpio(GPIO_NUM_25);
    if (status == ESP_OK) {
        Serial.println("v_ref routed to GPIO");
    } else {
        Serial.println("failed to route v_ref");
    }
}
#endif

void setupAdc(void)
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        Serial.println("eFuse Two Point: Supported");
    } else {
        Serial.println("eFuse Two Point: NOT supported");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        Serial.println("eFuse Vref: Supported");
    } else {
        Serial.println("eFuse Vref: NOT supported");
    }

    adc1_config_width(ADC_WIDTH_BIT_12);

    adc1_config_channel_atten(SENSOR_ADC, ADC_ATTEN_DB_2_5);
    adc1_config_channel_atten(BATTERY_ADC, ADC_ATTEN_DB_2_5);

    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_2_5, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);

    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
    } else {
        Serial.printf("Default Vref: %u mV\n", DEFAULT_VREF);
    }

#if defined(MEASURE_V_REF)
    measureVRef();

    while (true) {
    }
#endif
}

void setupDisplay(void)
{
    tft.init();
    tft.setRotation(1);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);

    tft.fillScreen(TFT_BLACK);

    // Swap the colour byte order when rendering
    tft.setSwapBytes(true);

    // Draw the logo
    tft.pushImage(50, 10, 220, 220, compressor_monitor_logo);

    delay(STARTUP_DELAY_MS);

    tft.fillScreen(TFT_BLACK);
}

void setup(void)
{
    Serial.begin(115200);
    Serial.println("Start");

    if (!SPIFFS.begin()) {
        Serial.println("An Error has occurred while mounting SPIFFS");
    }

    setupAdc();

    state.pressureLimitBar = DEFAULT_PRESSURE_LIMIT_BAR;

    state.serverState = SERVER_STATE_WIFI_DISCONNECTED;
    state.serverType = SERVER_TYPE_NONE;

    setupDisplay();

    pinMode(BEEPER_PIN, OUTPUT);

    pinMode(LED_1_PIN, OUTPUT);
    digitalWrite(LED_1_PIN, 0);

#if defined(HAS_SWITCH)
    pinMode(RELAIS_1_PIN, OUTPUT);
    pinMode(RELAIS_2_PIN, OUTPUT);

    digitalWrite(RELAIS_1_PIN, 1);
    digitalWrite(RELAIS_2_PIN, 1);
#endif

    setupButtons();
}

int64_t getTimeUntilPurgeMs(void)
{
    return state.lastPurgeRunTimeMs + 1000 * PURGE_INTERVAL_S - state.runTimeMs;
}

void readSensors(const uint64_t currentTimeMs)
{
    static uint32_t pressureSamples[VOLTAGE_SAMPLE_COUNT];
    static uint32_t batterySamples[VOLTAGE_SAMPLE_COUNT];
    static uint32_t pressureSumMv = 0;
    static uint32_t batterySumMv = 0;
    static unsigned sampleIndex = 0;
    static uint64_t lastRunTimeMs = 0;

    if (currentTimeMs - lastRunTimeMs >= 10) {
        lastRunTimeMs = currentTimeMs;

        uint32_t adcReading = esp_adc_cal_raw_to_voltage(adc1_get_raw(SENSOR_ADC), &adc_chars);
        pressureSumMv = pressureSumMv - pressureSamples[sampleIndex] + adcReading;
        pressureSamples[sampleIndex] = adcReading;

        float pressureV = (pressureSumMv / VOLTAGE_SAMPLE_COUNT) * SENSOR_ADC_SCALING;
        state.pressureBar = pressureV / SENSOR_SCALING + SENSOR_OFFSET;

        adcReading = esp_adc_cal_raw_to_voltage(adc1_get_raw(BATTERY_ADC), &adc_chars);
        batterySumMv = batterySumMv - batterySamples[sampleIndex] + adcReading;
        batterySamples[sampleIndex] = adcReading;

        state.batteryV = ((float)batterySumMv / 1000.0 / VOLTAGE_SAMPLE_COUNT) * BATTERY_ADC_SCALING;

        sampleIndex = (sampleIndex + 1) % VOLTAGE_SAMPLE_COUNT;
    }
}

void updateState(const uint64_t currentTimeMs)
{
    static bool lastPurgeNeededState = false;
#if defined(SHOW_IGNITION)
    static uint64_t lastRunTimeUpdateMs = 0;
#if defined(HAS_SWITCH)
    static pressureState_t lastPressureState = PRESSURE_STATE_FILLING;
#endif

    if (state.ignitionState != IGNITION_STATE_OFF) {
        if (!lastRunTimeUpdateMs) {
            lastRunTimeUpdateMs = currentTimeMs;
        } else {
            state.runTimeMs += currentTimeMs - lastRunTimeUpdateMs;
            lastRunTimeUpdateMs = currentTimeMs;
        }
    } else {
        lastRunTimeUpdateMs = 0;
    }
#else
    state.runTimeMs = currentTimeMs - STARTUP_DELAY_MS;
#endif

    int64_t timeUntilPurgeMs = getTimeUntilPurgeMs();
    if (timeUntilPurgeMs <= 0) {
        if (!lastPurgeNeededState) {
            state.inputState = INPUT_STATE_PURGE;
            lastPurgeNeededState = true;
        }
    } else {
        lastPurgeNeededState = false;
    }

#if defined(HAS_SWITCH)
    if (state.pressureBar >= state.pressureLimitBar + PRESSURE_STOP_THRESHOLD_BAR && !state.overrideCountdownStartedMs) {
        state.pressureState = PRESSURE_STATE_SAFETY_STOPPED;
        state.ignitionState = IGNITION_STATE_OFF;
        if (lastPressureState != state.pressureState) {
            state.inputState = INPUT_STATE_IGNITION;
        }
    } else
#endif
    if (state.pressureBar >= state.pressureLimitBar) {
        state.pressureState = PRESSURE_STATE_OVER;
#if defined(HAS_SWITCH)
        if (lastPressureState != state.pressureState) {
            state.inputState = INPUT_STATE_OVERRIDE;
        }
#endif
    } else if (state.pressureBar >= state.pressureLimitBar - PRESSURE_APPROACHING_THRESHOLD_BAR) {
        state.pressureState = PRESSURE_STATE_APPROACHING;
#if defined(HAS_SWITCH)
        if (lastPressureState != state.pressureState) {
            state.inputState = INPUT_STATE_OVERRIDE;
        }
#endif
    } else {
        state.pressureState = PRESSURE_STATE_FILLING;
    }

#if defined(HAS_SWITCH)
    if (state.overrideCountdownStartedMs) {
        if (state.ignitionState == IGNITION_STATE_OFF) {
            state.overrideCountdownStartedMs = 0;
        } else {
            if (currentTimeMs - state.overrideCountdownStartedMs >= OVERRIDE_DURATION_S * 1000) {
                state.overrideCountdownStartedMs = 0;
            }
        }
    }
#endif

    if (state.batteryV <= BATTERY_LOW_LIMIT_V) {
        state.batteryState = BATTERY_STATE_LOW;
    } else {
        state.batteryState = BATTERY_STATE_OK;
    }

#if defined(HAS_SWITCH)
    lastPressureState = state.pressureState;

    if (timeUntilPurgeMs <= -1000 * PURGE_GRACE_TIME_S) {
        state.ignitionState = IGNITION_STATE_OFF;
    }
#endif

    if (state.nextButtonRepeatEventMs && state.nextButtonRepeatEventMs <= currentTimeMs) {
        if (state.inputState == INPUT_STATE_PRESSURE_LIMIT) {
            handlePressureLimitChange(*state.repeatEventButton);
        }

        state.nextButtonRepeatEventMs += BUTTON_REPEAT_INTERVAL_MS;
    }
}

#if defined(HAS_SWITCH)
void updateOutput(void)
{
    digitalWrite(RELAIS_1_PIN, !state.ignitionState);
}
#endif

bool needsBeeperOn(beeperSequence_t sequence, uint32_t period, uint8_t position)
{
    if (period % sequence.period == sequence.offset && sequence.sequence[position]) {
        return true;
    }

    return false;
}

void updateBeeper(const uint64_t currentTimeMs)
{
    static uint64_t lastRunTimeMs = 0;
    static uint32_t sliceCount = 0;

    if (lastRunTimeMs + FREQUENCY_TO_MS(BEEPER_UPDATE_FREQUENCY_HZ) <= currentTimeMs) {
        lastRunTimeMs = currentTimeMs;

        sliceCount++;
        uint8_t position = sliceCount % BEEPER_SEQUENCE_LENGTH;
        uint32_t period = sliceCount / BEEPER_SEQUENCE_LENGTH;

        bool beeperOn = false;

#if defined(HAS_SWITCH)
        if (state.overrideCountdownStartedMs) {
            if (currentTimeMs - state.overrideCountdownStartedMs >= (OVERRIDE_DURATION_S - 10) * 1000) {
                beeperOn = beeperOn || needsBeeperOn(beeperSequenceOverrideEnding, period, position);
            } else {
                beeperOn = beeperOn || needsBeeperOn(beeperSequenceOverrideActive, period, position);
            }
        } else
#endif
        {
#if defined(SHOW_IGNITION)
            if (state.ignitionState == IGNITION_STATE_OFF) {
                beeperOn = beeperOn || needsBeeperOn(beeperSequenceIgnitionOff, period, position);
            }
#endif

            if (state.batteryState == BATTERY_STATE_LOW) {
                beeperOn = beeperOn || needsBeeperOn(beeperSequenceBatteryLow, period, position);
            }

            if (getTimeUntilPurgeMs() <= 0) {
                beeperOn = beeperOn || needsBeeperOn(beeperSequencePurgeNeeded, period, position);
            }

            beeperOn = beeperOn || needsBeeperOn(pressureStateBeeperSequences[state.pressureState], period, position);
        }

        digitalWrite(BEEPER_PIN, beeperOn);
        digitalWrite(LED_1_PIN, beeperOn);
    }
}

void updateDisplay(const uint64_t currentTimeMs)
{

/*
Geometry for 320 x 240 display:

Top row,
0, 0, 319, 239

Left column:
0, 70, 159, 239, in rows of 27 height

Right column:
160, 70, 319, 239, in rows of 27 height

*/

#define HEADER_X 10
#define HEADER_Y 0

#define ROW_1 70
#define ROW_2 97
#define ROW_3 124
#define ROW_4 151
#define ROW_5 178
#define ROW_6 205

#define COL_HEADING_1 10
#define COL_1 38
#define COL_HEADING_2 170
#define COL_2 198

#define IP_SIZE 15

    static uint64_t lastRunTimeMs = 0;

    if (lastRunTimeMs + FREQUENCY_TO_MS(DISPLAY_UPDATE_FREQUENCY_HZ) <= currentTimeMs) {
        lastRunTimeMs = currentTimeMs;

        tft.fillRect(HEADER_X, HEADER_Y, 220, 70, TFT_BLACK);
        tft.fillRect(COL_1, ROW_1, 122, 170, TFT_BLACK);
        tft.fillRect(COL_2, ROW_1, 122, 170, TFT_BLACK);
        tft.fillRect(COL_HEADING_2, ROW_5, 28, 27, TFT_BLACK);

        tft.setTextSize(3);

        tft.setTextColor(pressureStateColours[state.pressureState]);
        tft.setCursor(HEADER_X, HEADER_Y);
        tft.printf("%.1f", state.pressureBar);

        tft.setTextSize(1);

        // Column 1:
        tft.setCursor(COL_HEADING_1, ROW_1);
        tft.print("S:");
        tft.setCursor(COL_1, ROW_1);
        tft.print(pressureStateNames[state.pressureState]);

        tft.setCursor(COL_HEADING_1, ROW_2);
        tft.print("L:");
        tft.setCursor(COL_1, ROW_2);
        tft.printf("%d bar", state.pressureLimitBar);

#if defined(SHOW_IGNITION)
#if defined(HAS_SWITCH)
        if (state.overrideCountdownStartedMs) {
            if (currentTimeMs - state.overrideCountdownStartedMs >= (OVERRIDE_DURATION_S - 10) * 1000) {
                tft.setTextColor(TFT_RED);
            } else {
                tft.setTextColor(TFT_YELLOW);
            }
        } else
#endif
        {
            tft.setTextColor(ignitionStateColours[state.ignitionState]);
        }
        tft.setCursor(COL_HEADING_1, ROW_3);
        tft.print("I:");
        tft.setCursor(COL_1, ROW_3);
#if defined(HAS_SWITCH)
        if (state.overrideCountdownStartedMs) {
            tft.printf("%d s", (int)((state.overrideCountdownStartedMs + 1000 * OVERRIDE_DURATION_S - currentTimeMs) / 1000));
        } else
#endif
        {
            tft.printf(ignitionStateNames[state.ignitionState]);
        }
#endif

        tft.setTextColor(batteryStateColours[state.batteryState]);
        tft.setCursor(COL_HEADING_1, ROW_4);
        tft.print("B:");
        tft.setCursor(COL_1, ROW_4);
        tft.printf("%.2f V", state.batteryV);

#if defined(USE_WIFI)
        tft.setTextColor(serverStateColours[state.serverState]);
        tft.setCursor(COL_HEADING_1, ROW_5);
        tft.print("W:");
        tft.setCursor(COL_1, ROW_5);
        if (state.serverState < SERVER_STATE_WIFI_CONNECTED) {
            tft.printf("%s:%s", serverTypeNames[state.serverType], serverStateNames[state.serverState]);
        } else {
            char ip[IP_SIZE] = "\0";
            switch (state.serverType) {
#if defined(WIFI_CLIENT_SSID)
            case SERVER_TYPE_CLIENT:
                WiFi.localIP().toString().toCharArray(ip, IP_SIZE);

                break;
#endif
#if defined(WIFI_AP_SSID)
            case SERVER_TYPE_AP:
                apIp.toString().toCharArray(ip, IP_SIZE);

                break;
#endif
            default:
                break;
            }
            tft.printf("%s:%s", serverTypeNames[state.serverType], ip);
        }
#endif

        // Column 2:
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(COL_HEADING_2, ROW_1);
        tft.print("T:");
        tft.setCursor(COL_2, ROW_1);
        tft.printf("%.2f h", state.runTimeMs / 1000.0 / 3600);

        int64_t timeUntilPurgeMs = getTimeUntilPurgeMs();
        if (timeUntilPurgeMs <= 0) {
            tft.setTextColor(TFT_RED);
        } else if (timeUntilPurgeMs <= 60000) {
            tft.setTextColor(TFT_YELLOW);
        } else {
            tft.setTextColor(TFT_GREEN);
        }
        tft.setCursor(COL_HEADING_2, ROW_2);
        tft.print("P:");
        tft.setCursor(COL_2, ROW_2);
        tft.printf("%s%d:%02d min", (timeUntilPurgeMs <= 0) ? "-" : "", (int)abs(timeUntilPurgeMs / 60000), (int)abs((timeUntilPurgeMs / 1000) % 60));

        tft.setTextColor(TFT_GREEN);
        tft.setCursor(COL_HEADING_2, ROW_6);
        tft.print("I:");
        tft.setCursor(COL_2, ROW_6);
        tft.print(inputStateNames[state.inputState]);
    }
}

void updateButtons(void)
{
    buttonUp.loop();
    buttonDown.loop();
    buttonCycle.loop();
}

#if defined(USE_WIFI)

void updateDataJson(void)
{
    dataJson["pressureBar"] = state.pressureBar;
    dataJson["pressureLimitBar"] = state.pressureLimitBar;
    dataJson["pressureState"] = pressureStateNames[state.pressureState];
#if defined(SHOW_IGNITION)
    dataJson["ignitionState"] = ignitionStateNames[state.ignitionState];
#else
    dataJson["ignitionState"] = "ON";
#endif
#if defined(HAS_SWITCH)
    dataJson["overrideCountdownDurationMs"] = state.overrideCountdownStartedMs ? millis() - state.overrideCountdownStartedMs : 0;
#else
    dataJson["overrideCountdownDurationMs"] = 0;
#endif
    dataJson["runTimeMs"] = state.runTimeMs;
    dataJson["timeUntilPurgeMs"] = getTimeUntilPurgeMs();
    dataJson["batteryV"] = state.batteryV;
}

void updateDataResponse(void)
{
    String data;
    serializeJsonPretty(dataJson, data);
    data.toCharArray(getDataResponse, DATA_BUFFER_SIZE);
}

bool updateData(const uint64_t currentTimeMs)
{
    static uint64_t lastDataUpdateMs = 0;
    static bool responseNeedsUpdating = false;

    if (responseNeedsUpdating) {
        updateDataResponse();

        lastDataUpdateMs = currentTimeMs;
        responseNeedsUpdating = false;

        return true;
    } else if (lastDataUpdateMs + FREQUENCY_TO_MS(DATA_UPDATE_FREQUENCY_HZ) < currentTimeMs) {
        updateDataJson();

        responseNeedsUpdating = true;

        return true;
    }

    return false;
}

void handleGetData(void)
{
    webServer.send(200, F("application/json"), getDataResponse);
}

void updateWebServer(const uint64_t currentTimeMs)
{
    static uint64_t delayUntilMs = 0;

    if (state.serverType != SERVER_TYPE_AP) {
        int wifiStatus = WiFi.status();
        switch (wifiStatus) {
        case WL_CONNECTED:
            if (state.serverState == SERVER_STATE_WIFI_CONNECTING) {
                state.serverState = SERVER_STATE_WIFI_CONNECTED;
                state.serverType = SERVER_TYPE_CLIENT;
                delayUntilMs = 0;
            }

            break;
        case WL_CONNECT_FAILED:
        case WL_NO_SSID_AVAIL:
            delayUntilMs = 0;

            break;
        default:

            break;
        }
    }

    if (currentTimeMs < delayUntilMs) {
        return;
    }

    switch (state.serverState) {
    case SERVER_STATE_WIFI_DISCONNECTED:
#if defined(WIFI_CLIENT_SSID)
        WiFi.begin((char *)WIFI_CLIENT_SSID, (char *)WIFI_CLIENT_PASSWORD);
        delayUntilMs = currentTimeMs + 60000; // wait for WiFi to connect
        state.serverState = SERVER_STATE_WIFI_CONNECTING;

        break;
    case SERVER_STATE_WIFI_CONNECTING:
#endif
        state.serverState = SERVER_STATE_WIFI_DISCONNECTED;
#if defined(WIFI_AP_SSID)
        if (state.serverType != SERVER_TYPE_CLIENT) {
            WiFi.mode(WIFI_AP);
            WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
            state.serverType = SERVER_TYPE_AP;

            delayUntilMs = currentTimeMs + 1000; // wait for WiFi to start
            state.serverState = SERVER_STATE_WIFI_AP_SET_IP;
        }

        break;
    case SERVER_STATE_WIFI_AP_SET_IP:
        {
            IPAddress netmask(WIFI_AP_NETMASK);
            WiFi.softAPConfig(apIp, apIp, netmask);

            delayUntilMs = currentTimeMs + 1000; // wait for WiFi to start
            state.serverState = SERVER_STATE_WIFI_CONNECTED;
        }
#endif

        break;
    case SERVER_STATE_WIFI_CONNECTED:
        webServer.on(F("/api/getData"), handleGetData);

        webServer.serveStatic("/", SPIFFS, "/web/index.html");

        for (unsigned i = 0; i < sizeof(webResources) / sizeof(char *); i++) {
            char urlBuffer[64] = "/";
            char fileBuffer[64] = "/web/";

            webServer.serveStatic(strcat(urlBuffer, webResources[i]), SPIFFS, strcat(fileBuffer, webResources[i]));
        }

        webServer.begin();
        state.serverState = SERVER_STATE_STARTING;
        delayUntilMs = currentTimeMs + 100;

        break;
    case SERVER_STATE_STARTING:
        state.serverState = SERVER_STATE_RUNNING;

        break;
    default:
        webServer.handleClient();

        break;
    }
}
#endif

void loop(void)
{
    uint64_t loopStartMs = millis();

    readSensors(loopStartMs);

    updateState(loopStartMs);

#if defined(HAS_SWITCH)
    updateOutput();
#endif

    updateBeeper(loopStartMs);

    updateDisplay(loopStartMs);

    updateButtons();

#if defined(USE_WIFI)
    if (!updateData(loopStartMs)) {
        updateWebServer(loopStartMs);
    }
#endif

    uint64_t loopTimeMs =  millis() - loopStartMs;
    if (loopTimeMs < FREQUENCY_TO_MS(LOOP_FREQUENCY_HZ)) {
        delay(FREQUENCY_TO_MS(LOOP_FREQUENCY_HZ) - loopTimeMs);
    }
}
