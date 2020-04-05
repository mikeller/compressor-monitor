#include <stdio.h>

//#include <WiFi.h>

#include <TFT_eSPI.h>
#include <Button2.h>
#include <esp_adc_cal.h>

#define PRESSURE_APPROACHING_THRESHOLD_BAR 10
#define PRESSURE_STOP_THRESHOLD_BAR 10

#define OVERRIDE_DURATION_S 60

#define DEFAULT_PRESSURE_LIMIT_BAR 200

// Use MEASURE_V_REF to route VRef to GPIO 25 and measure it,
// then define DEFAULT_VREF as the measurement in mV.
//#define MEASURE_V_REF
//#define DEFAULT_VREF 1100
#define DEFAULT_VREF 1112
#define VOLTAGE_SAMPLE_COUNT 100

#define SENSOR_ADC ADC1_CHANNEL_6
// Input scaling of the ADC
// 2k : 1k divider
#define SENSOR_ADC_SCALING 3.0f
// Conversion scaling / offset of the pressure sensor [bar / mV, bar]
// 400 Bar sensor with output range 0.5V ... 4.5V
#define SENSOR_SCALING 10.0f
#define SENSOR_OFFSET (-50.0f)

#define BATTERY_ADC ADC1_CHANNEL_7

// Input scaling of the ADC
// 20k : 1k divider
#define BATTERY_ADC_SCALING 21.0f

#define BATTERY_LOW_LIMIT_V 10.5

#define BEEPER_PIN 18

#define RELAIS_1_PIN 19
#define RELAIS_2_PIN 21

#define BUTTON_UP_PIN 3
#define BUTTON_DOWN_PIN 22
#define BUTTON_CYCLE_PIN 1

#define LED_1_PIN 2

#define LOOP_FREQUENCY_HZ 100

#define BUTTON_REPEAT_DELAY_MS 500
#define BUTTON_REPEAT_INTERVAL_MS 100

#define BEEPER_SEQUENCE_LENGTH 20
#define BEEPER_MIN_DURATION_MS 10

#define ORANGE 0xFBE0

typedef struct beeperSequence_s {
    uint8_t period;
    uint8_t offset;
    bool sequence[BEEPER_SEQUENCE_LENGTH];
} beeperSequence_t;

typedef enum {
    IGNITION_STATE_OFF = 0,
    IGNITION_STATE_ON,
    IGNITION_STATE_CONFIRM,
    IGNITION_STATE_COUNT
} ignitionState_t;

static const char *ignitionStateNames[] = { "OFF", "ON", "CONFIRM" };

static const uint16_t ignitionStateColours[] = { TFT_RED, TFT_GREEN, TFT_YELLOW };

static const beeperSequence_t beeperSequenceIgnitionOff = {
    .period = 10,
    .offset = 0,
    .sequence = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1 },
};

typedef enum {
    PRESSURE_STATE_FILLING = 0,
    PRESSURE_STATE_APPROACHING,
    PRESSURE_STATE_OVER,
    PRESSURE_STATE_SAFETY_STOPPED,
    PRESSURE_STATE_COUNT
} pressureState_t;

static const char *pressureStateNames[] = { "FILL", "CLOSE", "OVER", "STOP" };

static const uint16_t pressureStateColours[] = { TFT_GREEN, TFT_YELLOW, ORANGE, TFT_RED };

static const beeperSequence_t pressureStateBeeperSequences[PRESSURE_STATE_COUNT] = {
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
    }, {
        .period = 2,
        .offset = 0,
        .sequence = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    },
};

typedef enum {
    BATTERY_STATE_OK = 0,
    BATTERY_STATE_LOW,
    BATTERY_STATE_COUNT
} batteryState_t;

static const uint16_t batteryStateColours[] = { TFT_GREEN, TFT_RED };

static const beeperSequence_t beeperSequenceBatteryLow = {
    .period = 10,
    .offset = 5,
    .sequence = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1 },
};

typedef enum {
    INPUT_STATE_PRESSURE_LIMIT = 0,
    INPUT_STATE_IGNITION,
    INPUT_STATE_OVERRIDE,
    INPUT_STATE_COUNT
} inputState_t;

static const char *inputStateNames[] = { "LIMIT", "IGNITION", "OVERRIDE" };

static const beeperSequence_t beeperSequenceOverrideActive = {
    .period = 1,
    .offset = 0,
    .sequence = { 1, 1, 1, 1, 1 },
};

static const beeperSequence_t beeperSequenceOverrideEnding = {
    .period = 1,
    .offset = 0,
    .sequence = { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1 },
};

typedef struct globalState_s {
    float pressureBar;
    float batteryV;
    uint64_t overrideCountdownStartedMs;
    uint8_t pressureLimitBar;
    bool ledOn;
    ignitionState_t ignitionState;
    pressureState_t pressureState;
    batteryState_t batteryState;
    inputState_t inputState;
    uint64_t runTimeMs;
    uint64_t nextButtonRepeatEventMs;
    Button2 *repeatEventButton;
} globalState_t;

globalState_t state;

TFT_eSPI tft = TFT_eSPI(240, 320);

Button2 buttonUp(BUTTON_UP_PIN);
Button2 buttonDown(BUTTON_DOWN_PIN);
Button2 buttonCycle(BUTTON_CYCLE_PIN);

esp_adc_cal_characteristics_t adc_chars;

/*
//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(uint32_t us)
{   
    esp_sleep_enable_timer_wakeup(us);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
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
    case INPUT_STATE_OVERRIDE:
        if (button == buttonUp) {
            state.overrideCountdownStartedMs = millis();
        } else {
            if (state.pressureState < PRESSURE_STATE_OVER) {
                state.overrideCountdownStartedMs = 0;
            }
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

void buttonInit(void)
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

void setup(void)
{
    Serial.begin(115200);
    Serial.println("Start");

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

    state.pressureLimitBar = DEFAULT_PRESSURE_LIMIT_BAR;

    tft.init();
    tft.setRotation(1);
    tft.setTextDatum(MC_DATUM);
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(4);

    pinMode(BEEPER_PIN, OUTPUT);
    
    pinMode(RELAIS_1_PIN, OUTPUT);
    pinMode(RELAIS_2_PIN, OUTPUT);

    digitalWrite(RELAIS_1_PIN, 1);
    digitalWrite(RELAIS_2_PIN, 1);

    pinMode(LED_1_PIN, OUTPUT);

    buttonInit();
}

void readSensors(void)
{
    static uint32_t pressureSamples[VOLTAGE_SAMPLE_COUNT];
    static uint32_t batterySamples[VOLTAGE_SAMPLE_COUNT];
    static uint32_t pressureSumMv = 0;
    static uint32_t batterySumMv = 0;
    static unsigned sampleIndex = 0;
    static uint64_t lastRunTimeMs = 0;
    
    uint64_t nowMs = millis();
    if (nowMs - lastRunTimeMs >= 10) {
        lastRunTimeMs = nowMs;

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

void updateState(void)
{
    static pressureState_t lastPressureState;
    static uint64_t lastRunTimeUpdateMs;

    uint64_t nowMs = millis();

    if (state.pressureBar >= state.pressureLimitBar + PRESSURE_STOP_THRESHOLD_BAR && !state.overrideCountdownStartedMs) {
        state.pressureState = PRESSURE_STATE_SAFETY_STOPPED;
        state.ignitionState = IGNITION_STATE_OFF;
        if (lastPressureState != state.pressureState) {
            state.inputState = INPUT_STATE_IGNITION;
        }
    } else if (state.pressureBar >= state.pressureLimitBar) {
        state.pressureState = PRESSURE_STATE_OVER;
        if (lastPressureState != state.pressureState) {
            state.inputState = INPUT_STATE_OVERRIDE;
        }
    } else if (state.pressureBar >= state.pressureLimitBar - PRESSURE_APPROACHING_THRESHOLD_BAR) {
        state.pressureState = PRESSURE_STATE_APPROACHING;
        if (lastPressureState != state.pressureState) {
            state.inputState = INPUT_STATE_OVERRIDE;
        }
    } else {
        state.pressureState = PRESSURE_STATE_FILLING;
    }

    if (state.overrideCountdownStartedMs) {
        if (state.ignitionState == IGNITION_STATE_OFF) {
            state.overrideCountdownStartedMs = 0;
        } else {
            if (nowMs - state.overrideCountdownStartedMs >= OVERRIDE_DURATION_S * 1000) {
                state.overrideCountdownStartedMs = 0;
            }
        }
    }

    if (state.batteryV <= BATTERY_LOW_LIMIT_V) {
        state.batteryState = BATTERY_STATE_LOW;
    } else {
        state.batteryState = BATTERY_STATE_OK;
    }

    lastPressureState = state.pressureState;

    if (state.nextButtonRepeatEventMs && state.nextButtonRepeatEventMs <= nowMs) {
        if (state.inputState == INPUT_STATE_PRESSURE_LIMIT) {
            handlePressureLimitChange(*state.repeatEventButton);
        }

        state.nextButtonRepeatEventMs += BUTTON_REPEAT_INTERVAL_MS;
    }

    if (state.ignitionState == IGNITION_STATE_ON) {
        if (!lastRunTimeUpdateMs) {
            lastRunTimeUpdateMs = nowMs;
        } else {
            state.runTimeMs += nowMs - lastRunTimeUpdateMs;
            lastRunTimeUpdateMs = nowMs;
        }
    } else {
        lastRunTimeUpdateMs = 0;
    }
}

void updateOutput(void)
{
    digitalWrite(RELAIS_1_PIN, !state.ignitionState);
}

bool needsBeeperOn(beeperSequence_t sequence, uint32_t period, uint8_t position)
{
    if (period % sequence.period == sequence.offset && sequence.sequence[position]) {
        return true;
    }

    return false;
}

void updateBeeper(void)
{
    static uint64_t lastRunTimeMs = 0;
    static uint32_t sliceCount = 0;

    uint64_t nowMs = millis();
    if (nowMs - lastRunTimeMs >= BEEPER_MIN_DURATION_MS) {
        lastRunTimeMs = nowMs;

        sliceCount++;
        uint8_t position = sliceCount % BEEPER_SEQUENCE_LENGTH;
        uint32_t period = sliceCount / BEEPER_SEQUENCE_LENGTH;

        bool beeperOn = false;

        if (!state.overrideCountdownStartedMs) {
            if (state.ignitionState == IGNITION_STATE_OFF) {
                beeperOn = beeperOn || needsBeeperOn(beeperSequenceIgnitionOff, period, position);
            }

            if (state.batteryState == BATTERY_STATE_LOW) {
                beeperOn = beeperOn || needsBeeperOn(beeperSequenceBatteryLow, period, position);
            }

            beeperOn = beeperOn || needsBeeperOn(pressureStateBeeperSequences[state.pressureState], period, position);
        } else {
            if (nowMs - state.overrideCountdownStartedMs >= (OVERRIDE_DURATION_S - 10) * 1000) {
                beeperOn = beeperOn || needsBeeperOn(beeperSequenceOverrideEnding, period, position);
            } else {
                beeperOn = beeperOn || needsBeeperOn(beeperSequenceOverrideActive, period, position);
            }
        }

        digitalWrite(BEEPER_PIN, beeperOn);
    }
}

void updateDisplay(void)
{

#define ROW_1 70
#define ROW_2 97
#define ROW_3 124
#define ROW_4 151
#define ROW_5 178
#define ROW_6 205

#define COL_HEADING_1 10
#define COL_1 35
#define COL_HEADING_2 170
#define COL_2 195

    static uint64_t lastRunTimeMs = 0;
    
    uint64_t nowMs = millis();
    if (nowMs - lastRunTimeMs >= 200) {
        lastRunTimeMs = nowMs;

        tft.fillRect(10, 0, 220, 70, TFT_BLACK);
        tft.fillRect(35, 70, 125, 170, TFT_BLACK);
        tft.fillRect(195, 70, 125, 170, TFT_BLACK);

        tft.setTextSize(3);

        tft.setTextColor(pressureStateColours[state.pressureState]);
        tft.setCursor(10, 0);
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

        if (state.overrideCountdownStartedMs) {
            if (nowMs - state.overrideCountdownStartedMs >= (OVERRIDE_DURATION_S - 10) * 1000) {
                tft.setTextColor(TFT_RED);
            } else {
                tft.setTextColor(TFT_YELLOW);
            }
        } else {
            tft.setTextColor(ignitionStateColours[state.ignitionState]);
        }
        tft.setCursor(COL_HEADING_1, ROW_3);
        tft.print("I:");
        tft.setCursor(COL_1, ROW_3);
        if (state.overrideCountdownStartedMs) {
            tft.printf("%d s", (int)((state.overrideCountdownStartedMs + 1000 * OVERRIDE_DURATION_S - nowMs) / 1000));
        } else {
            tft.printf(ignitionStateNames[state.ignitionState]);
        }

        tft.setTextColor(batteryStateColours[state.batteryState]);
        tft.setCursor(COL_HEADING_1, ROW_5);
        tft.print("B:");
        tft.setCursor(COL_1, ROW_5);
        tft.printf("%.2f V", state.batteryV);

        // Column 2:
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(COL_HEADING_2, ROW_1);
        tft.print("T:");
        tft.setCursor(COL_2, ROW_1);
        tft.printf("%.2f h", state.runTimeMs / 1000.0 / 3600);

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

/*void wifi_scan(void)
{
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);

    tft.drawString("Scan Network", tft.width() / 2, tft.height() / 2);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int16_t n = WiFi.scanNetworks();
    tft.fillScreen(TFT_BLACK);
    if (n == 0) {
        tft.drawString("no networks found", tft.width() / 2, tft.height() / 2);
    } else {
        tft.setCursor(0, 0);
        Serial.printf("Found %d net\n", n);
        for (int i = 0; i < n; ++i) {
            char buff[512];
            sprintf(buff,
                    "[%d]:%s(%d)",
                    i + 1,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i));
            tft.println(buff);
        }
    }
    WiFi.mode(WIFI_OFF);
}*/

void loop(void)
{
    readSensors();

    updateState();

    updateOutput();

    updateBeeper();

    updateDisplay();

    updateButtons();
}
