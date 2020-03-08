#include "base/base.h"
#include "base/logger.h"
#include "base/application.h"
#include "net/netInterface.h"
#include "net/TcpConnection.h"
#include "awsUdpClient.h"
#include "base/time.h"
#include "tcpUpload.h"

using std::endl;
using namespace base;
using namespace net;

class awsTcpClient : public TcpConnection {
public:

    awsTcpClient(const std::string ip, const std::string fileName) : m_IP(ip), m_fileName(fileName), udpsocket(nullptr),
    TcpConnection(this) {
    }

    void start(std::string ip, int port) {

        Connect(ip, port);
    }

    void sendPacket(uint8_t type, uint16_t payload) {

        STrace << "Send Type" << (int) type << " payload " << payload;

        TcpPacket tcpPacket;
        int size_of_packet = sizeof (struct TcpPacket);
        tcpPacket.type = type;
        tcpPacket.sequence_number = payload;

        char *send_buffer = (char*) malloc(size_of_packet);
        memset(send_buffer, 0, size_of_packet);
        memcpy(send_buffer, (char*) &tcpPacket, size_of_packet);
        send(send_buffer, size_of_packet);
        free(send_buffer);
    }

    void on_connect() {
        STrace << "on_connect Send Init: ";
        sendPacket(0, 0);
    }

    void shutdown() {
        Close();
    }

    void on_close(Listener* connection) {
        std::cout << " Close Con LocalIP" << connection->GetLocalIp() << " PeerIP" << connection->GetPeerIp() << std::endl << std::flush;
    }

    void on_read(Listener* connection, const char* data, size_t len) {
        STrace << "TCP on_read " << "len: " << len;
        // connection->send((const char*) send.c_str(), 5);

        if (len != sizeof (struct TcpPacket)) {
            LError("Fatal error: Some part of packet lost. ")
            return;
        }

        TcpPacket packet;
        memcpy(&packet, (void*) data, len);

        switch (packet.type) {
            case 1:
            {
                LTrace("First TCP Packet received")
                STrace << "TCP Received type " << (int) packet.type << " payload:" << packet.sequence_number;

                udpsocket = new awsUdpClient(m_IP, packet.sequence_number);
                udpsocket->start();
                
                udpsocket->sendFile(m_fileName);

                break;
            }
            case 2:
            {

                LTrace("First TCP Packet received")
                STrace << "TCP Received type " << packet.type << " payload:" << packet.sequence_number;
                break;
            }

            case 3:
            {
                LTrace("First TCP Packet received")
                STrace << "TCP Received type " << packet.type << " payload:" << packet.sequence_number;
                break;
            }
            default:
            {
                LError("Fatal TCP: Not a valid state")
            }
        };

    }

    // TcpConnection *tcpClient;

    awsUdpClient *udpsocket;
    std::string m_IP;
    std::string m_fileName;

};

int main(int argc, char** argv) {
    Logger::instance().add(new ConsoleChannel("debug", Level::Trace));

    Application app;


    int port = 41038;
    std::string ip = "18.221.232.217";
    std::string filename;

    if (argc > 1) {
        filename = argv[1];
    }

    if (argc > 2) {
        ip = argv[2];
    }

    if (argc > 3) {
        port = atoi(argv[3]);
    }

    awsTcpClient socket(ip, filename);
    socket.start(ip, port);

    app.waitForShutdown([&](void*) {
        socket.shutdown();

    });


    return 0;
}