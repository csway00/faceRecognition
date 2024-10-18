#include "widget.h"
#include "ui_widget.h"

#include <QHBoxLayout>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QJsonArray>
#include <QPainter>
#include <QCameraInfo>
#include "worker.h"


Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    mCameraList = QCameraInfo::availableCameras();
    for (const QCameraInfo &tmpCam : mCameraList) {
        //qDebug() << tmpCam.deviceName() << "|||" << tmpCam.description();
        ui->comboBox->addItem(tmpCam.description());
    }

    connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int index){

                //qDebug() << "摄像头:";
                //qDebug() << mCameraList[index].description();
                refreshTimer->stop();
                camera->stop();

                camera = new QCamera(mCameraList[index]);

                capture = new QCameraImageCapture(camera);
                camera->setViewfinder(finder);

                camera->setCaptureMode(QCamera::CaptureStillImage);
                capture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);

                connect(capture, &QCameraImageCapture::imageCaptured,
                        this, &Widget::showCamera);

                camera->start();
                refreshTimer->start(10);
            });

    //设置摄像头功能
    camera = new QCamera();
    finder = new QCameraViewfinder();
    capture = new QCameraImageCapture(camera);
    camera->setViewfinder(finder);

    camera->setCaptureMode(QCamera::CaptureStillImage);
    capture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);

    connect(capture, &QCameraImageCapture::imageCaptured,
            this, &Widget::showCamera);
    camera->start();

    //进行界面布局
    this->resize(1300, 700);
    QVBoxLayout* vboxl = new QVBoxLayout;
    vboxl->addWidget(ui->label);
    // vboxl->addWidget(ui->pushButton);
    vboxl->addWidget(ui->tackPicWidget);

    QVBoxLayout* vboxr = new QVBoxLayout;
    vboxr->addWidget(ui->comboBox);
    vboxr->addWidget(finder);
    vboxr->addWidget(ui->textBrowser);

    QHBoxLayout* hbox = new QHBoxLayout(this);
    hbox->addLayout(vboxl);
    hbox->addLayout(vboxr);
    this->setLayout(hbox);

    //利用定时器不断刷新拍照界面
    refreshTimer = new QTimer(this);
    //connect(refreshTimer, &QTimer::timeout, capture, &QCameraImageCapture::capture);
    connect(refreshTimer, &QTimer::timeout, this, [=]() {
        capture->capture();
    });
    refreshTimer->start(10);  //每秒刷新100张照片

    workThreadTimer = new QTimer(this);
    connect(workThreadTimer, &QTimer::timeout, this, [=]() {
        on_pushButton_clicked();
    });

    tokenManager = new QNetworkAccessManager(this);
    connect(tokenManager, &QNetworkAccessManager::finished, this, [=](QNetworkReply* reply) {
        if (reply->error() != QNetworkReply::NoError) { //错误处理
            qDebug() << reply->errorString();
            return;
        }

        //正常应答
        const QByteArray replyData = reply->readAll();
        //qDebug() << replyData;

        //Json解析
        QJsonParseError jsonErr;
        QJsonDocument doc = QJsonDocument::fromJson(replyData, &jsonErr);

        if (jsonErr.error == QJsonParseError::NoError) { //解析成功
            QJsonObject obj = doc.object();
            if (obj.contains("access_token")) {
                accessToken = obj.take("access_token").toString();
            }
            ui->textBrowser->setText(accessToken);
        } else {
            qDebug() << "JSON ERROR" << jsonErr.errorString();
        }

        reply->deleteLater();

        //on_pushButton_clicked();
        workThreadTimer->start(1500); //1s
    });

    imgManager = new QNetworkAccessManager(this);
    connect(imgManager, &QNetworkAccessManager::finished, this, [=](QNetworkReply* reply) {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << reply->errorString();
            return;
        }

        const QByteArray replyData = reply->readAll();
        //qDebug() << replyData;

        parseFaseJson(replyData);

        reply->deleteLater();

        //on_pushButton_clicked();
    });


    //qDebug() << tokenManager->supportedSchemes(); //输出:("ftp", "file", "qrc", "http", "https", "data")

    //拼接url得到token的url
    QUrl url("https://aip.baidubce.com/oauth/2.0/token");

    QUrlQuery query;
    query.addQueryItem("grant_type", "client_credentials");
    query.addQueryItem("client_id", "nMbVOhCZFidnvGw1IcRAVIeo");
    query.addQueryItem("client_secret", "Nq0UQlR4Sie58vh76GBYlmwQKq5Ewxfw");
    url.setQuery(query);
    //qDebug() << url;

    //qDebug() << "是否支持ssl:" << QSslSocket::supportsSsl(); //查看是否支持ssl;

    //配置ssl
    sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::QueryPeer);
    sslConfig.setProtocol(QSsl::TlsV1_2);

    //组装请求
    QNetworkRequest req;
    req.setUrl(url);
    req.setSslConfiguration(sslConfig);

    //发送get请求
    tokenManager->get(req);

}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_pushButton_clicked()
{
    //qDebug() << "begin";

    /*
创建子线程
创建工人
把工人送进子线程
绑定信号和槽的关系
启动子线程
给工人发通知干活*/

    workerThread = new QThread(this);
    Worker *mWorker = new Worker;
    mWorker->moveToThread(workerThread);

    connect(this, &Widget::beginWork, mWorker, &Worker::doWork);
    connect(workerThread, &QThread::finished, mWorker, &QObject::deleteLater);
    connect(mWorker, &Worker::resultReady, this, [=](const QByteArray postData, QThread *overThread) {

        // overThread->exit(); //关闭子线程;
        // overThread->wait(); //阻塞等待子线程关闭

        // 发送停止信号并等待线程停止
        QObject::connect(mWorker, &Worker::stopWork, workerThread, &QThread::requestInterruption);
        QObject::connect(mWorker, &Worker::resultReady, workerThread, &QThread::quit);

        // 销毁 QThread 对象之前等待线程停止
        QObject::connect(workerThread, &QThread::finished, mWorker, &QObject::deleteLater);
        workerThread->quit();  // 请求线程停止
        workerThread->wait();  // 等待线程停止

        qDebug() << "Thread stopped...";

        if (overThread->isFinished()) {
            qDebug() << "子线程结束了";
        } else {
            qDebug() << "子线程没结束";
        }

        //组装图像识别请求
        QUrl url("https://aip.baidubce.com/rest/2.0/face/v3/detect");

        QUrlQuery query;
        query.addQueryItem("access_token", accessToken);
        url.setQuery(query);

        //组装请求
        QNetworkRequest req;
        req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
        req.setUrl(url);
        req.setSslConfiguration(sslConfig);

        //qDebug() << "run 2"; ////输出测试哪个阶段导致卡顿


        imgManager->post(req, postData);

        //qDebug() << "end"; //输出测试哪个阶段导致卡顿

    });
    workerThread->start();

    emit beginWork(mImg, workerThread);

    //qDebug() << "run 1"; //输出测试哪个阶段导致卡顿

}

void Widget::parseFaseJson(const QByteArray &replyData)
{
    QJsonParseError jsonErr;
    QJsonDocument doc = QJsonDocument::fromJson(replyData, &jsonErr);

    QString faceInfo;

    if (jsonErr.error == QJsonParseError::NoError) { //解析成功
        QJsonObject obj = doc.object();
        if (obj.contains("result")) {
            QJsonObject resultObj = obj.take("result").toObject();

            if (resultObj.contains("timestamp")) {
                int tmpTimeTamp = resultObj.take("timestamp").toInt();
                if (tmpTimeTamp < curTimeTamp) {
                    return;
                } else {
                    curTimeTamp = tmpTimeTamp;
                }
            }

            //取出人脸表
            if (resultObj.contains("face_list")) {
                QJsonArray faceList = resultObj.take("face_list").toArray();
                //取出第一张人脸表
                QJsonObject face0obj = faceList[0].toObject();

                //取出人脸位置
                if (face0obj.contains("location")) {
                    QJsonObject tObj = face0obj.take("location").toObject();
                    mFaceLeft = tObj.take("left").toDouble();
                    mFaceTop = tObj.take("top").toDouble();
                    mFaceWidth = tObj.take("width").toDouble();
                    mFaceHeight = tObj.take("height").toDouble();
                }

                //取出年龄
                if (face0obj.contains("age")) {
                    age = face0obj.take("age").toDouble();
                    faceInfo.append("年龄: ").append(QString::number(age)).append("\r\n");
                }

                //取出颜值
                // "beauty": 33.62,
                if (face0obj.contains("beauty")) {
                    mBeauty = face0obj.take("beauty").toDouble();
                    faceInfo.append("颜值: ").append(QString::number(mBeauty)).append("\r\n");
                }

                //取出性别
                if (face0obj.contains("gender")) {
                    QJsonObject genderObj = face0obj.take("gender").toObject();
                    gender = genderObj.take("type").toString();
                    faceInfo.append("性别: ").append(gender).append("\r\n");
                }

                //取出是否戴眼镜:
                if (face0obj.contains("glasses")) {
                    QJsonObject tObj= face0obj.take("glasses").toObject();
                    mGlasses = (tObj.take("type").toString() == "none" ? "no" : "yes");
                    faceInfo.append("是否戴眼镜: ").append(mGlasses).append("\r\n");
                }

                //取出是否戴口罩:
                if (face0obj.contains("mask")) {
                    QJsonObject tObj= face0obj.take("mask").toObject();
                    mMask = (tObj.take("type").toInt() == 0 ? "no" : "yes");
                    faceInfo.append("是否戴口罩: ").append(mMask).append("\r\n");
                }

                //取出表情
                if (face0obj.contains("emotion")) {
                    QJsonObject tObj = face0obj.take("emotion").toObject();
                    QString t = tObj.take("type").toString();
                    faceInfo.append("表情: ").append(t).append("\r\n");
                }
            }
        }
    } else {
        qDebug() << "JSON ERROR" << jsonErr.errorString();
        return;
    }

    ui->textBrowser->setText(faceInfo);

}

void Widget::showCamera(int id, QImage img)
{
    Q_UNUSED(id)
    mImg = img;

    QPainter painter(&img);

    painter.setPen(Qt::yellow);
    painter.drawRect(mFaceLeft, mFaceTop, mFaceWidth, mFaceHeight);

    QFont font = painter.font();
    font.setPixelSize(30);
    painter.setFont(font);
    painter.setPen(Qt::red);

    int ht = 15;
    painter.drawText(mFaceLeft + mFaceWidth + 5, mFaceTop + 5 + ht,
                     QString("年龄: %1").arg(age));
    painter.drawText(mFaceLeft + mFaceWidth + 5, mFaceTop + 45 + ht,
                     QString("性别: %1").arg(gender));
    painter.drawText(mFaceLeft + mFaceWidth + 5, mFaceTop + 85 + ht,
                     QString("是否戴口罩: %1").arg(mMask));
    painter.drawText(mFaceLeft + mFaceWidth + 5, mFaceTop + 125 + ht,
                     QString("是否戴眼镜: %1").arg(mGlasses));
    painter.drawText(mFaceLeft + mFaceWidth + 5, mFaceTop + 165 + ht,
                     QString("颜值: %1").arg(mBeauty));

    ui->label->setPixmap(QPixmap::fromImage(img));
}

