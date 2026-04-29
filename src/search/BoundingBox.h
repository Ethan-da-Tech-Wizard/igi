#pragma once

#include <QString>

namespace igi::search {

struct WordBox {
    QString text;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float confidence = 0.0f; // 0.0 to 100.0
    int pageIndex = 0;

    WordBox() = default;

    WordBox(QString t, int x, int y, int w, int h, float conf, int pageIdx = 0)
        : text(std::move(t)), x(x), y(y), w(w), h(h), confidence(conf), pageIndex(pageIdx) {}
};

} // namespace igi::search
