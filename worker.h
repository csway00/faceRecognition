#ifndef WORKER_H
#define WORKER_H

#include <QImage>
#include <QObject>

class Worker : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);

public slots:
    void doWork(QImage mImg, QThread *workerThread);

signals:
    emit void resultReady(const QByteArray postData, QThread *workerThread);
    void stopWork();
};

#endif // WORKER_H
