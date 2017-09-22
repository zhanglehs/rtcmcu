#include "condition.hpp"
#include "condition_impl.hpp"



class FifoConditionImpl
            : public ConditionImpl<fifo_list>
{
public:
    FifoConditionImpl(Lockable& l)
            : ConditionImpl<fifo_list>(l)
    {
    }

};

Condition::Condition(Lockable& lock)
{

    _impl = new FifoConditionImpl(lock);

}


Condition::~Condition()
{

    if(_impl != 0)
        delete _impl;

}



void Condition::wait()
{

    _impl->wait();

}



bool Condition::wait(unsigned long ms)
{

    return _impl->wait(ms);

}



void Condition::signal()
{

    _impl->signal();

}


void Condition::broadcast()
{

    _impl->broadcast();

}


