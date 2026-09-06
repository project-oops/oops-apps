#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "oops/inject.h"

int main(void) {
    assert(sizeof(struct reg) >= 160);
    assert(oops_inject_elf(0, NULL, 0, NULL) == -1);
    uint8_t dummy[4] = {0};
    assert(oops_inject_elf(0, dummy, sizeof(dummy), NULL) == -1);
    printf("injector selftest: ok (register layout & argument safety verified)\n");
    return 0;
}
