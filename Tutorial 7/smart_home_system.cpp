#include "smart_home_system.h"

DigitalOut relayPin(D8);
InterruptIn killSwitch(D9);

DigitalOut rLine1(D0), rLine2(D1), rLine3(D2), rLine4(D3);
DigitalIn cLine1(D4, PullUp), cLine2(D5, PullUp), cLine3(D6, PullUp), cLine4(D7, PullUp);

int isEngineOn = 0;
int panicActive = 0;

char keypadLayout[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

void triggerEmergencyStop() {
    relayPin = 1;
    isEngineOn = 0;
    panicActive = 1;
}

char readKeypadBuffer() {
    DigitalOut* rowOutputs[4] = {&rLine1, &rLine2, &rLine3, &rLine4};
    DigitalIn* colInputs[4] = {&cLine1, &cLine2, &cLine3, &cLine4};
    
    for (int r = 0; r < 4; r++) {
        rLine1 = rLine2 = rLine3 = rLine4 = 1;
        *rowOutputs[r] = 0;
        
        for (int c = 0; c < 4; c++) {
            if (!(*colInputs[c])) {
                ThisThread::sleep_for(50);
                if (!(*colInputs[c])) {
                    while (!(*colInputs[c]));
                    return keypadLayout[r][c];
                }
            }
        }
    }
    return '\0';
}

void updateLcdScreen() {
    lcd_clear();
    if (panicActive == 1) {
        lcd_print("* SYSTEM HALTED *");
        lcd_set(0, 1);
        lcd_print("Emergency Break");
    } else {
        lcd_print("Drive Mode:");
        lcd_set(0, 1);
        if (isEngineOn == 1) {
            lcd_print("ACTIVE / ON");
        } else {
            lcd_print("DISABLED / OFF");
        }
    }
}

void initDriveSystem() {
    relayPin = 1;
    isEngineOn = 0;
    panicActive = 0;
    
    killSwitch.mode(PullUp);
    killSwitch.fall(&triggerEmergencyStop);
    
    lcd_init();
    updateLcdScreen();
}

void updateDriveSystem() {
    if (panicActive == 1) {
        updateLcdScreen();
        ThisThread::sleep_for(3000);
        panicActive = 0;
        updateLcdScreen();
    }

    char pressedKey = readKeypadBuffer();
    if (pressedKey != '\0') {
        if (pressedKey == '1') {
            relayPin = 0;
            isEngineOn = 1;
            updateLcdScreen();
        }
        else if (pressedKey == '2') {
            relayPin = 1;
            isEngineOn = 0;
            updateLcdScreen();
        }
    }
    ThisThread::sleep_for(20);
}
