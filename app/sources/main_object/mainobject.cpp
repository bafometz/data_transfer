#include "mainobject.h"

#include "../client/client.h"
#include "../helpers/helpers.h"
#include "../server/server.h"

#include <iostream>

MainObject::MainObject(int argc, char** argv)
{
    DatatPackage::generateCrc32Table();

    for (int i = 0; i < argc; ++i)
    {
        auto current_arg  = [&argv, &i]() { return std::string(argv[i]); };
        auto hasNextArg   = [&i, &argc]() -> bool { return ((i + 1) < argc); };
        auto isOnlyDigits = [](const std::string& str) { return str.find_first_not_of("0123456789") == std::string::npos; };
        if (argc == 1)
        {
            std::cout << "You need to supply one or more argument to this program" << std::endl;
            std::cout << usage_ << std::endl;
        }

        if (current_arg() == "-c")
        {
            isClient_ = true;
            if (hasNextArg())
            {
                i++;
                if (!helpers::isFileExist(std::string(argv[i])))
                {
                    std::cout << "File doesn't exist" << std::endl;
                    std::cout << usage_ << std::endl;
                    exit(1);
                }
                filepath_ = current_arg();
                continue;
            }
            else
            {
                std::cout << "Client args must be passed with path to file" << std::endl;
                std::cout << usage_ << std::endl;
            }
            continue;
        }

        if (current_arg() == "-s")
        {
            isServer_ = true;
            continue;
        }

        if (current_arg() == "-p" && hasNextArg())
        {
            i++;
            if (!isOnlyDigits(current_arg()))
            {
                std::cout << "Port must contain only digits, fallback to default port" << std::endl;
                continue;
            }

            port_ = std::stoi(current_arg());
            continue;
        }
    }

    if (isServer_ && isClient_)
    {
        std::cout << "Can't start both application" << std::endl;
        std::cout << usage_ << std::endl;
        std::exit(1);
    }
}

int MainObject::start()
{
    if (isServer_)
    {
        Server serv(port_);
        return serv.start();
    }
    else if (isClient_)
    {
        Client client("127.0.0.1", port_);


        // auto th1 = std::thread(
        //     [this]()
        //     {
        //         Client client("127.0.0.1", port_);
        //         client.sendFile(filepath_);
        //     });
        // auto th2 = std::thread(
        //     [this]()
        //     {
        //         Client client("127.0.0.1", port_);
        //         std::this_thread::sleep_for(std::chrono::milliseconds(500));
        //         client.sendFile(filepath_);
        //     });
        // auto th3 = std::thread(
        //     [this]()
        //     {
        //         Client client("127.0.0.1", port_);
        //         std::this_thread::sleep_for(std::chrono::milliseconds(750));
        //         client.sendFile(filepath_);
        //     });

        // th1.join();
        // th2.join();
        // th3.join();
        return  client.sendFile(filepath_);
    }
    else
    {
        return -1;
    }
    return -1;
}

MainObject::~MainObject() {}
