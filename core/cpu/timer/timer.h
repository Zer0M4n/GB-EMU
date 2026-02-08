#pragma once
#include <cstdint>
#include "mmu/mmu.h"

class timer {
public:
    timer(mmu& mmu_ref);
    void step(int cycles);
    void reset();

private:
    mmu& memory;
    int div_counter;
    int tima_counter;
};
