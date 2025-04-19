#include "SerialModbus.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QVariant>
#include <QEventLoop>
#include <QTimer>
#include "LoggerMacros.h"
#include <QDebug>

SerialModbus::SerialModbus(int serverAddress)
    :m_serverAddress(serverAddress)
{
    m_replay=new QModbusReply(QModbusReply::Common,m_serverAddress);
    connectDevice();
}

bool SerialModbus::connectDevice()
{
    if (m_modbusDevice) {
        delete m_modbusDevice;
        m_modbusDevice = nullptr;
    }

    m_modbusDevice = new QModbusRtuSerialClient();

    QList<QSerialPortInfo> infos=QSerialPortInfo::availablePorts();
    foreach(const QSerialPortInfo &info,infos)
    {
        QSerialPort serialPort;
        serialPort.setPortName(info.portName());
        serialPort.setBaudRate(QSerialPort::Baud115200);
        serialPort.setDataBits(QSerialPort::Data8);
        serialPort.setParity(QSerialPort::NoParity);
        serialPort.setStopBits(QSerialPort::OneStop);
        serialPort.setFlowControl(QSerialPort::NoFlowControl);

        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter, serialPort.portName());
        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter, serialPort.parity());
        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, serialPort.baudRate());
        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, serialPort.dataBits());
        m_modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, serialPort.stopBits());

        if (!m_modbusDevice->connectDevice()) {
            m_modbusDevice->disconnectDevice();
            continue;
        }
        else{
            const int startAddress = 1;
            const int numRegisters = 1;
            QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, startAddress, numRegisters);
            QModbusReply* reply=read_data(readUnit);
            if(reply->result().value(0)==0xdd){
                return true;
            }
            else{
                m_modbusDevice->disconnectDevice();
            }

        }
    }
    return false;
}

bool SerialModbus::write_data(QModbusDataUnit readUnit)
{
    return  block(readUnit,cmd_write);
}

QModbusReply* SerialModbus::read_data(QModbusDataUnit readUnit)
{

    block(readUnit,cmd_read);
    return m_replay;

}

bool SerialModbus::block(QModbusDataUnit readUnit,CmdType type)
{
    m_replay = type==cmd_read ? m_modbusDevice->sendReadRequest(readUnit, m_serverAddress):m_modbusDevice->sendWriteRequest(readUnit, m_serverAddress);
    if (m_replay) {
        QEventLoop loop;
        QObject::connect(m_replay, &QModbusReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);

        loop.exec();

        if (m_replay->isFinished()) {
            if (m_replay->error() == QModbusDevice::NoError) {
                return true;
            }else {
                QString error=QString("Modbus错误:%1").arg(m_replay->errorString());
                LOG_ERROR(error.toStdString());

            }
        } else {
            QString error=QString("Modbus错误:%1").arg("请求超时");
            LOG_ERROR(error.toStdString());
        }

    } else {
        QString error=QString("Modbus错误:%1").arg("发送请求失败");
        LOG_ERROR(error.toStdString());
    }

    return false;
}

