#ifndef SERVER_H
#define SERVER_H

#include <QDialog>
#include <QString>
#include <QVector>

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include <raspicam/raspicam.h>
#include <raspicam/raspicam_cv.h>
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <QApplication>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <string>

class QLabel;
class QTcpServer;
class QNetworkSession;

class Server : public QDialog
{
    Q_OBJECT

public:
    explicit Server(QWidget *parent = nullptr);
    cv::Mat tCapturedImage;
    cv::Mat tFlippedImage;
    raspicam::RaspiCam_Cv Camera; //Camera object

    std::string face_cascade_name = "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml";
    cv::CascadeClassifier face_cascade;

    inline QImage  cvMatToQImage( const cv::Mat &inMat );
    inline QPixmap cvMatToQPixmap( const cv::Mat &inMat );
    void detect();

    QImage image;

private slots:
    void sessionOpened();
    void sendFortune();
    void sendImage();
    void capture();
    void nouvelleConnection();
    void getClientData();
    void getCoordinate(QString  message);

private:
    QLabel *statusLabel = nullptr;
    QTcpServer *tcpServer = nullptr;
    QTcpSocket *client = nullptr;
    QVector<QString> fortunes;
    QNetworkSession *networkSession = nullptr;
    bool hasClient = false;
    quint32 tailleMsg = 0;
    QTimer timer;
};


#endif
