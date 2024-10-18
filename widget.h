#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QCamera>
#include <QCameraViewfinder>
#include <QCameraImageCapture>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QThread>


QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void on_pushButton_clicked();

private:
    QCamera *camera;
    QCameraViewfinder *finder;
    QCameraImageCapture * capture;
    QList<QCameraInfo> mCameraList;

private:
    QTimer *refreshTimer;
    QTimer *workThreadTimer;

private:
    QNetworkAccessManager *tokenManager;
    QNetworkAccessManager *imgManager;
    QSslConfiguration sslConfig;

private:
    QString accessToken;

private:
    QImage mImg;

private:
    void parseFaseJson(const QByteArray& replyData);

private:
    QThread *workerThread;

private:
    double mFaceLeft;
    double mFaceTop;
    double mFaceWidth;
    double mFaceHeight;

    QString gender;
    double age;
    QString mMask;
    double mBeauty;
    QString mGlasses;

private:
    int curTimeTamp;

signals:
    void beginWork(QImage mImg, QThread *workerThread);

private slots:
    void showCamera(int id, QImage img);


private:
    Ui::Widget *ui;
};
#endif // WIDGET_H
