/*
Copyright © 2020 Dmytro Korniienko (kDn)
JeeUI2 lib used under MIT License Copyright (c) 2019 Marsel Akhkamov

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
#pragma once
//typedef enum : uint8_t {TS_NONE=0, TS_VER1, TS_VER2} TIME_SOUND_TYPE; // виды озвучки времени (8 вариантов максимум)
typedef enum : uint8_t {AT_NONE=0, AT_FIRST, AT_SECOND, AT_THIRD, AT_FOURTH, AT_FIFTH, AT_RANDOM, AT_RANDOMMP3} ALARM_SOUND_TYPE; // виды будильников (8 вариантов максимум)

#include "config.h"
#include <DFMiniMp3.h>
#include "ts.h"
#include "evtloop.h"

#define MP3_SERIAL Serial1
#define DFPLAYER_DEFAULT_VOL  12
#define DFPLAYER_JSON_CFG_JSIZE 4096


class MP3PlayerController {
private:
      struct Flags {
        uint8_t timeSoundType:3;      // вид озвучивания времени
        uint8_t tAlarm:3;             // вид будильника
        bool ready:1;                 // закончилась ли инициализация
        //bool on:1;                    // включен ли...
        bool eff_playtrack:1;         // режим проигрывания треков эффектов
        bool eff_looptrack:1;         // зацикливать дорожку эффекта
        bool mute:1;                  // Player's DAC disabled
        bool isplaying:1;             // воспроизводится ли сейчас песня или эффект
        bool looptrack:1;             // if current track is looped (cmd has been sent to player)
      };

    Flags flags{};

    HardwareSerial& _serial;
    Task _tPeriodic; // периодический опрос плеера
    uint8_t _volume = DFPLAYER_DEFAULT_VOL;
    DfMp3_StatusState _state = DfMp3_StatusState_Idle;
    //uint16_t mp3filescount = 255; // кол-во файлов в каталоге MP3
    //uint16_t nextAdv=0; // следующее воспроизводимое сообщение (произношение минут после часов)

    //String soundfile; // хранилище пути/имени
    //void printSatusDetail();
    //void playAdvertise(int filenb);
    //void playFolder0(int filenb);
    //void restartSound();

  esp_event_handler_instance_t _lmp_ch_instance = nullptr;
  esp_event_handler_instance_t _lmp_set_instance = nullptr;

  static void event_hndlr(void* handler, esp_event_base_t base, int32_t id, void* event_data);

  // change events handler
  void _lmpChEventHandler(esp_event_base_t base, int32_t id, void* data);

  // set events handler

  void _lmpSetEventHandler(esp_event_base_t base, int32_t id, void* data);

  /**
   * @brief initialze player instance
   * 
   */
  void init();

public:
  MP3PlayerController(HardwareSerial& serial, DfMp3Type type = DfMp3Type::origin, uint32_t ackTimeout = DF_ACK_TIMEOUT);
  // d-tor
  ~MP3PlayerController();

  // Player instance
  DFMiniMp3 *dfp = nullptr;

  /**
   * @brief initialize player
   * 
   */
  void begin(int8_t rxPin, int8_t txPin, DfMp3Type type, uint32_t ackTimeout = DF_ACK_TIMEOUT);

  void begin(int8_t rxPin, int8_t txPin);


  void subscribe();

  void unsubscribe();

  void loop();

  // play/advertise current time
  void playTime(int hours, int minutes);

  bool isReady(){ return flags.ready; }

  /**
   * @brief play effect melody
   * 
   * @param effnb 
   */
  void playEffect(uint32_t effnb);

  /**
   * @brief set/unset playing effect sounds
   * 
   * @param value 
   */
  void setPlayEffects(bool value){ flags.eff_playtrack = value; }

  /**
   * @brief Set/uset loop effect's track
   * otherwise player will play next track on end
   * 
   * @param value 
   */
  void setLoopEffects(bool value){ flags.eff_looptrack = value; }

  uint8_t getVolume() const { return _volume; }
  void setVolume(uint8_t vol);

/*
    uint16_t getCurPlayingNb() {return prev_effnb;} // вернуть предыдущий для смещения
    void setupplayer(uint16_t effnb, const String &_soundfile) {soundfile = _soundfile; cur_effnb=effnb;};
    bool isAlarm() {return alarm;}
    bool isOn() {return on && ready;}
    bool isMP3Mode() {return mp3mode;}
    void setIsOn(bool val, bool forcePlay=true);


    void playName(uint16_t effnb);
    void setTempVolume(uint8_t vol);
    void setMP3count(uint16_t cnt) {mp3filescount = cnt;} // кол-во файлов в папке MP3
    uint16_t getMP3count() {return mp3filescount;}
    void setEqType(uint8_t val) { EQ(val); }
    void setPlayMP3(bool flag) {mp3mode = flag;}
    void setPlayEffect(bool flag) {effectmode = flag;}
    void setAlarm(bool flag) {alarm = flag; stop(); isplaying = false;}
    void StartAlarmSoundAtVol(ALARM_SOUND_TYPE val, uint8_t vol);
    void ReStartAlarmSound(ALARM_SOUND_TYPE val);
    void RestoreVolume() { setVolume(cur_volume); }
    void setCurEffect(uint16_t effnb) { prev_effnb=cur_effnb; cur_effnb = effnb%256; }
*/
    //void handle();
};
