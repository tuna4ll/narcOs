#include "user_lib.h"

int main(void) {
    return userlib_println("NarcOs") == 0 ? 0 : 1;
}
