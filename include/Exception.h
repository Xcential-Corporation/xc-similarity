#ifndef EXCEPTION_H
#define EXCEPTION_H
#include <string>

class Exception
{
public:
    Exception();
    virtual ~Exception();

    virtual std::string message() = 0;
};

#endif // EXCEPTION_H
