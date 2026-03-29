#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    pid_t fg_pid = SmallShell::getInstance().getForePID();
    if (fg_pid != -1) {
        if (kill(fg_pid, SIGKILL) == 0) {
            cout << "smash: process " << fg_pid << " was killed" << endl;
        } else {
            perror("smash error: kill failed");
        }
    }
}

