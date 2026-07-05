#include "mbed.h"
#include "UnbufferedSerial.h"

// Lab 4 - Sensor Alert System
// Ibrahim Huthaifa

UnbufferedSerial uart(USBTX, USBRX, 115200);

AnalogIn temperature_sensor(A0);
AnalogIn gas_sensor(A1);
AnalogIn potentiometer(A2);

DigitalOut buzzer_out(D8);
DigitalOut warning_led(LED2);

void send(const char* text) {
    while (*text) uart.write(text++, 1);
}

int main() {
    send("Lab 4 Ibrahim - Sensor System\r\n");

    while (1) {
        float temp_raw = temperature_sensor.read();
        float gas_raw  = gas_sensor.read();
        float pot_raw  = potentiometer.read();

        // convert readings
        float temp_c = temp_raw * 3300.0f / 10.0f;
        float gas_val = gas_raw * 1000.0f;
        float threshold_val = pot_raw * 100.0f;

        bool alert_on = false;
        const char* alert_cause = "None";

        // check temperature
        if (temp_c > threshold_val) {
            alert_on = true;
            alert_cause = "Temperature";
        }

        // check gas (higher scale)
        if (gas_val > threshold_val * 10.0f) {
            alert_on = true;
            alert_cause = "Gas";
        }

        buzzer_out = alert_on ? 1 : 0;
        warning_led = alert_on ? 1 : 0;

        char output[100];
        if (alert_on) {
            sprintf(output, "Buzzer ON - Cause: %s | Temp=%.1f Gas=%.0f Thresh=%.0f\r\n",
                    alert_cause, temp_c, gas_val, threshold_val);
        } else {
            sprintf(output, "System Normal | Temp=%.1f Gas=%.0f Thresh=%.0f\r\n",
                    temp_c, gas_val, threshold_val);
        }
        send(output);

        wait_us(1000000);
    }
}
