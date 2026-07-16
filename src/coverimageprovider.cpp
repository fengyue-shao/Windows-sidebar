#include "include/coverimageprovider.h"

CoverImageProvider::CoverImageProvider() :QQuickImageProvider(QQuickImageProvider::Image){

}
QImage CoverImageProvider::requestImage(const QString &id,QSize *size,const QSize &requestedSize){
    Q_UNUSED(id)
    Q_UNUSED(requestedSize)
    QMutexLocker locker(&m_mutex);
    if(size)
        *size=m_image.size();
    return m_image;
}
void CoverImageProvider::setImage(const QImage &image){
    QMutexLocker locker(&m_mutex);
    m_image=image;
}