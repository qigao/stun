/* Tango-private one-turn dispatch seam for real static-link regression. */
#ifndef TANGO_TUI_APP_INTERNAL_H
#define TANGO_TUI_APP_INTERNAL_H
#include "tui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Owner only. Drain bounded terminal input and async work, render once. */
bool tui_app_dispatch_once(tui_app_t *app);
#ifdef __cplusplus
}
#endif
#endif
