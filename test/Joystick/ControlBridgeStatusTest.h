#pragma once
#include "UnitTest.h"

class ControlBridgeStatusTest : public UnitTest
{
    Q_OBJECT
private slots:
    void _packetCompatibility();
    void _disconnectStopsForwarding();
    void _droppedEventsStopForwarding();
};
