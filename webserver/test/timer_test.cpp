#include "../include/timer.h"
#include <unistd.h>

void cb_func(client_data *user_data) {
    printf("call cb_func.\n");
}

int main(int argc, char* argv[]) {
    const int TIME_SLOT = 1;

    timer* t1 = new timer;
    time_t curr = time(NULL);
    t1->expire = curr + 2 * TIME_SLOT;
    t1->cb_func = cb_func;
    
    // TEST: ADD TIMER
    timer_lst tl = timer_lst();
    tl.add_timer(t1);
    
    sleep(1);
    timer* t2 = new timer;
    curr = time(NULL);
    t2->expire = curr + 3 * TIME_SLOT;
    t2->cb_func = cb_func;
    tl.add_timer(t2);
    tl.print();

    // TEST: TICK
    sleep(2);
    tl.tick();
    tl.print();
    
    // TEST: ADJUST TIMER
    timer* t3 = new timer;
    curr = time(NULL);
    t3->expire = curr + 3 * TIME_SLOT;
    t3->cb_func = cb_func;
    tl.add_timer(t3);
    tl.print();
    
    t2->expire = curr + 5 * TIME_SLOT;
    tl.adjust_timer(t2);
    tl.print();
    
    sleep(2);
    tl.tick();
    sleep(2);
    tl.tick();
    tl.print();
    
    // TEST: DELETE TIMER
    tl.del_timer(t2);
    tl.print();
    return 0;
}