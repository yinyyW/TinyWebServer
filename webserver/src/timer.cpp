#include "../include/timer.h"
#include <bits/types/time_t.h>
#include <cstddef>

timer_lst::timer_lst() : head(NULL), tail(NULL) {}

timer_lst::~timer_lst() {
    timer* tmp = head;
    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void timer_lst::add_timer(timer *t) {
    if (!t) {
        return;
    }
    if (!head) {
        head = tail = t;
        return;
    }
    if (t->expire < head->expire) {
        t->next = head;
        head->next = t;
        head = t;
        return;
    }
    timer* tmp = head->next;
    timer* prev = head;
    while (tmp) {
        if (t->expire < tmp->expire) {
            prev->next = t;
            t->next = tmp;
            tmp->prev = t;
            t->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp) {
        prev->next = t;
        t->prev = prev;
        t->next = NULL;
        tail = t;
    }
}

void timer_lst::adjust_timer(timer *t) {
    if (t == NULL) {
        return;
    }
    timer* tmp = t->next;
    if (tmp == NULL || t->expire < tmp->expire) {
        return;
    }
    if (t == head) {
        head = head->next;
        head->prev = NULL;
        // t->next = NULL;
        add_timer(t);
    } else {
        t->prev->next = t->next;
        t->next->prev = t->prev;
        add_timer(t);
    }
}

void timer_lst::del_timer(timer *t) {
    if (t == NULL) {
        return;
    }
    if (t == head && t == tail) {
        delete t;
        head = NULL;
        tail = NULL;
        return;
    }
    if (t == head) {
        head = head->next;
        head->prev = NULL;
        delete t;
        return;
    }
    if (t == tail) {
        tail = tail->prev;
        tail->next = NULL;
        delete t;
        return;
    }
    t->prev->next = t->next;
    t->next->prev = t->prev;
    delete t;
}

void timer_lst::tick() {
    if (head == NULL) {
        return;
    }
    time_t cur = time(NULL);
    timer* tmp = head;
    while (tmp) {
        if (cur < tmp->expire) {
            break;
        }
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if (head) {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void timer_lst::print() {
    if (head == NULL) {
        printf("empty list.\n");
        return;
    }
    timer* curr = head;
    int cnt = 1;
    while (curr != NULL) {
        printf("timer %d expire: %lu, ", cnt, curr->expire);
        curr = curr->next;
        ++cnt;
    }
    printf("\n");
}