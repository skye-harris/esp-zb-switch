// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
// Modifications Copyright 2024 Skye Harris
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "Switches/switches.h"
#include "Zigbee/zigbee.h"

/********************* GPIO functions **************************/
static QueueHandle_t gpioEventQueue = NULL;

static void IRAM_ATTR gpioIsrHandler(void *arg) {
    xQueueSendFromISR(gpioEventQueue, (switch_func_pair_t *)arg, NULL);
}

static void setGpioInterruptEnabled(bool enabled) {
    for (int i = 0; i < PAIR_SIZE(button_func_pair); ++i) {
        if (enabled) {
            enableInterrupt((button_func_pair[i]).pin);
        } else {
            disableInterrupt((button_func_pair[i]).pin);
        }
    }
}

static void onButtonPress(switch_func_pair_t *button_func_pair) {
    switch (button_func_pair->func) {
        case SWITCH_RESET_CONTROL:
            ZB_FactoryReset();
            break;
    }
}

void SW_InitSwitches() {
    // Init button switch
    for (int i = 0; i < PAIR_SIZE(button_func_pair); i++) {
        pinMode(button_func_pair[i].pin, INPUT_PULLUP);
        /* create a queue to handle gpio event from isr */
        gpioEventQueue = xQueueCreate(10, sizeof(switch_func_pair_t));
        if (gpioEventQueue == 0) {
            log_e("Queue was not created and must not be used");
            while (1);
        }
        attachInterruptArg(button_func_pair[i].pin, gpioIsrHandler, (void *)(button_func_pair + i), FALLING);
    }
}

void SW_Loop() {
    // Handle button switch in loop()
    uint8_t pin = 0;
    switch_func_pair_t button_func_pair;
    static switch_state_t switch_state = SWITCH_IDLE;

    /* check if there is any queue received, if yes read out the button_func_pair */
    if (xQueueReceive(gpioEventQueue, &button_func_pair, portMAX_DELAY)) {
        pin = button_func_pair.pin;
        setGpioInterruptEnabled(false);

        while (true) {
            bool value = digitalRead(pin);

            switch (switch_state) {
                case SWITCH_IDLE:
                    switch_state = (value == LOW) ? SWITCH_PRESS_DETECTED : SWITCH_IDLE;
                    break;
                case SWITCH_PRESS_DETECTED:
                    switch_state = (value == LOW) ? SWITCH_PRESS_DETECTED : SWITCH_RELEASE_DETECTED;
                    break;
                case SWITCH_RELEASE_DETECTED:
                    switch_state = SWITCH_IDLE;
                    /* callback to button_handler */
                    (*onButtonPress)(&button_func_pair);
                    break;

                default:
                    break;
            }

            if (switch_state == SWITCH_IDLE) {
                setGpioInterruptEnabled(true);

                break;
            }

            delay(10);
        }
    }
}