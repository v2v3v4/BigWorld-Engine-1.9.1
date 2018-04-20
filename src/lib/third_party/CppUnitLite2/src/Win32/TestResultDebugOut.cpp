#include "TestResultDebugOut.h"
#include "../Failure.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <windows.h>


void TestResultDebugOut::StartTests ()
{
    OutputDebugString("\n\nRunning unit tests...\n\n");
}


void TestResultDebugOut::AddFailure (const Failure & failure) 
{
    TestResult::AddFailure(failure);

    std::ostringstream oss;
    oss << failure;
    OutputDebugString(oss.str().c_str());
}

void TestResultDebugOut::EndTests () 
{
    TestResult::EndTests();

    std::ostringstream oss;
    oss << m_testCount << " tests run" << std::endl;
    if (m_failureCount > 0)
        oss << "****** There were " << m_failureCount << " failures." << std::endl;
    else
        oss << "There were no test failures." << std::endl;

    oss << "Test time: " << std::setprecision(3) << m_secondsElapsed << " seconds." << std::endl;

    OutputDebugString(oss.str().c_str());
    OutputDebugString("\n");
}
