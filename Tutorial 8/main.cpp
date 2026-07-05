#include "mbed.h"
#include "arm_book_lib.h"
#include "lcd.h"
#include "FATFileSystem.h"
#include "SDBlockDevice.h"
#include <cstdio>

#define FILTER_SIZE 100

AnalogIn potentiometerLine(A0);
AnalogIn lm35Input(A1);
AnalogIn mq2Input(A2);
PwmOut buzzerPwm(D9);

DigitalOut rowPins[4] = { DigitalOut(D0), DigitalOut(D1), DigitalOut(D2), DigitalOut(D3) };
DigitalIn colPins[4] = { DigitalIn(D4, PullUp), DigitalIn(D5, PullUp), DigitalIn(D6, PullUp), DigitalIn(D7, PullUp) };

SDBlockDevice sdStorage(D11, D12, D13, D10);
FATFileSystem diskFs("sd");

UnbufferedSerial pcTerminal(USBTX, USBRX, 115200);

float thermalArray[FILTER_SIZE];
int thermalArrayIdx = 0;

char customMatrixLayout[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

char currentLogMode = 'A'; 
int isSirenSounding = 0;

float calcSystemTemperature() {
    thermalArray[thermalArrayIdx] = lm35Input.read();
    thermalArrayIdx++;
    if (thermalArrayIdx >= FILTER_SIZE) {
        thermalArrayIdx = 0;
    }
    float aggregateSum = 0.0;
    for (int idx = 0; idx < FILTER_SIZE; idx++) {
        aggregateSum += thermalArray[idx];
    }
    float rawVoltage = (aggregateSum / FILTER_SIZE) * 3.3;
    return (rawVoltage * 100.0);
}

char scanKeypadMatrix() {
    for (int r = 0; r < 4; r++) {
        for (int i = 0; i < 4; i++) {
            rowPins[i] = 1;
        }
        rowPins[r] = 0;
        
        for (int c = 0; c < 4; c++) {
            if (colPins[c].read() == 0) {
                delay(40);
                if (colPins[c].read() == 0) {
                    while (colPins[c].read() == 0);
                    return customMatrixLayout[r][c];
                }
            }
        }
    }
    return '\0';
}

void writeSdLog(const char* identity, int reading) {
    sdStorage.init();
    diskFs.mount(&sdStorage);
    FILE* dataFile = fopen("/sd/log_data.txt", "a");
    if (dataFile != NULL) {
        fprintf(dataFile, "Alert: %s reached %d\n", identity, reading);
        fclose(dataFile);
    }
    diskFs.unmount();
    sdStorage.deinit();
}

void readSdLog() {
    sdStorage.init();
    diskFs.mount(&sdStorage);
    FILE* dataFile = fopen("/sd/log_data.txt", "r");
    if (dataFile != NULL) {
        pcTerminal.write("====== Pulling Event Records From SD Card ======\r\n", 50);
        char singleChar;
        while ((singleChar = fgetc(dataFile)) != EOF) {
            if (singleChar == '\n') {
                pcTerminal.write("\r\n", 2);
            } else {
                pcTerminal.write(&singleChar, 1);
            }
        }
        pcTerminal.write("================ End of File ================\r\n", 47);
        fclose(dataFile);
    } else {
        pcTerminal.write(">> [SD CARD] No Storage File Found or Empty.\r\n", 46);
    }
    diskFs.unmount();
    sdStorage.deinit();
}

void showDefaultPrompt() {
    lcd_clear();
    if (currentLogMode == 'A') {
        lcd_print("Mode: Temp Watch");
        lcd_set(0, 1);
        lcd_print("A:Temp  B:Gas");
    } else {
        lcd_print("Mode: Gas Watch");
        lcd_set(0, 1);
        lcd_print("A:Temp  B:Gas");
    }
}

int main() {
    buzzerPwm.write(0.0);
    
    for (int idx = 0; idx < FILTER_SIZE; idx++) {
        thermalArray[idx] = lm35Input.read();
    }

    lcd_init();
    showDefaultPrompt();

    int serialPrintTick = 0;
    int systemBlindInterval = 0;

    while (true) {
        float dynamicTemp = calcSystemTemperature();
        float dynamicPot = potentiometerLine.read();
        float guardCeiling = dynamicPot * 60.0;

        float dynamicGas = mq2Input.read() * 100.0;

        char typedKey = scanKeypadMatrix();

        if (systemBlindInterval > 0) {
            systemBlindInterval--;
            if (systemBlindInterval == 0 && isSirenSounding == 0) {
                showDefaultPrompt();
            }
        }

        if (typedKey == 'A' || typedKey == 'B') {
            if (isSirenSounding == 0) {
                currentLogMode = typedKey;
                char modeMsg[50];
                sprintf(modeMsg, ">> [SYSTEM] Log Scope Shifted: %s\r\n", (currentLogMode == 'A') ? "TEMPERATURE" : "GAS");
                pcTerminal.write(modeMsg, strlen(modeMsg));
                showDefaultPrompt();
            }
        }

        if (typedKey == '*') {
            if (isSirenSounding == 0) {
                readSdLog();
            }
        }

        int tempViolation = (dynamicTemp > guardCeiling) ? 1 : 0;
        int gasViolation = (dynamicGas > (guardCeiling * 1.5)) ? 1 : 0;

        int shouldAlarmTrigger = 0;
        if (currentLogMode == 'A' && tempViolation) shouldAlarmTrigger = 1;
        if (currentLogMode == 'B' && gasViolation) shouldAlarmTrigger = 1;

        if (shouldAlarmTrigger && isSirenSounding == 0 && systemBlindInterval == 0) {
            isSirenSounding = 1;
            buzzerPwm.period(1.0 / 2000.0);
            buzzerPwm.write(0.5);
            
            lcd_clear();
            lcd_print("!! WARNING !!");
            lcd_set(0, 1);
            
            if (currentLogMode == 'A') {
                lcd_print("Temp Threshold");
                char logConfirm[65];
                sprintf(logConfirm, ">> [ALERT LOGGED] Temp = %d C\r\n", (int)dynamicTemp);
                pcTerminal.write(logConfirm, strlen(logConfirm));
                writeSdLog("Temperature", (int)dynamicTemp);
            } else {
                lcd_print("Gas Leakage");
                char logConfirm[65];
                sprintf(logConfirm, ">> [ALERT LOGGED] Gas = %d ppm\r\n", (int)dynamicGas);
                pcTerminal.write(logConfirm, strlen(logConfirm));
                writeSdLog("Gas", (int)dynamicGas);
            }
        }

        if (isSirenSounding == 1) {
            if (typedKey == '#') {
                isSirenSounding = 0;
                buzzerPwm.write(0.0);
                
                lcd_clear();
                lcd_print("Alert Dismissed");
                lcd_set(0, 1);
                lcd_print("Resuming Monitor");
                
                for (int idx = 0; idx < FILTER_SIZE; idx++) {
                    thermalArray[idx] = lm35Input.read();
                }
                systemBlindInterval = 300;
            }
        }

        serialPrintTick++;
        if (serialPrintTick >= 100) {
            if (isSirenSounding == 0) {
                int wholeT = (int)dynamicTemp;
                int fractT = (int)((dynamicTemp - wholeT) * 10.0);
                if (fractT < 0) fractT = -fractT;

                int wholeC = (int)guardCeiling;
                int fractC = (int)((guardCeiling - wholeC) * 10.0);
                if (fractC < 0) fractC = -fractC;

                char serialFrame[180];
                sprintf(serialFrame, "Guarding | Track: %c | Temperature: %d.%d C | Gas: %d | Guard Limit: %d.%d C\r\n", 
                        currentLogMode, wholeT, fractT, (int)dynamicGas, wholeC, fractC);
                pcTerminal.write(serialFrame, strlen(serialFrame));
            }
            serialPrintTick = 0;
        }

        delay(CORE_PERIOD_MS);
    }
}
