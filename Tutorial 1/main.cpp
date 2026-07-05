#include "mbed.h"

DigitalOut light1(LED1);
DigitalOut light2(LED2);
DigitalOut light3(LED3);

void executeStartupPulse() {
    int cyclesRemaining = 4;
    while (cyclesRemaining > 0) {
        light1 = 1; light2 = 1; light3 = 1;
        ThisThread::sleep_for(100);
        light1 = 0; light2 = 0; light3 = 0;
        ThisThread::sleep_for(100);
        cyclesRemaining--;
    }
}

int main() {
    executeStartupPulse();

    int loopTicks = 0;
    int totalAFlashes = 0;
    
    int internalCounterB = 0;
    int internalCounterA = 0;

    while (totalAFlashes < 8) {
        light3 = !light3;

        internalCounterB++;
        if (internalCounterB >= 2) {
            light2 = !light2;
            internalCounterB = 0;
        }

        internalCounterA++;
        if (internalCounterA >= 4) {
            light1 = !light1;
            totalAFlashes++;
            internalCounterA = 0;
        }

        loopTicks++;
        ThisThread::sleep_for(250);
    }

    light1 = 1;
    light2 = 0;
    light3 = 0;

    while (true) {
        ThisThread::sleep_for(1000);
    }
}
