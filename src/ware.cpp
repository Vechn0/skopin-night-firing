#include "skopin/ware.hpp"

#include <stdexcept>

namespace skopin {

std::optional<std::string> validateWare(const Ware& ware) {
    if (ware.mark.empty()) {
        return "empty mark";
    }
    for (char ch : ware.mark) {
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            return "bad mark";
        }
    }
    if (ware.height_cm <= 0 || ware.height_cm > kKilnHeightCm) {
        return "bad height";
    }
    if (ware.thickness_mm <= 0) {
        return "bad thickness";
    }
    return std::nullopt;
}

bool fitsInKiln(const std::vector<Ware>& stack, const Ware& next) {
    int total_height = next.height_cm;
    for (const Ware& item : stack) {
        total_height += item.height_cm;
    }
    total_height += static_cast<int>(stack.size()) * kShelfCm;
    return total_height <= kKilnHeightCm;
}

}  // namespace skopin
