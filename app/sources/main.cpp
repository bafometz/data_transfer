#include "main_object/mainobject.h"

int main(int argc, char** argv)
{
    MainObject obj(argc, argv);
    return obj.start();
}
