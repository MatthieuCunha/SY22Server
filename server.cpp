#include <QtWidgets>
#include <QtNetwork>
#include <QtCore>

#include "server.h"


using namespace std;
using namespace cv;


Server::Server(QWidget *parent)
    : QDialog(parent)
    , statusLabel(new QLabel)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    statusLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    QNetworkConfigurationManager manager;
    if (manager.capabilities() & QNetworkConfigurationManager::NetworkSessionRequired) {
        // Get saved network configuration
        QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
        settings.beginGroup(QLatin1String("QtNetwork"));
        const QString id = settings.value(QLatin1String("DefaultNetworkConfiguration")).toString();
        settings.endGroup();

        // If the saved network configuration is not currently discovered use the system default
        QNetworkConfiguration config = manager.configurationFromIdentifier(id);
        if ((config.state() & QNetworkConfiguration::Discovered) !=
            QNetworkConfiguration::Discovered) {
            config = manager.defaultConfiguration();
        }

        networkSession = new QNetworkSession(config, this);
        connect(networkSession, &QNetworkSession::opened, this, &Server::sessionOpened);

        statusLabel->setText(tr("Opening network session."));
        networkSession->open();
    } else {
        sessionOpened();
    }

    fortunes << tr("You've been leading a dog's life. Stay off the furniture.")
             << tr("You've got to think about tomorrow.")
             << tr("You will be surprised by a loud noise.")
             << tr("You will feel hungry again in another hour.")
             << tr("You might have mail.")
             << tr("You cannot kill time without injuring eternity.")
             << tr("Computers are not intelligent. They only think they are.");
    auto quitButton = new QPushButton(tr("Quit"));
    quitButton->setAutoDefault(false);
    connect(quitButton, &QAbstractButton::clicked, this, &QWidget::close);
    //connect(tcpServer, &QTcpServer::newConnection, this, &Server::sendFortune);
    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(nouvelleConnection()));

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(quitButton);
    buttonLayout->addStretch(1);

    QVBoxLayout *mainLayout = nullptr;
    if (QGuiApplication::styleHints()->showIsFullScreen() || QGuiApplication::styleHints()->showIsMaximized()) {
        auto outerVerticalLayout = new QVBoxLayout(this);
        outerVerticalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));
        auto outerHorizontalLayout = new QHBoxLayout;
        outerHorizontalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Ignored));
        auto groupBox = new QGroupBox(QGuiApplication::applicationDisplayName());
        mainLayout = new QVBoxLayout(groupBox);
        outerHorizontalLayout->addWidget(groupBox);
        outerHorizontalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Ignored));
        outerVerticalLayout->addLayout(outerHorizontalLayout);
        outerVerticalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));
    } else {
        mainLayout = new QVBoxLayout(this);
    }

    mainLayout->addWidget(statusLabel);
    mainLayout->addLayout(buttonLayout);

    setWindowTitle(QGuiApplication::applicationDisplayName());

    Camera.set(CV_CAP_PROP_FRAME_HEIGHT,480);
    Camera.set(CV_CAP_PROP_FRAME_WIDTH,640);
    Camera.set(CV_CAP_PROP_FORMAT, CV_8UC1);

    //Open camera
    std::cout<<"Opening Camera..."<< std::endl;
    if ( !Camera.open()) { std::cerr<<"Error opening camera"<< std::endl;}


    if(!face_cascade.load(face_cascade_name)) {
        std::cout << "Error loading haarcascade" << std::endl;
    }else{
        std::cout << "loading haarcascade OK" << std::endl;
    }

    timer.start(3000);
}

void Server::nouvelleConnection(){
    //msgBox->append("sendData");
    if(!hasClient){
        hasClient = true;

        client = tcpServer->nextPendingConnection();
        connect(client, SIGNAL(disconnected()), client, SLOT(deleteLater()));
        connect(client,SIGNAL(readyRead()), this, SLOT(getClientData()));

        connect(&timer, SIGNAL(timeout()), this, SLOT(sendFortune()));
    }
}

void Server::getClientData()
{
    //qDebug()<<"hi from getClientData";
    QDataStream in(client);
    in.setVersion(QDataStream::Qt_5_7);
    if(tailleMsg == 0){
        qDebug() << client->bytesAvailable();

        if(client->bytesAvailable() < (int)sizeof(quint32)){
            return;
        }
        in>>tailleMsg;
    }

    if(client->bytesAvailable() < tailleMsg){
        return;
    }

    QString msg;
    in>>msg;
    //msgBox->append(msg);
    tailleMsg = 0;


    qDebug() << "message " << msg;
    getCoordinate(msg);

}

void Server::sessionOpened()
{
    // Save the used configuration
    if (networkSession) {
        QNetworkConfiguration config = networkSession->configuration();
        QString id;
        if (config.type() == QNetworkConfiguration::UserChoice)
            id = networkSession->sessionProperty(QLatin1String("UserChoiceConfiguration")).toString();
        else
            id = config.identifier();

        QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
        settings.beginGroup(QLatin1String("QtNetwork"));
        settings.setValue(QLatin1String("DefaultNetworkConfiguration"), id);
        settings.endGroup();
    }

    tcpServer = new QTcpServer(this);
    if (!tcpServer->listen(QHostAddress::AnyIPv4, 4661)) {
        QMessageBox::critical(this, tr("Fortune Server"),
                              tr("Unable to start the server: %1.")
                              .arg(tcpServer->errorString()));
        close();
        return;
    }
    QString ipAddress;
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    // use the first non-localhost IPv4 address
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (ipAddressesList.at(i) != QHostAddress::LocalHost &&
            ipAddressesList.at(i).toIPv4Address()) {
            ipAddress = ipAddressesList.at(i).toString();
            break;
        }
    }
    // if we did not find one, use IPv4 localhost
    if (ipAddress.isEmpty())
        ipAddress = QHostAddress(QHostAddress::LocalHost).toString();
    statusLabel->setText(tr("The server is running on\n\nIP: %1\nport: %2\n\n"
                            "Run the Fortune Client example now.")
                         .arg(ipAddress).arg(tcpServer->serverPort()));
}

void Server::sendFortune()
{
    /*QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_7);

    out << fortunes[1];

    QTcpSocket *clientConnection = tcpServer->nextPendingConnection();
    connect(clientConnection, &QAbstractSocket::disconnected,
            clientConnection, &QObject::deleteLater);

    clientConnection->write(block);
    clientConnection->disconnectFromHost();*/
    sendImage();
}

void Server::capture() {

        //capture
        Camera.grab();

        //extract the image
        Camera.retrieve ( tCapturedImage);//get camera image0
        flip(tCapturedImage, tFlippedImage, 0); // Tourner l'image
        detect();

        // Converti le cv::mat en Qpixmap
        //QPixmap pixmap_img = cvMatToQPixmap(Server::tFlippedImage);
        image = cvMatToQImage(tFlippedImage);
}


QImage Server::cvMatToQImage(const cv::Mat &inMat)
{
    switch ( inMat.type() )
          {
             // 8-bit, 4 channel
             case CV_8UC4:
             {
                QImage image( inMat.data,
                              inMat.cols, inMat.rows,
                              static_cast<int>(inMat.step),
                              QImage::Format_ARGB32 );

                return image;
             }

             // 8-bit, 3 channel
             case CV_8UC3:
             {
                QImage image( inMat.data,
                              inMat.cols, inMat.rows,
                              static_cast<int>(inMat.step),
                              QImage::Format_RGB888 );

                return image.rgbSwapped();
             }

             // 8-bit, 1 channel
             case CV_8UC1:
             {
    #if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
                QImage image( inMat.data,
                              inMat.cols, inMat.rows,
                              static_cast<int>(inMat.step),
                              QImage::Format_Grayscale8 );
    #else
                static QVector<QRgb>  sColorTable;

                // only create our color table the first time
                if ( sColorTable.isEmpty() )
                {
                   sColorTable.resize( 256 );

                   for ( int i = 0; i < 256; ++i )
                   {
                      sColorTable[i] = qRgb( i, i, i );
                   }
                }

                QImage image( inMat.data,
                              inMat.cols, inMat.rows,
                              static_cast<int>(inMat.step),
                              QImage::Format_Indexed8 );

                image.setColorTable( sColorTable );
    #endif

                return image;
             }

             default:
                std::cout << "ASM::cvMatToQImage() - cv::Mat image type not handled in switch:";
                break;
          }

          return QImage();

}

QPixmap Server::cvMatToQPixmap(const cv::Mat &inMat)
{
    return QPixmap::fromImage( cvMatToQImage( inMat ) );
}

void Server::detect() {

    // detect faces
    std::vector<cv::Rect> faces;
    face_cascade.detectMultiScale(Server::tFlippedImage, faces, 1.1, 2, 0|CV_HAAR_SCALE_IMAGE, cv::Size(30, 30));

    // Draw circles
    for(int i=0; i<faces.size(); i++) {
        cv::Point center(faces[i].x + faces[i].width*0.5, faces[i].y + faces[i].height*0.5);
        ellipse(Server::tFlippedImage, center, cv::Size(faces[i].width*0.5, faces[i].height*0.5), 0, 0, 360, cv::Scalar(255 ,0 , 255), 4, 8, 0);
    }

}

void Server::sendImage()
{

    if(client==nullptr){return;}
      qDebug() << client->state();

    capture();
    QByteArray tjpeg;
    QBuffer buffer(&tjpeg);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer,"JPG");
    buffer.close();

    QByteArray trame;
    QDataStream out(&trame, QIODevice::WriteOnly);
//    out.open();
    out.setVersion(QDataStream::Qt_5_7);
    out << (quint32)0;
    out<<tjpeg;
    out.device()->seek(0);
    out<<((trame.size()-sizeof(quint32)));
//    //QImage image = Server::getImage();
//    if(image.save(&buffer, "JPG")){
//        qDebug() << "buffer write success ";
//    }else{
//        qDebug() << "buffer write fail ";
//    }
//    out << (quint32)(trame.size() - sizeof(quint32));

    //client = tcpServer->nextPendingConnection();
    //connect(client,SIGNAL(disconnected()),client,SLOT(deleteLater()));
    client->write(trame);
}

void Server::getCoordinate(QString message){


    QStringList parts = message.split(',');

    int coorX = parts[0].toInt();
    int coorY = parts[1].toInt();

       // appel a la fonction d'entrainement

}
