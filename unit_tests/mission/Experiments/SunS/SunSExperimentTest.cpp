#include <chrono>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "OsMock.hpp"
#include "base/writer.h"
#include "experiment/suns/suns.hpp"
#include "experiments/experiments.h"
#include "fs/ExperimentFile.hpp"
#include "mock/FsMock.hpp"
#include "mock/GyroMock.hpp"
#include "mock/PayloadDeviceMock.hpp"
#include "mock/SunSDriverMock.hpp"
#include "mock/power.hpp"
#include "mock/time.hpp"
#include "power/power.h"

using testing::Eq;
using testing::Return;
using testing::_;
using testing::Invoke;
using testing::SetArgReferee;
using testing::DoAll;
using testing::ElementsAre;
using testing::InSequence;
using testing::StrEq;
using namespace experiment::suns;
using namespace experiments;
using experiments::IterationResult;
using experiments::StartResult;
using namespace std::chrono_literals;
using namespace devices::suns;
using devices::payload::PayloadTelemetry;
using devices::gyro::GyroscopeTelemetry;
using experiments::fs::ExperimentFile;
using services::fs::FileOpen;
using services::fs::FileAccess;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

namespace
{
    class SunSExperimentTest : public testing::Test
    {
      protected:
        SunSExperimentTest();

        testing::NiceMock<PowerControlMock> _power;
        testing::NiceMock<CurrentTimeMock> _timeProvider;
        testing::NiceMock<SunSDriverMock> _sunsExp;
        testing::NiceMock<PayloadDeviceMock> _payload;
        testing::NiceMock<GyroscopeMock> _gyro;
        testing::NiceMock<FsMock> _fs;
        testing::NiceMock<OSMock> _os;
        OSReset _osReset{InstallProxy(&_os)};

        SunSExperiment _exp{_power, _timeProvider, _sunsExp, _payload, _gyro, _fs};

        std::chrono::milliseconds _time{0ms};

        std::array<std::uint8_t, 900> _buf1;
        std::array<std::uint8_t, 900> _buf2;
    };

    SunSExperimentTest::SunSExperimentTest()
    {
        ON_CALL(_power, SensPower(_)).WillByDefault(Return(true));
        ON_CALL(_power, SunSPower(_)).WillByDefault(Return(true));

        ON_CALL(_timeProvider, GetCurrentTime()).WillByDefault(Invoke([this]() { return Some(this->_time); }));

        _fs.AddFile("/exp", _buf1);
        _fs.AddFile("/exp_sec", _buf2);
    }

    TEST_F(SunSExperimentTest, TestExperimentStartStop)
    {
        _exp.SetOutputFiles("/exp");
        auto r = _exp.Start();
        ASSERT_THAT(r, Eq(StartResult::Success));

        _exp.Stop(IterationResult::Finished);
    }

    TEST_F(SunSExperimentTest, IterationsTiming)
    {
        SunSExperimentParams params(1, 2, 2, 1s, 3, 5min);

        _exp.SetParameters(params);
        _exp.SetOutputFiles("/exp");

        _exp.Start();

        this->_time = 10h;

        auto r = _exp.Iteration();
        ASSERT_THAT(r, Eq(IterationResult::WaitForNextCycle));

        this->_time += 2min;

        r = _exp.Iteration();
        ASSERT_THAT(r, Eq(IterationResult::WaitForNextCycle));

        this->_time += 3min;

        r = _exp.Iteration();
        ASSERT_THAT(r, Eq(IterationResult::WaitForNextCycle));

        this->_time += 5min;

        r = _exp.Iteration();
        ASSERT_THAT(r, Eq(IterationResult::Finished));
    }

    TEST_F(SunSExperimentTest, IterationFlow)
    {
        _exp.SetParameters(SunSExperimentParams(1, 2, 3, 2s, 1, 1min));
        _exp.SetOutputFiles("/exp");

        _exp.Start();

        {
            InSequence s;
            EXPECT_CALL(_power, SensPower(true)).WillOnce(Return(true));
            EXPECT_CALL(_power, SunSPower(true)).WillOnce(Return(true));

            EXPECT_CALL(_os, Sleep(duration_cast<milliseconds>(3s)));

            EXPECT_CALL(_sunsExp, StartMeasurement(1, 2));
            EXPECT_CALL(_os, Sleep(duration_cast<milliseconds>(2s)));

            EXPECT_CALL(_sunsExp, StartMeasurement(1, 2));
            EXPECT_CALL(_os, Sleep(duration_cast<milliseconds>(2s)));

            EXPECT_CALL(_sunsExp, StartMeasurement(1, 2));

            EXPECT_CALL(_power, SensPower(false)).WillOnce(Return(true));
            EXPECT_CALL(_power, SunSPower(false)).WillOnce(Return(true));
        }
        auto r = _exp.Iteration();
        ASSERT_THAT(r, Eq(IterationResult::Finished));
    }

    TEST_F(SunSExperimentTest, GatherSingleDataPoint)
    {
        this->_time = 10s;

        MeasurementData expSunsData;
        PayloadTelemetry::SunsRef refSunsData;
        GyroscopeTelemetry gyroData;

        EXPECT_CALL(_sunsExp, StartMeasurement(_, _)).WillOnce(Return(OperationStatus::OK));

        EXPECT_CALL(_payload, MeasureSunSRef(_)).WillOnce(DoAll(SetArgReferee<0>(refSunsData), Return(OSResult::Success)));

        EXPECT_CALL(_sunsExp, WaitForData()).WillOnce(Return(OSResult::Success));

        EXPECT_CALL(_sunsExp, GetMeasuredData(_)).WillOnce(DoAll(SetArgReferee<0>(expSunsData), Return(OperationStatus::OK)));

        EXPECT_CALL(_gyro, read()).WillOnce(Return(Some(gyroData)));

        auto dataPoint = _exp.GatherSingleMeasurement();

        ASSERT_THAT(dataPoint.Timestamp, Eq(10s));
    }

    TEST_F(SunSExperimentTest, DefaultParameters)
    {
        SunSExperimentParams p;
        ASSERT_THAT(p.Gain(), Eq(0));
        ASSERT_THAT(p.ITime(), Eq(0));
        ASSERT_THAT(p.SamplesCount(), Eq(0));
        ASSERT_THAT(p.ShortDelay(), Eq(0s));
        ASSERT_THAT(p.SamplingSessionsCount(), Eq(0));
        ASSERT_THAT(p.LongDelay(), Eq(0s));
    }

    TEST_F(SunSExperimentTest, ShouldAbortExperimentIfPrimaryDataSetNotOpened)
    {
        _exp.SetOutputFiles("/exp");

        EXPECT_CALL(this->_fs, Open(StrEq("/exp"), _, _)).WillOnce(Return(MakeOpenedFile(OSResult::AccessDenied)));

        auto r = _exp.Start();

        ASSERT_THAT(r, Eq(StartResult::Failure));
    }

    TEST_F(SunSExperimentTest, ShouldAbortExperimentIfSecondaryDataSetNotOpened)
    {
        _exp.SetOutputFiles("/exp");

        EXPECT_CALL(this->_fs, Open(StrEq("/exp"), _, _)).WillOnce(Return(MakeOpenedFile(1)));
        EXPECT_CALL(this->_fs, Open(StrEq("/exp_sec"), _, _)).WillOnce(Return(MakeOpenedFile(OSResult::AccessDenied)));
        EXPECT_CALL(this->_fs, Close(1));

        auto r = _exp.Start();

        ASSERT_THAT(r, Eq(StartResult::Failure));
    }
}
