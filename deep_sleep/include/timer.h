#pragma once
#include <stdint.h>
#include <chrono>

void ConfigureBurtc();
void ArmBurtc();
std::chrono::milliseconds GetTime();
void UpdateTime();