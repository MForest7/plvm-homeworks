#pragma once
#include <optional>
#include <stdint.h>

std::optional<uint8_t> safe_read_byte(const uint8_t *ptr);
