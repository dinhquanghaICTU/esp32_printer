#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <stdbool.h>


void button_init(void);
bool button_is_pressed(void);
int button_hold(void);

#endif // __BUTTON_H__
