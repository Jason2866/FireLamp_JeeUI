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

#include "lamp.h"
#include "effectmath.h"
//#include "main.h"
extern LAMP myLamp; // Объект лампы

// Общий набор мат. функций и примитивов для обсчета эффектов

namespace EffectMath_PRIVATE {
    MATRIXFLAGS matrixflags;
    CRGB leds[NUM_LEDS]; // основной буфер вывода изображения
    CRGB overrun;
    
    CRGB *getUnsafeLedsArray(){return leds;}

    // ключевая функция с подстройкой под тип матрицы, использует MIRR_V и MIRR_H
    uint32_t getPixelNumber(int16_t x, int16_t y) // получить номер пикселя в ленте по координатам
    {
    #ifndef XY_EXTERN
        // хак с макроподстановкой, пусть живет пока
        #define MIRR_H matrixflags.MIRR_H
        #define MIRR_V matrixflags.MIRR_V
        
        if ((THIS_Y % 2 == 0) || MATRIX_TYPE)                     // если чётная строка
        {
            return ((uint32_t)THIS_Y * SEGMENTS * _WIDTH + THIS_X);
        }
        else                                                      // если нечётная строка
        {
            return ((uint32_t)THIS_Y * SEGMENTS * _WIDTH + _WIDTH - THIS_X - 1);
        }
    
        #undef MIRR_H
        #undef MIRR_V
    #else
        uint16_t i = (y * WIDTH) + x;
        uint16_t j = pgm_read_dword(&XYTable[i]);
        return j;
    #endif
    }
}

using namespace EffectMath_PRIVATE;

// для работы FastLed (blur2d)
uint16_t XY(uint8_t x, uint8_t y)
{
#ifdef ROTATED_MATRIX
  return getPixelNumber(y,x); // повернутое на 90 градусов
#else
  return getPixelNumber(x,y); // обычное подключение
#endif
}

//--------------------------------------
// ******** общие мат. функции переиспользуются в другом эффекте
uint8_t EffectMath::mapsincos8(bool map, uint8_t theta, uint8_t lowest, uint8_t highest) {
  uint8_t beat = map ? sin8(theta) : cos8(theta);
  return lowest + scale8(beat, highest - lowest);
}

void EffectMath::MoveFractionalNoise(bool _scale, const uint8_t noise3d[][WIDTH][HEIGHT], int8_t amplitude, float shift) {
  uint8_t zD;
  uint8_t zF;
  CRGB *leds = getUnsafeLedsArray(); // unsafe
  CRGB ledsbuff[NUM_LEDS];
  uint16_t _side_a = _scale ? HEIGHT : WIDTH;
  uint16_t _side_b = _scale ? WIDTH : HEIGHT;

  for(uint8_t i=0; i<NUM_LAYERS; i++)
    for (uint16_t a = 0; a < _side_a; a++) {
      uint8_t _pixel = _scale ? noise3d[i][0][a] : noise3d[i][a][0];
      int16_t amount = ((int16_t)(_pixel - 128) * 2 * amplitude + shift * 256);
      int8_t delta = ((uint16_t)fabs(amount) >> 8) ;
      int8_t fraction = ((uint16_t)fabs(amount) & 255);
      for (uint8_t b = 0 ; b < _side_b; b++) {
        if (amount < 0) {
          zD = b - delta; zF = zD - 1;
        } else {
          zD = b + delta; zF = zD + 1;
        }
        CRGB PixelA = CRGB::Black  ;
        if ((zD >= 0) && (zD < _side_b))
          PixelA = _scale ? EffectMath::getPixel(zD%WIDTH, a%HEIGHT) : EffectMath::getPixel(a%WIDTH, zD%HEIGHT);

        CRGB PixelB = CRGB::Black ;
        if ((zF >= 0) && (zF < _side_b))
          PixelB = _scale ? EffectMath::getPixel(zF%WIDTH, a%HEIGHT) : EffectMath::getPixel(a%WIDTH, zF%HEIGHT);
        uint16_t x = _scale ? b : a;
        uint16_t y = _scale ? a : b;
        ledsbuff[getPixelNumber(x%WIDTH, y%HEIGHT)] = (PixelA.nscale8(ease8InOutApprox(255 - fraction))) + (PixelB.nscale8(ease8InOutApprox(fraction)));   // lerp8by8(PixelA, PixelB, fraction );
      }
    }
  memcpy(leds, ledsbuff, sizeof(CRGB)* NUM_LEDS);
}

/**
 * Возвращает частное от а,б округленное до большего целого
 */
uint8_t EffectMath::ceil8(const uint8_t a, const uint8_t b){
  return a/b + !!(a%b);
}

// новый фейдер
void EffectMath::fadePixel(uint8_t i, uint8_t j, uint8_t step)
{
    CRGB &led = EffectMath::getPixel(i,j);
    if (!led) return; // см. приведение к bool для CRGB, это как раз тест на 0
    
    if (led.r >= 30U || led.g >= 30U || led.b >= 30U){
        led.fadeToBlackBy(step);
    }
    else{
        EffectMath::drawPixelXY(i, j, 0U);
    }
}

// функция плавного угасания цвета для всех пикселей
void EffectMath::fader(uint8_t step)
{
  for (uint8_t i = 0U; i < WIDTH; i++)
  {
    for (uint8_t j = 0U; j < HEIGHT; j++)
    {
      fadePixel(i, j, step);
    }
  }
}

/* kostyamat добавил
функция увеличения яркости */
CRGB EffectMath::makeBrighter( const CRGB& color, fract8 howMuchBrighter)
{
  CRGB incrementalColor = color;
  incrementalColor.nscale8( howMuchBrighter);
  return color + incrementalColor;
}

/* kostyamat добавил
 функция уменьшения яркости */
CRGB EffectMath::makeDarker( const CRGB& color, fract8 howMuchDarker )
{
  CRGB newcolor = color;
  newcolor.nscale8( 255 - howMuchDarker);
  return newcolor;
}

/* kostyamat добавил
 функция возвращает рандомное значение float между min и max 
 с шагом 1/1024 */
float EffectMath::randomf(float min, float max)
{
  return fmap(random(1024), 0, 1023, min, max);
}

/* kostyamat добавил
 функция возвращает true, если float
 ~= целое (первая цифра после запятой == 0) */
bool EffectMath::isInteger(float val) {
    float val1;
    val1 = val - (int)val;
    if ((int)(val1 * 10) == 0)
        return true;
    else
        return false;
}

// Функция создает вспышки в разных местах матрицы, параметр 0-255. Чем меньше, тем чаще.
void EffectMath::addGlitter(uint8_t chanceOfGlitter){
  if ( random8() < chanceOfGlitter) leds[random16(NUM_LEDS)] += CRGB::Gray;
}

uint32_t EffectMath::getPixColor(uint32_t thisSegm) // функция получения цвета пикселя по его номеру
{
  uint32_t thisPixel = thisSegm * SEGMENTS;
  if (thisPixel > NUM_LEDS - 1) return 0;
  return (((uint32_t)leds[thisPixel].r << 16) | ((uint32_t)leds[thisPixel].g << 8 ) | (uint32_t)leds[thisPixel].b);
}

// Заливает матрицу выбраным цветом
void EffectMath::fillAll(const CRGB &color) 
{
  for (int32_t i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = color;
  }
}

void EffectMath::drawPixelXY(int16_t x, int16_t y, const CRGB &color) // функция отрисовки точки по координатам X Y
{
  getPixel(x,y) = color;
}

void EffectMath::drawPixelXY(uint16_t x, uint16_t y, const CRGB &color, byte opt) // функция отрисовки точки по координатам X Y
{
#if  SEGMENTS > 1
  uint32_t thisPixel = getPixelNumber(x, y) * SEGMENTS;
  for (uint16_t i = 0; i < SEGMENTS; i++)
  {
    getLed[thisPixel + i] = color;
  switch (opt) {
  case 1:
    getLed[thisPixel + i] += color;
    break;
  case 2:
    getLed[thisPixel + i] -= color;
    break;
  case 3:
    getLed[thisPixel + i] *= color;
    break;
  case 4:
    getLed[thisPixel + i] /= color;
    break;
  default:
    getLed[thisPixel + i] = color;
    break;
  }
  }
#else
  switch (opt) {
  case 0:
    getPixel(x,y) = color;
    break;
  case 1:
    getPixel(x,y) += color;
    break;
  case 2:
    getPixel(x,y) -= color;
    break;
  case 3:
    getPixel(x,y) *= color;
    break;
  case 4:
    getPixel(x,y) /= color;
    break;
  default:
    getPixel(x,y) = color;
    break;
  }
#endif
}

void EffectMath::wu_pixel(uint32_t x, uint32_t y, CRGB col) {      //awesome wu_pixel procedure by reddit u/sutaburosu
  // extract the fractional parts and derive their inverses
  uint8_t xx = x & 0xff, yy = y & 0xff, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
  #define WU_WEIGHT(a,b) ((uint8_t) (((a)*(b)+(a)+(b))>>8))
  uint8_t wu[4] = {WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy),
                   WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (uint8_t i = 0; i < 4; i++) {
    uint16_t xn = (x >> 8) + (i & 1); uint16_t yn = (y >> 8) + ((i >> 1) & 1);
    CRGB clr = EffectMath::getPixColorXY(xn, yn);
    clr.r = qadd8(clr.r, (col.r * wu[i]) >> 8);
    clr.g = qadd8(clr.g, (col.g * wu[i]) >> 8);
    clr.b = qadd8(clr.b, (col.b * wu[i]) >> 8);

    EffectMath::drawPixelXY(xn, yn, clr);
  }
  #undef WU_WEIGHT
}

void EffectMath::drawPixelXYF(float x, float y, const CRGB &color, uint8_t darklevel)
{
  //if (x<-1.0 || y<-1.0 || x>((float)WIDTH) || y>((float)HEIGHT)) return;

  // extract the fractional parts and derive their inverses
  uint8_t xx = (x - (int)x) * 255, yy = (y - (int)y) * 255, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
  #define WU_WEIGHT(a,b) ((uint8_t) (((a)*(b)+(a)+(b))>>8))
  uint8_t wu[4] = {WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy),
                   WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (uint8_t i = 0; i < 4; i++) {
    int16_t xn = x + (i & 1), yn = y + ((i >> 1) & 1);
    CRGB clr = EffectMath::getPixColorXY(xn, yn);
    clr.r = qadd8(clr.r, (color.r * wu[i]) >> 8);
    clr.g = qadd8(clr.g, (color.g * wu[i]) >> 8);
    clr.b = qadd8(clr.b, (color.b * wu[i]) >> 8);
    if (darklevel > 0) EffectMath::drawPixelXY(xn, yn, EffectMath::makeDarker(clr, darklevel));
    else EffectMath::drawPixelXY(xn, yn, clr);
  }
  #undef WU_WEIGHT
}

void EffectMath::drawPixelXYF_X(float x, int16_t y, const CRGB &color, uint8_t darklevel)
{
  if (x<-1.0 || y<-1 || x>((float)WIDTH) || y>((float)HEIGHT)) return;

  // extract the fractional parts and derive their inverses
  uint8_t xx = (x - (int)x) * 255, ix = 255 - xx;
  // calculate the intensities for each affected pixel
  uint8_t wu[2] = {ix, xx};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (int8_t i = 1; i >= 0; i--) {
    int16_t xn = x + (i & 1);
    CRGB clr = EffectMath::getPixColorXY(xn, y);
    clr.r = qadd8(clr.r, (color.r * wu[i]) >> 8);
    clr.g = qadd8(clr.g, (color.g * wu[i]) >> 8);
    clr.b = qadd8(clr.b, (color.b * wu[i]) >> 8);
    if (darklevel > 0) EffectMath::drawPixelXY(xn, y, EffectMath::makeDarker(clr, darklevel));
    else EffectMath::drawPixelXY(xn, y, clr);
  }
}

void EffectMath::drawPixelXYF_Y(int16_t x, float y, const CRGB &color, uint8_t darklevel)
{
  if (x<-1 || y<-1.0 || x>((float)WIDTH) || y>((float)HEIGHT)) return;

  // extract the fractional parts and derive their inverses
  uint8_t yy = (y - (int)y) * 255, iy = 255 - yy;
  // calculate the intensities for each affected pixel
  uint8_t wu[2] = {iy, yy};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (int8_t i = 1; i >= 0; i--) {
    int16_t yn = y + (i & 1);
    CRGB clr = EffectMath::getPixColorXY(x, yn);
    clr.r = qadd8(clr.r, (color.r * wu[i]) >> 8);
    clr.g = qadd8(clr.g, (color.g * wu[i]) >> 8);
    clr.b = qadd8(clr.b, (color.b * wu[i]) >> 8);
    if (darklevel > 0) EffectMath::drawPixelXY(x, yn, EffectMath::makeDarker(clr, darklevel));
    else EffectMath::drawPixelXY(x, yn, clr);
  }
}

CRGB EffectMath::getPixColorXYF(float x, float y)
{
  //if (x<-1.0 || y<-1.0 || x>((float)WIDTH) || y>((float)HEIGHT)) return CRGB::Black;

  // extract the fractional parts and derive their inverses
  uint8_t xx = (x - (int)x) * 255, yy = (y - (int)y) * 255, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
  #define WU_WEIGHT(a,b) ((uint8_t) (((a)*(b)+(a)+(b))>>8))
  uint8_t wu[4] = {WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy),
                   WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  CRGB clr=CRGB::Black;
  for (uint8_t i = 0; i < 4; i++) {
    int16_t xn = x + (i & 1), yn = y + ((i >> 1) & 1);
    if(!i){
      clr = EffectMath::getPixColorXY(xn, yn);
    } else {
      CRGB tmpColor=EffectMath::getPixColorXY(xn, yn);
      clr.r = qadd8(clr.r, (tmpColor.r * wu[i]) >> 8);
      clr.g = qadd8(clr.g, (tmpColor.g * wu[i]) >> 8);
      clr.b = qadd8(clr.b, (tmpColor.b * wu[i]) >> 8);
    }
  }
  return clr;
  #undef WU_WEIGHT
}

CRGB EffectMath::getPixColorXYF_X(float x, int16_t y)
{
  if (x<-1.0 || y<-1.0 || x>((float)WIDTH) || y>((float)HEIGHT)) return CRGB::Black;

  // extract the fractional parts and derive their inverses
  uint8_t xx = (x - (int)x) * 255, ix = 255 - xx;
  // calculate the intensities for each affected pixel
  uint8_t wu[2] = {ix, xx};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  CRGB clr=CRGB::Black;
  for (int8_t i = 1; i >= 0; i--) {
      int16_t xn = x + (i & 1);
      if(i){
        clr = EffectMath::getPixColorXY(xn, y);
      } else {
        CRGB tmpColor=EffectMath::getPixColorXY(xn, y);
        clr.r = qadd8(clr.r, (tmpColor.r * wu[i]) >> 8);
        clr.g = qadd8(clr.g, (tmpColor.g * wu[i]) >> 8);
        clr.b = qadd8(clr.b, (tmpColor.b * wu[i]) >> 8);
      }
  }
  return clr;
}

CRGB EffectMath::getPixColorXYF_Y(int16_t x, float y)
{
  if (x<-1 || y<-1.0 || x>((float)WIDTH) || y>((float)HEIGHT)) return CRGB::Black;

  // extract the fractional parts and derive their inverses
  uint8_t yy = (y - (int)y) * 255, iy = 255 - yy;
  // calculate the intensities for each affected pixel
  uint8_t wu[2] = {iy, yy};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  CRGB clr=CRGB::Black;
  for (int8_t i = 1; i >= 0; i--) {
      int16_t yn = y + (i & 1);
      if(i){
        clr = EffectMath::getPixColorXY(x, yn);
      } else {
        CRGB tmpColor=EffectMath::getPixColorXY(x, yn);
        clr.r = qadd8(clr.r, (tmpColor.r * wu[i]) >> 8);
        clr.g = qadd8(clr.g, (tmpColor.g * wu[i]) >> 8);
        clr.b = qadd8(clr.b, (tmpColor.b * wu[i]) >> 8);
      }
  }
  return clr;
}

void EffectMath::drawLine(int x1, int y1, int x2, int y2, const CRGB &color){
  int deltaX = abs(x2 - x1);
  int deltaY = abs(y2 - y1);
  int signX = x1 < x2 ? 1 : -1;
  int signY = y1 < y2 ? 1 : -1;
  int error = deltaX - deltaY;

  drawPixelXY(x2, y2, color);
  while (x1 != x2 || y1 != y2) {
      drawPixelXY(x1, y1, color);
      int error2 = error * 2;
      if (error2 > -deltaY) {
          error -= deltaY;
          x1 += signX;
      }
      if (error2 < deltaX) {
          error += deltaX;
          y1 += signY;
      }
  }
}

void EffectMath::drawLineF(float x1, float y1, float x2, float y2, const CRGB &color){
  float deltaX = fabs(x2 - x1);
  float deltaY = fabs(y2 - y1);
  float error = deltaX - deltaY;

  float signX = x1 < x2 ? 0.5 : -0.5;
  float signY = y1 < y2 ? 0.5 : -0.5;

  while (x1 != x2 || y1 != y2) { // (true) - а я то думаю - "почему функция часто вызывает вылет по вачдогу?" А оно вон оно чё, Михалычь!
    if ((signX > 0. && x1 > x2 + signX) || (signX < 0. && x1 < x2 + signX))
      break;
    if ((signY > 0. && y1 > y2 + signY) || (signY < 0. && y1 < y2 + signY))
      break;
    drawPixelXYF(x1, y1, color);
    float error2 = error;
    if (error2 > -deltaY)
    {
      error -= deltaY;
      x1 += signX;
      }
      if (error2 < deltaX) {
          error += deltaX;
          y1 += signY;
      }
  }
}

void EffectMath::drawCircle(int x0, int y0, int radius, const CRGB &color){
  int a = radius, b = 0;
  int radiusError = 1 - a;

  if (radius == 0) {
    EffectMath::drawPixelXY(x0, y0, color);
    return;
  }

  while (a >= b)  {
    EffectMath::drawPixelXY(a + x0, b + y0, color);
    EffectMath::drawPixelXY(b + x0, a + y0, color);
    EffectMath::drawPixelXY(-a + x0, b + y0, color);
    EffectMath::drawPixelXY(-b + x0, a + y0, color);
    EffectMath::drawPixelXY(-a + x0, -b + y0, color);
    EffectMath::drawPixelXY(-b + x0, -a + y0, color);
    EffectMath::drawPixelXY(a + x0, -b + y0, color);
    EffectMath::drawPixelXY(b + x0, -a + y0, color);
    b++;
    if (radiusError < 0)
      radiusError += 2 * b + 1;
    else
    {
      a--;
      radiusError += 2 * (b - a + 1);
    }
  }
}

void EffectMath::drawCircleF(float x0, float y0, float radius, const CRGB &color, float step){
  float a = radius, b = 0.;
  float radiusError = step - a;

  if (radius <= step*2) {
    EffectMath::drawPixelXYF(x0, y0, color);
    return;
  }

  while (a >= b)  {
      EffectMath::drawPixelXYF(a + x0, b + y0, color, 50);
      EffectMath::drawPixelXYF(b + x0, a + y0, color, 50);
      EffectMath::drawPixelXYF(-a + x0, b + y0, color, 50);
      EffectMath::drawPixelXYF(-b + x0, a + y0, color, 50);
      EffectMath::drawPixelXYF(-a + x0, -b + y0, color, 50);
      EffectMath::drawPixelXYF(-b + x0, -a + y0, color, 50);
      EffectMath::drawPixelXYF(a + x0, -b + y0, color, 50);
      EffectMath::drawPixelXYF(b + x0, -a + y0, color, 50);

    b+= step;
    if (radiusError < 0.)
      radiusError += 2. * b + step;
    else
    {
      a-= step;
      radiusError += 2 * (b - a + step);
    }
  }
}

void EffectMath::nightMode(CRGB *leds)
{
    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        leds[i].r = dim8_video(leds[i].r);
        leds[i].g = dim8_video(leds[i].g);
        leds[i].b = dim8_video(leds[i].b);
    }
}
uint32_t EffectMath::getPixColorXY(int16_t x, int16_t y) { return getPixColor( getPixelNumber(x, y)); } // функция получения цвета пикселя в матрице по его координатам
//void EffectMath::setLedsfadeToBlackBy(uint16_t idx, uint8_t val) { leds[idx].fadeToBlackBy(val); }
void EffectMath::setLedsNscale8(uint16_t idx, uint8_t val) { leds[idx].nscale8(val); }
void EffectMath::dimAll(uint8_t value) { for (uint16_t i = 0; i < NUM_LEDS; i++) {leds[i].nscale8(value); } }
void EffectMath::blur2d(uint8_t val) {::blur2d(leds,WIDTH,HEIGHT,val);}

CRGB &EffectMath::getLed(uint16_t idx) { 
  if(idx<NUM_LEDS){
    return leds[idx];
  } else {
    return overrun;
  }
}

CRGB *EffectMath::setLed(uint16_t idx, CHSV val) { 
  if(idx<NUM_LEDS){
    leds[idx] = val;
    return &leds[idx];
  } else {
    return &overrun;
  }
}

CRGB *EffectMath::setLed(uint16_t idx, CRGB val) {
  if(idx<NUM_LEDS){
    leds[idx] = val;
    return &leds[idx];
  } else {
    return &overrun;
  }
}

CRGB *EffectMath::setLed(uint16_t idx, CHSV val, byte opt) { 
  if (idx < NUM_LEDS) {
    CRGB tempVal = val;
    switch (opt) {
    case 0:
      leds[idx] = tempVal;
      break;
    case 1:
      leds[idx] += tempVal;
      break;
    case 2:
      leds[idx] -= tempVal;
      break;
    case 3:
      leds[idx] *= tempVal;
      break;
    case 4:
      leds[idx] /= tempVal;
      break;
    default:
      leds[idx] = tempVal;
      break;
    }
    return &leds[idx];
  } else {
    return &overrun;
  }
}

CRGB *EffectMath::setLed(uint16_t idx, CRGB val, byte opt) {
  if (idx < NUM_LEDS) {
    switch (opt) {
    case 0:
      leds[idx] = val;
      break;
    case 1:
      leds[idx] += val;
      break;
    case 2:
      leds[idx] -= val;
      break;
    case 3:
      leds[idx] *= val;
      break;
    case 4:
      leds[idx] /= val;
      break;
    default:
      leds[idx] = val;
      break;
    }
    return &leds[idx];
  } else {
    return &overrun;
  }
}
