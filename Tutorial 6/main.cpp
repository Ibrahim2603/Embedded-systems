#include "mbed.h"
#include "arm_book_lib.h"
#include "lcd.h"

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

void showDefaultPrompt() {
    lcd_clear();
    lcd_print("Enter Code to");
    lcd_set(0, 1);
    lcd_print("Deactivate Alarm");
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
            if (systemBlindInterval == 0 && systemIsIsolated == 0) {
                showDefaultPrompt();
            }
        }

        int tempViolation = (dynamicTemp > guardCeiling) ? 1 : 0;
        int gasViolation = (dynamicGas > (guardCeiling * 1.5)) ? 1 : 0;

        if ((tempViolation || gasViolation) && systemIsIsolated == 0 && systemBlindInterval == 0) {
            systemIsIsolated = 1;
            tokenBufferLen = 0;
            
            buzzerPwm.period(1.0 / 2000.0);
            buzzerPwm.write(0.5);
            
            lcd_clear();
            lcd_print("!! WARNING !!");
            lcd_set(0, 1);
            if (tempViolation && gasViolation) lcd_print("Temp & Gas Leak");
            else if (tempViolation) lcd_print("Over Temperature");
            else lcd_print("Gas Threat");

            const char* alertMsg = ">> [SIREN ACTIVE] Triggered by: [Environmental Fault]\r\n";
            pcTerminal.write(alertMsg, strlen(alertMsg));
            const char* promptMsg = "Enter 3-Digit Code to Deactivate\r\n";
            pcTerminal.write(promptMsg, strlen(promptMsg));
        }

        if (systemIsIsolated == 1) {
            if (typedKey != '\0') {
                if (typedKey == '*') {
                    if (tokenBufferLen == 3 && tokenInputBuffer[0] == '1' && tokenInputBuffer[1] == '2' && tokenInputBuffer[2] == '3') {
                        systemIsIsolated = 0;
                        buzzerPwm.write(0.0);
                        tokenBufferLen = 0;
                        
                        lcd_clear();
                        lcd_print("System Cleared");
                        lcd_set(0, 1);
                        lcd_print("Alarm Silenced");
                        
                        const char* successMsg = "Code Correct. Resetting Environmental Status...\r\n";
                        pcTerminal.write(successMsg, strlen(successMsg));
                        
                        for (int idx = 0; idx < FILTER_SIZE; idx++) {
                            thermalArray[idx] = lm35Input.read();
                        }
                        systemBlindInterval = 300;
                    } else {
                        tokenBufferLen = 0;
                        lcd_clear();
                        lcd_print("Invalid Token");
                        lcd_set(0, 1);
                        lcd_print("Try Again...");
                        
                        const char* failMsg = "Wrong Code! Resetting Buffer...\r\n";
                        pcTerminal.write(failMsg, strlen(failMsg));
                        
                        delay(1500);
                        
                        lcd_clear();
                        lcd_print("!! WARNING !!");
                        lcd_set(0, 1);
                        if (calcSystemTemperature() > guardCeiling) lcd_print("Over Temperature");
                        else lcd_print("Gas Threat");
                    }
                } else if (typedKey != '#' && tokenBufferLen < 3) {
                    tokenInputBuffer[tokenBufferLen] = typedKey;
                    tokenBufferLen++;
                    
                    char keyEcho[35];
                    sprintf(keyEcho, "[KEY INPUT] Character: %c\r\n", typedKey);
                    pcTerminal.write(keyEcho, strlen(keyEcho));
                }
            }
        } else {
            if (typedKey == '2') {
                lcd_clear();
                lcd_print("Gas Status:");
                lcd_set(0, 1);
                char gasStr[16];
                sprintf(gasStr, "Level: %d", (int)dynamicGas);
                lcd_print(gasStr);
                systemBlindInterval = 300;
            } 
            else if (typedKey == '3') {
                lcd_clear();
                lcd_print("Temp Status:");
                lcd_set(0, 1);
                char tempStr[16];
                sprintf(tempStr, "Value: %d C", (int)dynamicTemp);
                lcd_print(tempStr);
                systemBlindInterval = 300;
            }
        }

        serialPrintTick++;
        if (serialPrintTick >= 100) {
            if (systemIsIsolated == 0) {
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
            }
            serialPrintTick = 0;
        }

        delay(CORE_PERIOD_MS);
    }
}
