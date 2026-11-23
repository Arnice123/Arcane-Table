#pragma once

#include <QWidget>
#include <QString>
#include <QColor>
#include <QRectF>
#include <QPainter>
#include <QPaintEvent>

class NameplateWidget : public QWidget {
public:
    explicit NameplateWidget(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);  // display-only, clicks fall through
    }

    void set_data(const QString& name, float hp_frac) { name_ = name; hp_ = hp_frac; update(); }
    void set_zoom(float zoom) { zoom_ = zoom; update(); }

protected:
    void paintEvent(QPaintEvent*) override {  // draw background, HP bar, name text, scaled by zoom_
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, false);

        p.fillRect(rect(), QColor(20, 20, 20, 200));
        QRectF bar(4 * zoom_, 10 * zoom_, 40 * zoom_, 3 * zoom_);
        p.fillRect(bar, Qt::darkGray);
        p.fillRect(QRectF(bar.left(), bar.top(), bar.width() * hp_, bar.height()), Qt::red);

        QFont f = p.font();
        f.setPixelSize(int(8 * zoom_));
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRectF(2 * zoom_, 0, width() - 4 * zoom_, 10 * zoom_), Qt::AlignLeft | Qt::AlignVCenter, name_);

    }

private:
    QString name_;
    float hp_ = 1.0f;
    float zoom_ = 1.0f;
};