/*
 * Agent CLI Entry Point
 */

#include "agent_controller.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    AgentController app;
    if (app.init()) app.run();
    return 0;
}
