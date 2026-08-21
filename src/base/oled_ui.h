#pragma once
#include "model.h"

enum Screen { SCREEN_LINK, SCREEN_ALERT, SCREEN_SUMMARY, SCREEN_CUSTODY, SCREEN_COUNT };

bool oledBegin();
void oledDraw(Screen screen, const BaseModel &m);
void oledSplash();
