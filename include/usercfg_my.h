#pragma once

//------------------------------ Основные Настройки
//#define LANG_FILE                   "text_res-RUS.h"      // для других языков при компиляции менять постфикс файла "text_res-***.h". 
	    													// Наличие нужного файла перевода смотреть в папке include в основной папке проекта 
//#define LAMP_DEBUG                                        // режим отладки, можно также включать в platformio.ini

#define RESTORE_STATE 1                                     // востанавливать состояние вкл/выкл, демо, после перезагрузки.
//#define DELAYED_EFFECTS         (1U)                        // отложенная загрузка эффектов
//#define CASHED_EFFECTS_NAMES    (1U)                        // кешировать имена эффектов, ВНИМАНИЕ!!! жрет память, использовать на свой страх и риск
#define SHOWSYSCONFIG                                       // Показывать системное меню
//#define MOOT                                                // Раскомментировать, если не нужен пункт настроек "Конфиги", 
                                                              // а так же изменение состояния произвольных пинов и загрузка конфигов в "События"
//#define DISABLE_LED_BUILTIN                                 // Отключить встроенный в плату светодиод, если нужно чтобы светил - закомментировать строку
//#define USE_FTP                                             // доступ к LittleFS по FTP (на уровне лампы, см. также EMBUI_USE_FTP - уровень фреймворка), логин/пароль: esp8266

#define USE_E131                                            // Если раскомментирован, появляется дополнительный эффект "E1.31 ресивер", 
                                                              // позволяющий транслировать на лампу еффекты из специализированных программ, типа Jinx!

//#define OPTIONS_PASSWORD F("password")			    // если задано, то настройки будут закрыты паролем


//------------------------------ Подключаемое дополнительное оборудование
#define ESP_USE_BUTTON                                      // если строка не закомментирована, должна быть подключена кнопка (иначе ESP может регистрировать "фантомные" нажатия и некорректно устанавливать яркость)
#define MIC_EFFECTS                                         // Включить использование микрофона для эффектов
#define MP3PLAYER                                           // Включить использование MP3 плеера (DF Player)
#define TM1637_CLOCK                                        // Использовать 7-ми сегментный LED-индикатор TM1637
#define DS18B20                                             // Использовать датчик температуры DS18b20 для управления вентилятором охлаждения матрицы
#define ENCODER                                             // Использовать Энкодер
#define RTC                                                 // Включить использование внешнего модуля RTC

//------------------------------ 


//------------------------------ Описание Оборудования (пины подключения, параметры и т.)


// настройка кнопки, если разрешена
#ifdef ESP_USE_BUTTON
#define BTN_PIN                 (35)                         // пин кнопки (D1), ВНИМАНИЕ! Не используйте для кнопки пины D0 и D4. На них кнопка не работает!
#define PULL_MODE               (HIGH_PULL)                   // подтяжка кнопки к нулю (для сенсорных кнопок на TP223) - LOW_PULL, подтяжка кнопки к питанию (для механических кнопок НО, на массу) - HIGH_PULL
// #define BUTTON_STEP_TIMEOUT   (75U)                         // каждые BUTTON_STEP_TIMEOUT мс будет генерироваться событие удержания кнопки (для регулировки яркости)
// #define BUTTON_CLICK_TIMEOUT  (500U)                        // максимальное время между нажатиями кнопки в мс, до достижения которого считается серия последовательных нажатий
// #define BUTTON_TIMEOUT        (500U)                        // с какого момента начинает считаться, что кнопка удерживается в мс
#endif



//------------------------------ Настройки LED Матрицы или Ленты

#define LAMP_PIN              (15)                          // пин матрицы (D3)
