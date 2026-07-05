#include "smart_home_system.h"

int main()
{
    initDriveSystem();
    while (true) {
        updateDriveSystem();
    }
}
