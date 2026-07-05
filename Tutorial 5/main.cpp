#include "mbed.h"
#include "arm_book_lib.h"

#define FILTER_SIZE 100
#define CORE_PERIOD_MS 10

AnalogIn potentiometerLine(A0);
AnalogIn lm35Input(A1);
AnalogIn mq2Input(A2);
PwmOut buzzerPwm(D9);

DigitalOut rowPins[4] = { DigitalOut(D0), DigitalOut(D1), DigitalOut(D2), DigitalOut(D3) };
DigitalIn colPins[4] = { DigitalIn(D4, PullUp), DigitalIn(D5, PullUp), DigitalIn(D6, PullUp), DigitalIn(D7, PullUp) };

UnbufferedSerial pcTerminal(USBTX, USBRX, 115200);

float thermalArray[FILTER_SIZE];
int thermalArrayIdx = 0;

int loggedAlarms[5] = {0, 0, 0, 0, 0};
int totalAlarmsTracked = 0;

char customMatrixLayout[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

char tokenInputBuffer[3] = {'\0', '\0', '\0'};
int tokenBufferLen = 0;
int systemIsIsolated = 0;

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

int main() {
    buzzerPwm.write(0.0);
    
    for (int idx = 0; idx < FILTER_SIZE; idx++) {
        thermalArray[idx] = lm35Input.read();
    }

    int serialPrintTick = 0;
    int systemBlindInterval = 0;

    while (true) {
        float dynamicTemp = calcSystemTemperature();
        float dynamicPot = potentiometerLine.read();
        float guardCeiling = 25.0 + (dynamicPot * 12.0);

        float dynamicGas = mq2Input.read() * 100.0;

        char typedKey = scanKeypadMatrix();

        if (systemBlindInterval > 0) {
            systemBlindInterval--;
        }

        int tempViolation = (dynamicTemp > guardCeiling) ? 1 : 0;

        if (tempViolation && systemBlindInterval == 0) {
            systemIsIsolated = 1;
            loggedAlarms[totalAlarmsTracked % 5] = (int)dynamicTemp;
            totalAlarmsTracked++;
            
            buzzerPwm.period(1.0 / 2000.0);
            buzzerPwm.write(0.5);
            
            const char* outputMsg = ">> [SIREN ACTIVE] Triggered by: [Temperature]\r\n";
            pcTerminal.write(outputMsg, strlen(outputMsg));
            
            const char* lockedPrompt = "Enter 3-Digit Code to Deactivate\r\n";
            pcTerminal.write(lockedPrompt, strlen(lockedPrompt));
            tokenBufferLen = 0;

            while (systemIsIsolated == 1) {
                char secretKey = scanKeypadMatrix();
                if (secretKey != '\0') {
                    if (secretKey == '*') {
                        if (tokenBufferLen == 3 && tokenInputBuffer[0] == '1' && tokenInputBuffer[1] == '2' && tokenInputBuffer[2] == '3') {
                            systemIsIsolated = 0;
                            buzzerPwm.write(0.0);
                            tokenBufferLen = 0;
                            
                            const char* correctionMsg = "Code Correct. Resetting Environmental Status...\r\n";
                            pcTerminal.write(correctionMsg, strlen(correctionMsg));
                            
                            for (int idx = 0; idx < FILTER_SIZE; idx++) {
                                thermalArray[idx] = lm35Input.read();
                            }
                            systemBlindInterval = 300;
                        } else {
                            const char* failMsg = "Wrong Code! Resetting...\r\n";
                            pcTerminal.write(failMsg, strlen(failMsg));
                            tokenBufferLen = 0;
                        }
                    } else if (secretKey != '#' && tokenBufferLen < 3) {
                        tokenInputBuffer[tokenBufferLen] = secretKey;
                        tokenBufferLen++;
                        
                        char keyFeed[35];
                        sprintf(keyFeed, "[KEY INPUT] Character: %c\r\n", secretKey);
                        pcTerminal.write(keyFeed, strlen(keyFeed));
                    }
                }
                delay(CORE_PERIOD_MS);
            }
        } else {
            buzzerPwm.write(0.0);

            if (typedKey == '#') {
                const char* headerStr = "--- Temperature Event Log ---\r\n";
                pcTerminal.write(headerStr, strlen(headerStr));
                for (int idx = 0; idx < 5; idx++) {
                    char historyLine[45];
                    sprintf(historyLine, "Log Entry %d: %d C\r\n", idx + 1, loggedAlarms[idx]);
                    pcTerminal.write(historyLine, strlen(historyLine));
                }
            }

            serialPrintTick++;
            if (serialPrintTick >= 100) {
                int wholeT = (int)dynamicTemp;
                int fractT = (int)((dynamicTemp - wholeT) * 10.0);
                if (fractT < 0) fractT = -fractT;

                int wholeC = (int)guardCeiling;
                int fractC = (int)((guardCeiling - wholeC) * 10.0);
                if (fractC < 0) fractC = -fractC;

                char serialFrame[160];
                sprintf(serialFrame, "System Normal | Temperature: %d.%d C | Gas Level: %d | Guard Limit: %d.%d C\r\n", 
                        wholeT, fractT, (int)dynamicGas, wholeC, fractC);
                
                pcTerminal.write(serialFrame, strlen(serialFrame));
                serialPrintTick = 0;
            }
        }

        delay(CORE_PERIOD_MS);
    }
}
