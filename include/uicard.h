#pragma once
#include <QRectF>

// Layout state only. Business services and component ownership stay outside this value type.
struct UiCard {
    QRectF geometry;
    bool enabled = true;
    bool pinned = false;
};
