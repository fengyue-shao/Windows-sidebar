#pragma once
#ifndef PERIODICMODULE_H
#define PERIODICMODULE_H

class PeriodicModule
{
public:
    virtual ~PeriodicModule() = default;
    virtual void refresh() = 0;
};

#endif // PERIODICMODULE_H
