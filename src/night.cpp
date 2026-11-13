#include "skopin/night.hpp"

#include "skopin/kiln.hpp"

#include <stdexcept>

#include "skopin/measures.hpp"

namespace skopin {

NightReport runNightFire(const std::vector<Ware>& queue) {
    // 1. Слева направо вычеркнуть duplicate, затем invalid, затем thick.
    //    Вычеркнутые не стоят в очереди и не рвут садку.
    NightReport report;
    std::vector<Ware> pending;
    std::vector<std::string> seen_marks;

    for (const Ware& ware : queue) {
        bool duplicate = false;
        for (const std::string& mark : seen_marks) {
            if (mark == ware.mark) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            report.refused.push_back({ware.mark, "duplicate"});
            continue;
        }

        seen_marks.push_back(ware.mark);

        if (validateWare(ware).has_value()) {
            report.refused.push_back({ware.mark, "invalid"});
            continue;
        }

        if (ware.thickness_mm > kCrackThicknessMm) {
            report.refused.push_back({ware.mark, "thick"});
            continue;
        }

        pending.push_back(ware);
    }

    if (pending.empty()) {
        return report;
    }

    // 2. Пока кто-то ждёт, собрать одну садку по правилам README.
    //    На стопку сажать с конца к началу: первое взятое снимается первым.
    std::vector<std::vector<Ware>> batches;

    while (!pending.empty()) {
        std::vector<Ware> batch;
        const Program program = pending[0].program;

        if (pending[0].kind == WareKind::Sagger) {
            batch.push_back(pending[0]);
            pending.erase(pending.begin());
            batches.push_back(batch);
            continue;
        }

        batch.push_back(pending[0]);
        std::vector<bool> taken(pending.size(), false);
        taken[0] = true;
        bool skipped = false;

        for (std::size_t i = 1; i < pending.size(); ++i) {
            const Ware& current = pending[i];
            if (current.program != program || current.kind == WareKind::Sagger) {
                break;
            }

            const bool fits = fitsInKiln(batch, current);
            if (fits && (!skipped || current.kind == WareKind::Rush)) {
                batch.push_back(current);
                taken[i] = true;
            } else {
                skipped = true;
            }
        }

        std::vector<Ware> remaining;
        for (std::size_t i = 0; i < pending.size(); ++i) {
            if (!taken[i]) {
                remaining.push_back(pending[i]);
            }
        }
        pending = std::move(remaining);
        batches.push_back(batch);
    }

    // 3. Провести её объектом Kiln. После утиля, за которым сразу полива,
    //    горн не остужать. stokes взять из kiln.stokes() в конце.
    Kiln kiln;

    for (std::size_t b = 0; b < batches.size(); ++b) {
        const std::vector<Ware>& batch = batches[b];
        const Program program = batch[0].program;
        const bool has_next = (b + 1 < batches.size());
        const bool next_is_glaze = has_next && (batches[b + 1][0].program == Program::Glaze);

        if (program == Program::Bisque) {
            if (kiln.door() == Door::Open) {
                kiln.close();
            }
            if (kiln.cone() == Cone::Bisque) {
                kiln.cool();
            }

            kiln.open();
            for (int i = static_cast<int>(batch.size()) - 1; i >= 0; --i) {
                kiln.load(batch[i]);
            }
            kiln.close();

            kiln.fire();

            kiln.open();
            for (std::size_t i = 0; i < batch.size(); ++i) {
                kiln.unload(batch[i].mark);
                report.passed.push_back(batch[i].mark);
            }

            if (next_is_glaze) {
                // Оставляем горн горячим на утильном конусе с открытой дверцей
            } else {
                kiln.close();
                kiln.cool();
            }
        } else {
            // Program::Glaze
            if (kiln.door() == Door::Closed) {
                kiln.open();
            }

            for (int i = static_cast<int>(batch.size()) - 1; i >= 0; --i) {
                kiln.load(batch[i]);
            }
            kiln.close();

            if (kiln.cone() == Cone::Cold) {
                kiln.fire();
                kiln.fire();
            } else if (kiln.cone() == Cone::Bisque) {
                kiln.fire();
            }

            kiln.cool();
            kiln.cool();

            kiln.open();
            for (std::size_t i = 0; i < batch.size(); ++i) {
                kiln.unload(batch[i].mark);
                report.passed.push_back(batch[i].mark);
            }
            kiln.close();
        }
    }

    report.stokes = kiln.stokes();
    return report;
}

}  // namespace skopin
