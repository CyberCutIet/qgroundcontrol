#pragma once
#include "UnitTest.h"

class RpiLinkStatusTest : public UnitTest
{
    Q_OBJECT
private slots:
    void _datagramsAndFreshness();
    void _orderingAndSessions();
};
