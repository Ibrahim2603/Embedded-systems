#include "mbed.h"

DigitalIn pinA(D2);
DigitalIn pinB(D3);
DigitalIn pinC(D4);
DigitalIn pinD(D5);

DigitalOut greenGateLed(LED1);
DigitalOut blueLockLed(LED2);
DigitalOut redAlertLed(LED3);

UnbufferedSerial debugUart(USBTX, USBRX, 115200);

int checkSwitchPress() {
    if (pinA.read() == 1) {
        ThisThread::sleep_for(25);
        if (pinA.read() == 1) {
            debugUart.write(">> Keypad Track: Switch A Enabled\r\n", 35);
            while (pinA.read() == 1);
            return 1;
        }
    }
    if (pinB.read() == 1) {
        ThisThread::sleep_for(25);
        if (pinB.read() == 1) {
            debugUart.write(">> Keypad Track: Switch B Enabled\r\n", 35);
            while (pinB.read() == 1);
            return 2;
        }
    }
    if (pinC.read() == 1) {
        ThisThread::sleep_for(25);
        if (pinC.read() == 1) {
            debugUart.write(">> Keypad Track: Switch C Enabled\r\n", 35);
            while (pinC.read() == 1);
            return 3;
        }
    }
    if (pinD.read() == 1) {
        ThisThread::sleep_for(25);
        if (pinD.read() == 1) {
            debugUart.write(">> Keypad Track: Switch D Enabled\r\n", 35);
            while (pinD.read() == 1);
            return 0;
        }
    }
    return -1;
}

int main() {
    pinA.mode(PullDown);
    pinB.mode(PullDown);
    pinC.mode(PullDown);
    pinD.mode(PullDown);

    greenGateLed = 0;
    blueLockLed = 0;
    redAlertLed = 1;

    debugUart.write("[SYSTEM] Barrier Control Terminal Online\r\n", 42);
    debugUart.write("[STATUS] Enter credential sequence...\r\n", 39);

    int badTriesCount = 0;
    int warningFlag = 0;

    int inputSlot[3] = {-1, -1, -1};

    Timer dynamicTimer;
    Timer oscillationTimer;

    while (true) {
        int loadedKeys = 0;

        while (loadedKeys < 3) {
            int pressedKeyId = checkSwitchPress();
            if (pressedKeyId != -1) {
                inputSlot[loadedKeys] = pressedKeyId;
                loadedKeys++;
                
                char statusMsg[45];
                sprintf(statusMsg, "[CAPTURE] Entry point %d set to: %d\r\n", loadedKeys, pressedKeyId);
                debugUart.write(statusMsg, strlen(statusMsg));
            }
            ThisThread::sleep_for(10);
        }

        debugUart.write("[VALIDATION] Checking user credentials...\r\n", 42);

        int codeIsValid = 0;
        if (inputSlot[0] == 1 && inputSlot[1] == 2 && inputSlot[2] == 3) {
            codeIsValid = 1;
        }

        if (codeIsValid == 1) {
            debugUart.write("[ACCESS] Match confirmed. Unlock signal sent.\r\n", 47);
            badTriesCount = 0;
            warningFlag = 0;
            redAlertLed = 0;
            greenGateLed = 1;
            ThisThread::sleep_for(5000);
            greenGateLed = 0;
            redAlertLed = 1;
            debugUart.write("[STATUS] Gate secured. Listening for input...\r\n", 47);
        } else {
            if (warningFlag == 1) {
                badTriesCount = 3;
            } else {
                badTriesCount++;
            }
            warningFlag = 0;

            char reportBuffer[55];
            sprintf(reportBuffer, "[DENIED] Warning! Authentication failed. Total: %d\r\n", badTriesCount);
            debugUart.write(reportBuffer, strlen(reportBuffer));

            if (badTriesCount == 2) {
                debugUart.write("[LOCKOUT] Threshold reached. Initializing 10s timeout.\r\n", 56);
                for (int idx = 0; idx < 20; idx++) {
                    redAlertLed = !redAlertLed;
                    ThisThread::sleep_for(500);
                }
                redAlertLed = 1;
                warningFlag = 1;
                debugUart.write("[STATUS] Timeout cleared. Peripheral inputs unblocked.\r\n", 56);
            } 
            else if (badTriesCount >= 3) {
                debugUart.write("[CRITICAL] Multiple breaches. Executing 2 min lockdown.\r\n", 57);
                blueLockLed = 1;
                warningFlag = 0;

                int overrideProgress = 0;
                int adminInput[3] = {-1, -1, -1};

                dynamicTimer.reset();
                dynamicTimer.start();
                oscillationTimer.reset();
                oscillationTimer.start();

                int trackerSecond = 0;

                while (chrono::duration_cast<chrono::seconds>(dynamicTimer.elapsed_time()).count() < 120) {
                    int elapsedSec = chrono::duration_cast<chrono::seconds>(dynamicTimer.elapsed_time()).count();
                    
                    if (elapsedSec != trackerSecond && elapsedSec % 10 == 0) {
                        char notification[60];
                        sprintf(notification, "[COUNTDOWN] Security isolation remaining: %d s\r\n", 120 - elapsedSec);
                        debugUart.write(notification, strlen(notification));
                        trackerSecond = elapsedSec;
                    }

                    if (chrono::duration_cast<chrono::milliseconds>(oscillationTimer.elapsed_time()).count() >= 500) {
                        redAlertLed = !redAlertLed;
                        oscillationTimer.reset();
                    }

                    int immediateKey = -1;
                    if (pinA.read() == 1) immediateKey = 1;
                    else if (pinB.read() == 1) immediateKey = 2;
                    else if (pinC.read() == 1) immediateKey = 3;
                    else if (pinD.read() == 1) immediateKey = 0;

                    if (immediateKey != -1) {
                        ThisThread::sleep_for(25);
                        while (pinA.read() == 1 || pinB.read() == 1 || pinC.read() == 1 || pinD.read() == 1);

                        debugUart.write("[OVERRIDE] Master authorization terminal active...\r\n", 52);
                        adminInput[0] = immediateKey;
                        overrideProgress = 1;

                        char masterEcho[45];
                        sprintf(masterEcho, "[ADMIN KEY] Token slot 1: %d\r\n", immediateKey);
                        debugUart.write(masterEcho, strlen(masterEcho));

                        while (overrideProgress < 3) {
                            int consecutiveKey = checkSwitchPress();
                            if (consecutiveKey != -1) {
                                adminInput[overrideProgress] = consecutiveKey;
                                overrideProgress++;
                                
                                char nestedEcho[45];
                                sprintf(nestedEcho, "[ADMIN KEY] Token slot %d: %d\r\n", overrideProgress, consecutiveKey);
                                debugUart.write(nestedEcho, strlen(nestedEcho));
                            }
                            ThisThread::sleep_for(10);
                        }

                        int adminIsValid = 0;
                        if (adminInput[0] == 0 && adminInput[1] == 0 && adminInput[2] == 0) {
                            adminIsValid = 1;
                        }

                        if (adminIsValid == 1) {
                            debugUart.write("[BYPASS] Master override accepted. Releasing isolation.\r\n", 57);
                            badTriesCount = 0;
                            break;
                        } else {
                            debugUart.write("[REJECTED] Admin validation failed. Token dropped.\r\n", 52);
                            overrideProgress = 0;
                        }
                    }
                    ThisThread::sleep_for(10);
                }

                dynamicTimer.stop();
                oscillationTimer.stop();
                blueLockLed = 0;
                redAlertLed = 1;
                debugUart.write("[STATUS] Isolation interval finished. Core reset.\r\n", 51);
            }
        }
        ThisThread::sleep_for(50);
    }
}
