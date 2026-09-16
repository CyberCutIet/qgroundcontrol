#pragma once
#include "UnitTest.h"

class WfbLinkStatusTest : public UnitTest
{
    Q_OBJECT
private slots:
    void _rssiOnlyScale();
    void _freshnessAndFallback();
    void _tcpFramesAndReconnect();
    void _packetLossWindow();
};
