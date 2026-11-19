#pragma once

#include <cstddef>
#include <vector>

#include <QString>
#include <QElapsedTimer>
#include <QTime>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QList>

class FrameStats {
public:
    void begin_frame();      // Stopwatch 1 start: called at the tick, BEFORE moving anything
    void frame_presented();  // Stopwatch 1 stop: called from the frameSwapped signal. One sample = total frame cost
    void begin_paint();      // Stopwatch 2 start: first line of paintGL
    void end_paint();        // Stopwatch 2 stop: last line of paintGL. One sample = GL-side time only

    // Throw away all samples (call after changing mode / count / zoom)
    void reset();

    struct Summary {
        double frame_ms_median, frame_ms_p95;   // the headline numbers
        double paint_ms_median, paint_ms_p95;   // where the time went
        std::size_t frames;
    };
    Summary summary() const;
    void append_csv(const QString& path, const QString& label) const;
private:
    QElapsedTimer frame_start_time;
    QElapsedTimer paint_start_time;

    std::vector<double> frame_time_diff;
    std::vector<double> paint_time_diff;

    double median(const std::vector<double>& values) const;
    double percentile(const std::vector<double>& values, double percentile) const;
};