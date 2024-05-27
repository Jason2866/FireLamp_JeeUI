/*
    This file is a part of FireLamp_JeeUI project
    https://github.com/vortigont/FireLamp_JeeUI

    Copyright © 2023-2024 Emil Muratov (Vortigont)

    FireLamp_JeeUI is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FireLamp_JeeUI is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FireLamp_JeeUI.  If not, see <https://www.gnu.org/licenses/>.

  (Этот файл — часть FireLamp_JeeUI.

   FireLamp_JeeUI - свободная программа: вы можете перераспространять ее и/или
   изменять ее на условиях Стандартной общественной лицензии GNU в том виде,
   в каком она была опубликована Фондом свободного программного обеспечения;
   либо версии 3 лицензии, либо (по вашему выбору) любой более поздней
   версии.

   FireLamp_JeeUI распространяется в надежде, что она будет полезной,
   но БЕЗО ВСЯКИХ ГАРАНТИЙ; даже без неявной гарантии ТОВАРНОГО ВИДА
   или ПРИГОДНОСТИ ДЛЯ ОПРЕДЕЛЕННЫХ ЦЕЛЕЙ. Подробнее см. в Стандартной
   общественной лицензии GNU.

   Вы должны были получить копию Стандартной общественной лицензии GNU
   вместе с этой программой. Если это не так, см.
   <https://www.gnu.org/licenses/>.)
*/

#pragma once
#include "esp_event.h"

// helper macro to reduce typing
#define EVT_POST(event_base, event_id) esp_event_post_to(evt::get_hndlr(), event_base, event_id, NULL, 0, portMAX_DELAY)
#define EVT_POST_DATA(event_base, event_id, event_data, data_size) esp_event_post_to(evt::get_hndlr(), event_base, event_id, event_data, data_size, portMAX_DELAY)

// ESP32 event loop defines
ESP_EVENT_DECLARE_BASE(LAMP_SET_EVENTS);        // declaration of Lamp setter Command events base (in reply to this command, an LAMP_CHANGE_EVENTS could be generated)
ESP_EVENT_DECLARE_BASE(LAMP_GET_EVENTS);        // declaration of Lamp getter Command events base (in reply to this command, an LAMP_STATE_EVENTS could be generated)
ESP_EVENT_DECLARE_BASE(LAMP_STATE_EVENTS);      // declaration of Lamp State publishing events base (those events are published on request, not on change)
ESP_EVENT_DECLARE_BASE(LAMP_CHANGE_EVENTS);     // declaration of Lamp State change notification events base (those events are published when state changes or in reply to "cmd set" events)

// Lamp's Event Loop
namespace evt {


// Lamp events
enum class lamp_t:int32_t {
  noop = 0,               // NoOp command
  // 0-9 are reserved for something extraordinary

  // **** Set/get state command events ****

  // lamp power and mode of operation
  pwr = 10,                 // get/set current power state, (optional) param int p: 0 - poweroff, 1 - poweron, 2 - pwrtoggle, 
  pwron,                    // switch power on
  pwroff,                   // switch power off
  pwrtoggle,                // power toggle

  // brightness control, parameter value - int
  brightness = 20,          // set/get brightness according to current scale, param: int n
  brightness_nofade,        // set brightness according to current scale and w/o fade effect
  brightness_lcurve,        // set brightness luma curve
  brightness_scale,         // set brightness scale
  brightness_step,          // step brightness incr/decr w/o fade, param: int n - step to shift

  // effects switching
  effSwitchTo = 30,         // switch to specific effect num, param: unsigned n
  effSwitchNext,
  effSwitchPrev,
  effSwitchRnd,
  effSwitchStep,            // switch n steps offset from current effect, param: signed n

  // mp3 player
  mp3vol = 40,              // get/set mp3 volume, param: int n
  mp3volstep,               // set mp3 volume increment/decrement, param: int n
  mp3mute,
  mp3unmute,
  mp3state,                 // get mp3player state, will publish current mp3player state both to event bus and EmbUI's feeders

  // Notifications
  cockoo = 50,              // play cockoo or talking clock. param int - track to play, 1 is for Talking clock, any other - number of track from signals folder
  alarmTrigger,             // Alarm event. Play alarm track. param int - track to play
  alarmStop,                // Stop alarm actions event

  // ext devices control
  btnLock = 100,            // Lock button
  btnUnLock,                // UnLock button
  btnLockState,             // request for button lock state, ButtonEventHandler will reply with LAMP_STATE_EVENTS:: btnUnLock/btnLock

  encoderMode = 110,        // change event, which mode encoder has switched to, param: unsigned

  // **** state change / notification events ****
  fadeStart = 1000,
  fadeEnd,                  // param: signed int - brightness level that fader ended with


  noop_end                  // NoOp
};


  /**
   * @brief Starts Lamp's event loop task
   * this loop will manage events processing between Lamp's components and controls
   *
   * @return esp_event_loop_handle_t* a pointer to loop handle
   */
  void start();

  /**
   * @brief Stops Lamp's event loop task
   *
   * @return esp_event_loop_handle_t* a pointer to loop handle
   */
  void stop();

  esp_event_loop_handle_t get_hndlr();


  void debug();

  void debug_hndlr(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
} // namespace evt
