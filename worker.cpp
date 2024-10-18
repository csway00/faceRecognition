#include "worker.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

Worker::Worker(QObject *parent)
    : QObject{parent}
{}

void Worker::doWork(QImage mImg, QThread *workerThread)
{

    qDebug() << "当前线程对象的地址: " << QThread::currentThread();
    //将图片转成base64编码
    QByteArray ba;
    QBuffer buff(&ba);
    mImg.save(&buff, "png");
    QString b64str = ba.toBase64();
    // qDebug() << "---------------------------------------------------";
    // qDebug() << b64str;

    //请求body 参数设置
    QJsonObject postJson;
    QJsonDocument doc;

    postJson.insert("image", b64str);
    postJson.insert("image_type", "BASE64");
    postJson.insert("face_field", "age,expression,face_shape,gender,"
                                  "glasses,emotion,face_type,mask,beauty");

    doc.setObject(postJson);
    QByteArray postData = doc.toJson(QJsonDocument::Compact);

    emit resultReady(postData, workerThread);
}
