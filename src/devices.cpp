/*
Copyright © 2023 Emil Muratov (vortigont)

    This file is part of FireLamp_JeeUI.

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

#include "devices.h"
#include "char_const.h"
#include "constants.h"    // EmbUI char constants
#include "embuifs.hpp"
#include "log.h"

// TM1637 disaplay class
#include "tm1637display.hpp"
// ESPAsyncButton
#include "bencoder.hpp"



// Device object placesholders

// TM1637 display https://github.com/vortigont/TM1637/
TMDisplay *tm1637 = nullptr;

// GPIO button
GPIOButton<ESPEventPolicy> *button = nullptr;
ButtonEventHandler *button_handler = nullptr;

void tm1637_setup(){
  DynamicJsonDocument doc(DISPLAY_CFG_JSIZE);
  if (!embuifs::deserializeFile(doc, TCONST_fcfg_display) || !doc.containsKey(T_tm1637)) return;      // config is missing, bad or has no TM1637 data

  JsonVariantConst cfg( doc[T_tm1637] );
  tm1637_configure(cfg);
}

void tm1637_configure(JsonVariantConst& tm){
  if (!tm[T_enabled]){
     // TM module disabled or config is invalid
    if (tm1637){
      delete tm1637;
      tm1637 = nullptr;
    }
    return;
  }

  if (!tm1637){
    // create TM1637 display object if it's pins are defined
    unsigned clk = tm[T_CLK] | -1;
    unsigned dio = tm[P_data] | -1;
    if (clk != -1 && dio != -1){
      //LOG(printf, "tm1637 using pins rx:%d, tx:%d\n", clk, dio);
      tm1637 = new TMDisplay(clk, dio);
      if (!tm1637) return;
    }
  }

  tm1637->brightness(tm[T_tm_brt_on], true);
  tm1637->brightness(tm[T_tm_brt_off], false);
  tm1637->clk_12h = tm[T_tm_12h];
  tm1637->clk_lzero = tm[T_tm_lzero];
  tm1637->init();

}

void button_cfg_load(){
  DynamicJsonDocument doc(BTN_EVENTS_CFG_JSIZE);
  if (!embuifs::deserializeFile(doc, T_benc_cfg)) return;      // config is missing, bad

  JsonVariantConst _cfg( doc[T_btn_cfg] );
  button_configure_gpio(_cfg);

  _cfg = doc[T_btn_events];
  button_configure_events(_cfg);
}

void button_configure_gpio(JsonVariantConst btn_cfg){

  if (!btn_cfg[T_enabled]){
     // button disabled or config is invalid
    if (button){
      delete button;
      button = nullptr;
    }
    if (button_handler){
      delete button_handler;
      button_handler = nullptr;
    }
    return;
  }

  int32_t gpio = btn_cfg[T_gpio] | -1;
  if (gpio == -1) return;                 // pin disabled
  bool debn = btn_cfg[T_debounce];

  // create GPIOButton object
  if (!button){
    button = new GPIOButton<ESPEventPolicy> (static_cast<gpio_num_t>(gpio), btn_cfg[T_logicL]);
    if (!button) return;
    button->setDebounce(debn);
    button->timeouts.setLongPress(btn_cfg[T_lpresst]);
    button->timeouts.setMultiClick(btn_cfg[T_mclickt]);
  } else {
    if ( gpio != button->getGPIO() ){
      button->setGPIO(static_cast<gpio_num_t>(gpio), btn_cfg[T_logicL]);
    }
    button->setDebounce(debn);
    button->timeouts.setLongPress(btn_cfg[T_lpresst]);
    button->timeouts.setMultiClick(btn_cfg[T_mclickt]);

    // button obj already exist, so no need to configure it further
    return;
  }

  button->enableEvent(ESPButton::event_t::press, false);
  button->enableEvent(ESPButton::event_t::release, false);

  button->enableEvent(ESPButton::event_t::longPress);
  button->enableEvent(ESPButton::event_t::longRelease);
  button->enableEvent(ESPButton::event_t::autoRepeat);
  button->enableEvent(ESPButton::event_t::multiClick);

  // set event loop to post events to
  ESPButton::set_event_loop_hndlr(evt::get_hndlr());

  button->enable();
}

void button_configure_events(JsonVariantConst btn_cfg){
  if (!button_handler){
    button_handler = new ButtonEventHandler();
    if (!button_handler) return;
    // subscribe only once, when object is newly created
    button_handler->subscribe(evt::get_hndlr());
  }

  button_handler->load(btn_cfg);     // load config
}





