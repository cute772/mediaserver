#include "base/base.h"
#include "base/logger.h"
#include "base/application.h"
#include "net/TcpServer.h"
#include "base/time.h"
#include "net/netInterface.h"
#include "tcpUpload.h"
#include "awsUdpServer.h"

using std::endl;
using namespace base;
using namespace net;


class awsTcpServer : public Listener {
public:

    awsTcpServer() {
    }

    void start(std::string ip, int port) {
        // socket.send("Arvind", "127.0.0.1", 7331);
        tcpServer = new TcpServer(this, ip, port);
        m_ip = ip;
    }

    void shutdown() {
        // socket.send("Arvind", "127.0.0.1", 7331);
        delete tcpServer;
        tcpServer = nullptr;

    }

    void on_close(Listener* connection) {

        std::cout << "TCP server closing, LocalIP" << connection->GetLocalIp() << " PeerIP" << connection->GetPeerIp() << std::endl << std::flush;

    }

    void sendPacket(Listener* connection, uint8_t type, uint16_t payload) {

        STrace << "Send " << type << " payload " << payload;

            TcpPacket tcpPacket;
        int size_of_packet = sizeof (struct TcpPacket);
        tcpPacket.type = type;
        tcpPacket.sequence_number = payload;
        
        char *send_buffer = (char*)malloc(size_of_packet);
        memset(send_buffer, 0, size_of_packet);
        memcpy(send_buffer, (char*) &tcpPacket, size_of_packet);
        connection->send(send_buffer, size_of_packet);
        free(send_buffer);
    }

    void on_read(Listener* connection, const char* data, size_t len) {
       // STrace << "TCP server on_read: " << data << "len: " << len;
      ///  std::string send = "12345";
        //connection->send((const char*) send.c_str(), 5);
        
        if (len != sizeof (struct TcpPacket)) {
            LError("Fatal error: Some part of packet lost. ")
            return;
        }

        TcpPacket packet;
        memcpy(&packet, (void*) data, len);

        switch (packet.type) {
            case 0:
            {
                LTrace("First TCP Packet received")
                STrace << "TCP Packet type " <<  (int) packet.type << " payload: " << packet.sequence_number;

                uint16_t port = 51038;

                int portmangersize = udpPortManager.size();
                if (portmangersize) {
                    port = (--udpPortManager.end())->first + 1;
                    // todo circular port allocation
                }
                
                awsUdpServer  *socket  = new awsUdpServer(m_ip, port);
                socket->start();
                udpPortManager[port]= socket;
               
                sendPacket( connection, 1, port);
              
                break;
            }
            case 2:
            {
                
                LTrace("First TCP Packet received")
                STrace << "TCP Received type " << (int) packet.type << " payload:" << packet.sequence_number;
                break;
            }

            case 3:
            {
                LTrace("First TCP Packet received")
                STrace << "TCP Received type " <<  packet.type << " payload:" << packet.sequence_number;
                break;
                break;
            }
            default:
            {
                LError("Fatal TCP: Not a valid state")
            }

        };


    }
    TcpServer *tcpServer;

    std::map<uint16_t, awsUdpServer* > udpPortManager;
    std::string m_ip;

};

int main(int argc, char** argv) {
    Logger::instance().add(new ConsoleChannel("debug", Level::Trace));


    int port = 41038;

    std::string ip = "0.0.0.0";

    if (argc > 1) {
        ip = argv[1];
    }

    if (argc > 2) {
        port = atoi(argv[2]);
    }


    Application app;

    awsTcpServer socket;
    socket.start(ip, port);



    app.waitForShutdown([&](void*) {

        socket.shutdown();

    });



    return 0;
}