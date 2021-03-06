#include "screen.h"

#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "eeprom.h"
#include "glcd.h"
#include "measure.h"

#define STR_BUFSIZE                     20

static Screen screen = SCREEN_END;

static Param paramTop = PARAM_SPEED;
static Param paramMid = PARAM_TRACK_DISTANCE;
static Param paramBtm = PARAM_TOTAL_DISTANCE;

static char strTop[PARAM_STRBUF];
static char strMid[PARAM_STRBUF];
static char strBtm[PARAM_STRBUF];

static Param paramSetup = PARAM_SETUP_AUTO_OFF;

static ColorMode colorMode = COLOR_MODE_FULL_COLOR;

static const ParamArea areaMainTop PROGMEM = {
    0, 0, 239, 119,
    2, 0,
    font_lcd_90, font_lcd_63,
};
static const ParamArea areaMainMid PROGMEM = {
    0, 120, 239, 219,
    2, 7,
    font_lcd_45, font_lcd_36,
};
static const ParamArea areaMainBtm PROGMEM = {
    0, 220, 239, 319,
    2, 7,
    font_lcd_45, font_lcd_36,
};

static const ParamArea areaSetupTop PROGMEM = {
    0, 0, 239, 101,
    2, 0,
    font_lcd_72, font_lcd_45,
};
static const ParamArea areaSetupMid PROGMEM = {
    0, 102, 239, 210,
    2, 7,
    font_lcd_72, font_lcd_45,
};
static const ParamArea areaSetupBtm PROGMEM = {
    0, 211, 239, 319,
    2, 7,
    font_lcd_72, font_lcd_45,
};

static const LcdText textMainTop_5_1 PROGMEM = {
    10, 29, 5, 1, ' ',
};
static const LcdText textMainTop_5 PROGMEM = {
    10, 29, 3, 0, ' ',
};
static const LcdText textParam_7_2 PROGMEM = {
    19, 36, 7, 2, ' ',
};
static const LcdText textParam_7_1 PROGMEM = {
    52, 46, 7, 1, ' ',
};
static const LcdText textParam_7 PROGMEM = {
    22, 46, 7, 0, ' ',
};

static const LcdTimeText textTime PROGMEM = {
    {5, 36, 2, 0, ' ',},
    {103, 36, 5, 2, '0',},
};

static const LcdText textParamTop_5 PROGMEM = {
    55, 29, 4, 0, ' ',
};
static const LcdText textParam_5 PROGMEM = {
    55, 36, 4, 0, ' ',
};

const char speedLabel[] PROGMEM = "SPEED";
static const ParamData speedParam PROGMEM = {
    LCD_COLOR_AQUA, speedLabel,
};

const char trackLabel[] PROGMEM = "TRACK";
static const ParamData trackParam PROGMEM = {
    LCD_COLOR_YELLOW, trackLabel,
};

const char trackTimeLabel[] PROGMEM = "Track time";
static const ParamData trackTimeParam PROGMEM = {
    LCD_COLOR_GREEN, trackTimeLabel,
};

const char speedAvgLabel[] PROGMEM = "Avg. speed";
static const ParamData speedAvgParam PROGMEM = {
    LCD_COLOR_OLIVE, speedAvgLabel,
};

const char cadenceLabel[] PROGMEM = "Cadence";
static const ParamData cadenceParam PROGMEM = {
    LCD_COLOR_CHARTREUSE, cadenceLabel,
};

const char distanceLabel[] PROGMEM = "TOTAL";
static const ParamData distanceParam PROGMEM = {
    LCD_COLOR_LIGHT_CORAL, distanceLabel,
};

const char autoOffLabel[] PROGMEM = "Auto off timeout";
static const ParamData autoOffParam PROGMEM = {
    LCD_COLOR_AQUA, autoOffLabel,
};

const char wheelLabel[] PROGMEM = "Wheel length";
static const ParamData wheelParam PROGMEM = {
    LCD_COLOR_AQUA, wheelLabel,
};

const char colorModeLabel[] PROGMEM = "Color mode";
static const ParamData colorModeParam PROGMEM = {
    LCD_COLOR_AQUA, colorModeLabel,
};

static char strbuf[STR_BUFSIZE + 1];

static char *mkNumString(int32_t number, uint8_t width, uint8_t dot, uint8_t lead)
{
    uint8_t numdiv;
    int8_t i;

    for (i = 0; i < width; i++)
        strbuf[i] = lead;
    strbuf[width] = '\0';
    i = 0;

    while (number > 0 || i < dot + 1 + !!dot) {
        if (number < 0) {
            strbuf[width - 1 - i] = '-';
        } else {
            numdiv = number % 10;
            number /= 10;
            strbuf[width - 1 - i] = numdiv + 0x30;
        }
        i++;
        if (dot == i) {
            strbuf[width - 1 - i] = '.';
            i++;
        }
    }
    return strbuf;
}

static void updateParam(const ParamData *paramPgm, const LcdText *lcdTextPgm, int32_t val,
                        Section section, ClearMode clear)
{
    // Read progmem values to RAM structures
    ParamData param;
    memcpy_P(&param, paramPgm, sizeof(ParamData));

    LcdText text;
    memcpy_P(&text, lcdTextPgm, sizeof(LcdText));

    ParamArea area;
    char *posStr;
    switch (section) {
    case SECTION_MAIN_TOP:
        memcpy_P(&area, &areaMainTop, sizeof(ParamArea));
        posStr = strTop;
        break;
    case SECTION_MAIN_MID:
        memcpy_P(&area, &areaMainMid, sizeof(ParamArea));
        posStr = strMid;
        break;
    case SECTION_MAIN_BTM:
        memcpy_P(&area, &areaMainBtm, sizeof(ParamArea));
        posStr = strBtm;
        break;
    case SECTION_SETUP_TOP:
        memcpy_P(&area, &areaSetupTop, sizeof(ParamArea));
        posStr = strTop;
        break;
    case SECTION_SETUP_MID:
        memcpy_P(&area, &areaSetupMid, sizeof(ParamArea));
        posStr = strMid;
        break;
    case SECTION_SETUP_BTM:
        memcpy_P(&area, &areaSetupBtm, sizeof(ParamArea));
        posStr = strBtm;
        break;
    default:
        return;
    }

    uint16_t paramColor = param.color;
    uint16_t labelColor = LCD_COLOR_GRAY;
    uint16_t bgColor = LCD_COLOR_BLACK;

    switch (colorMode) {
    case COLOR_MODE_WHITE_ON_BLACK:
        paramColor = LCD_COLOR_WHITE;
        labelColor = LCD_COLOR_WHITE;
        bgColor = LCD_COLOR_BLACK;
        break;
    case COLOR_MODE_BLACK_ON_WHITE:
        paramColor = LCD_COLOR_BLACK;
        labelColor = LCD_COLOR_BLACK;
        bgColor = LCD_COLOR_WHITE;
        break;
    default:
        break;
    }

    // Clear section area if required and draw constant text labels
    if (clear == CLEAR_ALL) {
        glcdDrawRectangle(area.left, area.top, area.right, area.bottom, bgColor);
        if (area.top)
            glcdDrawHorizLine(area.left + 2, area.right - 2, area.top + 3, labelColor);
        glcdLoadFont(font_ks0066_ru_24, labelColor, bgColor);
        glcdSetXY(area.labX, area.top + area.labY);
        glcdWriteString("> ");
        strcpy_P(strbuf, param.label);
        glcdWriteString(strbuf);
    }

    // Draw icon
    if (clear & CLEAR_ICON) {
        if (section >= SECTION_SETUP_TOP && section <= SECTION_SETUP_BTM) {
            uint16_t iconColor = bgColor;
            if (section - SECTION_SETUP_TOP == paramSetup - PARAM_SETUP_AUTO_OFF)
                iconColor = labelColor;
            glcdSetXY(area.labX, area.top + area.labY + 40);
            glcdWriteIcon(icon_pointer, iconColor, bgColor);
        }
    }

    // Redraw param value with selected LCD font
    char *valStr = mkNumString(val, text.len, text.dot, text.lead);

    glcdLoadLcdFont(area.fontMain, paramColor, bgColor);
    glcdSetXY(text.x, area.top + text.y);

    uint8_t i;
    for (i = 0; i < text.len; i++) {
        if (text.dot && i == text.len - text.dot - 1) {
            glcdLoadLcdFont(area.fontDeci, paramColor, bgColor);
            glcdSetY(area.top + text.y + pgm_read_byte(&area.fontMain[1]) - pgm_read_byte(
                         &area.fontDeci[1]));
        }
        if (clear & CLEAR_LCDDATA || (valStr[i] != posStr[i])) {
            posStr[i] = valStr[i];
            glcdWriteLcdChar(posStr[i]);
        } else {
            glcdSkipLcdChar(posStr[i]);
        }
    }
}

static void updateTime(const ParamData *paramPgm, const LcdTimeText *lcdTimeTextPgm, int32_t val,
                       Section section, ClearMode clear)
{
    updateParam(paramPgm, &lcdTimeTextPgm->hour, val / 3600, section, clear);
    glcdWriteLcdChar(':');
    val %= 3600;
    val = val / 60 * 100 + val % 60;
    updateParam(paramPgm, &lcdTimeTextPgm->minSec, val, section, CLEAR_LCDDATA);
}

static Param getParamType(Section section)
{
    switch (section) {
    case SECTION_MAIN_TOP:
        return paramTop;
    case SECTION_MAIN_MID:
        return paramMid;
    case SECTION_MAIN_BTM:
        return paramBtm;
    case SECTION_SETUP_TOP:
        return PARAM_SETUP_AUTO_OFF;
    case SECTION_SETUP_MID:
        return PARAM_SETUP_WHEEL;
    case SECTION_SETUP_BTM:
        return PARAM_SETUP_COLOR_MODE;
    default:
        return PARAM_END;
    }
}

static void updateSection(Section section, ClearMode clear)
{
    Param param = getParamType(section);
    int32_t value = measureGetValue(param);

    switch (param) {
    case PARAM_SPEED:
        value = value * 36 / 10 / 1000;      // mm/s => km/h
        updateParam(&speedParam, &textMainTop_5, value, section, clear);
        break;
    case PARAM_TRACK_TIME:                  // sec
        updateTime(&trackTimeParam, &textTime, value, section, clear);
        break;
    case PARAM_SPEED_AVG:
        value = value * 36 / 10 / 100;      // mm/s => 0.1km/h
        updateParam(&speedAvgParam, &textParam_7_1, value, section, clear);
        break;
    case PARAM_CADENCE:
        updateParam(&cadenceParam, &textParam_7_1, value, section, clear);
        break;
    case PARAM_TOTAL_DISTANCE:
        value = value / 1000;                // m => km
        updateParam(&distanceParam, &textParam_7, value, section, clear);
        break;
    case PARAM_TRACK_DISTANCE:
        value = value / 100;          // m => 0.1km
        updateParam(&trackParam, &textParam_7_1, value, section, clear);
        break;
    case PARAM_SETUP_AUTO_OFF:
        value = 30 * (measureGetAutoOff() + 1);
        updateParam(&autoOffParam, &textParamTop_5, value, section, clear);
        break;
    case PARAM_SETUP_WHEEL:
        updateParam(&wheelParam, &textParam_5, value, section, clear);
        break;
    case PARAM_SETUP_COLOR_MODE:
        value = colorMode;
        updateParam(&colorModeParam, &textParam_5, value, section, clear);
        break;
    default:
        break;
    }
}

static void screenDiffColorMode(int8_t value)
{
    if (colorMode || value > 0)
        colorMode += value;
    else
        colorMode = COLOR_MODE_END - 1;
    if (colorMode >= COLOR_MODE_END)
        colorMode = COLOR_MODE_FULL_COLOR;

    eeprom_update_byte((uint8_t *)EEPROM_COLOR_MODE, colorMode);
}

void screenInit(void)
{
//    paramMid = eeprom_read_byte((uint8_t *)EEPROM_PARAM_MID);
//    if (paramMid >= PARAM_END)
//        switchParam(SECTION_MAIN_MID);
//    paramBtm = eeprom_read_byte((uint8_t *)EEPROM_PARAM_BTM);
//    if (paramBtm >= PARAM_END)
//        switchParam(SECTION_MAIN_BTM);
    colorMode = eeprom_read_byte((uint8_t *)EEPROM_COLOR_MODE);
    if (colorMode >= COLOR_MODE_END)
        colorMode = COLOR_MODE_FULL_COLOR;
}

void switchParam(Section section)
{
    switch (section) {
    case SECTION_MAIN_MID:
        if (++paramMid >= PARAM_END)
            paramMid = PARAM_TRACK_DISTANCE;
        if (paramMid == paramBtm)
            if (++paramMid >= PARAM_END)
                paramMid = PARAM_TRACK_DISTANCE;
        eeprom_update_byte((uint8_t *)EEPROM_PARAM_MID, paramMid);
        break;
    case SECTION_MAIN_BTM:
        if (++paramBtm >= PARAM_END)
            paramBtm = PARAM_TRACK_DISTANCE;
        if (paramBtm == paramMid)
            if (++paramBtm >= PARAM_END)
                paramBtm = PARAM_TRACK_DISTANCE;
        eeprom_update_byte((uint8_t *)EEPROM_PARAM_BTM, paramBtm);
        break;
    default:
        return;
    }

    updateSection(section, CLEAR_ALL);
}

void switchParamSetup(void)
{
    if (++paramSetup >= PARAM_SETUP_END)
        paramSetup = PARAM_SETUP_AUTO_OFF;

    screenShowSetup(CLEAR_ICON);
}

void diffParamSetup(int8_t value)
{
    if (paramSetup != PARAM_SETUP_WHEEL) {
        if (value > 0)
            value = 1;
        else if (value < 0)
            value = -1;
    }
    switch (paramSetup) {
    case PARAM_SETUP_AUTO_OFF:
        measureDiffAutoMode(value);
        break;
    case PARAM_SETUP_WHEEL:
        measureDiffWheel(value);
        break;
    case PARAM_SETUP_COLOR_MODE:
        screenDiffColorMode(value);
        screenShowSetup(CLEAR_ALL);
        break;
    default:
        break;
    }
}

void screenShowMain(ClearMode clear)
{
    if (clear == CLEAR_NOTHING)
        clear = (SCREEN_MAIN != screen);

    updateSection(SECTION_MAIN_TOP, clear);
    updateSection(SECTION_MAIN_MID, clear);
    updateSection(SECTION_MAIN_BTM, clear);

    screen = SCREEN_MAIN;

    paramSetup = PARAM_SETUP_AUTO_OFF;
}

void screenShowSetup(ClearMode clear)
{
    if (clear == CLEAR_NOTHING)
        clear = (SCREEN_SETUP != screen);

    updateSection(SECTION_SETUP_TOP, clear);
    updateSection(SECTION_SETUP_MID, clear);
    updateSection(SECTION_SETUP_BTM, clear);

    screen = SCREEN_SETUP;
}

void screenUpdate(void)
{
    switch (screen) {
    case SCREEN_MAIN:
        screenShowMain(CLEAR_NOTHING);
        break;
    case SCREEN_SETUP:
        screenShowSetup(CLEAR_NOTHING);
        break;
    default:
        break;
    }
}

Screen screenGet(void)
{
    return screen;
}
