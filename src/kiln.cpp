#include "skopin/kiln.hpp"

#include <stdexcept>

#include "skopin/measures.hpp"

namespace skopin {

Kiln::Kiln()
    : cone_(Cone::Cold), door_(Door::Closed), stokes_(0) {}

Cone Kiln::cone() const {
    return cone_;
}

Door Kiln::door() const {
    return door_;
}

int Kiln::stokes() const {
    return stokes_;
}

std::vector<Ware> Kiln::stack() const {
    return stack_;
}

void Kiln::open() {
    // Повторное открытие и открытие на поливном конусе — KilnError.
    if (door_ == Door::Open) {
        throw KilnError("door is already open");
    }
    if (cone_ == Cone::Glaze) {
        throw KilnError("cannot open door at glaze cone");
    }
    door_ = Door::Open;
}

void Kiln::close() {
    if (door_ == Door::Closed) {
        throw KilnError("door is already closed");
    }
    door_ = Door::Closed;
}

void Kiln::fire() {
    // Отказ не тратит такт и не поднимает конус наполовину.
    if (door_ != Door::Closed) {
        throw KilnError("cannot fire with open door");
    }
    if (cone_ == Cone::Glaze) {
        throw KilnError("already at maximum cone");
    }
    for (const Ware& item : stack_) {
        if (item.thickness_mm > kCrackThicknessMm) {
            throw KilnError("ware is too thick to fire");
        }
    }
    if (cone_ == Cone::Bisque) {
        for (const Ware& item : stack_) {
            if (item.program == Program::Bisque) {
                throw KilnError("cannot fire bisque ware to glaze cone");
            }
        }
        cone_ = Cone::Glaze;
        for (std::size_t i = 0; i < seen_glaze_.size(); ++i) {
            seen_glaze_[i] = true;
        }
    } else {
        cone_ = Cone::Bisque;
    }
    ++stokes_;
}

void Kiln::cool() {
    // Ровно один конус вниз. Полива, не видевшая поливного огня, остыть не даёт.
    if (door_ != Door::Closed) {
        throw KilnError("cannot cool with open door");
    }
    if (cone_ == Cone::Cold) {
        throw KilnError("already cold");
    }
    if (cone_ == Cone::Bisque) {
        for (std::size_t i = 0; i < stack_.size(); ++i) {
            if (stack_[i].program == Program::Glaze && !seen_glaze_[i]) {
                throw KilnError("glaze ware has not seen glaze cone");
            }
        }
        cone_ = Cone::Cold;
    } else {
        cone_ = Cone::Bisque;
    }
    ++stokes_;
}

void Kiln::load(const Ware& ware) {
    // Внутри должна оказаться копия. Дверцу load сам не открывает.
    if (door_ != Door::Open) {
        throw KilnError("cannot load with closed door");
    }
    if (cone_ == Cone::Glaze) {
        throw KilnError("cannot load at glaze cone");
    }
    if (cone_ == Cone::Bisque && ware.program != Program::Glaze) {
        throw KilnError("only glaze program can be loaded at bisque cone");
    }
    if (validateWare(ware).has_value()) {
        throw KilnError("invalid ware");
    }
    for (const Ware& item : stack_) {
        if (item.mark == ware.mark) {
            throw KilnError("duplicate mark in kiln");
        }
    }
    if (ware.kind == WareKind::Sagger && !stack_.empty()) {
        throw KilnError("sagger must be loaded into empty kiln");
    }
    for (const Ware& item : stack_) {
        if (item.kind == WareKind::Sagger) {
            throw KilnError("kiln already contains a sagger");
        }
    }
    if (!fitsInKiln(stack_, ware)) {
        throw KilnError("ware does not fit in kiln");
    }

    stack_.push_back(ware);
    seen_glaze_.push_back(false);
}

void Kiln::unload(const std::string& mark) {
    // Снимается только верх стопки. Поливу с утильного конуса не снимают.
    if (door_ != Door::Open) {
        throw KilnError("cannot unload with closed door");
    }
    if (stack_.empty()) {
        throw KilnError("kiln is empty");
    }
    if (stack_.back().mark != mark) {
        throw KilnError("mark does not match top of stack");
    }
    if (stack_.back().program == Program::Glaze && cone_ != Cone::Cold) {
        throw KilnError("glaze ware can only be unloaded at cold cone");
    }

    stack_.pop_back();
    seen_glaze_.pop_back();
}

}  // namespace skopin
