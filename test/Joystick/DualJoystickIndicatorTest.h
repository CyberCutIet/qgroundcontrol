#pragma once

#include "UnitTest.h"

class DualJoystickIndicatorTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _connectionStatesWithoutVehicle();
    void _rssiStates();
    void _repeaterAndLossStates();
};
