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

#include "tm.h"
#include "lamp.h"

#if TM_SHOW_BANNER
// String welcome_banner = "FIRE_START"; // Список букв для вывода A Bb Cc Dd Ee F G Hh Ii J K Ll m Nn Oo P q r S t U v w x Y Z
/* Указывать можно в любом регистре, разделять лучше нижним подчеркиванием "_", если поставить пробел, то слова разделятся и будут отображаться по очереди, например сначала заскроллится "FIRE",
дойдет до конца, потухнет и только тогда появится "START"*/
#endif
uint8_t& TMCLOCK::getSetDelay() { // для доступа к переменной из других плагинов, достаточно в h-файл плагина добавить #include "tm.h"
  return tmDelayTime;
};

void TMCLOCK::tm_setup() {
    begin();
    clearScreen();
    LOG(printf_P, PSTR("tm1637 was initialized \n"));
}


void TMCLOCK::tm_loop() {
// this is so ugly!!!
#ifdef TM1637_CLOCK
  setBrightness((myLamp.isLampOn()) ? myLamp.getBrightOn() : myLamp.getBrightOff());         // Чекаем статус лампы и меняем яркость

  #if TM_SHOW_BANNER
  if (!bannerShowed) {
    showBanner();          // Выводим стартовый баннер
    return;
  }
  #endif


  if (tmDelayTime) { // пропускаем цикл вывода часов, давая возможность успеть увидеть инфу с другиг плагинов
    --tmDelayTime;
    return;
  }

  if(ipShow) {      // Пропускаем все, если выводится IP
    scrollip();
    return;
  }

/*
// todo fix this missing method
  if(TimeProcessor::getInstance().isDirtyTime()) {      // Светим --:--, если не подтянулось время с инета или не было настроено вручную
    auto d =  (showPoints) ? DisplayDigit().setG().setDot() : DisplayDigit().setG();
    const uint8_t rawBuffer[4] = {d, d, d, d};
    displayRawBytes(rawBuffer, 4);
  } else {
*/
    const tm* t = localtime(TimeProcessor::getInstance().now());  // Определяем для вывода времени 
    char dispTime[6];            // Массив для сбора времени

    sprintf (dispTime,
              myLamp.isTmZero() ? "%02d%s%02d" : "%d%s%02d",
              myLamp.isTm24() ? t->tm_hour : t->tm_hour % 12,
              showPoints ? "." : "",
              t->tm_min);

    myLamp.isTmZero() ? display(String(dispTime)) : ((t->tm_hour < 10 || (!myLamp.isTm24() && t->tm_hour > 12 && t->tm_hour < 22)) ? display(String(dispTime), true, false, 1) : display(String(dispTime)));
    showPoints=!showPoints;
#endif
}

#if TM_SHOW_BANNER
// | FUNC - Показать стартовый баннер
// |----------
void TMCLOCK::showBanner(){       
  static uint8_t l = 0;           // Переменная для баннера
  if (l == 21) return;
  l++;   // Добавляем счетчик и ограничиваем, чтобы не гонял по кругу
  if (WiFi.getMode() & WIFI_MODE_STA && l <= 20 && l > 4) {
    String ip = (String) F("IP.") + (String) WiFi.localIP().toString();
    splitIp(ip, F("."), splittedIp);
    display(formatIp(splittedIp, F("")))->scrollLeft(500, 4); // Запуск баннера (хоть и задержка указана 500, по факту она 1 сек), индекс 4 (выводит 4 цифры за раз)
  }
  else if (!WiFi.getMode() & WIFI_MODE_STA && l <= 20 && l > 4) display(String(F("__AP_192_168___4___1")))->scrollLeft(500, 4);  // Если нет подключения, то крутим айпи точки доступа
  if (l == 20) bannerShowed = 1;
}
#endif


void TMCLOCK::scrollip(){
  if (WiFi.getMode() & WIFI_MODE_STA) {
    String ip(F("IP."));
    ip += WiFi.localIP().toString();
    splitIp(ip, F("."), splittedIp);
    display(formatIp(splittedIp, ""))->scrollLeft(500, 4); // Запуск баннера (хоть и задержка указана 500, по факту она 1 сек), индекс 4 (выводит 4 цифры за раз)
  }
  else if (!(WiFi.getMode() & WIFI_MODE_STA)) display(String(F("__AP_192_168___4___1")))->scrollLeft(500, 4);  // Если нет подключения, то крутим айпи точки доступа
  ipShow--;
}


// | FUNC - Split IP
// |----------
void TMCLOCK::splitIp(String str, String dlm, String dest[])
{
  int nextPos      = str.indexOf(dlm);

  for(short i=0; i<5; i++)
  {
    dest[i] = str.substring(0, nextPos);

    str     = str.substring(nextPos+1);
    nextPos = str.indexOf(dlm);
  }
}


// | FUNC - Format IP
// |----------
String TMCLOCK::formatIp(String inArr[], String dlm)
{
  String tmp    = F("____");
  String output = F("");

  for(uint8_t i=0; i<5; i++){
    String crnt = inArr[i];

    for(uint8_t j=0; j<crnt.length(); j++){
      tmp.setCharAt(tmp.length()-j-1, crnt.charAt(crnt.length()-j-1));
    }

    output += tmp;
    if(i<3){
      output += dlm;
    }

    tmp    = F("____");
  }

  return output;
}
