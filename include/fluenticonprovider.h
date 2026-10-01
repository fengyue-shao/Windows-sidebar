#pragma once
#include <QQuickImageProvider>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>

class FluentIconProvider final : public QQuickImageProvider {
public:
    FluentIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        const auto parts = id.split('/');
        if (parts.size() != 2 || !QRegularExpression("^[a-z0-9_]+$").match(parts[0]).hasMatch()) return {};
        const QColor color("#" + parts[1]);
        if (!color.isValid()) return {};
        QSvgRenderer renderer(":/qt/qml/qt_rainmeter/assets/fluent/" + parts[0] + "_20_regular.svg");
        if (!renderer.isValid()) return {};
        const QSize output(qBound(1, requested.width() > 0 ? requested.width() : 40, 256),
                           qBound(1, requested.height() > 0 ? requested.height() : 40, 256));
        QImage image(output, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), color);
        painter.end();
        if (size) *size = output;
        return image;
    }
};
