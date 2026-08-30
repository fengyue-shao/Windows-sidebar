#pragma once
#ifndef COVERIMAGEPROVIDER_H
#define COVERIMAGEPROVIDER_H
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

class CoverImageProvider : public QQuickImageProvider
{
public:
    CoverImageProvider();
    QImage requestImage(const QString &id,QSize *size,const QSize &requestedSize) override;
    bool setImage(const QImage& image);
private:
    QImage m_image;
    QMutex m_mutex;
};

#endif // COVERIMAGEPROVIDER_H
