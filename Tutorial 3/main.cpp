#include "mbed.h"
#include "arm_book_lib.h"

DigitalOut globalAlertLed(LED1);

UnbufferedSerial pcLink(USBTX, USBRX, 115200);

bool gasFlag = OFF;
bool tempFlag = OFF;

void setupSystem();
void manageSerialBuffer();

int main()
{
    setupSystem();
    while (true) {
        manageSerialBuffer();
    }
}

void setupSystem()
{
    globalAlertLed = OFF;
}

void extracted(char &incomingChar) {
  if (pcLink.readable()) {
    pcLink.read(&incomingChar, 1);

    switch (incomingChar) {
    case '1':
      gasFlag = !gasFlag;
      if (gasFlag) {
        const char *msg = ">> Gas Channel: ENFORCED\r\n";
        pcLink.write(msg, strlen(msg));
      } else {
        const char *msg = ">> Gas Channel: RELEASED\r\n";
        pcLink.write(msg, strlen(msg));
      }
      break;

    case '2':
      if (gasFlag) {
        const char *msg = " Gas Status: THREAT DETECTED\r\n";
        pcLink.write(msg, strlen(msg));
      } else {
        const char *msg = " Gas Status: STABLE / CLEAR\r\n";
        pcLink.write(msg, strlen(msg));
      }
      break;

    case '3':
      tempFlag = !tempFlag;
      if (tempFlag) {
        const char *msg = ">> Temp Channel: EXCEEDED\r\n";
        pcLink.write(msg, strlen(msg));
      } else {
        const char *msg = ">> Temp Channel: REGULAR\r\n";
        pcLink.write(msg, strlen(msg));
      }
      break;

    case '4':
      if (tempFlag) {
        const char *msg = " Thermal Status: CRITICAL OVERHEAT\r\n";
        pcLink.write(msg, strlen(msg));
      } else {
        const char *msg = " Thermal Status: SECURE / NORMAL\r\n";
        pcLink.write(msg, strlen(msg));
      }
      break;
    }

    if (gasFlag || tempFlag) {
      globalAlertLed = ON;
    } else {
      globalAlertLed = OFF;
    }
  }
}
void manageSerialBuffer() {
  char incomingChar = '\0';
  extracted(incomingChar);
}
