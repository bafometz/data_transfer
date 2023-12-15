#ifndef MAINOBJECT_H
#define MAINOBJECT_H
#include <string>

class MainObject
{
  public:
    explicit MainObject(int argc, char** argv);
    MainObject(const MainObject&)            = delete;
    MainObject(MainObject&&)                 = delete;
    MainObject& operator=(const MainObject&) = delete;
    MainObject& operator=(MainObject&&)      = delete;

    int start();
    ~MainObject();

  private:
    bool              isServer_ = false;
    bool              isClient_ = false;
    int               port_     = 7071;
    std::string       filepath_ {};
    const std::string usage_ =
        R"(
       Usage:

        DataTransfer [type_arg] [additional_arg]
        type args:
            -s Start server for reciving application, additional_arg ignored
            -c Start client for sending data, additional arg required

        [additional_arg]
            /path/to/file - The path to the file to be sent.

        [optional_args]
            -p port - The number of the port that the server will open or to
                   which the client will be connected
         )";
};

#endif  // MAINOBJECT_H
