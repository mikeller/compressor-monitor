#include <stdio.h>

//#include <WiFi.h>

#include <TFT_eSPI.h>
#include <Button2.h>
#include <esp_adc_cal.h>

#define PRESSURE_APPROACHING_THRESHOLD_BAR 10
#define PRESSURE_STOP_THRESHOLD_BAR 10

#define OVERRIDE_DURATION_S 60

#define DEFAULT_PRESSURE_LIMIT_BAR 200

//#define DEFAULT_VREF 1100 // Use adc2_vref_to_gpio() to obtain a better estimate
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

#define BUZZER_PIN 18

#define RELAIS_1_PIN 19
#define RELAIS_2_PIN 21

#define BUTTON_UP_PIN 3
#define BUTTON_DOWN_PIN 22
#define BUTTON_CYCLE_PIN 1

#define LED_1_PIN 2

#define LOOP_FREQUENCY_HZ 100

typedef enum {
    IGNITION_STATE_OFF = 0,
    IGNITION_STATE_ON,
    IGNITION_STATE_CONFIRM,
    IGNITION_STATE_COUNT
} ignitionState_t;

static const char *ignitionStates[] = { "OFF", "ON", "CONFIRM" };

typedef enum {
    PRESSURE_STATE_FILLING = 0,
    PRESSURE_STATE_APPROACHING,
    PRESSURE_STATE_OVER,
    PRESSURE_STATE_SAFETY_STOPPED,
    PRESSURE_STATE_COUNT
} pressureState_t;

static const char *pressureStates[] = { "FILLING", "APPROACHING", "OVER", "STOPPED" };

typedef enum {
    BATTERY_STATE_OK = 0,
    BATTERY_STATE_LOW,
    BATTERY_STATE_COUNT
} batteryState_t;

typedef enum {
    INPUT_STATE_PRESSURE_LIMIT = 0,
    INPUT_STATE_IGNITION,
    INPUT_STATE_OVERRIDE,
    INPUT_STATE_COUNT
} inputState_t;

static const char *inputStates[] = { "LIMIT", "IGNITION", "OVERRIDE" };

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
} globalState_t;

globalState_t state;

TFT_eSPI tft = TFT_eSPI(240, 320);

Button2 buttonUp(BUTTON_UP_PIN);
Button2 buttonDown(BUTTON_DOWN_PIN);
Button2 buttonCycle(BUTTON_CYCLE_PIN);

esp_adc_cal_characteristics_t adc_chars;

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(uint32_t us)
{   
    esp_sleep_enable_timer_wakeup(us);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

void handleUpButton(Button2 &b)
{
    switch (state.inputState) {
    case INPUT_STATE_PRESSURE_LIMIT:
        if (state.pressureLimitBar < 300) {
            state.pressureLimitBar++;
        }

        break;
    case INPUT_STATE_IGNITION:
        if (state.ignitionState == IGNITION_STATE_OFF) {
            state.ignitionState = IGNITION_STATE_ON;
        } else if (state.ignitionState == IGNITION_STATE_CONFIRM) {
            state.ignitionState = IGNITION_STATE_ON;
        }

        break;
    case INPUT_STATE_OVERRIDE:
        state.overrideCountdownStartedMs = millis();

        break;
    default:

        break;
    }
}

void handleDownButton(Button2 &b)
{
    switch (state.inputState) {
    case INPUT_STATE_PRESSURE_LIMIT:
        if (state.pressureLimitBar > 0) {
            state.pressureLimitBar--;
        }

        break;
    case INPUT_STATE_IGNITION:
        if (state.ignitionState == IGNITION_STATE_ON) {
            state.ignitionState = IGNITION_STATE_CONFIRM;
        } else if (state.ignitionState == IGNITION_STATE_CONFIRM) {
            state.ignitionState = IGNITION_STATE_OFF;
        }

        break;
    case INPUT_STATE_OVERRIDE:

        break;
    default:
        state.overrideCountdownStartedMs = millis();

        break;
    }
}

void handleCycleButton(Button2 &b)
{
    state.inputState = (inputState_t)((state.inputState + 1) % INPUT_STATE_COUNT);
}

void buttonInit(void)
{
    buttonUp.setPressedHandler(handleUpButton);
    buttonDown.setPressedHandler(handleDownButton);
    buttonCycle.setPressedHandler(handleCycleButton);
}

void setup(void)
{
    Serial.begin(115200);
    Serial.println("Start");

    state.pressureLimitBar = DEFAULT_PRESSURE_LIMIT_BAR;
    
    tft.init();
    tft.setRotation(1);
    tft.setTextDatum(MC_DATUM);
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(4);

    pinMode(BUZZER_PIN, OUTPUT);
    
    pinMode(RELAIS_1_PIN, OUTPUT);
    pinMode(RELAIS_2_PIN, OUTPUT);

    digitalWrite(RELAIS_1_PIN, 1);
    digitalWrite(RELAIS_2_PIN, 1);
 
    pinMode(LED_1_PIN, OUTPUT);

    buttonInit();

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

//    esp_err_t status = adc2_vref_to_gpio(GPIO_NUM_25);
//    if (status == ESP_OK) {
//        Serial.println("v_ref routed to GPIO");
//    } else {
//        Serial.println("failed to route v_ref");
//    }
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

    uint64_t nowMs = millis();

    if (state.pressureBar < state.pressureLimitBar - PRESSURE_APPROACHING_THRESHOLD_BAR) {
        state.pressureState = PRESSURE_STATE_FILLING;
    } else if (state.pressureBar < state.pressureLimitBar) {
        state.pressureState = PRESSURE_STATE_APPROACHING;
        if (lastPressureState != state.pressureState) {
            state.inputState = INPUT_STATE_OVERRIDE;
        }
    } else if (state.pressureBar < state.pressureLimitBar + PRESSURE_STOP_THRESHOLD_BAR || state.overrideCountdownStartedMs) {
        state.pressureState = PRESSURE_STATE_OVER;
        if (lastPressureState != state.pressureState) {
            state.inputState = INPUT_STATE_OVERRIDE;
        }
    } else {
        state.pressureState = PRESSURE_STATE_SAFETY_STOPPED;
        state.ignitionState = IGNITION_STATE_OFF;
        if (lastPressureState != state.pressureState) {
            state.inputState = INPUT_STATE_IGNITION;
        }
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

    lastPressureState = state.pressureState;
}

void updateOutput(void)
{
    digitalWrite(RELAIS_1_PIN, !state.ignitionState);
}

void updateDisplay(void)
{
    static uint64_t lastRunTimeMs = 0;
    
    uint64_t nowMs = millis();
    if (nowMs - lastRunTimeMs >= 200) {
        lastRunTimeMs = nowMs;

        tft.fillRect(10, 0, 220, 70, TFT_BLACK);
        tft.fillRect(125, 97, 195, 27, TFT_BLACK);
        tft.fillRect(125, 70, 125, 170, TFT_BLACK);

        tft.setTextSize(3);

        tft.setTextColor(TFT_RED);
        tft.setCursor(10, 0);
        tft.printf("%.1f", state.pressureBar);

        tft.setTextSize(1);

        tft.setTextColor(TFT_RED);
        tft.setCursor(10, 70);
        tft.print("Limit:");
        tft.setCursor(125, 70);
        tft.printf("%d bar", state.pressureLimitBar);

        tft.setTextColor(TFT_GREEN);
        tft.setCursor(10, 97);
        tft.print("State:");
        tft.setCursor(125, 97);
        tft.print(pressureStates[state.pressureState]);

        tft.setTextColor(TFT_GREEN);
        tft.setCursor(10, 124);
        tft.print("Ignition:");
        tft.setCursor(125, 124);
        if (state.overrideCountdownStartedMs) {
            tft.printf("%d s", (int)((state.overrideCountdownStartedMs + 1000 * OVERRIDE_DURATION_S - nowMs) / 1000));
        } else {
            tft.printf(ignitionStates[state.ignitionState]);
        }

        tft.setTextColor(TFT_GREEN);
        tft.setCursor(10, 178);
        tft.print("Battery:");
        tft.setCursor(125, 178);
        tft.printf("%.2f V", state.batteryV);

        tft.setTextColor(TFT_GREEN);
        tft.setCursor(10, 151);
        tft.print("Run time:");
        tft.setCursor(125, 151);
        tft.printf("%.2f h", state.runTimeMs / 1000.0 / 3600);

        tft.setTextColor(TFT_GREEN);
        tft.setCursor(10, 207);
        tft.print("Input:");
        tft.setCursor(125, 207);
        tft.print(inputStates[state.inputState]);
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

    updateDisplay();

    updateButtons();
}
