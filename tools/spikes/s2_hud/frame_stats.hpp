#pragma once

#include <cstddef>

#include <QString>

class FrameStats {
public:
    void begin_frame();
    void end_frame();

    void frame_presented();

    void reset();

    struct Summary {
        double cpu_ms_median;
        double cpu_ms_p95;
        double interval_ms_median;
        double interval_ms_p95;
        std::size_t frames;
    };

    Summary summary() const;

    void append_csv(const QString& path, const QString& label) const;
};