#include "frame_stats.hpp"

#include <algorithm>
#include <cmath>

void FrameStats::begin_frame() { frame_start_time.start(); }
void FrameStats::frame_presented() {
    frame_time_diff.push_back(frame_start_time.nsecsElapsed() / 1'000'000.0);
}

void FrameStats::begin_paint() { paint_start_time.start(); }
void FrameStats::end_paint() {
    paint_time_diff.push_back(paint_start_time.nsecsElapsed() / 1'000'000.0);
}

void FrameStats::reset() {
    frame_time_diff.clear();
    paint_time_diff.clear();
}

FrameStats::Summary FrameStats::summary() const {
    Summary sum{};

    sum.frame_ms_median = median(frame_time_diff);
    sum.paint_ms_median = median(paint_time_diff);

    sum.frame_ms_p95 = percentile(frame_time_diff, 0.95);
    sum.paint_ms_p95 = percentile(paint_time_diff, 0.95);

    sum.frames = frame_time_diff.size();

    return sum;
}

double FrameStats::median(const std::vector<double>& values) const
{
    if (values.empty()) return 0.0;

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    const std::size_t mid = sorted.size() / 2;

    if (sorted.size() % 2 == 0)
        return (sorted[mid - 1] + sorted[mid]) / 2.0;

    return sorted[mid];
}

double FrameStats::percentile(const std::vector<double>& values, double percentile) const
{
    if (values.empty()) return 0.0;
    percentile = std::clamp(percentile, 0.0, 1.0);

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    const double index = (sorted.size() - 1) * percentile;

    const auto lower = static_cast<std::size_t>(std::floor(index));
    const auto upper = static_cast<std::size_t>(std::ceil(index));

    const double weight = index - lower;

    return sorted[lower] + weight * (sorted[upper] - sorted[lower]);
}

void FrameStats::append_csv(const QString& path, const QString& label) const {
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) return;

    FrameStats::Summary sum = summary();

    QTextStream out(&file);

    if (file.size() == 0)
    {
        out << "label,frame_ms_median,frame_ms_p95,"
               "paint_ms_median,paint_ms_p95,frames\n";
    }

    out << label << "," << sum.frame_ms_median << "," << sum.frame_ms_p95 << "," << sum.paint_ms_median << "," << sum.paint_ms_p95 << "," << sum.frames << "\n";

    file.close();
}