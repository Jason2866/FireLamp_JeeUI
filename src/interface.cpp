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

#include "main.h"
#include "interface.h"
#include "effects.h"
#include "ui.h"
#include "extra_tasks.h"
#include "events.h"

#ifdef TM1637_CLOCK
    #include "tm.h"				// Подключаем функции
#endif
#ifdef DS18B20
#include "DS18B20.h"			// термодатчик даллас
#endif
#ifdef ENCODER
    #include "enc.h"
#endif
#ifdef RTC
    #include "rtc.h"
#endif
#include LANG_FILE                  //"text_res.h"

#ifdef DS18B20
#include "DS18B20.h"
#endif

/**
 * можно нарисовать свой собственный интефейс/обработчики с нуля, либо
 * подключить статический класс с готовыми формами для базовых системных натсроек и дополнить интерфейс.
 * необходимо помнить что существуют системные переменные конфигурации с зарезервированными именами.
 * Список имен системных переменных можно найти в файле "constants.h"
 */
#include "basicui.h"

namespace INTERFACE {
// ------------- глобальные переменные построения интерфейса
// планировщик заполнения списка
Task *optionsTask = nullptr;        // задача для отложенной генерации списка
Task *delayedOptionTask = nullptr;  // текущая отложенная задача, для сброса при повторных входах
CtrlsTask *ctrlsTask = nullptr;       // планировщик контролов

static EffectListElem *confEff = nullptr; // эффект, который сейчас конфигурируется на странице "Управление списком эффектов"
static DEV_EVENT *cur_edit_event = NULL; // текущее редактируемое событие, сбрасывается после сохранения
// ------------- глобальные переменные построения интерфейса
} // namespace INTERFACE
using namespace INTERFACE;

// функция пересоздания/отмены генерации списка эффектов
/*
void recreateoptionsTask(bool isCancelOnly=false){
    if(optionsTask)
        optionsTask->cancel();
    if(delayedOptionTask)
        delayedOptionTask->cancel(); // отмена предыдущей задачи, если была запущена
    if(!isCancelOnly){
        embui.autosave();
        optionsTask = new Task(INDEX_BUILD_DELAY * TASK_SECOND, TASK_ONCE, delayedcall_show_effects, &ts, false, nullptr, [](){
            optionsTask=nullptr;
        }, true);
        optionsTask->enableDelayed();
    }
}
*/

/**
 * @brief enumerator with a files of effect lists for webui 
 * i.e. cached json files with effect names for drop down lists 
 */
enum class lstfile_t {
    selected,
    full,
    all
};

/**
 * @brief rebuild cached json file with effects names list
 * i.e. used for sideloading in WebUI
 * @param full - rebuild full list or brief, excluding hidden effs
 * todo: implement an event queue
 */
void rebuild_effect_list_files(lstfile_t lst){
    if (delayedOptionTask)      // task is already running, skip
        return;
    // schedule a task to rebuild effects names list files
    // todo: add UI update call here
    delayedOptionTask = new Task(500, TASK_ONCE,
        [lst](){
            switch (lst){
                case lstfile_t::full :
                    build_eff_names_list_file(myLamp.effects, true);
                    break;
                case lstfile_t::all :
                    build_eff_names_list_file(myLamp.effects);
                    build_eff_names_list_file(myLamp.effects, true);
                    break;
                default :
                    build_eff_names_list_file(myLamp.effects);
            }
        },
        &ts, true, nullptr, [](){ delayedOptionTask=nullptr; }, true
    );
}


// Функция преобразования для конфига
uint64_t stoull(const String &str){
    uint64_t tmp = 0;
    LOG(printf_P, PSTR("STOULL %s \n"), str.c_str());
    for (uint8_t i = 0; i < str.length(); i++){
        if (i)
            tmp *= 10;
        tmp += (int)str[i] - 48;
    }
    return tmp;
}
// Функция преобразования для конфига
String ulltos(uint64_t longlong){
    String bfr;
    while (longlong){
        int8_t i = longlong % 10;
        bfr += String(i);
        longlong /= 10;
    }
    String tmp;
    for (int i = bfr.length()-1; i >= 0; i--)
        tmp += (String)bfr[i];
    return tmp;
}

bool check_recovery_state(bool isSet){
    //return false; // оключено до выяснения... какого-то хрена не работает :(
#ifndef ESP8266
    static RTC_DATA_ATTR uint32_t chk;
#else
    uint32_t chk;
    ESP.rtcUserMemoryRead(0,&chk,sizeof(chk));
#endif
    if(isSet && (chk&0xFF00)==0xDB00){
        uint16_t data = chk&0x00FF;
        data++;
        chk=0xDB00|data;
        LOG(printf_P, PSTR("Reboot count=%d\n"), data);
#ifdef ESP8266
        ESP.rtcUserMemoryWrite(0, &chk, sizeof(chk));
#endif
        if(data>3)
            return true; // все плохо, три перезагрузки...
        else
            return false; // все хорошо
    } else {
        chk=0xDB00; // сбрасываем цикл перезагрузок
#ifdef ESP8266
        ESP.rtcUserMemoryWrite(0, &chk, sizeof(chk));
#endif
    }
    return false;
}

void resetAutoTimers(bool isEffects=false) // сброс таймера демо и настройка автосохранений
{
    myLamp.demoTimer(T_RESET);
    if(isEffects)
        myLamp.effects.autoSaveConfig();
}

#ifdef AUX_PIN
void AUX_toggle(bool key)
{
    if (key)
    {
        digitalWrite(AUX_PIN, AUX_LEVEL);
        embui.var(FPSTR(TCONST_AUX), ("1"));
    }
    else
    {
        digitalWrite(AUX_PIN, !AUX_LEVEL);
        embui.var(FPSTR(TCONST_AUX), ("0"));
    }
}
#endif

/**
 * @brief - callback function that is triggered every EMBUI_PUB_PERIOD seconds via EmbUI scheduler
 * used to publish periodic updates to websocket clients, if any
 * 
 */
void pubCallback(Interface *interf){
    LOG(println, F("pubCallback :"));
    if (!interf) return;
    interf->json_frame_value();
    interf->value(FPSTR(TCONST_pTime), embui.timeProcessor.getFormattedShortTime(), true);

#if !defined(ESP32) || !defined(BOARD_HAS_PSRAM)    
    #ifdef PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48_SECHEAP_SHARED
        uint32_t iram;
        uint32_t dram;
        {
            HeapSelectIram ephemeral;
            iram = ESP.getFreeHeap();
        }
        {
            HeapSelectDram ephemeral;
            dram = ESP.getFreeHeap();
        }
        interf->value(FPSTR(TCONST_pMem), String(dram)+" / "+String(iram), true);
    #else
        interf->value(FPSTR(TCONST_pMem), String(myLamp.getLampState().freeHeap), true);
    #endif
#else
    if(psramFound()){
        interf->value(FPSTR(TCONST_pMem), String(ESP.getFreeHeap())+" / "+String(ESP.getFreePsram()), true);
        LOG(printf_P, PSTR("Free PSRAM: %d\n"), ESP.getFreePsram());
    } else {
        interf->value(FPSTR(TCONST_pMem), String(myLamp.getLampState().freeHeap), true);
    }
#endif
    char fuptime[16];
    uint32_t tm = embui.getUptime();
    sprintf_P(fuptime, PSTR("%u.%02u:%02u:%02u"),tm/86400,(tm/3600)%24,(tm/60)%60,tm%60);
    interf->value(FPSTR(TCONST_pUptime), String(fuptime), true);
    interf->value(FPSTR(TCONST_pFS), String(myLamp.getLampState().fsfreespace), true);
#ifdef DS18B20
    interf->value(FPSTR(TCONST_pTemp), String(getTemp())+F("°C"), true);
#endif
    int32_t rssi = myLamp.getLampState().rssi;
    interf->value(FPSTR(TCONST_pRSSI), String(constrain(map(rssi, -85, -40, 0, 100),0,100)) + F("% (") + String(rssi) + F("dBm)"), true);
    interf->json_frame_flush();
}

void block_menu(Interface *interf, JsonObject *data){
    if (!interf) return;
    // создаем меню
    interf->json_section_menu();

    interf->option(FPSTR(TCONST_effects), FPSTR(TINTF_000));   //  Эффекты
    interf->option(FPSTR(TCONST_lamptext), FPSTR(TINTF_001));   //  Вывод текста
    interf->option(FPSTR(TCONST_drawing), FPSTR(TINTF_0CE));   //  Рисование
#ifdef USE_STREAMING
    interf->option(FPSTR(TCONST_streaming), FPSTR(TINTF_0E2));   //  Трансляция
#endif
    interf->option(FPSTR(TCONST_show_event), FPSTR(TINTF_011));   //  События
    interf->option(FPSTR(TCONST_settings), FPSTR(TINTF_002));   //  настройки

    interf->json_section_end();
}

/**
 * Страница с контролами параметров эфеекта
 * выводится при в разделе "Управление списком эффектов"
 */
void block_effects_config_param(Interface *interf, JsonObject *data){
    if (!interf || !confEff) return;

    String tmpName, tmpSoundfile;
    myLamp.effects.loadeffname(tmpName,confEff->eff_nb);
    myLamp.effects.loadsoundfile(tmpSoundfile,confEff->eff_nb);
    interf->json_section_begin(FPSTR(TCONST_set_effect));
    interf->text(FPSTR(TCONST_effname), tmpName, FPSTR(TINTF_089), false);
#ifdef MP3PLAYER
    interf->text(FPSTR(TCONST_soundfile), tmpSoundfile, FPSTR(TINTF_0B2), false);
#endif
    interf->checkbox(FPSTR(TCONST_eff_sel), confEff->canBeSelected()? "1" : "0", FPSTR(TINTF_003), false);
    interf->checkbox(FPSTR(TCONST_eff_fav), confEff->isFavorite()? "1" : "0", FPSTR(TINTF_004), false);

    interf->spacer();

    interf->select(FPSTR(TCONST_effSort), FPSTR(TINTF_040));
    interf->option(String(SORT_TYPE::ST_BASE), FPSTR(TINTF_041));
    interf->option(String(SORT_TYPE::ST_END), FPSTR(TINTF_042));
    interf->option(String(SORT_TYPE::ST_IDX), FPSTR(TINTF_043));
    interf->option(String(SORT_TYPE::ST_AB), FPSTR(TINTF_085));
    interf->option(String(SORT_TYPE::ST_AB2), FPSTR(TINTF_08A));
#ifdef MIC_EFFECTS
    interf->option(String(SORT_TYPE::ST_MIC), FPSTR(TINTF_08D));  // эффекты с микрофоном
#endif
    interf->json_section_end();
    //interf->checkbox(FPSTR(TCONST_numInList), myLamp.getLampSettings().numInList ? "1" : "0", FPSTR(TINTF_090), false); // нумерация в списке эффектов
#ifdef MIC_EFFECTS
    //interf->checkbox(FPSTR(TCONST_effHasMic), myLamp.getLampSettings().effHasMic ? "1" : "0", FPSTR(TINTF_091), false); // значек микрофона в списке эффектов
#endif

    interf->button_submit(FPSTR(TCONST_set_effect), FPSTR(TINTF_008), FPSTR(P_GRAY));
    interf->button_submit_value(FPSTR(TCONST_set_effect), FPSTR(TCONST_copy), FPSTR(TINTF_005));
    //if (confEff->eff_nb&0xFF00) { // пока удаление только для копий, но в теории можно удалять что угодно
        // interf->button_submit_value(FPSTR(TCONST_set_effect), FPSTR(TCONST_del_), FPSTR(TINTF_006), FPSTR(P_RED));
    //}

    interf->json_section_line();
    interf->button_submit_value(FPSTR(TCONST_set_effect), FPSTR(TCONST_delfromlist), FPSTR(TINTF_0B5), FPSTR(TCONST_orange));
    interf->button_submit_value(FPSTR(TCONST_set_effect), FPSTR(TCONST_delall), FPSTR(TINTF_0B4), FPSTR(P_RED));
    interf->json_section_end();

    interf->button_submit_value(FPSTR(TCONST_set_effect), FPSTR(TCONST_makeidx), FPSTR(TINTF_007), FPSTR(TCONST_black));

    interf->json_section_end();
}

/**
 * Сформировать и вывести страницу с контролами для настроек параметров эффектов
 * здесь выводится ПОЛНЫЙ сипсок эффектов
 */
void show_effects_config_param(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_effects_config_param(interf, data);
    interf->json_frame_flush();
}

/**
 * обработчик установок эффекта
 */
void set_effects_config_param(Interface *interf, JsonObject *data){
    if (!confEff || !data) return;
    EffectListElem *effect = confEff;
    
    //bool isNumInList =  (*data)[FPSTR(TCONST_numInList)] == "1";
#ifdef MIC_EFFECTS
    bool isEffHasMic =  (*data)[FPSTR(TCONST_effHasMic)] == "1";
    myLamp.setEffHasMic(isEffHasMic);
#endif
    SORT_TYPE st = (*data)[FPSTR(TCONST_effSort)].as<SORT_TYPE>();

    if(myLamp.getLampState().isInitCompleted){
        LOG(printf_P, PSTR("Settings: call removeLists()\n"));
        bool isRecreate = false;
        //isRecreate = myLamp.getLampSettings().numInList!=isNumInList;
#ifdef MIC_EFFECTS
        //isRecreate = (myLamp.getLampSettings().effHasMic!=isEffHasMic) || isRecreate;
#endif
        isRecreate = (myLamp.effects.getEffSortType()!=st) || isRecreate;

        if(isRecreate){
            myLamp.effects.setEffSortType(st);
            //myLamp.setNumInList(isNumInList);
            //myLamp.effects.removeLists();
            rebuild_effect_list_files(lstfile_t::all);
        }
    }
    //myLamp.setNumInList(isNumInList);

    SETPARAM(FPSTR(TCONST_effSort), myLamp.effects.setEffSortType(st));
    save_lamp_flags();
    
    String act = (*data)[FPSTR(TCONST_set_effect)];
    // action is to "copy" effect
    if (act == FPSTR(TCONST_copy)) {
        Task *_t = new Task(
            300,
            TASK_ONCE, [effect](){
                                myLamp.effects.copyEffect(effect); // копируем текущий
                                myLamp.effects.makeIndexFileFromList(); // создаем индекс по списку и на выход
                                Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 1024) : nullptr;
                                section_main_frame(interf, nullptr);
                                delete interf;
                                rebuild_effect_list_files(lstfile_t::all);
            },
            &ts, false, nullptr, nullptr, true);
        _t->enableDelayed();
        return;
    //} else if (act == FPSTR(TCONST_del_)) {
    } else if (act == FPSTR(TCONST_delfromlist) || act == FPSTR(TCONST_delall)) {
        uint16_t tmpEffnb = effect->eff_nb;
        bool isCfgRemove = (act == FPSTR(TCONST_delall));
        LOG(printf_P,PSTR("confEff->eff_nb=%d\n"), tmpEffnb);
        if(tmpEffnb==myLamp.effects.getCurrent()){
            myLamp.effects.directMoveBy(EFF_ENUM::EFF_NONE);
            remote_action(RA_EFF_NEXT, NULL);
        }
        String tmpStr=F("- ");
        tmpStr+=String(tmpEffnb);
        myLamp.sendString(tmpStr.c_str(), CRGB::Red);
        confEff = myLamp.effects.getEffect(EFF_ENUM::EFF_NONE);
        if(isCfgRemove){
            Task *_t = new Task(
                300,
                TASK_ONCE, [effect](){
                                    myLamp.effects.deleteEffect(effect, true); // удаляем эффект из ФС
                                    myLamp.effects.makeIndexFileFromFS(); // создаем индекс по файлам ФС и на выход
                                    Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 1024) : nullptr;
                                    section_main_frame(interf, nullptr);
                                    delete interf;
                                    rebuild_effect_list_files(lstfile_t::all);
                                    },
                &ts, false, nullptr, nullptr, true);
            _t->enableDelayed();
        } else {
            Task *_t = new Task(
                300,
                TASK_ONCE, [effect](){
                                    myLamp.effects.deleteEffect(effect, false); // удаляем эффект из списка
                                    myLamp.effects.makeIndexFileFromList(); // создаем индекс по текущему списку и на выход
                                    Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 1024) : nullptr;
                                    section_main_frame(interf, nullptr);
                                    delete interf;
                                    rebuild_effect_list_files(lstfile_t::all); },
                &ts, false, nullptr, nullptr, true);
            _t->enableDelayed();
        }
        return;
    } else if (act == FPSTR(TCONST_makeidx)) {
        Task *_t = new Task(
            300,
            TASK_ONCE, [](){
                                myLamp.effects.makeIndexFileFromFS(); // создаем индекс по файлам ФС и на выход
                                Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 1024) : nullptr;
                                section_main_frame(interf, nullptr);
                                delete interf;
                                rebuild_effect_list_files(lstfile_t::all); },
            &ts, false, nullptr, nullptr, true);
        _t->enableDelayed();
        return;
    } else {
        effect->canBeSelected((*data)[FPSTR(TCONST_eff_sel)] == "1");
        effect->isFavorite((*data)[FPSTR(TCONST_eff_fav)] == "1");
        myLamp.effects.setSoundfile((*data)[FPSTR(TCONST_soundfile)], effect);
// #ifdef CASHED_EFFECTS_NAMES
//         effect->setName((*data)[FPSTR(TCONST_effname)]);
// #endif
        myLamp.effects.setEffectName((*data)[FPSTR(TCONST_effname)], effect);
    }

    resetAutoTimers();
    myLamp.effects.makeIndexFileFromList(); // обновить индексный файл после возможных изменений
    section_main_frame(interf, nullptr);
}


/**
 * блок формирования страницы с контролами для настроек параметров эффектов
 * здесь выводится ПОЛНЫЙ сипсок эффектов
 */
void block_effects_config(Interface *interf, JsonObject *data){
    if (!interf) return;

    interf->json_section_main(FPSTR(TCONST_effects_config), FPSTR(TINTF_009));
    confEff = myLamp.effects.getSelectedListElement();
    //interf->select(FPSTR(TCONST_effListConf), String((int)confEff->eff_nb), String(FPSTR(TINTF_00A)), true);

    if(LittleFS.exists(FPSTR(TCONST_eff_fulllist_json))){
        // формируем и отправляем кадр с запросом подгрузки внешнего ресурса
        interf->json_frame_custom(FPSTR(T_XLOAD));
        interf->json_section_content();
        interf->select(FPSTR(TCONST_effListConf), String((int)confEff->eff_nb), String(FPSTR(TINTF_00A)),
                        true,   // direct
                        false,  // skiplabel
                        FPSTR(TCONST_eff_fulllist_json)
                );
        interf->json_section_end();
        // generate block with effect settings controls
        block_effects_config_param(interf, nullptr);
        interf->spacer();
        interf->button(FPSTR(TCONST_effects), FPSTR(TINTF_00B));
        interf->json_section_end();
        return;
    }

    interf->constant(F("cmt"), F("Rebuilding effects list, pls wait..."));
    rebuild_effect_list_files(lstfile_t::full);
}

// Построение выпадающего списка эффектов для вебморды
/*
void delayedcall_show_effects(){

    LOG(println, F("=== GENERATE EffLIst for GUI (fslowlist.json) ==="));
    uint16_t effnb = confEff ? (int)confEff->eff_nb : myLamp.effects.getSelected(); // если confEff не NULL, то мы в конфирурировании, иначе в основном режиме

    if(delayedOptionTask)
        delayedOptionTask->cancel(); // отмена предыдущей задачи, если была запущена

    File *slowlist = nullptr;
    if(!LittleFS.exists(confEff ? FPSTR(TCONST_fslowlist) : FPSTR(TCONST_fslowlist))){
        slowlist = new fs::File;
        *slowlist = LittleFS.open(FPSTR(TCONST__tmplist_tmp), "w");
    } else {
        // формируем и отправляем кадр с запросом подгрузки внешнего ресурса
        LOG(println, F("fslowlist.json exist, sending xload frame"));
        Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 512) : nullptr;
        interf->json_frame_custom(FPSTR(T_XLOAD));
        interf->json_section_content();
        interf->select(confEff?FPSTR(TCONST_effListConf):FPSTR(TCONST_effListMain), String(effnb), String(FPSTR(TINTF_00A)), true, true, String(FPSTR(TCONST_fslowlist)));
        interf->json_section_end();
        interf->json_frame_flush();
        delete interf;
        interf = nullptr;
        return;
    }

    EffectListElem **peff = new (EffectListElem *); // выделяем память под укзатель на указатель
    *peff = nullptr; // чистим содержимое

    delayedOptionTask = new Task(300, TASK_FOREVER,
        // loop
        [peff, slowlist](){
            EffectListElem *&eff = *peff; // здесь ссылка на указатель, т.к. нам нужно менять значение :)
            //LOG(print,(uint32_t)peff); LOG(print," "); LOG(println,(uint32_t)*peff);
            bool firsttime = false;
            if(eff == nullptr && slowlist){
                slowlist->print('[');
                firsttime = true;
            }

            String effname((char *)0);
            ////MIC_SYMB;
            size_t cnt = 5; // генерим по 5 элементов
            bool numList = myLamp.getLampSettings().numInList;
            while (--cnt) {
                eff = myLamp.effects.getNextEffect(eff);
                if (eff != nullptr){
                    myLamp.effects.loadeffname(effname, eff->eff_nb);
                    LOG(println, effname);
                    if(confEff || eff->eff_nb || (!eff->eff_nb && eff->canBeSelected())){ // если в конфигурировании или не 0 или 0 эффект и он может быть выбран
                        String name =                             (!confEff ? EFF_NUMBER(eff->eff_nb) : String(eff->eff_nb) + (eff->eff_nb>255 ? String(F(" (")) + String(eff->eff_nb&0xFF) + String(F(")")) : String("")) + String(F(". "))) +
                            effname +
                            MIC_SYMBOL(eff->eff_nb);
                        if(slowlist){
                            slowlist->printf_P(PSTR("%s{\"label\":\"%s\",\"value\":\"%s\"}"), firsttime?"":",", name.c_str(), String(eff->eff_nb).c_str());
                            firsttime = false;
                        }
                    }
                } else {
                    // тут перебрали все элементы и готовы к завершению
                    EffectListElem * first_eff=myLamp.effects.getFirstEffect();
                    if(!confEff && first_eff && !first_eff->canBeSelected()){ // если мы не в конфигурировании эффектов и первый не может быть выбран, то пустой будет добавлен в конец
                        if(slowlist){
                            slowlist->printf_P(PSTR(",{\"label\":\"\",\"value\":\"0\"}"));
                        }
                    }
                    if(slowlist){
                        slowlist->print(']');
                        slowlist->close();
#ifdef ESP32
                        delay(50);
#endif
                        LittleFS.rename(FPSTR(TCONST__tmplist_tmp), FPSTR(TCONST_fslowlist));
                        delete (fs::FS *)slowlist;
                    }

                    Task *_t = &ts.currentTask();
                    _t->disable();
                    return;
                }
            }
        },
        &ts, true,
        nullptr,
        //onDisable
        [peff](){
            LOG(println, F("=== GENERATE EffLIst for GUI completed ==="));
            // формируем и отправляем кадр с запросом подгрузки внешнего ресурса
            Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 512) : nullptr;
            uint16_t effnb = confEff?(int)confEff->eff_nb:myLamp.effects.getSelected(); // если confEff не NULL, то мы в конфирурировании, иначе в основном режиме
            interf->json_frame_custom(FPSTR(T_XLOAD));
            interf->json_section_content();
            interf->select(confEff?FPSTR(TCONST_effListConf):FPSTR(TCONST_effListMain), String(effnb), String(FPSTR(TINTF_00A)), true, true, String(confEff?FPSTR(TCONST_fslowlist):FPSTR(TCONST_fslowlist)));
            interf->json_section_end();
            interf->json_frame_flush();
            delete interf;
            delete peff; // освободить указатель на указатель
            delayedOptionTask = nullptr;
        }, true
    );
}
*/

void show_effects_config(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_effects_config(interf, data);
    interf->json_frame_flush();
}

void set_effects_config_list(Interface *interf, JsonObject *data){
    if (!data) return;
    uint16_t num = (*data)[FPSTR(TCONST_effListConf)].as<uint16_t>();

    if(confEff){ // если переключаемся, то сохраняем предыдущие признаки в эффект до переключения
        LOG(printf_P, PSTR("eff_sel: %d eff_fav : %d\n"), (*data)[FPSTR(TCONST_eff_sel)].as<bool>(),(*data)[FPSTR(TCONST_eff_fav)].as<bool>());
    }

    confEff = myLamp.effects.getEffect(num);
    show_effects_config_param(interf, data);
    resetAutoTimers();
}

#ifdef EMBUI_USE_MQTT
void mqtt_publish_selected_effect_config_json(){
  if (!embui.isMQTTconected()) return;
  embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_eff_config), myLamp.effects.getSerializedEffConfig(myLamp.effects.getSelected(), myLamp.getNormalizedLampBrightness()), true);
}
#endif

void block_effects_param(Interface *interf, JsonObject *data){
    // if no mqtt or ws clients, just quit
    if (!embui.isMQTTconected() && !embui.ws.count()) return;

    // there could be no ws clients connected
    if(interf) interf->json_section_begin(FPSTR(TCONST_effects_param));

    LList<std::shared_ptr<UIControl>> &controls = myLamp.effects.getControls();
    uint8_t ctrlCaseType; // тип контрола, старшие 4 бита соответствуют CONTROL_CASE, младшие 4 - CONTROL_TYPE
#ifdef MIC_EFFECTS
   bool isMicOn = myLamp.isMicOnOff();
   LOG(printf_P,PSTR("Make UI for %d controls\n"), controls.size());
    for(unsigned i=0; i<controls.size();i++)
        if(controls[i]->getId()==7 && controls[i]->getName().startsWith(FPSTR(TINTF_020)))
            isMicOn = isMicOn && controls[i]->getVal().toInt();
#endif
    LOG(printf_P, PSTR("block_effects_param() got %u ctrls\n"), controls.size());
    for (const auto &ctrl : controls){
        ctrlCaseType = ctrl->getType();
        switch(ctrlCaseType>>4){
            case CONTROL_CASE::HIDE :
                continue;
                break;
            case CONTROL_CASE::ISMICON :
#ifdef MIC_EFFECTS
                //if(!myLamp.isMicOnOff()) continue;
                if(!isMicOn && (!myLamp.isMicOnOff() || !(ctrl->getId()==7 && ctrl->getName().startsWith(FPSTR(TINTF_020))==1) )) continue;
#else
                continue;
#endif          
                break;
            case CONTROL_CASE::ISMICOFF :
#ifdef MIC_EFFECTS
                //if(myLamp.isMicOnOff()) continue;
                if(isMicOn && (myLamp.isMicOnOff() || !(ctrl->getId()==7 && ctrl->getName().startsWith(FPSTR(TINTF_020))==1) )) continue;
#else
                continue;
#endif   
                break;
            default: break;
        }

        bool isRandDemo = (myLamp.getLampSettings().dRand && myLamp.getMode()==LAMPMODE::MODE_DEMO);
        String ctrlId(FPSTR(TCONST_dynCtrl));
        ctrlId += ctrl->getId();
        String ctrlName = ctrl->getId() ? ctrl->getName() : (myLamp.IsGlobalBrightness() ? FPSTR(TINTF_00C) : FPSTR(TINTF_00D));

        switch(ctrlCaseType&0x0F){
            case CONTROL_TYPE::RANGE :
                {
                    if(isRandDemo && ctrl->getId()>0 && !(ctrl->getId()==7 && ctrl->getName().startsWith(FPSTR(TINTF_020))==1))
                        ctrlName=String(FPSTR(TINTF_0C9))+ctrlName;
                    int value = ctrl->getId() ? ctrl->getVal().toInt() : myLamp.getNormalizedLampBrightness();
                    if(interf) interf->range(
                        ctrlId
                        ,String(value)
                        ,ctrl->getMin()
                        ,ctrl->getMax()
                        ,ctrl->getStep()
                        , ctrlName
                        , true);
#ifdef EMBUI_USE_MQTT
                    embui.publish(String(FPSTR(TCONST_embui_pub_)) + ctrlId, String(value), true);
#endif
                }
                break;
            case CONTROL_TYPE::EDIT :
                {
                    String ctrlName = ctrl->getName();
                    if(isRandDemo && ctrl->getId()>0 && !(ctrl->getId()==7 && ctrl->getName().startsWith(FPSTR(TINTF_020))==1))
                        ctrlName=String(FPSTR(TINTF_0C9))+ctrlName;
                    
                    if(interf) interf->text(String(FPSTR(TCONST_dynCtrl)) + String(ctrl->getId())
                    , ctrl->getVal()
                    , ctrlName
                    , true
                    );
#ifdef EMBUI_USE_MQTT
                    embui.publish(String(FPSTR(TCONST_embui_pub_)) + ctrlId, ctrl->getVal(), true);
#endif
                    break;
                }
            case CONTROL_TYPE::CHECKBOX :
                {
                    String ctrlName = ctrl->getName();
                    if(isRandDemo && ctrl->getId()>0 && !(ctrl->getId()==7 && ctrl->getName().startsWith(FPSTR(TINTF_020))==1))
                        ctrlName=String(FPSTR(TINTF_0C9))+ctrlName;

                    if(interf) interf->checkbox(String(FPSTR(TCONST_dynCtrl)) + String(ctrl->getId())
                    , ctrl->getVal()
                    , ctrlName
                    , true
                    );
#ifdef EMBUI_USE_MQTT
                    embui.publish(String(FPSTR(TCONST_embui_pub_)) + ctrlId, ctrl->getVal(), true);
#endif
                    break;
                }
            default:
#ifdef EMBUI_USE_MQTT
                    embui.publish(String(FPSTR(TCONST_embui_pub_)) + ctrlId, ctrl->getVal(), true);
#endif
                break;
        }
    }
#ifdef EMBUI_USE_MQTT
    // publish full effect config via mqtt
    mqtt_publish_selected_effect_config_json();
#endif
    if(interf) interf->json_section_end();
    LOG(println, F("eof block_effects_param()"));
}

void show_effects_param(Interface *interf, JsonObject *data){
    LOG(println, F("show_effects_param()"));
    if(interf) interf->json_frame_interface();
    block_effects_param(interf, data);
    if(interf) interf->json_frame_flush();
}

void set_effects_list(Interface *interf, JsonObject *data){
    LOG(println, "set_effects_list()");
    if (!data) return;
    uint16_t num = (*data)[FPSTR(TCONST_effListMain)].as<uint16_t>();
    uint16_t nextEff = myLamp.effects.getSelected();        // get next eff with preloaded controls
    EffectListElem *eff = myLamp.effects.getEffect(num);
    if (!eff) return;

    if(myLamp.getMode()==LAMPMODE::MODE_WHITELAMP && num!=1){
        myLamp.startNormalMode(true);
        DynamicJsonDocument doc(512);
        JsonObject obj = doc.to<JsonObject>();
        CALL_INTF(FPSTR(TCONST_ONflag), myLamp.isLampOn() ? "1" : "0", set_onflag);
        return;
    }

    // сбросить флаг рандомного демо
    myLamp.setDRand(myLamp.getLampSettings().dRand);

    // if this request is for some other effect than preloaded seletedEffect, than need to switch effect
    if (eff->eff_nb != nextEff) {
        LOG(printf_P, PSTR("UI EFF switch to:%d, selected:%d, isOn:%d, mode:%d\n"), eff->eff_nb, nextEff, myLamp.isLampOn(), myLamp.getMode());
        if (myLamp.isLampOn()) {
            myLamp.switcheffect(SW_SPECIFIC, myLamp.getFaderFlag(), eff->eff_nb);
        } else {
            myLamp.effects.directMoveBy(eff->eff_nb); // переходим прямо на выбранный эффект 
        }
        if(myLamp.getMode()==LAMPMODE::MODE_NORMAL)
            embui.var(FPSTR(TCONST_effListMain), (*data)[FPSTR(TCONST_effListMain)]);
        resetAutoTimers();
    }

    // publish effect's controls to WebUI and MQTT
    show_effects_param(interf, data);
#ifdef EMBUI_USE_MQTT
    if (embui.isMQTTconected()){
        embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(CMD_EFFECT), String(eff->eff_nb), true);
        // not needed, already done in show_effects_param(), also same as mqtt_publish_selected_effect_config_json();
        //embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_eff_config), myLamp.effects.getSerializedEffConfig(String(eff->eff_nb).toInt(), myLamp.getNormalizedLampBrightness()), true);
    }
#endif
}

// этот метод меняет контролы БЕЗ синхронизации со внешними системами
void direct_set_effects_dynCtrl(JsonObject *data){
    if (!data) return;

    String ctrlName;
    LList<std::shared_ptr<UIControl>>&controls = myLamp.effects.getControls();
    for(unsigned i=0; i<controls.size();i++){
        ctrlName = String(FPSTR(TCONST_dynCtrl))+String(controls[i]->getId());
        if((*data).containsKey(ctrlName)){
            if(!i){ // яркость???
                byte bright = (*data)[ctrlName];
                if (myLamp.getNormalizedLampBrightness() != bright) {
                    myLamp.setLampBrightness(bright);
                    if(myLamp.isLampOn())
                        myLamp.setBrightness(myLamp.getNormalizedLampBrightness(), !((*data)[FPSTR(TCONST_nofade)]));
                    if (myLamp.IsGlobalBrightness()) {
                        embui.var(FPSTR(TCONST_GlobBRI), (*data)[ctrlName]);
                    } else
                        resetAutoTimers(true);
                } else {
                    myLamp.setLampBrightness(bright);
                    if (!myLamp.IsGlobalBrightness())
                        resetAutoTimers(true);
                }
            } else {
                controls[i]->setVal((*data)[ctrlName]); // для всех остальных
                resetAutoTimers(true);
            }
            if(myLamp.effects.worker) // && myLamp.effects.getEn()
                myLamp.effects.worker->setDynCtrl(controls[i].get());
            break;
        }
    }
}

void set_effects_dynCtrl(Interface *interf, JsonObject *data){
    if (!data) return;

    // static unsigned long timeout = 0;
    // if(timeout+110UL>millis()) return;
    // timeout = millis();

    // попытка повышения стабильности, отдаем управление браузеру как можно быстрее...
    if((*data).containsKey(FPSTR(TCONST_force)))
        direct_set_effects_dynCtrl(data);

    if(ctrlsTask){
        if(ctrlsTask->replaceIfSame(data)){
            ctrlsTask->restartDelayed();
            return;
        }
    }
   
    //LOG(println, "Delaying dynctrl");

    ctrlsTask = new CtrlsTask(data, 300, TASK_ONCE,
        [](){
            CtrlsTask *task = (CtrlsTask *)ts.getCurrentTask();
            JsonObject storage = task->getData();
            JsonObject *data = &storage; // task->getData();
            if(!data) return;

            LOG(println, "publishing & sending dynctrl...");
            #ifdef LAMP_DEBUG
            String tmp; serializeJson(*data,tmp);LOG(println, tmp);
            #endif

            direct_set_effects_dynCtrl(data);
#ifdef EMBUI_USE_MQTT
            mqtt_publish_selected_effect_config_json();
            for (JsonPair kv : *data){
                embui.publish(String(FPSTR(TCONST_embui_pub_)) + String(kv.key().c_str()), kv.value().as<String>(), true);
            }
#endif
            // отправка данных в WebUI
            {
                bool isLocalMic = false;
                Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 512) : nullptr;
                if (interf) {
                    interf->json_frame_value();
                    for (JsonPair kv : *data){
                        interf->value(kv.key().c_str(), kv.value(), false);
                        if(String(kv.key().c_str())==String(F("dynCtrl7"))) // будем считать что это микрофон дергается, тогда обновим все контролы
                            isLocalMic = true;
                    }
                    interf->json_frame_flush();
                    delete interf;
                }
                if(isLocalMic){
                    Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 1024) : nullptr;
                    show_effects_param(interf, data);
                    delete interf;
                }
            }
            if(task==ctrlsTask)
                ctrlsTask = nullptr;

            delete task;
        },
        &ts,
        false
    );
    ctrlsTask->restartDelayed();
}

/**
 * Блок с наборами основных переключателей лампы
 * вкл/выкл, демо, кнопка и т.п.
 */
void block_main_flags(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_begin(FPSTR(TCONST_flags));
    interf->json_section_line("");
    interf->checkbox(FPSTR(TCONST_ONflag), String(myLamp.isLampOn()), FPSTR(TINTF_00E), true);
    interf->checkbox(FPSTR(TCONST_Demo), String(myLamp.getMode() == LAMPMODE::MODE_DEMO), FPSTR(TINTF_00F), true);
    interf->checkbox(FPSTR(TCONST_GBR), String(myLamp.IsGlobalBrightness()), FPSTR(TINTF_010), true);
#ifndef MOOT
    interf->checkbox(FPSTR(TCONST_Events), String(myLamp.IsEventsHandled()), FPSTR(TINTF_011), true);
    interf->checkbox(FPSTR(TCONST_drawbuff), String(myLamp.isDrawOn()), FPSTR(TINTF_0CE), true);
#endif
#ifdef MIC_EFFECTS
    interf->checkbox(FPSTR(TCONST_Mic), myLamp.isMicOnOff()? "1" : "0", FPSTR(TINTF_012), true);
#endif
#ifdef AUX_PIN
    interf->checkbox(FPSTR(TCONST_AUX), FPSTR(TCONST_AUX), true);
#endif
#ifdef ESP_USE_BUTTON
    interf->checkbox(FPSTR(TCONST_Btn), myButtons->isButtonOn()? "1" : "0", FPSTR(TINTF_013), true);
#endif
#ifdef MP3PLAYER
    interf->checkbox(FPSTR(TCONST_isOnMP3), myLamp.isONMP3()? "1" : "0", FPSTR(TINTF_099), true);
#endif
#ifdef LAMP_DEBUG
    interf->checkbox(FPSTR(TCONST_debug), myLamp.isDebugOn()? "1" : "0", FPSTR(TINTF_08E), true);
#endif
    interf->json_section_end();
#ifdef MP3PLAYER
    interf->json_section_line(F("line124")); // спец. имя - разбирается внутри html
    if(mp3->isMP3Mode()){
        interf->button(FPSTR(CMD_MP3_PREV), FPSTR(TINTF_0BD), FPSTR(P_GRAY));
        interf->button(FPSTR(CMD_MP3_NEXT), FPSTR(TINTF_0BE), FPSTR(P_GRAY));
        interf->button(FPSTR(TCONST_mp3_p5), FPSTR(TINTF_0BF), FPSTR(P_GRAY));
        interf->button(FPSTR(TCONST_mp3_n5), FPSTR(TINTF_0C0), FPSTR(P_GRAY));
    }
    //interf->button("time", FPSTR(TINTF_016), FPSTR(TCONST__5f9ea0));    
    interf->json_section_end();
    interf->range(String(FPSTR(TCONST_mp3volume)), String(1), String(30), String(1), String(FPSTR(TINTF_09B)), true);
#endif
    interf->json_section_end();
}

/**
 * Формирование и вывод интерфейса с основными переключателями
 * вкл/выкл, демо, кнопка и т.п.
 */
void show_main_flags(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_main_flags(interf, data);
    interf->spacer();
    interf->button(FPSTR(TCONST_effects), FPSTR(TINTF_00B));
    interf->json_frame_flush();
}

/* Страница "Эффекты" (начальная страница)
    здесь выводится список эффектов который не содержит "скрытые" элементы
*/
void block_effects_main(Interface *interf, JsonObject *data, bool fast=true){
    confEff = NULL; // т.к. не в конфигурировании, то сбросить данное значение
    if (!interf) return;

    interf->json_section_main(FPSTR(TCONST_effects), FPSTR(TINTF_000));

    interf->json_section_line(FPSTR(TCONST_flags));
    interf->checkbox(FPSTR(TCONST_ONflag), myLamp.isLampOn()? "1" : "0", FPSTR(TINTF_00E), true);
    interf->button(FPSTR(TCONST_show_flags), FPSTR(TINTF_014));
    interf->json_section_end();

    // 'next', 'prev' buttons << >>
    interf->json_section_line(FPSTR(TCONST_mode));
    interf->button(FPSTR(TCONST_eff_prev), FPSTR(TINTF_015), FPSTR(TCONST__708090));
    interf->button(FPSTR(TCONST_eff_next), FPSTR(TINTF_016), FPSTR(TCONST__5f9ea0));
    interf->json_section_end();


    if(LittleFS.exists(FPSTR(TCONST_eff_list_json))){
        // формируем и отправляем кадр с запросом подгрузки внешнего ресурса
        interf->json_frame_custom(FPSTR(T_XLOAD));
        interf->json_section_content();
        // side load drop-down list from /eff_list.json file
        interf->select(FPSTR(TCONST_effListMain), String(myLamp.effects.getSelected()), String(FPSTR(TINTF_00A)), true, false, FPSTR(TCONST_eff_list_json));
        interf->json_section_end();
        block_effects_param(interf, data);
        interf->button(FPSTR(TCONST_effects_config), FPSTR(TINTF_009));
        interf->json_section_end();
    } else {
        interf->constant(F("cmt"), F("Rebuilding effects list, pls wait..."));
        rebuild_effect_list_files(lstfile_t::selected);
    }

    interf->json_section_end();
}

void set_eff_prev(Interface *interf, JsonObject *data){
    remote_action(RA::RA_EFF_PREV, NULL);
}

void set_eff_next(Interface *interf, JsonObject *data){
    remote_action(RA::RA_EFF_NEXT, NULL);
}

/**
 * Обработка вкл/выкл лампы
 */
void set_onflag(Interface *interf, JsonObject *data){
    if (!data) return;

    bool newpower = TOGLE_STATE((*data)[FPSTR(TCONST_ONflag)], myLamp.isLampOn());
    if (newpower != myLamp.isLampOn()) {
        if (newpower) {
            // включаем через switcheffect, т.к. простого isOn недостаточно чтобы запустить фейдер и поменять яркость (при необходимости)
            myLamp.switcheffect(SW_SPECIFIC, myLamp.getFaderFlag(), myLamp.effects.getEn());
            myLamp.changePower(newpower);
#ifdef MP3PLAYER
            if(myLamp.getLampSettings().isOnMP3)
                mp3->setIsOn(true);
#endif
#if !defined(ESP_USE_BUTTON) && !defined(ENCODER)
            if(millis()<20000){        // 10 секунд мало, как показала практика, ставим 20
                Task *_t = new Task(
                    INDEX_BUILD_DELAY * TASK_SECOND,
                    TASK_ONCE, [](){ myLamp.sendString(WiFi.localIP().toString().c_str(), CRGB::White); },
                    &ts, false, nullptr, nullptr, true);
                _t->enableDelayed();
            }
#endif
#ifdef EMBUI_USE_MQTT
            embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_on), "1", true);
            embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_mode), String(myLamp.getMode()), true);
            embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST__demo), String(myLamp.getMode()==LAMPMODE::MODE_DEMO?"1":"0"), true);
#endif
        } else {
            resetAutoTimers(); // автосохранение конфига будет отсчитываться от этого момента
            //myLamp.changePower(newpower);
            Task *_t = new Task(300, TASK_ONCE,
                                [](){ // при выключении бывает эксепшен, видимо это слишком длительная операция, разносим во времени и отдаем управление
                                myLamp.changePower(false);
                #ifdef MP3PLAYER
                                mp3->setIsOn(false);
                #endif
                #ifdef RESTORE_STATE
                                save_lamp_flags(); // злобный баг, забыть передернуть флаги здесь)))), не вздумать убрать!!! Отлавливал его кучу времени
                #endif
                #ifdef EMBUI_USE_MQTT
                                embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_on), "0", true);
                                embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_mode), String(myLamp.getMode()), true);
                                embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST__demo), String(myLamp.getMode()==LAMPMODE::MODE_DEMO?"1":"0"), true);
                #endif
                                },
                                &ts, false, nullptr, nullptr, true);
            _t->enableDelayed();
        }
    }
#ifdef RESTORE_STATE
    save_lamp_flags();
#endif
}

void set_demoflag(Interface *interf, JsonObject *data){
    if (!data) return;
    resetAutoTimers();
    // Специально не сохраняем, считаю что демо при старте не должно запускаться
    bool newdemo = TOGLE_STATE((*data)[FPSTR(TCONST_Demo)], (myLamp.getMode() == LAMPMODE::MODE_DEMO));
    switch (myLamp.getMode()) {
        case LAMPMODE::MODE_OTA:
        case LAMPMODE::MODE_ALARMCLOCK:
        case LAMPMODE::MODE_NORMAL:
        case LAMPMODE::MODE_RGBLAMP:
            if(newdemo)
                myLamp.startDemoMode(embui.param(FPSTR(TCONST_DTimer)).toInt());
            break;
        case LAMPMODE::MODE_DEMO:
        case LAMPMODE::MODE_WHITELAMP:
            if(!newdemo)
                myLamp.startNormalMode();
            break;
        default:;
    }
#ifdef RESTORE_STATE
    embui.var(FPSTR(TCONST_Demo), (*data)[FPSTR(TCONST_Demo)]);
#endif
    myLamp.setDRand(myLamp.getLampSettings().dRand);
#ifdef EMBUI_USE_MQTT
    embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_mode), String(myLamp.getMode()), true);
    embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST__demo), String(myLamp.getMode()==LAMPMODE::MODE_DEMO?"1":"0"), true);
#endif
}

#ifdef AUX_PIN
void set_auxflag(Interface *interf, JsonObject *data){
    if (!data) return;
    if (((*data)[FPSTR(TCONST_AUX)] == "1") != (digitalRead(AUX_PIN) == AUX_LEVEL ? true : false)) {
        AUX_toggle(!(digitalRead(AUX_PIN) == AUX_LEVEL ? true : false));
    }
}
#endif

void set_gbrflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setIsGlobalBrightness((*data)[FPSTR(TCONST_GBR)] == "1");
#ifdef EMBUI_USE_MQTT
    embui.publish(String(FPSTR(TCONST_embui_pub_)) + String(FPSTR(TCONST_gbright)), String(myLamp.IsGlobalBrightness() ? "1" : "0"), true);
#endif
    save_lamp_flags();
    if (myLamp.isLampOn()) {
        myLamp.setBrightness(myLamp.getNormalizedLampBrightness());
    }
    show_effects_param(interf, data);
}

void block_lamp_config(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_hidden(FPSTR(TCONST_lamp_config), FPSTR(TINTF_018));

    interf->json_section_begin(FPSTR(TCONST_edit_lamp_config));
    String filename=embui.param(FPSTR(TCONST_fileName));
    String cfg(FPSTR(TINTF_018)); cfg+=" ("; cfg+=filename; cfg+=")";

    // проверка на наличие конфигураций
    if(LittleFS.begin()){
#ifdef ESP32
        File tst = LittleFS.open(FPSTR(TCONST__backup_idx));
        if(tst.openNextFile())
#else
        Dir tst = LittleFS.openDir(FPSTR(TCONST__backup_idx));
        if(tst.next())
#endif    
        {
            interf->select(FPSTR(TCONST_fileName), cfg);
#ifdef ESP32
            File root = LittleFS.open(FPSTR(TCONST__backup_idx));
            File file = root.openNextFile();
#else
            Dir dir = LittleFS.openDir(FPSTR(TCONST__backup_idx));
#endif
            String fn;
#ifdef ESP32
            while (file) {
                fn=file.name();
                if(!file.isDirectory()){
#else
            while (dir.next()) {
                fn=dir.fileName();
#endif

                fn.replace(FPSTR(TCONST__backup_idx_),F(""));
                //LOG(println, fn);
                interf->option(fn, fn);
#ifdef ESP32
                    file = root.openNextFile();
                }
            }
#else
            }
#endif
            interf->json_section_end(); // select

            interf->json_section_line();
                interf->button_submit_value(FPSTR(TCONST_edit_lamp_config), FPSTR(TCONST_load), FPSTR(TINTF_019), FPSTR(P_GREEN));
                interf->button_submit_value(FPSTR(TCONST_edit_lamp_config), FPSTR(TCONST_save), FPSTR(TINTF_008));
                interf->button_submit_value(FPSTR(TCONST_edit_lamp_config), FPSTR(TCONST_delCfg), FPSTR(TINTF_006), FPSTR(P_RED));
            interf->json_section_end(); // json_section_line
            filename.clear();
            interf->spacer();
        }
    }
    interf->json_section_begin(FPSTR(TCONST_add_lamp_config));
        interf->text(FPSTR(TCONST_fileName2), filename, FPSTR(TINTF_01A), false);
        interf->button_submit(FPSTR(TCONST_add_lamp_config), FPSTR(TINTF_01B));
    interf->json_section_end();

    interf->json_section_end(); // json_section_begin
    interf->json_section_end(); // json_section_hidden
}

void show_lamp_config(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_lamp_config(interf, data);
    interf->json_frame_flush();
}

void edit_lamp_config(Interface *interf, JsonObject *data){
    // Рбоата с конфигурациями в ФС
    if (!data) return;
    String name = (data->containsKey(FPSTR(TCONST_fileName)) ? (*data)[FPSTR(TCONST_fileName)] : (*data)[FPSTR(TCONST_fileName2)]);
    String act = (*data)[FPSTR(TCONST_edit_lamp_config)];

    if(name.isEmpty() || act.isEmpty())
        name = (*data)[FPSTR(TCONST_fileName2)].as<String>();
    LOG(printf_P, PSTR("name=%s, act=%s\n"), name.c_str(), act.c_str());

    if(name.isEmpty()) return;

    if (act == FPSTR(TCONST_delCfg)) { // удаление
        String filename = String(FPSTR(TCONST__backup_glb_)) + name;
        if (LittleFS.begin()) LittleFS.remove(filename);

        filename = String(FPSTR(TCONST__backup_idx_)) + name;
        if (LittleFS.begin()) LittleFS.remove(filename);

        filename = String(FPSTR(TCONST__backup_evn_)) + name;
        if (LittleFS.begin()) LittleFS.remove(filename);
#ifdef ESP_USE_BUTTON
        filename = String(FPSTR(TCONST__backup_btn_)) + name;
        if (LittleFS.begin()) LittleFS.remove(filename);
#endif
    } else if (act == FPSTR(TCONST_load)) { // загрузка
        //myLamp.changePower(false);
        resetAutoTimers();

        String filename = String(FPSTR(TCONST__backup_glb_)) + name;
        embui.load(filename.c_str());

        filename = String(FPSTR(TCONST__backup_idx_)) + name;
        myLamp.effects.initDefault(filename.c_str());

        filename = String(FPSTR(TCONST__backup_evn_)) + name;
        myLamp.events.loadConfig(filename.c_str());
#ifdef ESP_USE_BUTTON
        filename = String(FPSTR(TCONST__backup_btn_)) + name;
        myButtons->clear();
        if (!myButtons->loadConfig()) {
            default_buttons();
        }
#endif
        //embui.var(FPSTR(TCONST_fileName), name);

        String str = String(F("CFG:")) + name;
        myLamp.sendString(str.c_str(), CRGB::Red);

        Task *_t = new Task(3*TASK_SECOND, TASK_ONCE, [](){ myLamp.effects.makeIndexFileFromFS(); sync_parameters(); }, &ts, false, nullptr, nullptr, true);
        _t->enableDelayed();

    } else { // создание
        if(!name.endsWith(F(".json"))){
            name.concat(F(".json"));
        }

        String filename = String(FPSTR(TCONST__backup_glb_)) + name;
        embui.save(filename.c_str(), true);

        filename = String(FPSTR(TCONST__backup_idx_)) + name;
        myLamp.effects.makeIndexFileFromList(filename.c_str(), false);

        filename = String(FPSTR(TCONST__backup_evn_)) + name;
        myLamp.events.saveConfig(filename.c_str());
#ifdef ESP_USE_BUTTON
        filename = String(FPSTR(TCONST__backup_btn_)) + name;
        myButtons->saveConfig(filename.c_str());
#endif
    }

    show_lamp_config(interf, data);
}

void block_lamp_textsend(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_begin(FPSTR(TCONST_textsend));

    interf->spacer(FPSTR(TINTF_01C));
    interf->text(FPSTR(TCONST_msg), FPSTR(TINTF_01D));
    interf->color(FPSTR(TCONST_txtColor), FPSTR(TINTF_01E));
    interf->button_submit(FPSTR(TCONST_textsend), FPSTR(TINTF_01F), FPSTR(P_GRAY));

    interf->json_section_hidden(FPSTR(TCONST_text_config), FPSTR(TINTF_002));
        interf->json_section_begin(FPSTR(TCONST_edit_text_config));
            interf->spacer(FPSTR(TINTF_001));
                interf->range(FPSTR(TCONST_txtSpeed), String(110U-embui.param((FPSTR(TCONST_txtSpeed))).toInt()), String(10), String(100), String(5), String(FPSTR(TINTF_044)));
                interf->range(FPSTR(TCONST_txtOf), String(-1), String((int)(HEIGHT>6?HEIGHT:6)-6), String(1), FPSTR(TINTF_045));
                interf->range(FPSTR(TCONST_txtBfade), String(0), String(255), String(1), FPSTR(TINTF_0CA));
                
            interf->spacer(FPSTR(TINTF_04E));
                interf->number(FPSTR(TCONST_ny_period), FPSTR(TINTF_04F));
                //interf->number(FPSTR(TCONST_ny_unix), FPSTR(TINTF_050));
                String datetime;
                TimeProcessor::getDateTimeString(datetime, embui.param(FPSTR(TCONST_ny_unix)).toInt());
                interf->text(FPSTR(TCONST_ny_unix), datetime, FPSTR(TINTF_050), false);
                interf->button_submit(FPSTR(TCONST_edit_text_config), FPSTR(TINTF_008), FPSTR(P_GRAY));
            interf->spacer();
                //interf->button(FPSTR(TCONST_effects), FPSTR(TINTF_00B));
                interf->button(FPSTR(TCONST_lamptext), FPSTR(TINTF_00B));
        interf->json_section_end();
    interf->json_section_end();

    interf->json_section_end();
}

void set_lamp_textsend(Interface *interf, JsonObject *data){
    if (!data) return;
    resetAutoTimers(); // откладываем автосохранения
    String tmpStr = (*data)[FPSTR(TCONST_txtColor)];
    embui.var(FPSTR(TCONST_txtColor), tmpStr);
    embui.var(FPSTR(TCONST_msg), (*data)[FPSTR(TCONST_msg)]);

    tmpStr.replace(F("#"), F("0x"));
    myLamp.sendString((*data)[FPSTR(TCONST_msg)], (CRGB::HTMLColorCode)strtol(tmpStr.c_str(), NULL, 0));
}

void block_drawing(Interface *interf, JsonObject *data){
    //Страница "Рисование"
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_drawing), FPSTR(TINTF_0CE));

    DynamicJsonDocument doc(512);
    JsonObject param = doc.to<JsonObject>();

    param[FPSTR(TCONST_width)] = WIDTH;
    param[FPSTR(TCONST_height)] = HEIGHT;
    param[FPSTR(TCONST_blabel)] = FPSTR(TINTF_0CF);
    param[FPSTR(TCONST_drawClear)] = FPSTR(TINTF_0D9);


    interf->checkbox(FPSTR(TCONST_drawbuff), myLamp.isDrawOn()? "1" : "0", FPSTR(TINTF_0CE), true);
    interf->custom(String(FPSTR(TCONST_drawing_ctrl)),String(FPSTR(TCONST_drawing)),embui.param(FPSTR(TCONST_txtColor)),String(FPSTR(TINTF_0D0)), param);
    param.clear();

    interf->json_section_end();
}

void set_drawing(Interface *interf, JsonObject *data){
    if (!data) return;

    String value = (*data)[FPSTR(TCONST_drawing_ctrl)];
    if((*data).containsKey(FPSTR(TCONST_drawing_ctrl)) && value!=F("null"))
        remote_action(RA_DRAW, value.c_str(), NULL);
    else {
        String key = String(FPSTR(TCONST_drawing_ctrl))+String(F("_fill"));
        if((*data).containsKey(key)){
            value = (*data)[key].as<String>();
            remote_action(RA_FILLMATRIX, value.c_str(), NULL);
        }
    }
}
void set_clear(Interface *interf, JsonObject *data){
    if (!data) return;
    remote_action(RA_FILLMATRIX, "#000000", NULL);
}

void block_lamptext(Interface *interf, JsonObject *data){
    //Страница "Вывод текста"
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_lamptext), FPSTR(TINTF_001));

    block_lamp_textsend(interf, data);

    interf->json_section_end();
}

void set_text_config(Interface *interf, JsonObject *data){
    if (!data) return;
    (*data)[FPSTR(TCONST_txtSpeed)]=String(110U-(*data)[FPSTR(TCONST_txtSpeed)].as<int>());
    SETPARAM(FPSTR(TCONST_txtSpeed), myLamp.setTextMovingSpeed((*data)[FPSTR(TCONST_txtSpeed)].as<int>()));
    SETPARAM(FPSTR(TCONST_txtOf), myLamp.setTextOffset((*data)[FPSTR(TCONST_txtOf)]));
    SETPARAM(FPSTR(TCONST_ny_period), myLamp.setNYMessageTimer((*data)[FPSTR(TCONST_ny_period)]));
    SETPARAM(FPSTR(TCONST_txtBfade), myLamp.setBFade((*data)[FPSTR(TCONST_txtBfade)]));

    String newYearTime = (*data)[FPSTR(TCONST_ny_unix)]; // Дата/время наструпления нового года с интерфейса
    struct tm t;
    tm *tm=&t;
    localtime_r(TimeProcessor::now(), tm);  // reset struct to local now()

    // set desired date
    tm->tm_year = newYearTime.substring(0,4).toInt()-EMBUI_TM_BASE_YEAR;
    tm->tm_mon  = newYearTime.substring(5,7).toInt()-1;
    tm->tm_mday = newYearTime.substring(8,10).toInt();
    tm->tm_hour = newYearTime.substring(11,13).toInt();
    tm->tm_min  = newYearTime.substring(14,16).toInt();
    tm->tm_sec  = 0;

    time_t ny_unixtime = mktime(tm);
    LOG(printf_P, PSTR("Set New Year at %d %d %d %d %d (%ld)\n"), tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, ny_unixtime);

    //SETPARAM(FPSTR(TCONST_ny_unix), myLamp.setNYUnixTime(ny_unixtime));
    embui.var(FPSTR(TCONST_ny_unix),String(ny_unixtime)); myLamp.setNYUnixTime(ny_unixtime);

    if(!interf){
        interf = embui.ws.count()? new Interface(&embui, &embui.ws, 1024) : nullptr;
        //section_text_frame(interf, data);
        section_main_frame(interf, nullptr); // вернемся на главный экран (то же самое при начальном запуске)
        delete interf;
    } else
        section_text_frame(interf, data);
}

#ifdef MP3PLAYER
// show page with MP3 Player setup
void block_settings_mp3(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_settings_mp3), FPSTR(TINTF_099));

    // show message if DFPlayer is not available
    if (!mp3->isReady()){
        interf->constant(F("cmt"), F("MP3 player is not connected, not ready or not responding :("));
    }

    interf->checkbox(FPSTR(TCONST_isOnMP3), myLamp.isONMP3()? "1" : "0", FPSTR(TINTF_099), true);
    interf->range(FPSTR(TCONST_mp3volume), String(1), String(30), String(1), FPSTR(TINTF_09B), true);
    
    interf->json_section_begin(FPSTR(TCONST_set_mp3));
    interf->spacer(FPSTR(TINTF_0B1));
    interf->json_section_line(); // расположить в одной линии
        interf->checkbox(FPSTR(TCONST_playName), myLamp.getLampSettings().playName ? "1" : "0", FPSTR(TINTF_09D), false);
    interf->json_section_end();
    interf->json_section_line(); // расположить в одной линии
        interf->checkbox(FPSTR(TCONST_playEffect), myLamp.getLampSettings().playEffect ? "1" : "0", FPSTR(TINTF_09E), false);
        interf->checkbox(FPSTR(TCONST_playMP3), myLamp.getLampSettings().playMP3 ? "1" : "0", FPSTR(TINTF_0AF), false);
    interf->json_section_end();

    //interf->checkbox(FPSTR(TCONST_playTime), myLamp.getLampSettings().playTime ? "1" : "0", FPSTR(TINTF_09C), false);
    interf->select(FPSTR(TCONST_playTime), String(myLamp.getLampSettings().playTime), String(FPSTR(TINTF_09C)), false);
    interf->option(String(TIME_SOUND_TYPE::TS_NONE), FPSTR(TINTF_0B6));
    interf->option(String(TIME_SOUND_TYPE::TS_VER1), FPSTR(TINTF_0B7));
    interf->option(String(TIME_SOUND_TYPE::TS_VER2), FPSTR(TINTF_0B8));
    interf->json_section_end();

    interf->select(FPSTR(TCONST_alarmSound), String(myLamp.getLampSettings().alarmSound), String(FPSTR(TINTF_0A3)), false);
    interf->option(String(ALARM_SOUND_TYPE::AT_NONE), FPSTR(TINTF_09F));
    interf->option(String(ALARM_SOUND_TYPE::AT_FIRST), FPSTR(TINTF_0A0));
    interf->option(String(ALARM_SOUND_TYPE::AT_SECOND), FPSTR(TINTF_0A4));
    interf->option(String(ALARM_SOUND_TYPE::AT_THIRD), FPSTR(TINTF_0A5));
    interf->option(String(ALARM_SOUND_TYPE::AT_FOURTH), FPSTR(TINTF_0A6));
    interf->option(String(ALARM_SOUND_TYPE::AT_FIFTH), FPSTR(TINTF_0A7));
    interf->option(String(ALARM_SOUND_TYPE::AT_RANDOM), FPSTR(TINTF_0A1));
    interf->option(String(ALARM_SOUND_TYPE::AT_RANDOMMP3), FPSTR(TINTF_0A2));
    interf->json_section_end();
    interf->checkbox(FPSTR(TCONST_limitAlarmVolume), myLamp.getLampSettings().limitAlarmVolume ? "1" : "0", FPSTR(TINTF_0B3), false);

    interf->select(FPSTR(TCONST_eqSetings), String(myLamp.getLampSettings().MP3eq), String(FPSTR(TINTF_0A8)), false);
    interf->option(String(DFPLAYER_EQ_NORMAL), FPSTR(TINTF_0A9));
    interf->option(String(DFPLAYER_EQ_POP), FPSTR(TINTF_0AA));
    interf->option(String(DFPLAYER_EQ_ROCK), FPSTR(TINTF_0AB));
    interf->option(String(DFPLAYER_EQ_JAZZ), FPSTR(TINTF_0AC));
    interf->option(String(DFPLAYER_EQ_CLASSIC), FPSTR(TINTF_0AD));
    interf->option(String(DFPLAYER_EQ_BASS), FPSTR(TINTF_0AE));
    interf->json_section_end();
    
    interf->number(String(FPSTR(TCONST_mp3count)), String(mp3->getMP3count()), String(FPSTR(TINTF_0B0)));
    //SETPARAM(FPSTR(TCONST_mp3count), mp3->setMP3count((*data)[FPSTR(TCONST_mp3count)].as<int>())); // кол-во файлов в папке мп3

    interf->button_submit(FPSTR(TCONST_set_mp3), FPSTR(TINTF_008), FPSTR(P_GRAY));
    interf->json_section_end();

    interf->spacer();
    interf->button(FPSTR(TCONST_settings), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_mp3(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_mp3(interf, data);
    interf->json_frame_flush();
}

void set_settings_mp3(Interface *interf, JsonObject *data){
    if (!data) return;

    resetAutoTimers(); // сдвинем таймеры автосейва, т.к. длительная операция
    uint8_t val = (*data)[FPSTR(TCONST_eqSetings)].as<uint8_t>(); myLamp.setEqType(val); mp3->setEqType(val); // пишет в плеер!

    myLamp.setPlayTime((*data)[FPSTR(TCONST_playTime)].as<int>());
    myLamp.setPlayName((*data)[FPSTR(TCONST_playName)]=="1");
    myLamp.setPlayEffect((*data)[FPSTR(TCONST_playEffect)]=="1"); mp3->setPlayEffect(myLamp.getLampSettings().playEffect);
    myLamp.setAlatmSound((ALARM_SOUND_TYPE)(*data)[FPSTR(TCONST_alarmSound)].as<int>());
    myLamp.setPlayMP3((*data)[FPSTR(TCONST_playMP3)]=="1"); mp3->setPlayMP3(myLamp.getLampSettings().playMP3);
    myLamp.setLimitAlarmVolume((*data)[FPSTR(TCONST_limitAlarmVolume)]=="1");

    SETPARAM(FPSTR(TCONST_mp3count), mp3->setMP3count((*data)[FPSTR(TCONST_mp3count)].as<int>())); // кол-во файлов в папке мп3
    SETPARAM(FPSTR(TCONST_mp3volume)); // тоже пишет в плеер, разносим во времени

    save_lamp_flags();
    //BasicUI::section_settings_frame(interf, data);
    section_settings_frame(interf, data);
}
#endif

#ifdef MIC_EFFECTS
void block_settings_mic(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_settings_mic), FPSTR(TINTF_020));

    interf->checkbox(FPSTR(TCONST_Mic), myLamp.isMicOnOff()? "1" : "0", FPSTR(TINTF_012), true);

    interf->json_section_begin(FPSTR(TCONST_set_mic));
    if (!myLamp.isMicCalibration()) {
        interf->number(String(FPSTR(TCONST_micScale)), String((float)(round(myLamp.getLampState().getMicScale() * 100) / 100)), String(FPSTR(TINTF_022)), String(0.01), String(0), String(2));
        interf->number(String(FPSTR(TCONST_micNoise)), String((float)(round(myLamp.getLampState().getMicNoise() * 100) / 100)), String(FPSTR(TINTF_023)), String(0.01), String(0), String(32));
        interf->range(String(FPSTR(TCONST_micnRdcLvl)), String((int)myLamp.getLampState().getMicNoiseRdcLevel()), String(0), String(4), String(1), String(FPSTR(TINTF_024)), false);

        interf->button_submit(FPSTR(TCONST_set_mic), FPSTR(TINTF_008), FPSTR(P_GRAY));
        interf->json_section_end();

        interf->spacer();
        interf->button(FPSTR(TCONST_mic_cal), FPSTR(TINTF_025), FPSTR(P_RED));
    } else {
        interf->button(FPSTR(TCONST_mic_cal), FPSTR(TINTF_027), FPSTR(P_RED) );
    }

    interf->spacer();
    interf->button(FPSTR(TCONST_settings), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_mic(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_mic(interf, data);
    interf->json_frame_flush();
}

void set_settings_mic(Interface *interf, JsonObject *data){
    if (!data) return;
    float scale = (*data)[FPSTR(TCONST_micScale)]; //atof((*data)[FPSTR(TCONST_micScale)].as<String>().c_str());
    float noise = (*data)[FPSTR(TCONST_micNoise)]; //atof((*data)[FPSTR(TCONST_micNoise)].as<String>().c_str());
    mic_noise_reduce_level_t rdl = static_cast<mic_noise_reduce_level_t>((*data)[FPSTR(TCONST_micnRdcLvl)].as<unsigned>());

    // LOG(printf_P, PSTR("scale=%2.3f noise=%2.3f rdl=%d\n"),scale,noise,rdl);
    // String tmpStr;
    // serializeJson(*data, tmpStr);
    // LOG(printf_P, PSTR("*data=%s\n"),tmpStr.c_str());

    SETPARAM(FPSTR(TCONST_micScale), myLamp.getLampState().setMicScale(scale));
    SETPARAM(FPSTR(TCONST_micNoise), myLamp.getLampState().setMicNoise(noise));
    SETPARAM(FPSTR(TCONST_micnRdcLvl), myLamp.getLampState().setMicNoiseRdcLevel(rdl));

    //BasicUI::section_settings_frame(interf, data);
    section_settings_frame(interf, data);
}

void set_micflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setMicOnOff((*data)[FPSTR(TCONST_Mic)] == "1");
    save_lamp_flags();
    show_effects_param(interf,data);
}

void set_settings_mic_calib(Interface *interf, JsonObject *data){
    //if (!data) return;
    if (!myLamp.isMicOnOff()) {
        myLamp.sendString(String(FPSTR(TINTF_026)).c_str(), CRGB::Red);
    } else if(!myLamp.isMicCalibration()) {
        myLamp.sendString(String(FPSTR(TINTF_025)).c_str(), CRGB::Red);
        myLamp.setMicCalibration();
    } else {
        myLamp.sendString(String(FPSTR(TINTF_027)).c_str(), CRGB::Red);
    }

    show_settings_mic(interf, data);
}
#endif

// после завершения сканирования обновляем список WiFi
void scan_complete(int n){
    Interface *interf = EmbUI::GetInstance()->ws.count()? new Interface(EmbUI::GetInstance(), &EmbUI::GetInstance()->ws) : nullptr;
    LOG(printf_P, PSTR("UI WiFi: Scan complete %d networks found\n"), n);
    if(interf){
        interf->json_frame_interface();
        interf->json_section_line(FPSTR(T_LOAD_WIFI));
        String ssid = WiFi.SSID();
        interf->select_edit(FPSTR(P_WCSSID), ssid, String(FPSTR(TINTF_02C)));
        for (int i = 0; i < WiFi.scanComplete(); i++) {
            interf->option(WiFi.SSID(i), WiFi.SSID(i));
            LOG(printf_P, PSTR("UI WiFi: WiFi Net %s\n"), WiFi.SSID(i).c_str());
        }
        if(ssid.isEmpty())
            interf->option("", ""); // at the end of list
        interf->json_section_end();
        interf->button(FPSTR(T_SET_SCAN), FPSTR(TINTF_0DA), FPSTR(P_GREEN), 22);
        interf->json_section_end();
        interf->json_frame_flush();

        delete interf;
    }
    Task *_t = new Task(
        TASK_SECOND,
        TASK_ONCE, [](){
            if (WiFi.scanComplete() >= 0) {
                EmbUI::GetInstance()->sysData.isWiFiScanning = false;
                WiFi.scanDelete();
                LOG(printf_P, PSTR("UI WiFi: Scan List deleted\n"));
            }
        },
        &ts, false);
    _t->enableDelayed();
}

void set_scan_wifi(Interface *interf, JsonObject *data){
    if (!interf) return;

    if (WiFi.scanComplete() == -2) {
        LOG(printf_P, PSTR("UI WiFi: WiFi scan starting\n"));
        interf->json_frame_custom(FPSTR(T_XLOAD));
        interf->json_section_content();
        interf->constant(FPSTR(T_SET_SCAN), FPSTR(TINTF_0DA), true, FPSTR(P_GREEN), 22);
        interf->json_section_end();
        interf->json_frame_flush();

        Task *t = new Task(300, TASK_ONCE, nullptr, &ts, false, nullptr, [](){
            EmbUI::GetInstance()->sysData.isWiFiScanning = true;
            #ifdef ESP8266
            WiFi.scanNetworksAsync(scan_complete);     // Сканируем с коллбеком, по завершению скана запустится scan_complete()
            #endif
            #ifdef ESP32
            EmbUI::GetInstance()->setWiFiScanCB(&scan_complete);
            WiFi.scanNetworks(true);         // У ESP нет метода с коллбеком, поэтому просто сканируем
            #endif
        }, true);
        t->enableDelayed();
    }
};

// Блок настроек WiFi
void block_only_wifi(Interface *interf, JsonObject *data) {
    interf->spacer(FPSTR(TINTF_031));
    interf->select(String(FPSTR(P_WIFIMODE)), embui.param(FPSTR(P_WIFIMODE)), String(FPSTR(TINTF_033)));
        interf->option("0", FPSTR(TINTF_029));
        interf->option("1", FPSTR(TINTF_02F));
        interf->option("2", FPSTR(TINTF_046));
    interf->json_section_end();

    interf->comment(FPSTR(TINTF_032));

    interf->text(FPSTR(P_hostname), FPSTR(TINTF_02B));
    interf->password(FPSTR(P_APpwd), FPSTR(TINTF_034));

    interf->spacer(FPSTR(TINTF_02A));
    interf->json_section_line(FPSTR(T_LOAD_WIFI));
    interf->select_edit(FPSTR(P_WCSSID), String(WiFi.SSID()), String(FPSTR(TINTF_02C)));
    interf->json_section_end();
    interf->button(FPSTR(T_SET_SCAN), FPSTR(TINTF_0DA), FPSTR(P_GREEN), 22); // отступ
    interf->json_section_end();
    interf->password(FPSTR(TCONST_wcpass), FPSTR(TINTF_02D));
    interf->button_submit(FPSTR(T_SET_WIFI), FPSTR(TINTF_02E), FPSTR(P_GRAY));
}

// формирование интерфейса настроек WiFi/MQTT
void block_settings_wifi(Interface *interf, JsonObject *data){
    if (!interf) return;

    interf->json_section_main(FPSTR(TCONST_settings_wifi), FPSTR(TINTF_081));

    // форма настроек Wi-Fi
    interf->json_section_hidden(FPSTR(T_SET_WIFI), FPSTR(TINTF_028));
        block_only_wifi(interf, data);
    interf->json_section_end();

#ifdef EMBUI_USE_MQTT
    // форма настроек MQTT
    interf->json_section_hidden(FPSTR(TCONST_set_mqtt), FPSTR(TINTF_035));
    interf->text(FPSTR(P_m_host), FPSTR(TINTF_036));
    interf->number(FPSTR(P_m_port), FPSTR(TINTF_037));
    interf->text(FPSTR(P_m_user), FPSTR(TINTF_038));
    interf->password(FPSTR(P_m_pass), FPSTR(TINTF_02D));
    interf->text(FPSTR(P_m_pref), FPSTR(TINTF_08C));
    interf->number(FPSTR(P_m_tupd), FPSTR(TINTF_039));
    interf->button_submit(FPSTR(TCONST_set_mqtt), FPSTR(TINTF_03A), FPSTR(P_GRAY));
    interf->json_section_end();
#endif

#ifdef EMBUI_USE_FTP
    // форма настроек FTP
    interf->json_section_hidden("H", FPSTR(TINTF_0DB));
        interf->json_section_begin("C", "");
            interf->checkbox(FPSTR(T_CHK_FTP), String(embui.cfgData.isftp), FPSTR(TINTF_0DB), true);
        interf->json_section_end();
        interf->json_section_begin(FPSTR(T_SET_FTP), "");
            interf->text(FPSTR(P_ftpuser), FPSTR(TINTF_038));
            interf->password(FPSTR(P_ftppass), FPSTR(TINTF_02D));
            interf->button_submit(FPSTR(T_SET_FTP), FPSTR(TINTF_008), FPSTR(P_GRAY));
        interf->json_section_end();
    interf->json_section_end();
#endif

    interf->spacer();
    interf->button(FPSTR(TCONST_settings), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_wifi(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_wifi(interf, data);
    interf->json_frame_flush();
    if(!EmbUI::GetInstance()->sysData.isWiFiScanning){ // автосканирование при входе в настройки
        EmbUI::GetInstance()->sysData.isWiFiScanning = true;
        set_scan_wifi(interf, data);
    }
}

// настройка подключения WiFi
void set_settings_wifi(Interface *interf, JsonObject *data){
    if (!data) return;

    BasicUI::set_settings_wifi(interf, data);
    section_settings_frame(interf, data);
}

#ifdef EMBUI_USE_MQTT
void set_settings_mqtt(Interface *interf, JsonObject *data){
    if (!data) return;
    BasicUI::set_settings_mqtt(interf,data);
    embui.mqttReconnect();
    int interval = (*data)[FPSTR(P_m_tupd)];
    LOG(print, F("New MQTT interval: ")); LOG(println, interval);
    myLamp.setmqtt_int(interval);
    section_settings_frame(interf, data);
}
#endif

#ifdef EMBUI_USE_FTP
// настройка ftp
void set_ftp(Interface *interf, JsonObject *data){
    if (!data) return;

    BasicUI::set_ftp(interf, data);
    section_settings_frame(interf, data);
}
#endif

void block_settings_other(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_set_other), FPSTR(TINTF_002));
    
    interf->spacer(FPSTR(TINTF_030));
#if !defined(MATRIXx4) and !defined(XY_EXTERN)
    interf->checkbox(FPSTR(TCONST_MIRR_H), myLamp.getLampSettings().MIRR_H ? "1" : "0", FPSTR(TINTF_03B), false);
    interf->checkbox(FPSTR(TCONST_MIRR_V), myLamp.getLampSettings().MIRR_V ? "1" : "0", FPSTR(TINTF_03C), false);
#endif
    interf->checkbox(FPSTR(TCONST_isFaderON), myLamp.getLampSettings().isFaderON ? "1" : "0", FPSTR(TINTF_03D), false);
    interf->checkbox(FPSTR(TCONST_isClearing), myLamp.getLampSettings().isEffClearing ? "1" : "0", FPSTR(TINTF_083), false);
    interf->checkbox(FPSTR(TCONST_DRand), myLamp.getLampSettings().dRand ? "1" : "0", FPSTR(TINTF_03E), false);
    interf->checkbox(FPSTR(TCONST_showName), myLamp.getLampSettings().showName ? "1" : "0", FPSTR(TINTF_09A), false);
    interf->range(FPSTR(TCONST_DTimer), String(30), String(250), String(5), FPSTR(TINTF_03F));
    float sf = embui.param(FPSTR(TCONST_spdcf)).toFloat();
    interf->range(String(FPSTR(TCONST_spdcf)), String(sf), String(0.25), String(4.0), String(0.25), String(FPSTR(TINTF_0D3)), false);
#ifdef SHOWSYSCONFIG
    interf->checkbox(FPSTR(TCONST_isShowSysMenu), myLamp.getLampSettings().isShowSysMenu ? "1" : "0", FPSTR(TINTF_093), false); // отображение системного меню
#endif
#ifdef TM1637_CLOCK
    interf->spacer(FPSTR(TINTF_0D4));
    interf->checkbox(FPSTR(TCONST_tm24), myLamp.getLampSettings().tm24 ? String("1") : String("0"), FPSTR(TINTF_0D7), false);
    interf->checkbox(FPSTR(TCONST_tmZero), myLamp.getLampSettings().tmZero ? String("1") : String("0"), FPSTR(TINTF_0D8), false);
    interf->range(FPSTR(TCONST_tmBrightOn), String(myLamp.getBrightOn()), String(0), String(7), String(1), FPSTR(TINTF_0D5), false);
    interf->range(FPSTR(TCONST_tmBrightOff), String(myLamp.getBrightOff()), String(0), String(7), String(1), FPSTR(TINTF_0D6), false);
    #ifdef DS18B20
    interf->checkbox(FPSTR(TCONST_ds18b20), myLamp.getLampSettings().isTempOn ? String("1") : String("0"), FPSTR(TINTF_0E0), false);
    #endif
#endif
    interf->spacer(FPSTR(TINTF_0BA));
    interf->range(FPSTR(TCONST_alarmP), String(myLamp.getAlarmP()), String(1), String(15), String(1), FPSTR(TINTF_0BB), false);
    interf->range(FPSTR(TCONST_alarmT), String(myLamp.getAlarmT()), String(1), String(15), String(1), FPSTR(TINTF_0BC), false);

    interf->button_submit(FPSTR(TCONST_set_other), FPSTR(TINTF_008), FPSTR(P_GRAY));

    interf->spacer();
    interf->button(FPSTR(TCONST_settings), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_other(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_other(interf, data);
    interf->json_frame_flush();
}

void set_settings_other(Interface *interf, JsonObject *data){
    if (!data) return;
    LOG(printf_P,PSTR("Mark sso 1\n"));
    resetAutoTimers();
    LOG(printf_P,PSTR("Mark sso 2\n"));

    DynamicJsonDocument *_str = new DynamicJsonDocument(1024);
    (*_str)=(*data);

    Task *_t = new Task(300, TASK_ONCE, [_str](){
        JsonObject dataStore = (*_str).as<JsonObject>();
        JsonObject *data = &dataStore;
    LOG(printf_P,PSTR("Mark sso 3\n"));

        // LOG(printf_P,PSTR("Settings: %s\n"),tmpData.c_str());
        myLamp.setMIRR_H((*data)[FPSTR(TCONST_MIRR_H)] == "1");
        myLamp.setMIRR_V((*data)[FPSTR(TCONST_MIRR_V)] == "1");
        myLamp.setFaderFlag((*data)[FPSTR(TCONST_isFaderON)] == "1");
        myLamp.setClearingFlag((*data)[FPSTR(TCONST_isClearing)] == "1");
        myLamp.setDRand((*data)[FPSTR(TCONST_DRand)] == "1");
        myLamp.setShowName((*data)[FPSTR(TCONST_showName)] == "1");
    LOG(printf_P,PSTR("Mark sso 4\n"));

        SETPARAM(FPSTR(TCONST_DTimer), ({if (myLamp.getMode() == LAMPMODE::MODE_DEMO){ myLamp.demoTimer(T_DISABLE); myLamp.demoTimer(T_ENABLE, embui.param(FPSTR(TCONST_DTimer)).toInt()); }}));
    LOG(printf_P,PSTR("Mark sso 4.5\n"));

        float sf = (*data)[FPSTR(TCONST_spdcf)];
        SETPARAM(FPSTR(TCONST_spdcf), myLamp.setSpeedFactor(sf));
    LOG(printf_P,PSTR("Mark sso 5\n"));

        myLamp.setIsShowSysMenu((*data)[FPSTR(TCONST_isShowSysMenu)] == "1");

    #ifdef TM1637_CLOCK
        uint8_t tmBri = ((*data)[FPSTR(TCONST_tmBrightOn)]).as<uint8_t>()<<4; // старшие 4 бита
        tmBri = tmBri | ((*data)[FPSTR(TCONST_tmBrightOff)]).as<uint8_t>(); // младшие 4 бита
        embui.var(FPSTR(TCONST_tmBright), String(tmBri)); myLamp.setTmBright(tmBri);
        myLamp.settm24((*data)[FPSTR(TCONST_tm24)] == "1");
        myLamp.settmZero((*data)[FPSTR(TCONST_tmZero)] == "1");
        #ifdef DS18B20
        myLamp.setTempDisp((*data)[FPSTR(TCONST_ds18b20)] == "1");
        #endif
    #endif

        uint8_t alatmPT = ((*data)[FPSTR(TCONST_alarmP)]).as<uint8_t>()<<4; // старшие 4 бита
        alatmPT = alatmPT | ((*data)[FPSTR(TCONST_alarmT)]).as<uint8_t>(); // младшие 4 бита
        embui.var(FPSTR(TCONST_alarmPT), String(alatmPT)); myLamp.setAlarmPT(alatmPT);
        //SETPARAM(FPSTR(TCONST_alarmPT), myLamp.setAlarmPT(alatmPT));
        //LOG(printf_P, PSTR("alatmPT=%d, alatmP=%d, alatmT=%d\n"), alatmPT, myLamp.getAlarmP(), myLamp.getAlarmT());
    LOG(printf_P,PSTR("Mark sso 6\n"));

        save_lamp_flags();
        delete _str; },
        &ts, false, nullptr, nullptr, true
    );
    _t->enableDelayed();

    //BasicUI::section_settings_frame(interf, data);
    LOG(printf_P,PSTR("Mark sso 7\n"));
    if(interf)
        section_settings_frame(interf, data);
}

// страницу-форму настроек времени строим методом фреймворка (ломает переводы, возвращено обратно)
void show_settings_time(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();

    // Headline
    interf->json_section_main(FPSTR(T_SET_TIME), FPSTR(TINTF_051));

    interf->comment(FPSTR(TINTF_052));     // комментарий-описание секции

    // сперва рисуем простое поле с текущим значением правил временной зоны из конфига
    interf->text(FPSTR(P_TZSET), FPSTR(TINTF_053));

    // user-defined NTP server
    interf->text(FPSTR(P_userntp), FPSTR(TINTF_054));
    // manual date and time setup
    interf->comment(FPSTR(TINTF_055));
    interf->datetime(FPSTR(P_DTIME), "", true);
    interf->hidden(FPSTR(P_DEVICEDATETIME),""); // скрытое поле для получения времени с устройства
    interf->button_submit(FPSTR(T_SET_TIME), FPSTR(TINTF_008), FPSTR(P_GRAY));

    interf->spacer();

    // exit button
    interf->button(FPSTR(T_SETTINGS), FPSTR(TINTF_00B));

    // close and send frame
    interf->json_section_end();
    interf->json_frame_flush();

    // формируем и отправляем кадр с запросом подгрузки внешнего ресурса со списком правил временных зон
    // полученные данные заместят предыдущее поле выпадающим списком с данными о всех временных зонах
    interf->json_frame_custom(FPSTR(T_XLOAD));
    interf->json_section_content();
                    //id            val                         label   direct  skipl URL for external data
    interf->select(FPSTR(P_TZSET), embui.param(FPSTR(P_TZSET)), "",     false,  true, F("/js/tz.json"));
    interf->json_section_end();
    interf->json_frame_flush();
}

void set_settings_time(Interface *interf, JsonObject *data){
    BasicUI::set_settings_time(interf, data);
    myLamp.sendString(String(F("%TM")).c_str(), CRGB::Green);
#ifdef RTC
    rtc.updateRtcTime();
#endif
    section_settings_frame(interf, data);
}

void block_settings_update(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_hidden(FPSTR(T_DO_OTAUPD), FPSTR(TINTF_056));
    interf->spacer(FPSTR(TINTF_059));
    interf->file(FPSTR(T_DO_OTAUPD), FPSTR(T_DO_OTAUPD), FPSTR(TINTF_05A));
    interf->button_confirm(FPSTR(T_REBOOT), FPSTR(TINTF_096), FPSTR(TINTF_0E1), !data?String(FPSTR(P_RED)):String(""));       // кнопка перехода в настройки времени
    interf->json_section_end();
}

void block_settings_event(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_show_event), FPSTR(TINTF_011));

    interf->checkbox(FPSTR(TCONST_Events), myLamp.IsEventsHandled()? "1" : "0", FPSTR(TINTF_086), true);

    interf->json_section_begin(FPSTR(TCONST_event_conf));
    interf->select(FPSTR(TCONST_eventList), String(0), String(FPSTR(TINTF_05B)), false);

    int num = 0;
    LList<DEV_EVENT *> *events= myLamp.events.getEvents();
    for(unsigned i=0; i<events->size(); i++){
        interf->option(String(num), (*events)[i]->getName());
        ++num;
    }
    interf->json_section_end();

    interf->json_section_line();
    interf->button_submit_value(FPSTR(TCONST_event_conf), FPSTR(TCONST_edit), FPSTR(TINTF_05C), FPSTR(P_GREEN));
    interf->button_submit_value(FPSTR(TCONST_event_conf), FPSTR(TCONST_delete), FPSTR(TINTF_006), FPSTR(P_RED));
    interf->json_section_end();

    interf->json_section_end();

    interf->button(FPSTR(TCONST_event_conf), FPSTR(TINTF_05D));

    interf->spacer();
    interf->button(FPSTR(TCONST_effects), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_event(Interface *interf, JsonObject *data){
    if (!interf) return;

    if(cur_edit_event && !myLamp.events.isEnumerated(*cur_edit_event)){
        LOG(println, F("Удалено временное событие!"));
        delete cur_edit_event;
        cur_edit_event = NULL;
    } else {
        cur_edit_event = NULL;
    }

    interf->json_frame_interface();
    block_settings_event(interf, data);
    interf->json_frame_flush();
}

void set_eventflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setIsEventsHandled((*data)[FPSTR(TCONST_Events)] == "1");
    save_lamp_flags();
}

void set_event_conf(Interface *interf, JsonObject *data){
    DEV_EVENT event;
    String act;
    if (!data) return;

    //String output;
    //serializeJson((*data), output);
    //LOG(println, output.c_str());

    if(cur_edit_event){
        myLamp.events.delEvent(*cur_edit_event);
    } else if (data->containsKey(FPSTR(TCONST_eventList))) {
        unsigned num = (*data)[FPSTR(TCONST_eventList)];
        LList<DEV_EVENT *> *events = myLamp.events.getEvents();
        if(events->size()>num)
            events->remove(num);
    }

    if (data->containsKey(FPSTR(TCONST_enabled))) {
        event.isEnabled = ((*data)[FPSTR(TCONST_enabled)] == "1");
    } else {
        event.isEnabled = true;
    }

    event.d1 = ((*data)[FPSTR(TCONST_d1)] == "1");
    event.d2 = ((*data)[FPSTR(TCONST_d2)] == "1");
    event.d3 = ((*data)[FPSTR(TCONST_d3)] == "1");
    event.d4 = ((*data)[FPSTR(TCONST_d4)] == "1");
    event.d5 = ((*data)[FPSTR(TCONST_d5)] == "1");
    event.d6 = ((*data)[FPSTR(TCONST_d6)] == "1");
    event.d7 = ((*data)[FPSTR(TCONST_d7)] == "1");
    event.setEvent((EVENT_TYPE)(*data)[FPSTR(TCONST_evList)].as<long>());
    event.setRepeat((*data)[FPSTR(TCONST_repeat)]);
    event.setStopat((*data)[FPSTR(TCONST_stopat)]);
    String tmEvent = (*data)[FPSTR(TCONST_tmEvent)];

    struct tm t;
    tm *tm=&t;
    localtime_r(TimeProcessor::now(), tm);  // reset struct to local now()

    // set desired date
    tm->tm_year=tmEvent.substring(0,4).toInt()-EMBUI_TM_BASE_YEAR;
    tm->tm_mon = tmEvent.substring(5,7).toInt()-1;
    tm->tm_mday=tmEvent.substring(8,10).toInt();
    tm->tm_hour=tmEvent.substring(11,13).toInt();
    tm->tm_min=tmEvent.substring(14,16).toInt();
    tm->tm_sec=0;

    time_t ut = mktime(tm);
    event.setUnixtime(ut);
    LOG(printf_P, PSTR("Set Event at %4d-%2d-%2d %2d:%2d:00 -> %llu\n"), tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, (unsigned long long)ut);

    String buf; // внешний буффер, т.к. добавление эвента ниже
    switch(event.getEvent()){
        case EVENT_TYPE::ALARM: {
                DynamicJsonDocument doc(1024);
                doc[FPSTR(TCONST_alarmP)] = (*data)[FPSTR(TCONST_alarmP)];
                doc[FPSTR(TCONST_alarmT)] = (*data)[FPSTR(TCONST_alarmT)];
                doc[FPSTR(TCONST_msg)] = (*data)[FPSTR(TCONST_msg)];

#ifdef MP3PLAYER
                doc[FPSTR(TCONST_afS)] = (*data)[FPSTR(TCONST_afS)];
                doc[FPSTR(TCONST_lV)] = (*data)[FPSTR(TCONST_lV)];
                doc[FPSTR(TCONST_sT)] = (*data)[FPSTR(TCONST_sT)];
#endif
                serializeJson(doc,buf);
                buf.replace("\"","'");
                event.setMessage(buf);
                myLamp.events.addEvent(event);
            }
            break;
        case EVENT_TYPE::SEND_TIME: {
                DynamicJsonDocument doc(1024);
                doc[FPSTR(TCONST_isShowOff)] = (*data)[FPSTR(TCONST_isShowOff)];
#ifdef MP3PLAYER
                doc[FPSTR(TCONST_isPlayTime)] = (*data)[FPSTR(TCONST_isPlayTime)];
#endif
                serializeJson(doc,buf);
                buf.replace("\"","'");
                event.setMessage(buf);
                myLamp.events.addEvent(event);
            }
            break;
        default:
            event.setMessage((*data)[FPSTR(TCONST_msg)]);
            myLamp.events.addEvent(event);
            break;
    }
    myLamp.events.saveConfig();
    cur_edit_event = NULL;
    show_settings_event(interf, data);
}

void show_event_conf(Interface *interf, JsonObject *data){
    String act;
    bool edit = false;
    unsigned num = 0;
    if (!interf || !data) return;

    LOG(print,F("event_conf=")); LOG(println, (*data)[FPSTR(TCONST_event_conf)].as<String>()); //  && data->containsKey(FPSTR(TCONST_event_conf))

    if (data->containsKey(FPSTR(TCONST_eventList))) {
        DEV_EVENT *curr = NULL;
        num = (*data)[FPSTR(TCONST_eventList)];

        LList<DEV_EVENT *> *events = myLamp.events.getEvents();
        if(events->size()>num)
            curr = events->get(num);

        if (!curr) return;
        act = (*data)[FPSTR(TCONST_event_conf)].as<String>();
        cur_edit_event = curr;
        edit = true;
    } else if(cur_edit_event != NULL){
        if(data->containsKey(FPSTR(TCONST_evList)))
            cur_edit_event->setEvent((*data)[FPSTR(TCONST_evList)].as<EVENT_TYPE>()); // меняем тип налету
        if(myLamp.events.isEnumerated(*cur_edit_event))
            edit = true;
    } else {
        LOG(println, "Созданан пустой эвент!");
        cur_edit_event = new DEV_EVENT();
    }

    if (act == FPSTR(TCONST_delete)) {
        myLamp.events.delEvent(*cur_edit_event);
        cur_edit_event = NULL;
        myLamp.events.saveConfig();
        show_settings_event(interf, data);
        return;
    } else if (data->containsKey(FPSTR(TCONST_save))) {
        set_event_conf(interf, data);
        return;
    }

    interf->json_frame_interface();

    if (edit) {
        interf->json_section_main(FPSTR(TCONST_set_event), FPSTR(TINTF_05C));
        interf->constant(FPSTR(TCONST_eventList), String(num), cur_edit_event->getName());
        interf->checkbox(FPSTR(TCONST_enabled), (cur_edit_event->isEnabled? "1" : "0"), FPSTR(TINTF_05E), false);
    } else {
        interf->json_section_main(FPSTR(TCONST_set_event), FPSTR(TINTF_05D));
    }

    interf->json_section_line();
        interf->select(FPSTR(TCONST_evList), String(cur_edit_event->getEvent()), String(FPSTR(TINTF_05F)), true);
            interf->option(String(EVENT_TYPE::ON), FPSTR(TINTF_060));
            interf->option(String(EVENT_TYPE::OFF), FPSTR(TINTF_061));
            interf->option(String(EVENT_TYPE::DEMO), FPSTR(TINTF_062));
            interf->option(String(EVENT_TYPE::ALARM), FPSTR(TINTF_063));
            interf->option(String(EVENT_TYPE::SEND_TEXT), FPSTR(TINTF_067));
            interf->option(String(EVENT_TYPE::SEND_TIME), FPSTR(TINTF_068));
            interf->option(String(EVENT_TYPE::SET_EFFECT), FPSTR(TINTF_00A));
            interf->option(String(EVENT_TYPE::SET_WARNING), FPSTR(TINTF_0CB));

            interf->option(String(EVENT_TYPE::SET_GLOBAL_BRIGHT), FPSTR(TINTF_00C));
            interf->option(String(EVENT_TYPE::SET_WHITE_LO), FPSTR(TINTF_0EA));
            interf->option(String(EVENT_TYPE::SET_WHITE_HI), FPSTR(TINTF_0EB));

#ifndef MOOT
#ifdef AUX_PIN
            interf->option(String(EVENT_TYPE::AUX_ON), FPSTR(TINTF_06A));
            interf->option(String(EVENT_TYPE::AUX_OFF), FPSTR(TINTF_06B));
            interf->option(String(EVENT_TYPE::AUX_TOGGLE), FPSTR(TINTF_06C));
#endif
            interf->option(String(EVENT_TYPE::LAMP_CONFIG_LOAD), FPSTR(TINTF_064));
            interf->option(String(EVENT_TYPE::EFF_CONFIG_LOAD), FPSTR(TINTF_065));
#ifdef ESP_USE_BUTTON
            interf->option(String(EVENT_TYPE::BUTTONS_CONFIG_LOAD), FPSTR(TINTF_0E9));
#endif
            interf->option(String(EVENT_TYPE::EVENTS_CONFIG_LOAD), FPSTR(TINTF_066));
            interf->option(String(EVENT_TYPE::PIN_STATE), FPSTR(TINTF_069));
#endif
        interf->json_section_end();
        interf->datetime(FPSTR(TCONST_tmEvent), cur_edit_event->getDateTime(), String(FPSTR(TINTF_06D)));
    interf->json_section_end();
    interf->json_section_line();
        interf->number(FPSTR(TCONST_repeat), String(cur_edit_event->getRepeat()), FPSTR(TINTF_06E));
        interf->number(FPSTR(TCONST_stopat), String(cur_edit_event->getStopat()), FPSTR(TINTF_06F));
    interf->json_section_end();

    switch(cur_edit_event->getEvent()){
        case EVENT_TYPE::ALARM: {
                DynamicJsonDocument doc(1024);
                String buf = cur_edit_event->getMessage();
                buf.replace("'","\"");
                DeserializationError err = deserializeJson(doc,buf);
                int alarmP = !err && doc.containsKey(FPSTR(TCONST_alarmP)) ? doc[FPSTR(TCONST_alarmP)].as<uint8_t>() : myLamp.getAlarmP();
                int alarmT = !err && doc.containsKey(FPSTR(TCONST_alarmT)) ? doc[FPSTR(TCONST_alarmT)].as<uint8_t>() : myLamp.getAlarmT();
                String msg = !err && doc.containsKey(FPSTR(TCONST_msg)) ? doc[FPSTR(TCONST_msg)] : cur_edit_event->getMessage();

                interf->spacer(FPSTR(TINTF_0BA));
                interf->text(FPSTR(TCONST_msg), msg, FPSTR(TINTF_070), false);
                interf->json_section_line();
                    interf->range(FPSTR(TCONST_alarmP), String(alarmP), String(1), String(15), String(1), FPSTR(TINTF_0BB), false);
                    interf->range(FPSTR(TCONST_alarmT), String(alarmT), String(1), String(15), String(1), FPSTR(TINTF_0BC), false);
                interf->json_section_end();
#ifdef MP3PLAYER
                String limitAlarmVolume = !err && doc.containsKey(FPSTR(TCONST_lV)) ? doc[FPSTR(TCONST_lV)] : String(myLamp.getLampSettings().limitAlarmVolume ? "1" : "0");
                String alarmFromStart = !err && doc.containsKey(FPSTR(TCONST_afS)) ? doc[FPSTR(TCONST_afS)] : String("1");
                String st = !err && doc.containsKey(FPSTR(TCONST_sT)) ? doc[FPSTR(TCONST_sT)] : String(myLamp.getLampSettings().alarmSound);
                interf->json_section_line();
                    interf->checkbox(FPSTR(TCONST_afS), alarmFromStart, FPSTR(TINTF_0D1), false);
                    interf->checkbox(FPSTR(TCONST_lV), limitAlarmVolume, FPSTR(TINTF_0D2), false);
                interf->json_section_end();
                interf->select(FPSTR(TCONST_sT), st, String(FPSTR(TINTF_0A3)), false);
                    interf->option(String(ALARM_SOUND_TYPE::AT_NONE), FPSTR(TINTF_09F));
                    interf->option(String(ALARM_SOUND_TYPE::AT_FIRST), FPSTR(TINTF_0A0));
                    interf->option(String(ALARM_SOUND_TYPE::AT_SECOND), FPSTR(TINTF_0A4));
                    interf->option(String(ALARM_SOUND_TYPE::AT_THIRD), FPSTR(TINTF_0A5));
                    interf->option(String(ALARM_SOUND_TYPE::AT_FOURTH), FPSTR(TINTF_0A6));
                    interf->option(String(ALARM_SOUND_TYPE::AT_FIFTH), FPSTR(TINTF_0A7));
                    interf->option(String(ALARM_SOUND_TYPE::AT_RANDOM), FPSTR(TINTF_0A1));
                    interf->option(String(ALARM_SOUND_TYPE::AT_RANDOMMP3), FPSTR(TINTF_0A2));
                interf->json_section_end();
#endif
            }
            break;
        case EVENT_TYPE::SEND_TIME: {
                DynamicJsonDocument doc(1024);
                String buf = cur_edit_event->getMessage();
                buf.replace("'","\"");
                DeserializationError err = deserializeJson(doc,buf);
                String isShowOff  = !err && doc.containsKey(FPSTR(TCONST_isShowOff)) ? doc[FPSTR(TCONST_isShowOff)] : String("0");
                String isPlayTime = !err && doc.containsKey(FPSTR(TCONST_isPlayTime)) ? doc[FPSTR(TCONST_isPlayTime)] : String("0");
                
                //String msg = !err && doc.containsKey(FPSTR(TCONST_msg)) ? doc[FPSTR(TCONST_msg)] : cur_edit_event->getMessage();

                interf->spacer("");
                //interf->text(FPSTR(TCONST_msg), msg, FPSTR(TINTF_070), false);
                interf->json_section_line();
                    interf->checkbox(FPSTR(TCONST_isShowOff), isShowOff, FPSTR(TINTF_0EC), false);
#ifdef MP3PLAYER
                    interf->checkbox(FPSTR(TCONST_isPlayTime), isPlayTime, FPSTR(TINTF_0ED), false);
#endif
                interf->json_section_end();
            }
            break;
        default:
            interf->text(FPSTR(TCONST_msg), cur_edit_event->getMessage(), FPSTR(TINTF_070), false);
            break;
    }
    interf->json_section_hidden(FPSTR(TCONST_repeat), FPSTR(TINTF_071));
        interf->json_section_line();
            interf->checkbox(FPSTR(TCONST_d1), (cur_edit_event->d1? "1" : "0"), FPSTR(TINTF_072), false);
            interf->checkbox(FPSTR(TCONST_d2), (cur_edit_event->d2? "1" : "0"), FPSTR(TINTF_073), false);
            interf->checkbox(FPSTR(TCONST_d3), (cur_edit_event->d3? "1" : "0"), FPSTR(TINTF_074), false);
            interf->checkbox(FPSTR(TCONST_d4), (cur_edit_event->d4? "1" : "0"), FPSTR(TINTF_075), false);
            interf->checkbox(FPSTR(TCONST_d5), (cur_edit_event->d5? "1" : "0"), FPSTR(TINTF_076), false);
            interf->checkbox(FPSTR(TCONST_d6), (cur_edit_event->d6? "1" : "0"), FPSTR(TINTF_077), false);
            interf->checkbox(FPSTR(TCONST_d7), (cur_edit_event->d7? "1" : "0"), FPSTR(TINTF_078), false);
        interf->json_section_end();
    interf->json_section_end();

    if (edit) {
        interf->hidden(FPSTR(TCONST_save), "1"); // режим редактирования
        interf->button_submit(FPSTR(TCONST_set_event), FPSTR(TINTF_079));
    } else {
        interf->hidden(FPSTR(TCONST_save), "0"); // режим добавления
        interf->button_submit(FPSTR(TCONST_set_event), FPSTR(TINTF_05D), FPSTR(P_GREEN));
    }

    interf->spacer();
    interf->button(FPSTR(TCONST_show_event), FPSTR(TINTF_00B));

    interf->json_section_end();
    interf->json_frame_flush();
}

void set_eventlist(Interface *interf, JsonObject *data){
    if (!data) return;
    
    if(cur_edit_event && cur_edit_event->getEvent()!=(*data)[FPSTR(TCONST_evList)].as<EVENT_TYPE>()){ // только если реально поменялось, то обновляем интерфейс
        show_event_conf(interf,data);
    } else if((*data).containsKey(FPSTR(TCONST_save))){ // эта часть срабатывает даже если нажата кнопка "обновить, следовательно ловим эту ситуацию"
        set_event_conf(interf, data); //через какую-то хитрую жопу отработает :)
    }
}
#ifdef ESP_USE_BUTTON
    void set_gaugetype(Interface *interf, JsonObject *data){
        if (!data) return;
        myLamp.setGaugeType((*data)[FPSTR(TCONST_EncVG)].as<GAUGETYPE>());
        save_lamp_flags();
    }
#endif

#ifdef ESP_USE_BUTTON
void block_settings_butt(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_show_button), FPSTR(TINTF_013));

    interf->checkbox(FPSTR(TCONST_Btn), myButtons->isButtonOn()? "1" : "0", FPSTR(TINTF_07B), true);
    interf->select(String(FPSTR(TCONST_EncVG)), String(myLamp.getLampSettings().GaugeType), String(FPSTR(TINTF_0DD)), true);
        interf->option(String(GAUGETYPE::GT_NONE), String(FPSTR(TINTF_0EE)));
        interf->option(String(GAUGETYPE::GT_VERT), String(FPSTR(TINTF_0EF)));
        interf->option(String(GAUGETYPE::GT_HORIZ), String(FPSTR(TINTF_0F0)));
    interf->json_section_end();
    interf->spacer();

    interf->json_section_begin(FPSTR(TCONST_butt_conf));
    interf->select(FPSTR(TCONST_buttList), String(0), String(FPSTR(TINTF_07A)), false);
    for (int i = 0; i < myButtons->size(); i++) {
        interf->option(String(i), (*myButtons)[i]->getName());
    }
    interf->json_section_end();

    interf->json_section_line();
    interf->button_submit_value(FPSTR(TCONST_butt_conf), FPSTR(TCONST_edit), FPSTR(TINTF_05C), FPSTR(P_GREEN));
    interf->button_submit_value(FPSTR(TCONST_butt_conf), FPSTR(TCONST_delete), FPSTR(TINTF_006), FPSTR(P_RED));
    interf->json_section_end();

    interf->json_section_end();

    interf->button(FPSTR(TCONST_butt_conf), FPSTR(TINTF_05D));

    interf->spacer();
    interf->button(FPSTR(TCONST_settings), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_butt(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_butt(interf, data);
    interf->json_frame_flush();
}

void set_butt_conf(Interface *interf, JsonObject *data){
    if (!data) return;
    Button *btn = nullptr;
    bool on = ((*data)[FPSTR(TCONST_on)] == "1");
    bool hold = ((*data)[FPSTR(TCONST_hold)] == "1");
    bool onetime = ((*data)[FPSTR(TCONST_onetime)] == "1");
    uint8_t clicks = (*data)[FPSTR(TCONST_clicks)];
    String param = (*data)[FPSTR(TCONST_bparam)].as<String>();
    BA action = (BA)(*data)[FPSTR(TCONST_bactList)].as<long>();

    if (data->containsKey(FPSTR(TCONST_buttList))) {
        int num = (*data)[FPSTR(TCONST_buttList)];
        if (num < myButtons->size()) {
            btn = (*myButtons)[num];
        }
    }
    if (btn) {
        btn->action = action;
        btn->flags.on = on;
        btn->flags.hold = hold;
        btn->flags.click = clicks;
        btn->flags.onetime = onetime;
        btn->setParam(param);
    } else {
        myButtons->add(new Button(on, hold, clicks, onetime, action, param));
    }

    myButtons->saveConfig();
    show_settings_butt(interf, data);
}

void show_butt_conf(Interface *interf, JsonObject *data){
    if (!interf || !data) return;

    Button *btn = nullptr;
    String act;
    int num = 0;

    if (data->containsKey(FPSTR(TCONST_buttList))) {
        num = (*data)[FPSTR(TCONST_buttList)];
        if (num < myButtons->size()) {
            act = (*data)[FPSTR(TCONST_butt_conf)].as<String>();
            btn = (*myButtons)[num];
        }
    }

    if (act == FPSTR(TCONST_delete)) {
        myButtons->remove(num);
        myButtons->saveConfig();
        show_settings_butt(interf, data);
        return;
    } else
    if (data->containsKey(FPSTR(TCONST_save))) {
        set_butt_conf(interf, data);
        return;
    }


    interf->json_frame_interface();

    if (btn) {
        interf->json_section_main(FPSTR(TCONST_set_butt), FPSTR(TINTF_05C));
        interf->constant(FPSTR(TCONST_buttList), String(num), btn->getName());
    } else {
        interf->json_section_main(FPSTR(TCONST_set_butt), FPSTR(TINTF_05D));
    }

    interf->select(FPSTR(TCONST_bactList), String(btn? btn->action : 0), String(FPSTR(TINTF_07A)), false);
    for (int i = 1; i < BA::BA_END; i++) {
        interf->option(String(i), FPSTR(btn_get_desc((BA)i)));
    }
    interf->json_section_end();

    interf->text(FPSTR(TCONST_bparam),(btn? btn->getParam() : String("")),FPSTR(TINTF_0B9),false);

    interf->checkbox(FPSTR(TCONST_on), (btn? btn->flags.on : 0)? "1" : "0", FPSTR(TINTF_07C), false);
    interf->checkbox(FPSTR(TCONST_hold), (btn? btn->flags.hold : 0)? "1" : "0", FPSTR(TINTF_07D), false);
    interf->number(String(FPSTR(TCONST_clicks)), String(btn? btn->flags.click : 0), String(FPSTR(TINTF_07E)), String(1), String(0), String(7));
    interf->checkbox(String(FPSTR(TCONST_onetime)), String((btn? btn->flags.onetime&1 : 0)? "1" : "0"), FPSTR(TINTF_07F), false);

    if (btn) {
        interf->hidden(FPSTR(TCONST_save), "1");
        interf->button_submit(FPSTR(TCONST_set_butt), FPSTR(TINTF_079));
    } else {
        interf->button_submit(FPSTR(TCONST_set_butt), FPSTR(TINTF_05D), FPSTR(P_GREEN));
    }

    interf->spacer();
    interf->button(FPSTR(TCONST_show_butt), FPSTR(TINTF_00B));

    interf->json_section_end();
    interf->json_frame_flush();
}

void set_btnflag(Interface *interf, JsonObject *data){
    if (!data) return;
    //SETPARAM(FPSTR(TCONST_Btn), myButtons->setButtonOn((*data)[FPSTR(TCONST_Btn)] == "1"));
    bool isSet = (*data)[FPSTR(TCONST_Btn)] == "1";
    myButtons->setButtonOn(isSet);
    myLamp.setButton(isSet);
    save_lamp_flags();
}
#endif  // BUTTON

#ifdef ENCODER
void block_settings_enc(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_set_enc), FPSTR(TINTF_0DC));

    interf->select(String(FPSTR(TCONST_EncVG)), String(myLamp.getLampSettings().GaugeType), String(FPSTR(TINTF_0DD)), true);
        interf->option(String(GAUGETYPE::GT_NONE), String(FPSTR(TINTF_0EE)));
        interf->option(String(GAUGETYPE::GT_VERT), String(FPSTR(TINTF_0EF)));
        interf->option(String(GAUGETYPE::GT_HORIZ), String(FPSTR(TINTF_0F0)));
    interf->json_section_end();
    interf->color(FPSTR(TCONST_EncVGCol), FPSTR(TINTF_0DE));
    interf->spacer();

    interf->color(FPSTR(TCONST_encTxtCol), FPSTR(TINTF_0DF));
    interf->range(FPSTR(TCONST_encTxtDel), String(110U-getEncTxtDelay()), String(10), String(100), String(5), String(FPSTR(TINTF_044)), false);
    interf->button_submit(FPSTR(TCONST_set_enc), FPSTR(TINTF_008), FPSTR(P_GRAY));
    interf->spacer();
    interf->button(FPSTR(TCONST_settings), FPSTR(TINTF_00B));
    interf->json_section_end();
}
void show_settings_enc(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_enc(interf, data);
    interf->json_frame_flush();
}
void set_settings_enc(Interface *interf, JsonObject *data){
    if (!data) return;

    myLamp.setGaugeType((*data)[FPSTR(TCONST_EncVG)].as<GAUGETYPE>());
    save_lamp_flags();
    SETPARAM(FPSTR(TCONST_EncVGCol));
    String tmpStr = (*data)[FPSTR(TCONST_EncVGCol)];
    tmpStr.replace(F("#"), F("0x"));
    GAUGE::GetGaugeInstance()->setGaugeTypeColor((CRGB)strtol(tmpStr.c_str(), NULL, 0));

    SETPARAM(FPSTR(TCONST_encTxtCol));
    String tmpStr2 = (*data)[FPSTR(TCONST_encTxtCol)];
    tmpStr2.replace(F("#"), F("0x"));
    setEncTxtColor((CRGB)strtol(tmpStr2.c_str(), NULL, 0));
    (*data)[FPSTR(TCONST_encTxtDel)]=JsonUInt(110U-(*data)[FPSTR(TCONST_encTxtDel)].as<int>());
    SETPARAM(FPSTR(TCONST_encTxtDel), setEncTxtDelay((*data)[FPSTR(TCONST_encTxtDel)]))
    section_settings_frame(interf, data);
}
#endif  // ENCODER

void set_debugflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setDebug((*data)[FPSTR(TCONST_debug)] == "1");
    save_lamp_flags();
}

void set_drawflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setDraw((*data)[FPSTR(TCONST_drawbuff)] == "1");
    save_lamp_flags();
}

#ifdef MP3PLAYER
void set_mp3flag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setONMP3((*data)[FPSTR(TCONST_isOnMP3)] == "1");
    if(myLamp.isLampOn())
        mp3->setIsOn(myLamp.isONMP3(), true); // при включенной лампе - форсировать воспроизведение
    else {
        mp3->setIsOn(myLamp.isONMP3(), false); // при выключенной - не форсировать, но произнести время, но не ранее чем через 10с после перезагрузки
        if(myLamp.isONMP3() && millis()>10000)
            if(!data->containsKey(FPSTR(TCONST_force)) || (data->containsKey(FPSTR(TCONST_force)) && (*data)[FPSTR(TCONST_force)] == "1")) // при наличие force="1" или без этого ключа
                mp3->playTime(embui.timeProcessor.getHours(), embui.timeProcessor.getMinutes(), (TIME_SOUND_TYPE)myLamp.getLampSettings().playTime);
    }
    save_lamp_flags();
}

void set_mp3volume(Interface *interf, JsonObject *data){
    if (!data) return;
    int volume = (*data)[FPSTR(TCONST_mp3volume)];
    SETPARAM(FPSTR(TCONST_mp3volume), mp3->setVolume(volume));
}

void set_mp3_player(Interface *interf, JsonObject *data){
    if (!data) return;

    if(!myLamp.isONMP3()) return;
    uint16_t cur_palyingnb = mp3->getCurPlayingNb();
    if(data->containsKey(FPSTR(CMD_MP3_PREV))){
        mp3->playEffect(cur_palyingnb-1,"");
    } else if(data->containsKey(FPSTR(CMD_MP3_NEXT))){
        mp3->playEffect(cur_palyingnb+1,"");
    } else if(data->containsKey(FPSTR(TCONST_mp3_p5))){
        mp3->playEffect(cur_palyingnb-5,"");
    } else if(data->containsKey(FPSTR(TCONST_mp3_n5))){
        mp3->playEffect(cur_palyingnb+5,"");
    }
}

#endif


void section_effects_frame(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_080));
    block_effects_main(interf, data);
    interf->json_frame_flush();
}

void section_text_frame(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_080));
    block_lamptext(interf, data);
    interf->json_frame_flush();
}

void section_drawing_frame(Interface *interf, JsonObject *data){
    // Рисование
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_080));
    block_drawing(interf, data);
    interf->json_frame_flush();
}
#ifdef USE_STREAMING
void block_streaming(Interface *interf, JsonObject *data){
    //Страница "Трансляция"
    interf->json_section_main(FPSTR(TCONST_streaming), FPSTR(TINTF_0E2));
        interf->json_section_line();
            interf->checkbox(FPSTR(TCONST_ONflag), String(myLamp.isLampOn()), FPSTR(TINTF_00E), true);
            interf->checkbox(FPSTR(TCONST_isStreamOn), myLamp.isStreamOn() ? F("1") : F("0"), FPSTR(TINTF_0E2), true);
            interf->checkbox(FPSTR(TCONST_direct), myLamp.isDirect() ? F("1") : F("0"), FPSTR(TINTF_0E6), true);
            interf->checkbox(FPSTR(TCONST_mapping), myLamp.isMapping() ? F("1") : F("0"), FPSTR(TINTF_0E7), true);
        interf->json_section_end();
        interf->select(FPSTR(TCONST_stream_type), embui.param(FPSTR(TCONST_stream_type)), (String)FPSTR(TINTF_0E3), true);
            interf->option(String(E131), FPSTR(TINTF_0E4));
            interf->option(String(SOUL_MATE), FPSTR(TINTF_0E5));
        interf->json_section_end();
        interf->range(FPSTR(TCONST_bright), (String)myLamp.getBrightness(), F("0"), F("255"), F("1"), (String)FPSTR(TINTF_00D), true);
        if (embui.param(FPSTR(TCONST_stream_type)).toInt() == E131){
            interf->range(FPSTR(TCONST_Universe), embui.param(FPSTR(TCONST_Universe)), F("1"), F("255"), F("1"), (String)FPSTR(TINTF_0E8), true);
            interf->comment(String(F("Universes:")) + String(ceil((float)HEIGHT / (512U / (WIDTH * 3))), 0U) + String(F(";    X:")) + String(WIDTH) + String(F(";    Y:")) + String(512U / (WIDTH * 3)));
            interf->comment(String(F("Как настроить разметку матрицы в Jinx! можно посмотреть <a href=\"https://community.alexgyver.ru/threads/wifi-lampa-budilnik-proshivka-firelamp_jeeui-gpl.2739/page-454#post-103219\">на форуме</a>")));
        }
    interf->json_section_end();
}
void section_streaming_frame(Interface *interf, JsonObject *data){
    // Трансляция
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_080));
    block_streaming(interf, data);
    interf->json_frame_flush();
}

void set_streaming(Interface *interf, JsonObject *data){
    if (!data) return;
    bool flag = (*data)[FPSTR(TCONST_isStreamOn)] == "1";
    myLamp.setStream(flag);
    LOG(printf_P, PSTR("Stream set %d \n"), flag);
    if (flag) {
        STREAM_TYPE type = (STREAM_TYPE)embui.param(FPSTR(TCONST_stream_type)).toInt();
        if (ledStream) {
            if (ledStream->getStreamType() != type){
                Led_Stream::clearStreamObj();
            }
        }
        Led_Stream::newStreamObj(type);
    }
    else {
        Led_Stream::clearStreamObj();
    }
    save_lamp_flags();
}

void set_streaming_drirect(Interface *interf, JsonObject *data){
    if (!data) return;
    bool flag = (*data)[FPSTR(TCONST_direct)] == "1";
    myLamp.setDirect(flag);
    if (ledStream){
        if (flag) {
#ifdef EXT_STREAM_BUFFER
            myLamp.setStreamBuff(false);
#else
            myLamp.clearDrawBuf();
#endif
            myLamp.effectsTimer(T_DISABLE);
            FastLED.clear();
            FastLED.show();
        }
        else {
            myLamp.effectsTimer(T_ENABLE);
#ifdef EXT_STREAM_BUFFER
            myLamp.setStreamBuff(true);
#else
            if (!myLamp.isDrawOn())             // TODO: переделать с запоминанием старого стейта
                myLamp.setDrawBuff(true);
#endif
        }
    }
    save_lamp_flags();
}
void set_streaming_mapping(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setMapping((*data)[FPSTR(TCONST_mapping)] == "1");
    save_lamp_flags();
}
void set_streaming_bright(Interface *interf, JsonObject *data){
    if (!data) return;
    remote_action(RA_CONTROL, (String(FPSTR(TCONST_dynCtrl))+F("0")).c_str(), String((*data)[FPSTR(TCONST_bright)].as<String>()).c_str(), NULL);
}

void set_streaming_type(Interface *interf, JsonObject *data){
    if (!data) return;
    SETPARAM(FPSTR(TCONST_stream_type));
    STREAM_TYPE type = (STREAM_TYPE)(*data)[FPSTR(TCONST_stream_type)].as<int>();
    LOG(printf_P, PSTR("Stream Type %d \n"), type);
    if (myLamp.isStreamOn()) {
        if (ledStream) {
            if (ledStream->getStreamType() == type)
                return;
            Led_Stream::clearStreamObj();
        }
        Led_Stream::newStreamObj(type);
    }
    section_streaming_frame(interf, data);
}

void set_streaming_universe(Interface *interf, JsonObject *data){
    if (!data) return;
    SETPARAM(FPSTR(TCONST_Universe));
    if (ledStream) {
        if (ledStream->getStreamType() == E131) {
            Led_Stream::newStreamObj(E131);
        }
    }
}
#endif
// Точка входа в настройки
void user_settings_frame(Interface *interf, JsonObject *data);

void section_settings_frame(Interface *interf, JsonObject *data){
    // Страница "Настройки"
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_080));

    interf->json_section_main(FPSTR(T_SETTINGS), FPSTR(TINTF_002));
#ifdef OPTIONS_PASSWORD
    if(!myLamp.getLampState().isOptPass){
        interf->json_section_line(FPSTR(TCONST_set_opt_pass));
            interf->password(FPSTR(TCONST_opt_pass), FPSTR(TINTF_02D));
            interf->button_submit(FPSTR(TCONST_set_opt_pass), FPSTR(TINTF_01F), "", 19);
        interf->json_section_end();
    } else {
        interf->button(FPSTR(T_SH_TIME), FPSTR(TINTF_051));
        interf->button(FPSTR(T_SH_NETW), FPSTR(TINTF_081));
        user_settings_frame(interf, data);
        interf->spacer();
        block_settings_update(interf, data);
    }
#else
    interf->button(FPSTR(T_SH_TIME), FPSTR(TINTF_051));
    interf->button(FPSTR(T_SH_NETW), FPSTR(TINTF_081));
    user_settings_frame(interf, data);
    interf->spacer();
    block_settings_update(interf, data);
#endif
    interf->json_section_end();
    interf->json_frame_flush();
}

#ifdef OPTIONS_PASSWORD
void set_opt_pass(Interface *interf, JsonObject *data){
    if(!data) return;

    if((*data)[FPSTR(TCONST_opt_pass)]==OPTIONS_PASSWORD){
        LOG(println, F("Options unlocked for 10 minutes"));
        myLamp.getLampState().isOptPass = true;
        Task *_t = new Task(TASK_MINUTE*10, TASK_ONCE, [](){ myLamp.getLampState().isOptPass = false; }, &ts, false, nullptr, nullptr, true ); // через 10 минут отключаем
        _t->enableDelayed();
        section_settings_frame(interf, nullptr);
    }
}
#endif  // OPTIONS_PASSWORD

void user_settings_frame(Interface *interf, JsonObject *data){
if (!interf) return;
#ifdef MIC_EFFECTS
    interf->button(FPSTR(TCONST_show_mic), FPSTR(TINTF_020));
#endif
#ifdef MP3PLAYER
    interf->button(FPSTR(TCONST_show_mp3), FPSTR(TINTF_099));
#endif

#ifdef ESP_USE_BUTTON
    interf->button(FPSTR(TCONST_show_butt), FPSTR(TINTF_013));
#endif
#ifdef ENCODER
    interf->button(FPSTR(TCONST_encoder), FPSTR(TINTF_0DC));
#endif
    interf->button(FPSTR(TCONST_show_other), FPSTR(TINTF_082));
#ifdef SHOWSYSCONFIG
    if(myLamp.isShowSysMenu())
        interf->button(FPSTR(TCONST_ESPsysSettings), FPSTR(TINTF_08F));
#endif
#ifndef MOOT
    block_lamp_config(interf, data);
#endif

}

void section_main_frame(Interface *interf, JsonObject *data){
    if (!interf) return;

    interf->json_frame_interface(FPSTR(TINTF_080));

    block_menu(interf, data);
    block_effects_main(interf, data);

    interf->json_frame_flush();

    if(!embui.sysData.wifi_sta && embui.param(FPSTR(P_WIFIMODE))=="0"){
        // форсируем выбор вкладки настройки WiFi если контроллер не подключен к внешней AP
        interf->json_frame_interface();
            interf->json_section_main(FPSTR(T_SET_WIFI), FPSTR(TINTF_028));
            block_only_wifi(interf, data);
            interf->json_section_end();
        interf->json_frame_flush();
        if(!EmbUI::GetInstance()->sysData.isWiFiScanning){ // автосканирование при входе в настройки
            EmbUI::GetInstance()->sysData.isWiFiScanning = true;
            set_scan_wifi(interf, data);
        }
    }
}

void section_sys_settings_frame(Interface *interf, JsonObject *data){
    // Страница "Настройки ESP"
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_08F));

    block_menu(interf, data);
    interf->json_section_main(FPSTR(TCONST_sysSettings), FPSTR(TINTF_08F));
        interf->spacer(FPSTR(TINTF_092)); // заголовок
        interf->json_section_line(FPSTR(TINTF_092)); // расположить в одной линии
#ifdef ESP_USE_BUTTON
            interf->number(FPSTR(TCONST_PINB),FPSTR(TINTF_094),String(1),String(0),String(16));
#endif
#ifdef MP3PLAYER
            interf->number(FPSTR(TCONST_PINMP3RX),FPSTR(TINTF_097),String(1),String(0),String(16));
            interf->number(FPSTR(TCONST_PINMP3TX),FPSTR(TINTF_098),String(1),String(0),String(16));
#endif
        interf->json_section_end(); // конец контейнера
        interf->spacer();
        interf->number(FPSTR(TCONST_CLmt),FPSTR(TINTF_095),String(100),String(0),String(16000));

        //interf->json_section_main(FPSTR(TCONST_edit), "");
        interf->iframe(FPSTR(TCONST_edit), FPSTR(TCONST_edit));
        //interf->json_section_end();

        interf->button_submit(FPSTR(TCONST_sysSettings), FPSTR(TINTF_008), FPSTR(P_GRAY));

        interf->spacer();
        interf->button(FPSTR(TCONST_settings), FPSTR(TINTF_00B));
    interf->json_section_end();
    
    interf->json_frame_flush();
}

void set_sys_settings(Interface *interf, JsonObject *data){
    if(!data) return;

#ifdef ESP_USE_BUTTON
    {String tmpChk = (*data)[FPSTR(TCONST_PINB)]; if(tmpChk.toInt()>16) return;}
#endif
#ifdef MP3PLAYER
    {String tmpChk = (*data)[FPSTR(TCONST_PINMP3RX)]; if(tmpChk.toInt()>16) return;}
    {String tmpChk = (*data)[FPSTR(TCONST_PINMP3TX)]; if(tmpChk.toInt()>16) return;}
#endif
    {String tmpChk = (*data)[FPSTR(TCONST_CLmt)]; if(tmpChk.toInt()>16000) return;}

#ifdef ESP_USE_BUTTON
    SETPARAM(FPSTR(TCONST_PINB));
#endif
#ifdef MP3PLAYER
    SETPARAM(FPSTR(TCONST_PINMP3RX));
    SETPARAM(FPSTR(TCONST_PINMP3TX));
#endif
    SETPARAM(FPSTR(TCONST_CLmt));

    if(!embui.sysData.isWSConnect){ // если последние 5 секунд не было коннекта, защита от зацикливания ребута
        myLamp.sendString(String(FPSTR(TINTF_096)).c_str(), CRGB::Red, true);
        new Task(TASK_SECOND, TASK_ONCE, nullptr, &ts, true, nullptr, [](){ embui.autosave(true); LOG(println, F("Rebooting...")); remote_action(RA::RA_REBOOT, NULL, NULL); });
    }
    section_effects_frame(interf,data);
}

void set_lamp_flags(Interface *interf, JsonObject *data){
    if(!data) return;
    SETPARAM(FPSTR(TCONST_syslampFlags));
}

void save_lamp_flags(){
    DynamicJsonDocument doc(160);
    JsonObject obj = doc.to<JsonObject>();
    obj[FPSTR(TCONST_syslampFlags)] = ulltos(myLamp.getLampFlags());
    set_lamp_flags(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
}

// кастомный обработчик, для реализации особой обработки событий сокетов
bool ws_action_handle(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    bool res = false; // false == EmbUI default action
    switch(type){
        case AwsEventType::WS_EVT_ERROR :
            {
                resetAutoTimers();
                uint16_t effNum = myLamp.effects.getSelected();
                myLamp.effects.directMoveBy(EFF_NONE);
                myLamp.effects.removeConfig(effNum);
                myLamp.effects.directMoveBy(effNum);
                //remote_action(RA_EFFECT, String(effNum).c_str(), NULL);
                String tmpStr=F("- ");
                tmpStr+=effNum;
                myLamp.sendString(tmpStr.c_str(), CRGB::Red);

                res = true;
                break;
            }
        default :
            res = false; 
            break;
    }
    return res;
}

// кастомный обработчик, для поддержки приложения WLED APP ( https://play.google.com/store/apps/details?id=com.aircoookie.WLED )
bool notfound_handle(AsyncWebServerRequest *request, const String& req)
{
    if (!(req.indexOf(F("win")) == 1)) return false;
    LOG(println,req);

    uint8_t bright = myLamp.getLampBrightness();
    if ((req.indexOf(F("&T=2")) > 1)){
        if(myLamp.isLampOn()){
            remote_action(RA::RA_OFF, NULL);
            bright = 0;
        }
        else
            remote_action(RA::RA_ON, NULL);
    }

    if ((req.indexOf(F("&A=")) > 1)){
        bright = req.substring(req.indexOf(F("&A="))+3).toInt();
        if(bright)
            remote_action(RA::RA_BRIGHT_NF, (String(FPSTR(TCONST_dynCtrl))+"0").c_str(), String(bright).c_str(), NULL);
    }

    String result = F("<?xml version=\"1.0\" ?><vs><ac>");
    result.concat(myLamp.isLampOn()?bright:0);
    result.concat(F("</ac><ds>"));
    result.concat(embui.param(FPSTR(P_hostname)));
    result.concat(F(".local-")); //lampname.local-IP
    result.concat(WiFi.localIP().toString());
    result.concat(F("</ds></vs>"));

    request->send(200, FPSTR(PGmimexml), result);
    return true;
}

/**
 * Набор конфигурационных переменных и обработчиков интерфейса
 */
void create_parameters(){
    LOG(println, F("Создание дефолтных параметров"));
    // создаем дефолтные параметры для нашего проекта
    embui.var_create(FPSTR(TCONST_syslampFlags), ulltos(myLamp.getLampFlags())); // Дефолтный набор флагов
    embui.var_create(FPSTR(TCONST_effListMain), F("1"));   // "effListMain"
    embui.var_create(FPSTR(P_m_tupd), String(DEFAULT_MQTTPUB_INTERVAL)); // "m_tupd" интервал отправки данных по MQTT в секундах (параметр в энергонезависимой памяти)

    //WiFi
    embui.var_create(FPSTR(P_hostname), F(""));
    embui.var_create(FPSTR(P_WIFIMODE), String("0"));       // STA/AP/AP+STA, STA by default
    embui.var_create(FPSTR(P_APpwd), "");                   // пароль внутренней точки доступа

    // параметры подключения к MQTT
    embui.var_create(FPSTR(P_m_host), F("")); // Дефолтные настройки для MQTT
    embui.var_create(FPSTR(P_m_port), F("1883"));
    embui.var_create(FPSTR(P_m_user), F(""));
    embui.var_create(FPSTR(P_m_pass), F(""));
    embui.var_create(FPSTR(P_m_pref), embui.mc);  // m_pref == MAC по дефолту
    embui.var_create(FPSTR(TCONST_fileName), F("cfg1.json")); // "fileName"

#ifdef AUX_PIN
    embui.var_create(FPSTR(TCONST_AUX), "0");
#endif
    embui.var_create(FPSTR(TCONST_msg), F(""));
    embui.var_create(FPSTR(TCONST_txtColor), FPSTR(TCONST__ffffff));
    embui.var_create(FPSTR(TCONST_txtBfade), String(FADETOBLACKVALUE));
    embui.var_create(FPSTR(TCONST_txtSpeed), F("100"));
    embui.var_create(FPSTR(TCONST_txtOf), F("0"));
    embui.var_create(FPSTR(TCONST_effSort), F("1"));
    embui.var_create(FPSTR(TCONST_GlobBRI), F("127"));

    // date/time related vars
/*
    embui.var_create(FPSTR(TCONST_0057), "");
    embui.var_create(FPSTR(TCONST_0058), "");
*/
    embui.var_create(FPSTR(TCONST_ny_period), F("0"));
    embui.var_create(FPSTR(TCONST_ny_unix), FPSTR(TCONST_1609459200));

#ifdef MIC_EFFECTS
    embui.var_create(FPSTR(TCONST_micScale),F("1.28"));
    embui.var_create(FPSTR(TCONST_micNoise),F("0.00"));
    embui.var_create(FPSTR(TCONST_micnRdcLvl),F("0"));
#endif

#ifdef RESTORE_STATE
    embui.var_create(FPSTR(TCONST_Demo), "0");
#endif

    embui.var_create(FPSTR(TCONST_DTimer), String(60)); // Дефолтное значение, настраивается из UI
    embui.var_create(FPSTR(TCONST_alarmPT), String(F("85"))); // 5<<4+5, старшие и младшие 4 байта содержат 5

    embui.var_create(FPSTR(TCONST_spdcf), String(F("1.0")));

    // пины и системные настройки
#ifdef ESP_USE_BUTTON
    embui.var_create(FPSTR(TCONST_PINB), String(BTN_PIN)); // Пин кнопки
    embui.var_create(FPSTR(TCONST_EncVG), String(GAUGETYPE::GT_VERT));         // Тип шкалы
#endif
#ifdef ENCODER
    embui.var_create(FPSTR(TCONST_encTxtCol), F("#FFA500"));  // Дефолтный цвет текста (Orange)
    embui.var_create(FPSTR(TCONST_encTxtDel), F("40"));        // Задержка прокрутки текста
    embui.var_create(FPSTR(TCONST_EncVG), String(GAUGETYPE::GT_VERT));  // Тип шкалы
    embui.var_create(FPSTR(TCONST_EncVGCol), F("#FF2A00"));  // Дефолтный цвет шкалы
#endif

#ifdef MP3PLAYER
    embui.var_create(FPSTR(TCONST_PINMP3RX), String(MP3_RX_PIN)); // Пин RX плеера
    embui.var_create(FPSTR(TCONST_PINMP3TX), String(MP3_TX_PIN)); // Пин TX плеера
    embui.var_create(FPSTR(TCONST_mp3volume),F("15")); // громкость
    embui.var_create(FPSTR(TCONST_mp3count),F("255")); // кол-во файлов в папке mp3
#endif
#ifdef TM1637_CLOCK
    embui.var_create(FPSTR(TCONST_tmBright), String(F("82"))); // 5<<4+5, старшие и младшие 4 байта содержат 5
    // embui.var_create(FPSTR(TCONST_tmBrightOn), F("5"));   // Яркость при вкл
    // embui.var_create(FPSTR(TCONST_tmBrightOff), F("1"));    // Яркость при выкл
#endif
    embui.var_create(FPSTR(TCONST_CLmt), String(CURRENT_LIMIT)); // Лимит по току
#ifdef USE_STREAMING
    embui.var_create(FPSTR(TCONST_stream_type), String(SOUL_MATE)); // Тип трансляции
    embui.var_create(FPSTR(TCONST_Universe), F("1")); // Universe для E1.31
#endif
    // далее идут обработчики параметров

   /**
    * регистрируем статические секции для web-интерфейса с системными настройками,
    * сюда входит:
    *  - WiFi-manager
    *  - установка часового пояса/правил перехода сезонного времени
    *  - установка текущей даты/времени вручную
    *  - базовые настройки MQTT
    *  - OTA обновление прошивки и образа файловой системы
    */
    BasicUI::add_sections(true); //

    embui.section_handle_add(FPSTR(TCONST_sysSettings), set_sys_settings);

    embui.section_handle_add(FPSTR(TCONST_syslampFlags), set_lamp_flags);

    embui.section_handle_add(FPSTR(TCONST_main), section_main_frame);
    embui.section_handle_add(FPSTR(TCONST_show_flags), show_main_flags);

    embui.section_handle_add(FPSTR(TCONST_effects), section_effects_frame);
    embui.section_handle_add(FPSTR(TCONST_effects_param), show_effects_param);
    embui.section_handle_add(FPSTR(TCONST_effListMain), set_effects_list);
    embui.section_handle_add(FPSTR(TCONST_dynCtrl_), set_effects_dynCtrl);

    embui.section_handle_add(FPSTR(TCONST_eff_prev), set_eff_prev);
    embui.section_handle_add(FPSTR(TCONST_eff_next), set_eff_next);

    embui.section_handle_add(FPSTR(TCONST_effects_config), show_effects_config);
    embui.section_handle_add(FPSTR(TCONST_effListConf), set_effects_config_list);
    embui.section_handle_add(FPSTR(TCONST_set_effect), set_effects_config_param);

    embui.section_handle_add(FPSTR(TCONST_ONflag), set_onflag);
    embui.section_handle_add(FPSTR(TCONST_Demo), set_demoflag);
    embui.section_handle_add(FPSTR(TCONST_GBR), set_gbrflag);
#ifdef AUX_PIN
    embui.section_handle_add(FPSTR(TCONST_AUX), set_auxflag);
#endif
    embui.section_handle_add(FPSTR(TCONST_drawing), section_drawing_frame);
#ifdef USE_STREAMING    
    embui.section_handle_add(FPSTR(TCONST_streaming), section_streaming_frame);
    embui.section_handle_add(FPSTR(TCONST_isStreamOn), set_streaming);
    embui.section_handle_add(FPSTR(TCONST_stream_type), set_streaming_type);
    embui.section_handle_add(FPSTR(TCONST_direct), set_streaming_drirect);
    embui.section_handle_add(FPSTR(TCONST_mapping), set_streaming_mapping);
    embui.section_handle_add(FPSTR(TCONST_Universe), set_streaming_universe);
    embui.section_handle_add(FPSTR(TCONST_bright), set_streaming_bright);
#endif
    embui.section_handle_add(FPSTR(TCONST_ESPsysSettings), section_sys_settings_frame);
    embui.section_handle_add(FPSTR(TCONST_lamptext), section_text_frame);
    embui.section_handle_add(FPSTR(TCONST_textsend), set_lamp_textsend);
    embui.section_handle_add(FPSTR(TCONST_add_lamp_config), edit_lamp_config);
    embui.section_handle_add(FPSTR(TCONST_edit_lamp_config), edit_lamp_config);

    embui.section_handle_add(FPSTR(TCONST_edit_text_config), set_text_config);
    embui.section_handle_add(FPSTR(TCONST_drawing_ctrl_), set_drawing);
    embui.section_handle_add(FPSTR(TCONST_drawClear), set_clear);
    embui.section_handle_add(FPSTR(TCONST_drawbuff), set_drawflag);

    // меняю обработчики для страницы настроек :)
    embui.section_handle_remove(FPSTR(T_SETTINGS));
    embui.section_handle_add(FPSTR(T_SETTINGS), section_settings_frame); // своя главная страница настроек, со своим переводом

    embui.section_handle_remove(FPSTR(T_SH_NETW)); // своя страница настроек сети, со своим переводом
    embui.section_handle_add(FPSTR(T_SH_NETW), show_settings_wifi);

    embui.section_handle_remove(FPSTR(T_SH_TIME)); // своя страница настроек времени, со своим переводом
    embui.section_handle_add(FPSTR(T_SH_TIME), show_settings_time);

    embui.section_handle_remove(FPSTR(T_SET_WIFI));
    embui.section_handle_add(FPSTR(T_SET_WIFI), set_settings_wifi);

    embui.section_handle_remove(FPSTR(T_SET_TIME));
    embui.section_handle_add(FPSTR(T_SET_TIME), set_settings_time);
#ifdef EMBUI_USE_MQTT
    embui.section_handle_remove(FPSTR(T_SET_MQTT));
    embui.section_handle_add(FPSTR(T_SET_MQTT), set_settings_mqtt);
#endif
#ifdef EMBUI_USE_FTP
    embui.section_handle_remove(FPSTR(T_SET_FTP));
    embui.section_handle_add(FPSTR(T_SET_FTP), set_ftp);
#endif

    embui.section_handle_remove(FPSTR(T_SET_SCAN));
    embui.section_handle_add(FPSTR(T_SET_SCAN), set_scan_wifi);         // обработка сканирования WiFi

    embui.section_handle_add(FPSTR(TCONST_show_other), show_settings_other);
    embui.section_handle_add(FPSTR(TCONST_set_other), set_settings_other);

    #ifdef OPTIONS_PASSWORD
    embui.section_handle_add(FPSTR(TCONST_set_opt_pass), set_opt_pass);
    #endif // OPTIONS_PASSWORD

#ifdef MIC_EFFECTS
    embui.section_handle_add(FPSTR(TCONST_show_mic), show_settings_mic);
    embui.section_handle_add(FPSTR(TCONST_set_mic), set_settings_mic);
    embui.section_handle_add(FPSTR(TCONST_Mic), set_micflag);
    embui.section_handle_add(FPSTR(TCONST_mic_cal), set_settings_mic_calib);
#endif
    embui.section_handle_add(FPSTR(TCONST_show_event), show_settings_event);
    embui.section_handle_add(FPSTR(TCONST_event_conf), show_event_conf);
    embui.section_handle_add(FPSTR(TCONST_set_event), set_event_conf);
    embui.section_handle_add(FPSTR(TCONST_Events), set_eventflag);
    embui.section_handle_add(FPSTR(TCONST_evList), set_eventlist);
#ifdef ESP_USE_BUTTON
    embui.section_handle_add(FPSTR(TCONST_show_butt), show_settings_butt);
    embui.section_handle_add(FPSTR(TCONST_butt_conf), show_butt_conf);
    embui.section_handle_add(FPSTR(TCONST_set_butt), set_butt_conf);
    embui.section_handle_add(FPSTR(TCONST_Btn), set_btnflag);
    embui.section_handle_add(FPSTR(TCONST_EncVG), set_gaugetype);
#endif

#ifdef LAMP_DEBUG
    embui.section_handle_add(FPSTR(TCONST_debug), set_debugflag);
#endif

#ifdef MP3PLAYER
    embui.section_handle_add(FPSTR(TCONST_isOnMP3), set_mp3flag);
    embui.section_handle_add(FPSTR(TCONST_mp3volume), set_mp3volume);
    embui.section_handle_add(FPSTR(TCONST_show_mp3), show_settings_mp3);
    embui.section_handle_add(FPSTR(TCONST_set_mp3), set_settings_mp3);

    embui.section_handle_add(FPSTR(CMD_MP3_PREV), set_mp3_player);
    embui.section_handle_add(FPSTR(CMD_MP3_NEXT), set_mp3_player);
    embui.section_handle_add(FPSTR(TCONST_mp3_p5), set_mp3_player);
    embui.section_handle_add(FPSTR(TCONST_mp3_n5), set_mp3_player);
#endif
#ifdef ENCODER
    embui.section_handle_add(FPSTR(TCONST_encoder), show_settings_enc);
    embui.section_handle_add(FPSTR(TCONST_set_enc), set_settings_enc);
#endif
}

void sync_parameters(){
    DynamicJsonDocument doc(1024);
    //https://arduinojson.org/v6/api/jsondocument/
    //JsonDocument::to<T>() clears the document and converts it to the specified type. Don’t confuse this function with JsonDocument::as<T>() that returns a reference only if the requested type matches the one in the document.
    JsonObject obj = doc.to<JsonObject>();

    if(check_recovery_state(true)){
        LOG(printf_P,PSTR("Critical Error: Lamp recovered from corrupted effect number: %s\n"),String(embui.param(FPSTR(TCONST_effListMain))).c_str());
        embui.var(FPSTR(TCONST_effListMain),String(0)); // что-то пошло не так, был циклический ребут, сбрасываем эффект
    }

#ifdef EMBUI_USE_MQTT
    myLamp.setmqtt_int(embui.param(FPSTR(P_m_tupd)).toInt());
#endif

    String syslampFlags(embui.param(FPSTR(TCONST_syslampFlags)));
    LAMPFLAGS tmp;
    tmp.lampflags = stoull(syslampFlags); //atol(embui.param(FPSTR(TCONST_syslampFlags)).c_str());
//#ifndef ESP32
//    LOG(printf_P, PSTR("tmp.lampflags=%llu (%s)\n"), tmp.lampflags, syslampFlags.c_str());
//#endif
    LOG(printf_P, PSTR("tmp.lampflags=%llu\n"), tmp.lampflags);

    obj[FPSTR(TCONST_drawbuff)] = tmp.isDraw ? "1" : "0";
    set_drawflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>(); // https://arduinojson.org/v6/how-to/reuse-a-json-document/

#ifdef LAMP_DEBUG
    obj[FPSTR(TCONST_debug)] = tmp.isDebug ? "1" : "0";
    set_debugflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
#endif

    //LOG(printf_P,PSTR("tmp.isEventsHandled=%d\n"), tmp.isEventsHandled);
    obj[FPSTR(TCONST_Events)] = tmp.isEventsHandled ? "1" : "0";
    CALL_INTF_OBJ(set_eventflag);
    //set_eventflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
    embui.timeProcessor.attach_callback(std::bind(&LAMP::setIsEventsHandled, &myLamp, myLamp.IsEventsHandled())); // только после синка будет понятно включены ли события

    myLamp.setGlobalBrightness(embui.param(FPSTR(TCONST_GlobBRI)).toInt()); // починить бросок яркости в 255 при первом включении
    obj[FPSTR(TCONST_GBR)] = tmp.isGlobalBrightness ? "1" : "0";
    set_gbrflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

#ifdef RESTORE_STATE
    obj[FPSTR(TCONST_ONflag)] = tmp.ONflag ? "1" : "0";
    if(tmp.ONflag){ // если лампа включена, то устанавливаем эффект ДО включения
        CALL_SETTER(FPSTR(TCONST_effListMain), embui.param(FPSTR(TCONST_effListMain)), set_effects_list);
    }
    set_onflag(nullptr, &obj);
    if(!tmp.ONflag){ // иначе - после
        CALL_SETTER(FPSTR(TCONST_effListMain), embui.param(FPSTR(TCONST_effListMain)), set_effects_list);
    }
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
    if(myLamp.isLampOn())
        CALL_SETTER(FPSTR(TCONST_Demo), embui.param(FPSTR(TCONST_Demo)), set_demoflag); // Демо через режимы, для него нужнен отдельный флаг :(
#else
    CALL_SETTER(FPSTR(TCONST_effListMain), embui.param(FPSTR(TCONST_effListMain)), set_effects_list);
#endif

    if(tmp.isGlobalBrightness)
        CALL_SETTER(String(FPSTR(TCONST_dynCtrl)) + "0", myLamp.getLampBrightness(), set_effects_dynCtrl);

#ifdef MP3PLAYER
Task *t = new Task(DFPLAYER_START_DELAY+500, TASK_ONCE, nullptr, &ts, false, nullptr, [tmp](){
    if(!mp3->isReady()){
        LOG(println, F("DFPlayer not ready yet..."));
        if(millis()<10000){
            ts.getCurrentTask()->restartDelayed(TASK_SECOND*2);
            return;
        }
    }
    
    DynamicJsonDocument doc(1024);
    //https://arduinojson.org/v6/api/jsondocument/
    //JsonDocument::to<T>() clears the document and converts it to the specified type. Don’t confuse this function with JsonDocument::as<T>() that returns a reference only if the requested type matches the one in the document.
    JsonObject obj = doc.to<JsonObject>();
    //obj[FPSTR(TCONST_mp3volume)] = embui.param(FPSTR(TCONST_mp3volume));  // пишет в плеер!
    obj[FPSTR(TCONST_playTime)] = tmp.playTime;
    obj[FPSTR(TCONST_playName)] = tmp.playName ? "1" : "0";
    obj[FPSTR(TCONST_playEffect)] = tmp.playEffect ? "1" : "0";
    obj[FPSTR(TCONST_alarmSound)] = String(tmp.alarmSound);
    obj[FPSTR(TCONST_eqSetings)] = String(tmp.MP3eq); // пишет в плеер!
    obj[FPSTR(TCONST_playMP3)] = tmp.playMP3 ? "1" : "0";
    obj[FPSTR(TCONST_mp3count)] = embui.param(FPSTR(TCONST_mp3count));
    obj[FPSTR(TCONST_limitAlarmVolume)] = tmp.limitAlarmVolume ? "1" : "0";

    set_settings_mp3(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    mp3->setupplayer(myLamp.effects.getEn(), myLamp.effects.getSoundfile()); // установить начальные значения звука
    obj[FPSTR(TCONST_isOnMP3)] = tmp.isOnMP3 ? "1" : "0";
    set_mp3flag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    CALL_SETTER(FPSTR(TCONST_mp3volume), embui.param(FPSTR(TCONST_mp3volume)), set_mp3volume);
}, true);
t->enableDelayed();
#endif

#ifdef AUX_PIN
    CALL_SETTER(FPSTR(TCONST_AUX), embui.param(FPSTR(TCONST_AUX)), set_auxflag);
#endif

    myLamp.setClearingFlag(tmp.isEffClearing);

    obj[FPSTR(TCONST_numInList)] = tmp.numInList ? "1" : "0";
    myLamp.setNumInList(tmp.numInList);
#ifdef MIC_EFFECTS
    obj[FPSTR(TCONST_effHasMic)] = tmp.effHasMic ? "1" : "0";
    myLamp.setEffHasMic(tmp.effHasMic);
#endif
    SORT_TYPE type = (SORT_TYPE)embui.param(FPSTR(TCONST_effSort)).toInt();
    obj[FPSTR(TCONST_effSort)] = type;
    set_effects_config_param(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

#ifdef ESP_USE_BUTTON
    obj[FPSTR(TCONST_Btn)] = tmp.isBtn ? "1" : "0";
    CALL_INTF_OBJ(set_btnflag);
    obj[FPSTR(TCONST_EncVG)] = String(tmp.GaugeType);
    CALL_INTF_OBJ(set_gaugetype);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
#endif
#ifdef ENCODER
    obj[FPSTR(TCONST_encTxtCol)] = embui.param(FPSTR(TCONST_encTxtCol));
    obj[FPSTR(TCONST_encTxtDel)] = (110U - embui.param(FPSTR(TCONST_encTxtDel)).toInt());
    obj[FPSTR(TCONST_EncVG)] = tmp.GaugeType ? "1" : "0";;
    obj[FPSTR(TCONST_EncVGCol)] = embui.param(FPSTR(TCONST_EncVGCol));
    set_settings_enc(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
#endif

    obj[FPSTR(TCONST_txtSpeed)] = String(110U - embui.param(FPSTR(TCONST_txtSpeed)).toInt());
    obj[FPSTR(TCONST_txtOf)] = embui.param(FPSTR(TCONST_txtOf));
    obj[FPSTR(TCONST_ny_period)] = embui.param(FPSTR(TCONST_ny_period));
    obj[FPSTR(TCONST_txtBfade)] = embui.param(FPSTR(TCONST_txtBfade));

    String datetime;
    TimeProcessor::getDateTimeString(datetime, embui.param(FPSTR(TCONST_ny_unix)).toInt());
    obj[FPSTR(TCONST_ny_unix)] = datetime;
    
    set_text_config(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

#ifdef USE_STREAMING
    obj[FPSTR(TCONST_isStreamOn)] = tmp.isStream ? "1" : "0";
    set_streaming(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    obj[FPSTR(TCONST_direct)] = tmp.isDirect ? "1" : "0";
    set_streaming_drirect(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    obj[FPSTR(TCONST_mapping)] = tmp.isMapping ? "1" : "0";
    set_streaming_mapping(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    obj[FPSTR(TCONST_stream_type)] = embui.param(FPSTR(TCONST_stream_type));
    set_streaming_type(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    obj[FPSTR(TCONST_Universe)] = embui.param(FPSTR(TCONST_Universe));
    set_streaming_universe(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
#endif


    obj[FPSTR(TCONST_isFaderON)] = tmp.isFaderON ? "1" : "0";
    obj[FPSTR(TCONST_isClearing)] = tmp.isEffClearing ? "1" : "0";
    obj[FPSTR(TCONST_MIRR_H)] = tmp.MIRR_H ? "1" : "0";
    obj[FPSTR(TCONST_MIRR_V)] = tmp.MIRR_V ? "1" : "0";
    obj[FPSTR(TCONST_DRand)] = tmp.dRand ? "1" : "0";
    obj[FPSTR(TCONST_showName)] = tmp.showName ? "1" : "0";
    obj[FPSTR(TCONST_isShowSysMenu)] = tmp.isShowSysMenu ? "1" : "0";

#ifdef TM1637_CLOCK
    uint8_t tmBright = embui.param(FPSTR(TCONST_tmBright)).toInt();
    obj[FPSTR(TCONST_tmBrightOn)] = tmBright>>4;
    obj[FPSTR(TCONST_tmBrightOff)] = tmBright&0x0F;
    obj[FPSTR(TCONST_tm24)] = tmp.tm24 ? "1" : "0";
    obj[FPSTR(TCONST_tmZero)] = tmp.tmZero ? "1" : "0";
    #ifdef DS18B20
    obj[FPSTR(TCONST_ds18b20)] = tmp.isTempOn ? "1" : "0";
    #endif
#endif

    uint8_t alarmPT = embui.param(FPSTR(TCONST_alarmPT)).toInt();
    obj[FPSTR(TCONST_alarmP)] = alarmPT>>4;
    obj[FPSTR(TCONST_alarmT)] = alarmPT&0x0F;

    obj[FPSTR(TCONST_spdcf)] = embui.param(FPSTR(TCONST_spdcf));

    set_settings_other(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

#ifdef MIC_EFFECTS
    obj[FPSTR(TCONST_Mic)] = tmp.isMicOn ? "1" : "0";
    myLamp.getLampState().setMicAnalyseDivider(0);
    set_micflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    // float scale = atof(embui.param(FPSTR(TCONST_micScale)).c_str());
    // float noise = atof(embui.param(FPSTR(TCONST_micNoise)).c_str());
    // mic_noise_reduce_level_t lvl=(mic_noise_reduce_level_t)embui.param(FPSTR(TCONST_micnRdcLvl)).toInt();

    obj[FPSTR(TCONST_micScale)] = embui.param(FPSTR(TCONST_micScale)); //scale;
    obj[FPSTR(TCONST_micNoise)] = embui.param(FPSTR(TCONST_micNoise)); //noise;
    obj[FPSTR(TCONST_micnRdcLvl)] = embui.param(FPSTR(TCONST_micnRdcLvl)); //lvl;
    set_settings_mic(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
#endif

    //save_lamp_flags(); // обновить состояние флагов (закомментированно, окончательно состояние установится через 0.3 секунды, после set_settings_other)

    //--------------- начальная инициализация состояния
    myLamp.getLampState().freeHeap = ESP.getFreeHeap();
#ifdef ESP8266
    FSInfo fs_info;
    LittleFS.info(fs_info);
    myLamp.getLampState().fsfreespace = fs_info.totalBytes-fs_info.usedBytes;
    myLamp.getLampState().HeapFragmentation = ESP.getHeapFragmentation();
#endif
#ifdef ESP32
    myLamp.getLampState().fsfreespace = LittleFS.totalBytes() - LittleFS.usedBytes();
    myLamp.getLampState().HeapFragmentation = 0;
#endif
    //--------------- начальная инициализация состояния

    check_recovery_state(false); // удаляем маркер, считаем что у нас все хорошо...
    Task *_t = new Task(TASK_SECOND, TASK_ONCE, [](){ // откладыаем задачу на 1 секунду, т.к. выше есть тоже отложенные инициализации, см. set_settings_other()
        myLamp.getLampState().isInitCompleted = true; // ставим признак того, что инициализация уже завершилась, больше его не менять и должен быть в самом конце sync_parameters() !!!
    }, &ts, false, nullptr, nullptr, true);
    _t->enableDelayed();
    LOG(println, F("sync_parameters() done"));
}

// обработка эвентов лампы
void event_worker(DEV_EVENT *event){
    RA action = RA_UNKNOWN;
    LOG(printf_P, PSTR("%s - %s\n"), ((DEV_EVENT *)event)->getName().c_str(), embui.timeProcessor.getFormattedShortTime().c_str());

    switch (event->getEvent()) {
    case EVENT_TYPE::ON: action = RA_ON; break;
    case EVENT_TYPE::OFF: action = RA_OFF; break;
    case EVENT_TYPE::DEMO: action = RA_DEMO; break;
    case EVENT_TYPE::ALARM: action = RA_ALARM; break;
    case EVENT_TYPE::LAMP_CONFIG_LOAD: action = RA_LAMP_CONFIG; break;
#ifdef ESP_USE_BUTTON
    case EVENT_TYPE::BUTTONS_CONFIG_LOAD:  action = RA_BUTTONS_CONFIG; break;
#endif
    case EVENT_TYPE::EFF_CONFIG_LOAD:  action = RA_EFF_CONFIG; break;
    case EVENT_TYPE::EVENTS_CONFIG_LOAD: action = RA_EVENTS_CONFIG; break;
    case EVENT_TYPE::SEND_TEXT:  action = RA_SEND_TEXT; break;
    case EVENT_TYPE::SEND_TIME:  action = RA_SEND_TIME; break;
#ifdef AUX_PIN
    case EVENT_TYPE::AUX_ON: action = RA_AUX_ON; break;
    case EVENT_TYPE::AUX_OFF: action = RA_AUX_OFF; break;
    case EVENT_TYPE::AUX_TOGGLE: action = RA_AUX_TOGLE; break;
#endif
    case EVENT_TYPE::PIN_STATE: {
        if ((event->getMessage()).isEmpty()) break;

        String tmpS = event->getMessage();
        tmpS.replace(F("'"),F("\"")); // так делать не красиво, но шопаделаешь...
        StaticJsonDocument<256> doc;
        deserializeJson(doc, tmpS);
        JsonArray arr = doc.as<JsonArray>();
        for (size_t i = 0; i < arr.size(); i++) {
            JsonObject item = arr[i];
            uint8_t pin = item[FPSTR(TCONST_pin)].as<int>();
            String action = item[FPSTR(TCONST_act)].as<String>();
            pinMode(pin, OUTPUT);
            switch(action.c_str()[0]){
                case 'H':
                    digitalWrite(pin, HIGH); // LOW
                    break;
                case 'L':
                    digitalWrite(pin, LOW); // LOW
                    break;
                case 'T':
                    digitalWrite(pin, !digitalRead(pin)); // inverse
                    break;
                default:
                    break;
            }
        }
        break;
    }
    case EVENT_TYPE::SET_EFFECT: action = RA_EFFECT; break;
    case EVENT_TYPE::SET_WARNING: action = RA_WARNING; break;
    case EVENT_TYPE::SET_GLOBAL_BRIGHT: action = RA_GLOBAL_BRIGHT; break;
    case EVENT_TYPE::SET_WHITE_HI: action = RA_WHITE_HI; break;
    case EVENT_TYPE::SET_WHITE_LO: action = RA_WHITE_LO; break;
    default:;
    }

    remote_action(action, event->getMessage().c_str(), NULL);
}

void show_progress(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    interf->json_section_hidden(FPSTR(T_DO_OTAUPD), String(FPSTR(TINTF_056)) + String(F(" : ")) + (*data)[FPSTR(TINTF_05A)].as<String>()+ String("%"));
    interf->json_section_end();
    interf->json_frame_flush();
}

uint8_t uploadProgress(size_t len, size_t total){
    DynamicJsonDocument doc(256);
    JsonObject obj = doc.to<JsonObject>();
    static int prev = 0; // используется чтобы не выводить повторно предыдущее значение, хрен с ней, пусть живет
    float part = total / 50.0;
    int curr = len / part;
    uint8_t progress = 100*len/total;
    if (curr != prev) {
        prev = curr;
        for (int i = 0; i < curr; i++) Serial.print(F("="));
        Serial.print(F("\n"));
        obj[FPSTR(TINTF_05A)] = String(progress);
        CALL_INTF_OBJ(show_progress);
    }
    if (myLamp.getGaugeType()!=GAUGETYPE::GT_NONE){
        GAUGE::GaugeShow(len, total, 100);
    }
    return progress;
}

// Функции обработчики и другие служебные
#ifdef ESP_USE_BUTTON
void default_buttons(){
    myButtons->clear();
    // Выключена
    myButtons->add(new Button(false, false, 1, true, BA::BA_ON)); // 1 клик - ON
    myButtons->add(new Button(false, false, 2, true, BA::BA_DEMO)); // 2 клика - Демо
    myButtons->add(new Button(false, true, 0, true, BA::BA_WHITE_LO)); // удержание Включаем белую лампу в мин яркость
    myButtons->add(new Button(false, true, 1, true, BA::BA_WHITE_HI)); // удержание + 1 клик Включаем белую лампу в полную яркость
    myButtons->add(new Button(false, true, 0, false, BA::BA_BRIGHT)); // удержание из выключенного - яркость
    myButtons->add(new Button(false, true, 1, false, BA::BA_BRIGHT)); // удержание из выключенного - яркость

    // Включена
    myButtons->add(new Button(true, false, 1, true, BA::BA_OFF)); // 1 клик - OFF
    myButtons->add(new Button(true, false, 2, true, BA::BA_EFF_NEXT)); // 2 клика - след эффект
    myButtons->add(new Button(true, false, 3, true, BA::BA_EFF_PREV)); // 3 клика - пред эффект
    myButtons->add(new Button(true, false, 5, true, BA::BA_SEND_IP)); // 5 клика - показ IP
    myButtons->add(new Button(true, false, 6, true, BA::BA_SEND_TIME)); // 6 клика - показ времени
    myButtons->add(new Button(true, false, 7, true, BA::BA_EFFECT, String(F("253")))); // 7 кликов - эффект часы
    myButtons->add(new Button(true, true, 0, false, BA::BA_BRIGHT)); // удержание яркость
    myButtons->add(new Button(true, true, 1, false, BA::BA_SPEED)); // удержание + 1 клик скорость
    myButtons->add(new Button(true, true, 2, false, BA::BA_SCALE)); // удержание + 2 клика масштаб
}
#endif


// набор акшенов, которые дергаются из всех мест со всех сторон
void remote_action(RA action, ...){
    LOG(printf_P, PSTR("RA %d: "), action);
    DynamicJsonDocument doc(512);
    JsonObject obj = doc.to<JsonObject>();

    char *key = NULL, *val = NULL, *value = NULL;
    va_list prm;
    va_start(prm, action);
    while ((key = (char *)va_arg(prm, char *)) && (val = (char *)va_arg(prm, char *))) {
        LOG(printf_P, PSTR("%s = %s"), key, val);
        obj[key] = val;
    }
    va_end(prm);
    if (key && !val) {
        value = key;
        LOG(printf_P, PSTR("%s"), value);
    }
    LOG(println);

    switch (action) {
        case RA::RA_ON:
            CALL_INTF(FPSTR(TCONST_ONflag), "1", set_onflag);
            if(value){
                StringTask *t = new StringTask(value, 3 * TASK_SECOND, TASK_ONCE, nullptr, &ts, false, nullptr,  [](){
                    StringTask *cur = (StringTask *)ts.getCurrentTask();
                    remote_action(RA::RA_SEND_TEXT, cur->getData(), NULL);
                }, true);
                t->enableDelayed();
            }
            break;
        case RA::RA_OFF: {
                // нажатие кнопки точно отключает ДЕМО и белую лампу возвращая в нормальный режим
                myLamp.stopRGB(); // выключение RGB-режима
                if(value){
                   remote_action(RA::RA_SEND_TEXT, value, NULL);
                }
                new Task(500, TASK_FOREVER, [value](){
                    if((!myLamp.isPrintingNow() && value) || !value){ // отложенное выключение только для случая когда сообщение выводится в этом же экшене, а не чужое
                        Task *task = ts.getCurrentTask();
                        DynamicJsonDocument doc(512);
                        JsonObject obj = doc.to<JsonObject>();
                        LAMPMODE mode = myLamp.getMode();
                        if(mode!=LAMPMODE::MODE_NORMAL){
                            CALL_INTF(FPSTR(TCONST_Demo), "0", set_demoflag); // отключить демо, если было включено
                            if (myLamp.IsGlobalBrightness()) {
                                embui.var(FPSTR(TCONST_GlobBRI), String(myLamp.getLampBrightness())); // сохранить восстановленную яркость в конфиг, если она глобальная
                            }
                        }
                        CALL_INTF(FPSTR(TCONST_ONflag), "0", set_onflag);
                        task->disable();
                    }
                }, &ts, true, nullptr, nullptr, true);
            }
            break;
        case RA::RA_DEMO:
            CALL_INTF(FPSTR(TCONST_ONflag), "1", set_onflag); // включим, если было отключено
            if(value && String(value)=="0"){
                CALL_INTF(FPSTR(TCONST_Demo), "0", set_demoflag);
                myLamp.startNormalMode();
            } else {
                CALL_INTF(FPSTR(TCONST_Demo), "1", set_demoflag);
                resetAutoTimers();
                myLamp.startDemoMode();
            }
            break;
        // trigger effect change in Demo mode
        case RA::RA_DEMO_NEXT:
            if (myLamp.getLampSettings().dRand) {
                myLamp.switcheffect(SW_RND, myLamp.getFaderFlag());
            } else {
                myLamp.switcheffect(SW_NEXT_DEMO, myLamp.getFaderFlag());
            }
            // postponed action to publish eff changes
            new Task(TASK_SECOND, TASK_ONCE, nullptr, &ts, true, nullptr, [](){ remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL); }, true);
            break;
        // called on effect change events
        case RA::RA_EFFECT: {
            LAMPMODE mode=myLamp.getMode();
            if(mode==LAMPMODE::MODE_WHITELAMP && myLamp.effects.getSelected()!=1){
                myLamp.startNormalMode(true);
                StaticJsonDocument<200>doc;
                JsonObject obj = doc.to<JsonObject>();
                CALL_INTF(FPSTR(TCONST_ONflag), !myLamp.isLampOn() ? "1" : "0", set_onflag);
                break;
            } else if(mode==LAMPMODE::MODE_NORMAL){
                embui.var(FPSTR(TCONST_effListMain), value); // сохранить в конфиг изменившийся эффект
            }
            CALL_INTF(FPSTR(TCONST_effListMain), value, set_effects_list); // публикация будет здесь
            break;
        }
        case RA::RA_GLOBAL_BRIGHT:
            if (atoi(value) > 0){
                CALL_INTF(FPSTR(TCONST_GBR), F("1"), set_gbrflag);
                return remote_action(RA_CONTROL, (String(FPSTR(TCONST_dynCtrl))+F("0")).c_str(), value, NULL);
            }
            else
                CALL_INTF(FPSTR(TCONST_GBR), value, set_gbrflag);
            break;
        case RA::RA_BRIGHT_NF:
            obj[FPSTR(TCONST_nofade)] = true;
            obj[FPSTR(TCONST_force)] = true;
            //CALL_INTF_OBJ(set_effects_dynCtrl);
            set_effects_dynCtrl(nullptr, &obj);
            break;
        case RA::RA_CONTROL:
            //CALL_INTF_OBJ(set_effects_dynCtrl);
            obj[FPSTR(TCONST_force)] = true;
            set_effects_dynCtrl(nullptr, &obj);
            break;
#ifdef MP3PLAYER
        case RA::RA_MP3_PREV:
            if(!myLamp.isONMP3()) return;
            mp3->playEffect(mp3->getCurPlayingNb()-(int)value,"");
            break;
        case RA::RA_MP3_NEXT:
            if(!myLamp.isONMP3()) return;
            mp3->playEffect(mp3->getCurPlayingNb()+(int)value,"");
            break;
        case RA::RA_MP3_SOUND:
            if(!myLamp.isONMP3()) return;
            mp3->playEffect((int)value,"");
            break;
        case RA::RA_PLAYERONOFF:
            obj[FPSTR(TCONST_force)] = "0"; // не озвучивать время
            CALL_INTF(FPSTR(TCONST_isOnMP3), value, set_mp3flag);
            break;
        case RA::RA_MP3_VOL:
            if(!myLamp.isONMP3()) return;
            obj[FPSTR(TCONST_mp3volume)] = atoi(value);
            set_mp3volume(nullptr, &obj);
            break;
#endif
#ifdef MIC_EFFECTS
        case RA::RA_MIC:
            CALL_INTF_OBJ(show_settings_mic);
            break;
        case RA::RA_MICONOFF:
            CALL_INTF(FPSTR(TCONST_Mic), value, set_micflag);
            break;
#endif
        case RA::RA_EFF_NEXT:
            resetAutoTimers(); // сборс таймера демо, если есть перемещение
            myLamp.switcheffect(SW_NEXT, myLamp.getFaderFlag());
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_EFF_PREV:
            resetAutoTimers(); // сборс таймера демо, если есть перемещение
            myLamp.switcheffect(SW_PREV, myLamp.getFaderFlag());
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_EFF_RAND:
            myLamp.switcheffect(SW_RND, myLamp.getFaderFlag());
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_WHITE_HI:
            myLamp.switcheffect(SW_WHITE_HI);
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_WHITE_LO:
            myLamp.switcheffect(SW_WHITE_LO);
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_ALARM:
            ALARMTASK::startAlarm(&myLamp, value);
            break;
        case RA::RA_ALARM_OFF:
            ALARMTASK::stopAlarm();
            break;
        case RA::RA_REBOOT: {
                remote_action(RA::RA_WARNING, F("[16711680,3000,500]"), NULL);
                Task *t = new Task(3 * TASK_SECOND, TASK_ONCE, nullptr, &ts, false, nullptr, [](){ ESP.restart(); });
                t->enableDelayed();
            }
            break;
        case RA::RA_WIFI_REC:
            //CALL_INTF(FPSTR(TINTF_028), FPSTR(TCONST_STA), BasicUI::set_settings_wifi);
            CALL_INTF(FPSTR(TINTF_028), FPSTR(TCONST_STA), set_settings_wifi);
            break;
        case RA::RA_LAMP_CONFIG:
            if (value && *value) {
                String filename = String(FPSTR(TCONST__backup_glb_));
                filename.concat(value);
                embui.load(filename.c_str());
                sync_parameters();
            }
            break;
        case RA::RA_EFF_CONFIG:
            if (value && *value) {
                String filename = String(FPSTR(TCONST__backup_idx_));
                filename.concat(value);
                myLamp.effects.initDefault(filename.c_str());
            }
            break;
#ifdef ESP_USE_BUTTON
        case RA::RA_BUTTONS_CONFIG:
            if (value && *value) {
                String filename = String(FPSTR(TCONST__backup_btn_));
                filename.concat(value);
                myButtons->clear();
                if (!myButtons->loadConfig()) {
                    default_buttons();
                }
            }
            break;
#endif
        case RA::RA_EVENTS_CONFIG:
            if (value && *value) {
                String filename = String(FPSTR(TCONST__backup_evn_));
                filename.concat(value);
                myLamp.events.loadConfig(filename.c_str());
            }
            break;
        case RA::RA_SEND_TEXT: {
            String tmpStr = embui.param(FPSTR(TCONST_txtColor));
            if (value && *value) {
                String tmpStr = embui.param(FPSTR(TCONST_txtColor));
                tmpStr.replace(F("#"),F("0x"));
                CRGB::HTMLColorCode color = (CRGB::HTMLColorCode)strtol(tmpStr.c_str(), NULL, 0);

                myLamp.sendString(value, color);
            }
            break;
        }
        case RA::RA_WARNING: {
            String str=value;
            String msg;
            DynamicJsonDocument doc(256);
            deserializeJson(doc,str);
            JsonArray arr = doc.as<JsonArray>();
            uint32_t col=CRGB::Red, dur=1000, per=250, type=0;

            for (size_t i = 0; i < arr.size(); i++) {
                switch(i){
                    case 0: {
                        String tmpStr = arr[i];
                        tmpStr.replace(F("#"), F("0x"));
                        long val = strtol(tmpStr.c_str(), NULL, 0);
                        col = val;
                        break;
                    }
                    case 1: dur = arr[i]; break;
                    case 2: per = arr[i]; break;
                    case 3: type = arr[i]; break;
                    case 4: msg = arr[i].as<String>(); break;
                    default : break;
                }
			}
            myLamp.showWarning(col,dur,per,type,true,msg.isEmpty()?(const char *)nullptr:msg.c_str());
            break; 
        }

        case RA::RA_DRAW: {
            String str=value;
            DynamicJsonDocument doc(256);
            deserializeJson(doc,str);
            JsonArray arr = doc.as<JsonArray>();
            CRGB col=CRGB::White;
            uint16_t x=WIDTH/2U, y=HEIGHT/2U;

            for (size_t i = 0; i < arr.size(); i++) {
                switch(i){
                    case 0: {
                        String tmpStr = arr[i];
                        tmpStr.replace(F("#"), F("0x"));
                        unsigned long val = strtol(tmpStr.c_str(), NULL, 0);
                        LOG(printf_P, PSTR("%s:%ld\n"), tmpStr.c_str(), val);
                        col = val;
                        break;
                    }
                    case 1: x = arr[i]; break;
                    case 2: y = arr[i]; break;
                    default : break;
                }
			}
            myLamp.writeDrawBuf(col,x,y);
            break; 
        }
        case RA::RA_RGB: {
            String tmpStr = value;
            if(tmpStr.indexOf(",")!=-1){
                int16_t pos = 0;
                int16_t frompos = 0;
                uint8_t val = 0;
                uint32_t res = 0;
                do {
                    frompos = pos;
                    pos = tmpStr.indexOf(",", pos);
                    if(pos!=-1){
                        val = tmpStr.substring(frompos,pos).toInt();
                        res=(res<<8)|val;
                        pos++;
                    } else if(frompos<(signed)tmpStr.length()){
                        val = tmpStr.substring(frompos,tmpStr.length()).toInt();
                        res=(res<<8)|val; 
                    }
                } while(pos!=-1);
                CRGB color=CRGB(res);
                myLamp.startRGB(color);
                break;
            }
            tmpStr.replace(F("#"), F("0x"));
            long val = strtol(tmpStr.c_str(), NULL, 0);
            LOG(printf_P, PSTR("%s:%ld\n"), tmpStr.c_str(), val);
            CRGB color=CRGB(val);
            myLamp.startRGB(color);
            break; 
        }
        case RA::RA_FILLMATRIX: {
            String tmpStr = value;
            if(tmpStr.indexOf(",")!=-1){
                int16_t pos = 0;
                int16_t frompos = 0;
                uint8_t val = 0;
                uint32_t res = 0;
                do {
                    frompos = pos;
                    pos = tmpStr.indexOf(",", pos);
                    if(pos!=-1){
                        val = tmpStr.substring(frompos,pos).toInt();
                        res=(res<<8)|val;
                        pos++;
                    } else if(frompos<(signed)tmpStr.length()){
                        val = tmpStr.substring(frompos,tmpStr.length()).toInt();
                        res=(res<<8)|val; 
                    }
                } while(pos!=-1);
                LOG(printf_P,PSTR("RA_FILLMATRIX: %d\n"), res);
                CRGB color=CRGB(res);
                myLamp.fillDrawBuf(color);
                break;
            }
            tmpStr.replace(F("#"), F("0x"));
            long val = strtol(tmpStr.c_str(), NULL, 0);
            LOG(printf_P, PSTR("%s:%ld\n"), tmpStr.c_str(), val);
            CRGB color=CRGB(val);
            myLamp.fillDrawBuf(color);
            break; 
        }

        case RA::RA_SEND_IP:
            myLamp.sendString(WiFi.localIP().toString().c_str(), CRGB::White);
#ifdef TM1637_CLOCK
            tm1637.setIpShow();
#endif
            break;
        case RA::RA_SEND_TIME:
            myLamp.periodicTimeHandle(value, true);
            //myLamp.sendString(String(F("%TM")).c_str(), CRGB::Green);
            break;
#ifdef AUX_PIN
        case RA::RA_AUX_ON:
            obj[FPSTR(TCONST_AUX)] = true;
            set_auxflag(nullptr, &obj);
            CALL_INTF(FPSTR(TCONST_AUX), "1", set_auxflag);
            break;
        case RA::RA_AUX_OFF:
            obj[FPSTR(TCONST_AUX)] = false;
            set_auxflag(nullptr, &obj);
            CALL_INTF(FPSTR(TCONST_AUX), "0", set_auxflag);
            break;
        case RA::RA_AUX_TOGLE:
            AUX_toggle(!digitalRead(AUX_PIN));
            CALL_INTF(FPSTR(TCONST_AUX), digitalRead(AUX_PIN) == AUX_LEVEL ? "1" : "0", set_auxflag);
            break;
#endif
        default:
            break;
    }
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
}

String httpCallback(const String &param, const String &value, bool isset){
    String result = F("Ok");
    String upperParam = param;
    upperParam.toUpperCase();
    RA action = RA_UNKNOWN;
    LOG(printf_P, PSTR("HTTP: %s - %s\n"), upperParam.c_str(), value.c_str());

    if(!isset) {
        LOG(println, F("GET"));
        if (upperParam == FPSTR(CMD_ON))
            { result = myLamp.isLampOn() ? "1" : "0"; }
        else if (upperParam == FPSTR(CMD_OFF))
            { result = !myLamp.isLampOn() ? "1" : "0"; }
        else if (upperParam == FPSTR(CMD_G_BRIGHT))
            { result = myLamp.IsGlobalBrightness() ? "1" : "0"; }
        else if (upperParam == FPSTR(CMD_DEMO))
            { result = myLamp.getMode() == LAMPMODE::MODE_DEMO ? "1" : "0"; }
#ifdef MP3PLAYER
        else if (upperParam == FPSTR(CMD_PLAYER)) 
            { result = myLamp.isONMP3() ? "1" : "0"; }
        else if (upperParam == FPSTR(CMD_MP3_SOUND)) 
            { result = String(mp3->getCurPlayingNb()); }
        else if (upperParam == FPSTR(CMD_MP3_PREV)) { action = RA_MP3_PREV; remote_action(action, "1", NULL); }
        else if (upperParam == FPSTR(CMD_MP3_NEXT)) { action = RA_MP3_NEXT; remote_action(action, "1", NULL); }
#endif
#ifdef MIC_EFFECTS
        else if (upperParam == FPSTR(CMD_MIC)) 
            { result = myLamp.isMicOnOff() ? "1" : "0"; }
#endif
        else if (upperParam == FPSTR(CMD_EFFECT))
            { result = String(myLamp.effects.getCurrent());  }
        else if (upperParam == FPSTR(CMD_WARNING))
            { myLamp.showWarning(CRGB::Orange,5000,500); }
        else if (upperParam == FPSTR(CMD_EFF_CONFIG)) {
                String result = myLamp.effects.getSerializedEffConfig(myLamp.effects.getCurrent(), myLamp.getNormalizedLampBrightness());
#ifdef EMBUI_USE_MQTT
                embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_eff_config), result, true);
#endif
                return result;
            }
        else if (upperParam == FPSTR(CMD_CONTROL)) {
            LList<std::shared_ptr<UIControl>>&controls = myLamp.effects.getControls();
            for(unsigned i=0; i<controls.size();i++){
                if(value == String(controls[i]->getId())){
                    result = String(F("[")) + controls[i]->getId() + String(F(",\"")) + (controls[i]->getId()==0 ? String(myLamp.getNormalizedLampBrightness()) : controls[i]->getVal()) + String(F("\"]"));
#ifdef EMBUI_USE_MQTT
                    embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_control), result, true);
#endif
                    return result;
                }
            }
        }
        else if (upperParam == FPSTR(CMD_LIST))  {
            result = F("[");
            bool first=true;
            EffectListElem *eff = nullptr;
            String effname((char *)0);
            while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
                result = result + String(first ? F("") : F(",")) + eff->eff_nb;
                first=false;
            }
            result = result + F("]");
        }
        else if (upperParam == FPSTR(CMD_SHOWLIST))  {
            result = F("[");
            bool first=true;
            EffectListElem *eff = nullptr;
            String effname((char *)0);
            while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
                if (eff->canBeSelected()) {
                    result = result + String(first ? F("") : F(",")) + eff->eff_nb;
                    first=false;
                }
            }
            result = result + F("]");
        }
        else if (upperParam == FPSTR(CMD_DEMOLIST))  {
            result = F("[");
            bool first=true;
            EffectListElem *eff = nullptr;
            String effname((char *)0);
            while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
                if (eff->isFavorite()) {
                    result = result + String(first ? F("") : F(",")) + eff->eff_nb;
                    first=false;
                }
            }
            result = result + F("]");
        }
        else if (upperParam == FPSTR(CMD_EFF_NAME))  {
            String effname((char *)0);
            uint16_t effnum = String(value).toInt();
            effnum = effnum ? effnum : myLamp.effects.getCurrent();
            myLamp.effects.loadeffname(effname, effnum);
            result = String(F("["))+effnum+String(",\"")+effname+String("\"]");
        }
        else if (upperParam == FPSTR(CMD_EFF_ONAME))  {
            String effname((char *)0);
            uint16_t effnum = String(value).toInt();
            effnum = effnum ? effnum : myLamp.effects.getCurrent();
            effname = FPSTR(T_EFFNAMEID[(uint8_t)effnum]);
            result = String(F("["))+effnum+String(",\"")+effname+String("\"]");
        }
        else if (upperParam == FPSTR(CMD_MOVE_NEXT)) { action = RA_EFF_NEXT;  remote_action(action, value.c_str(), NULL); }
        else if (upperParam == FPSTR(CMD_MOVE_PREV)) { action = RA_EFF_PREV;  remote_action(action, value.c_str(), NULL); }
        else if (upperParam == FPSTR(CMD_MOVE_RND)) { action = RA_EFF_RAND;  remote_action(action, value.c_str(), NULL); }
        else if (upperParam == FPSTR(CMD_REBOOT)) { action = RA_REBOOT;  remote_action(action, value.c_str(), NULL); }
        else if (upperParam == FPSTR(CMD_ALARM)) { result = myLamp.isAlarm() ? "1" : "0"; }
        else if (upperParam == FPSTR(CMD_MATRIX)) { char buf[32]; sprintf_P(buf, PSTR("[%d,%d]"), WIDTH, HEIGHT);  result = buf; }
#ifdef EMBUI_USE_MQTT        
        embui.publish(String(FPSTR(TCONST_embui_pub_)) + upperParam, result, true);
#endif
        return result;
    } else {
        LOG(println, F("SET"));
        if (upperParam == FPSTR(CMD_ON)) { action = (value!="0" ? RA_ON : RA_OFF); remote_action(action, NULL, NULL); return result; }
        else if (upperParam == FPSTR(CMD_OFF)) { action = (value!="0" ? RA_OFF : RA_ON); remote_action(action, NULL, NULL); return result; }
        else if (upperParam == FPSTR(CMD_DEMO)) action = RA_DEMO;
        else if (upperParam == FPSTR(CMD_MSG)) action = RA_SEND_TEXT;
        else if (upperParam == FPSTR(CMD_EFFECT)) action = RA_EFFECT;
        else if (upperParam == FPSTR(CMD_MOVE_NEXT)) action = RA_EFF_NEXT;
        else if (upperParam == FPSTR(CMD_MOVE_PREV)) action = RA_EFF_PREV;
        else if (upperParam == FPSTR(CMD_MOVE_RND)) action = RA_EFF_RAND;
        else if (upperParam == FPSTR(CMD_REBOOT)) action = RA_REBOOT;
        else if (upperParam == FPSTR(CMD_ALARM)) action = RA_ALARM;
        else if (upperParam == FPSTR(CMD_G_BRIGHT)) action = RA_GLOBAL_BRIGHT;
        else if (upperParam == FPSTR(CMD_WARNING)) action = RA_WARNING;
        else if (upperParam == FPSTR(CMD_DRAW)) action = RA_DRAW;
        else if (upperParam == FPSTR(CMD_FILL_MATRIX)) action = RA_FILLMATRIX;
        else if (upperParam == FPSTR(CMD_RGB)) action = RA_RGB;
#ifdef MP3PLAYER
        else if (upperParam == FPSTR(CMD_MP3_PREV)) action = RA_MP3_PREV;
        else if (upperParam == FPSTR(CMD_MP3_NEXT)) action = RA_MP3_NEXT;
        else if (upperParam == FPSTR(CMD_MP3_SOUND)) action = RA_MP3_SOUND;
        else if (upperParam == FPSTR(CMD_PLAYER)) action = RA_PLAYERONOFF;
        else if (upperParam == FPSTR(CMD_MP3_VOLUME)) { action = RA_MP3_VOL; remote_action(action, value.c_str(), NULL); return result; }
#endif
#ifdef MIC_EFFECTS
        else if (upperParam == FPSTR(CMD_MIC)) action = RA_MICONOFF;
#endif
        //else if (upperParam.startsWith(FPSTR(TCONST_dynCtrl))) { action = RA_CONTROL; remote_action(action, upperParam.c_str(), value.c_str(), NULL); return result; }
        else if (upperParam == FPSTR(CMD_EFF_CONFIG)) {
            return httpCallback(upperParam, "", false); // set пока не реализована
        }
        else if (upperParam == FPSTR(CMD_CONTROL) || upperParam == FPSTR(CMD_INC_CONTROL)) {
            String str=value;
            DynamicJsonDocument doc(256);
            deserializeJson(doc,str);
            JsonArray arr = doc.as<JsonArray>();
            uint16_t id=0;
            String val="";

            if(arr.size()<2){ // мало параметров, т.е. это GET команда, возвращаем состояние контрола
                return httpCallback(FPSTR(CMD_CONTROL), value, false);
            }

            if(upperParam == FPSTR(CMD_INC_CONTROL)){ // это команда увеличения контрола на значение, соотвественно получаем текущее
                val = arr[1].as<String>().toInt();
                str = httpCallback(FPSTR(CMD_CONTROL), arr[0], false);
                doc.clear(); doc.garbageCollect();
                deserializeJson(doc,str);
                arr = doc.as<JsonArray>();
                arr[1] = arr[1].as<String>().toInt()+val.toInt();
            }

            for (size_t i = 0; i < arr.size(); i++) {
                switch(i){
                    case 0: {
                        id = arr[i].as<uint16_t>();
                        break;
                    }
                    case 1: val = arr[i].as<String>(); break;
                    default : break;
                }
			}
            remote_action(RA_CONTROL, (String(FPSTR(TCONST_dynCtrl))+id).c_str(), val.c_str(), NULL);
            //result = String(F("[")) + String(id) + String(F(",\"")) + val + String(F("\"]"));
            //embui.publish(String(FPSTR(TCONST_embui_pub_)) + FPSTR(TCONST_control), result, true);

            return httpCallback(FPSTR(CMD_CONTROL), String(id), false); // т.к. отложенный вызов, то иначе обрабатыаем
        }
        else if (upperParam == FPSTR(CMD_EFF_NAME))  {
            String effname((char *)0);
            uint16_t effnum=String(value).toInt();
            myLamp.effects.loadeffname(effname, effnum);
            result = String(F("["))+effnum+String(",\"")+effname+String("\"]");
#ifdef EMBUI_USE_MQTT
            embui.publish(String(FPSTR(TCONST_embui_pub_)) + upperParam, result, true);
#endif
            return result;
        }
        else if (upperParam == FPSTR(CMD_EFF_ONAME))  {
            String effname((char *)0);
            uint16_t effnum=String(value).toInt();
            effname = FPSTR(T_EFFNAMEID[(uint8_t)effnum]);
            result = String(F("["))+effnum+String(",\"")+effname+String("\"]");
#ifdef EMBUI_USE_MQTT
            embui.publish(String(FPSTR(TCONST_embui_pub_)) + upperParam, result, true);
#endif
            return result;
        }
#ifdef AUX_PIN
        else if (upperParam == FPSTR(CMD_AUX_ON)) action = RA_AUX_ON;
        else if (upperParam == FPSTR(CMD_AUX_OFF))  action = RA_AUX_OFF;
        else if (upperParam == FPSTR(CMD_AUX_TOGGLE))  action = RA_AUX_TOGLE;
#endif
        remote_action(action, value.c_str(), NULL);
    }
    return result;
}
