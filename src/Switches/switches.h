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

/* Switch configuration */
#include <Arduino.h>
#define GPIO_FACTORY_RESET_SWITCH GPIO_NUM_9
#define PAIR_SIZE(TYPE_STR_PAIR) (sizeof(TYPE_STR_PAIR) / sizeof(TYPE_STR_PAIR[0]))

typedef enum {
    SWITCH_RESET_CONTROL
} switch_func_t;

typedef struct {
    uint8_t pin;
    switch_func_t func;
} switch_func_pair_t;

typedef enum {
    SWITCH_IDLE,
    SWITCH_PRESS_ARMED,
    SWITCH_PRESS_DETECTED,
    SWITCH_PRESSED,
    SWITCH_RELEASE_DETECTED,
} switch_state_t;

static switch_func_pair_t button_func_pair[] = {
    {GPIO_FACTORY_RESET_SWITCH, SWITCH_RESET_CONTROL},
};

void SW_InitSwitches();
void SW_Loop();