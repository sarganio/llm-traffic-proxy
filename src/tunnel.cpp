#include "proxy/socket.hpp"
#include <thread>
#include <iostream>

namespace proxy
{
    namespace
    {
        void forward_one_way(Socket &src, Socket &dst)
        {

            try
            {
                std::array<std::byte, 8192> buffer;
                ptrdiff_t dataSize;
                while (true)
                {
                    dataSize = src.recv_some(buffer);
                    if (dataSize == 0)
                    {
                        dst.shutdown_write();
                        break;
                    }

                    dst.send_all(std::span{buffer}.first(static_cast<std::size_t>(dataSize)));
                }
            }
            catch (const std::exception& e)
            {
                std::cerr<<e.what()<<std::endl;
            }
        }
    }
    void relay_bidirectional(Socket &client, Socket &upstream)
    {
        std::array<std::byte, 8192> buffers;
        std::jthread toClientThread(forward_one_way, std::ref(upstream), std::ref(client));
        forward_one_way(client, upstream);
    }
}
