/*
 * Copyright (c) 2020 Tencent Cloud. All rights reserved.

 * Licensed under the MIT License (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT

 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#ifndef __BOARD_OPS_H__
#define __BOARD_OPS_H__

#include "esp_err.h"


void board_init(void);

void led_set(uint32_t h, uint32_t s, uint32_t v);

void led_on(uint32_t h, uint32_t s, uint32_t v);

void led_off(void);

#endif 

