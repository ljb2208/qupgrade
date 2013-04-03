//#include <QJsonDocument>
#include <QFile>

#include "qgcfirmwareupgradeworker.h"

#include <qgc.h>
#include "uploader.h"
#include "qextserialenumerator.h"

#include <QDebug>

QGCFirmwareUpgradeWorker::QGCFirmwareUpgradeWorker(QObject *parent) :
    QObject(parent),
    _abortUpload(false),
    _filterBoardId(5),
    port(NULL)
{
}

QGCFirmwareUpgradeWorker* QGCFirmwareUpgradeWorker::putWorkerInThread(const QString &filename)
{
    QGCFirmwareUpgradeWorker *worker = NULL;
    QThread *thread = NULL;

    worker = new QGCFirmwareUpgradeWorker;
    worker->setFilename(filename);
    thread = new QThread;

    worker->moveToThread(thread);
//    connect(worker, SIGNAL(error(QString)), parent, SLOT(errorString(QString)));
    connect(thread, SIGNAL(started()), worker, SLOT(loadFirmware()));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    thread->start();

    // Starts an event loop, and emits workerThread->started()
    thread->start();
    return worker;
}


void QGCFirmwareUpgradeWorker::startContinousScan()
{
    exitThread = false;
    while (!exitThread) {
        //        if (detect()) {
        //            break;
        //        }
        SLEEP::msleep(100);
    }

    if (exitThread) {
        port->close();
        delete port;
        exit(0);
    }
}

void QGCFirmwareUpgradeWorker::detect()
{
    

}


void QGCFirmwareUpgradeWorker::setBoardId(int id) {
    _filterBoardId = id;
}

void QGCFirmwareUpgradeWorker::setFilename(const QString &filename)
{
    this->filename = filename;
}

void QGCFirmwareUpgradeWorker::loadFirmware()
{
    qDebug() << __FILE__ << __LINE__ << "LOADING FW" << filename;

    emit upgradeStatusChanged(tr("Attempting to upload file:\n%1").arg(filename));

    while (!_abortUpload) {

        SLEEP::usleep(200000);

        QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();

        foreach (QextPortInfo info, ports) {

            // Check for valid handles
            if (info.portName.isEmpty())
                continue;

            if (info.physName.contains("PX4") || info.vendorID == 9900 /* 3DR */) {

                qDebug() << "port name:"       << info.portName;
                qDebug() << "friendly name:"   << info.friendName;
                qDebug() << "physical name:"   << info.physName;
                qDebug() << "enumerator name:" << info.enumName;
                qDebug() << "vendor ID:"       << info.vendorID;
                qDebug() << "product ID:"      << info.productID;

                qDebug() << "===================================";

                QString openString = info.portName;

                qDebug() << "UPLOAD ATTEMPT";

                // Spawn upload thread

                if (port == NULL) {
                    PortSettings settings = {BAUD115200, DATA_8, PAR_NONE, STOP_1, FLOW_OFF, 10};

                    port = new QextSerialPort(openString, settings, QextSerialPort::Polling);
                    port->setTimeout(0);
                    port->setQueryMode(QextSerialPort::Polling);
                } else {
                    port->close();
                    port->setPortName(openString);
                }

                PX4_Uploader uploader(port);
                // Relay status to top-level UI
                connect(&uploader, SIGNAL(upgradeProgressChanged(int)), this, SIGNAL(upgradeProgressChanged(int)));
                connect(&uploader, SIGNAL(upgradeStatusChanged(QString)), this, SIGNAL(upgradeStatusChanged(QString)));

                // Die-hard flash the binary
                emit upgradeStatusChanged(tr("Found PX4 board on port %1").arg(info.portName));

                //            quint32 board_id;
                //            quint32 board_rev;
                //            quint32 flash_size;
                //            bool insync = false;
                //            QString humanReadable;
                //uploader.get_bl_info(board_id, board_rev, flash_size, humanReadable, insync);

                int ret = uploader.upload(filename, _filterBoardId);

                // bail out on success
                if (ret == 0) {
                    emit loadFinished(true);
                    emit finished();
                    return;
                }
            } else if (info.physName.contains("3DR Radio") || info.vendorID == 9900 /* 3DR */) {

            } else {
                // Attempt to scan for devices attached via FTDI, e.g. 3DR Radios

                // Instantiate 3DR radio uploader

                // XXX needs checking for type of firmware file
            }
        }
    }

    _abortUpload = false;
    emit loadFinished(false);
    emit finished();

}

void QGCFirmwareUpgradeWorker::abortUpload()
{
    _abortUpload = true;
}

void QGCFirmwareUpgradeWorker::abort()
{
    exitThread = true;
}
