#ifndef QCASUZMQ_H
#define QCASUZMQ_H

#define dCast static_cast<zmqData::dataType>

#include <QObject>
#include <QTime>

#include <fstream>

#include <nzmqt/nzmqt.hpp>
#include "QCustomPlot/qcustomplot.h"

#include "dev_msgs.pb.h"
#include "globalHeader.h"

namespace zmqData {
    enum dataType {IR_F, IR_FL, IR_BL, IR_B, IR_BR, IR_FR, // _IR_num = 6
        Temp_F, Temp_L, Temp_B, Temp_R, Temp_Top, Temp_Pcb, Temp_Ring, Temp_Wax, // _Temp_num = 8
        Frequency, Amplitude, StdDev,
        Peltier, Airflow, Speaker, LED};

    const static int _IR_num = 6;
    const static int _Temp_num = 8;
    const static int _dataType_num = 21;

    class zmqBuffer : public QObject, public QCPDataMap
        {
            Q_OBJECT
        private:
            QString _legendName;
            QString _casuName;
            dataType _key;
        public:
            zmqBuffer(QString casuName, dataType key);
            void insert(const double &key, const QCPData &value);
            void erase(QMap::iterator it);
            QString getLegendName();
            QString getCasuName();
            dataType getDataType();
            double getLastTime();
        signals:
            void updatePlot();
        };
}

class QCasuZMQ : public QObject
{
    Q_OBJECT
    friend class QCasuSceneItem;
public:
    explicit QCasuZMQ(QObject *parent = 0, QString casuName = QString());
    zmqData::zmqBuffer* getBuffer(zmqData::dataType key);
    double getValue(zmqData::dataType key);
    QColor getLedColor();
    bool getState(zmqData::dataType key);
    int getAvgSamplingTime();
    QString getName();

    void setAddresses(QString sub, QString pub, QString msg);
    void setAddresses(QStringList addresses);

    QStringList getAddresses();
    bool sendSetpoint(QList<QByteArray> message);

    bool isConnected();


private:
    nzmqt::ZMQContext* _context;
    nzmqt::ZMQSocket* _pubSock;
    nzmqt::ZMQSocket* _subSock;

    QString _subAddr;
    QString _pubAddr;
    QString _msgAddr;
    QString _name;

    QTimer* _connectionTimer;

    QMap<zmqData::dataType, zmqData::zmqBuffer*> _buffers;
    QMap<zmqData::dataType, QCPData> _values;
    QMap<zmqData::dataType, double> _lastDataTime;
    QMap<zmqData::dataType, bool> _state;
    QColor _ledColor;

    std::ofstream _logFile;
    QString _logName;
    bool _logOpen;

    bool _connected = false;

    void openLogFile();
    void closeLogFile();

    void addToBuffer(zmqData::dataType key, QCPData data);
    void connectZMQ();

signals:
    void updated(zmqData::dataType key);
    void connectMsg(QString message);

private slots:
    void messageReceived(const QList<QByteArray>& message);
};

#endif // QCASUZMQ_H
