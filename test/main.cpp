#include <iostream>
#include <chrono>
#include <thread>
#include "simple_log.h"

int main(){
    simple_log::init();
    const auto TAG = "main";
    LOG_I(TAG, "Hello World");
    return 0;
}
