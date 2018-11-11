// RBotFirmware
// Rob Dobson 2016

#pragma once

#include "Utils.h"
#include "RobotBase.h"
#include "math.h"

class RobotMugBot : public RobotBase
{
public:
    // Defaults
    static constexpr int maxHomingSecs_default = 30;
    static constexpr int _homingLinearFastStepTimeUs = 15;
    static constexpr int _homingLinearSlowStepTimeUs = 24;

public:

    static bool ptToActuator(AxisFloats& targetPt, AxisFloats& outActuator, AxisPosition& curPos, AxesParams& axesParams, bool allowOutOfBounds)
    {
        // Note that the rotation angle comes straight from the Y parameter
        // This means that drawings in the range 0 .. 240mm height (assuming 1:1 scaling is chosen)
        // will translate directly to the surface of the mug and makes the drawing
        // mug-radius independent

        // Check machine bounds and fix the value if required
        bool ptWasValid = axesParams.ptInBounds(targetPt, !allowOutOfBounds);

        // Perform conversion
        for (int axisIdx = 0; axisIdx < RobotConsts::MAX_AXES; axisIdx++)
        {
            // Axis val from home point
            float axisValFromHome = targetPt.getVal(axisIdx) - axesParams.getHomeOffsetVal(axisIdx);
            // Convert to steps and add offset to home in steps
            outActuator.setVal(axisIdx, axisValFromHome * axesParams.getStepsPerUnit(axisIdx)
                            + axesParams.gethomeOffSteps(axisIdx));

            // Log.notice("ptToActuator %f -> %f (homeOffVal %f, homeOffSteps %ld)\n",
            //         pt.getVal(axisIdx), actuatorCoords._pt[axisIdx],
            //         axesParams.getHomeOffsetVal(axisIdx), axesParams.gethomeOffSteps(axisIdx));
        }
        return ptWasValid;
    }

    static void actuatorToPt(AxisFloats& targetActuator, AxisFloats& outPt, AxisPosition& curPos, AxesParams& axesParams)
    {
        // Perform conversion
        for (int axisIdx = 0; axisIdx < RobotConsts::MAX_AXES; axisIdx++)
        {
            float ptVal = targetActuator.getVal(axisIdx) - axesParams.gethomeOffSteps(axisIdx);
            ptVal = ptVal / axesParams.getStepsPerUnit(axisIdx) + axesParams.getHomeOffsetVal(axisIdx);
            outPt.setVal(axisIdx, ptVal);
            // Log.notice("actuatorToPt %d %f -> %f (perunit %f)\n", axisIdx, actuatorCoords.getVal(axisIdx),
            //                 ptVal, axesParams.getStepsPerUnit(axisIdx));
        }
    }

    static void correctStepOverflow(AxisPosition& curPos, AxesParams& axesParams)
    {
    }

private:
    // Homing state
    typedef enum HOMING_STATE
    {
        HOMING_STATE_IDLE,
        HOMING_STATE_INIT,
        HOMING_STATE_SEEK_ENDSTOP,
        HOMING_STATE_BACK_OFF,
        HOMING_STATE_COMPLETE
    } HOMING_STATE;
    HOMING_STATE _homingState;
    HOMING_STATE _homingStateNext;

    // Homing variables
    unsigned long _homeReqMillis;
    int _homingStepsDone;
    int _homingStepsLimit;
    bool _homingApplyStepLimit;
    unsigned long _maxHomingSecs;
    typedef enum HOMING_SEEK_TYPE { HSEEK_NONE, HSEEK_ON, HSEEK_OFF } HOMING_SEEK_TYPE;
    HOMING_SEEK_TYPE _homingSeekAxis1Endstop0;
    bool _homeX, _homeY, _homeZ;
    // the following values determine which stepper moves during the current homing stage
    typedef enum HOMING_STEP_TYPE { HSTEP_NONE, HSTEP_FORWARDS, HSTEP_BACKWARDS } HOMING_STEP_TYPE;
    HOMING_STEP_TYPE _homingAxis1Step;
    double _timeBetweenHomingStepsUs;

public:
    RobotMugBot(const char* pRobotTypeName, MotionHelper& motionHelper) :
        RobotBase(pRobotTypeName, motionHelper)
    {
        _homingState = HOMING_STATE_IDLE;
        _homeReqMillis = 0;
        _homingStepsDone = 0;
        _homingStepsLimit = 0;
        _maxHomingSecs = maxHomingSecs_default;
        _homeX = _homeY = _homeZ = false;
        _timeBetweenHomingStepsUs = _homingLinearSlowStepTimeUs;
        _motionHelper.setTransforms(ptToActuator, actuatorToPt, correctStepOverflow);
    }

    // Set config
    // bool init(const char* robotConfigStr)
    // {
        // Info
        // Log.notice("Constructing %s from %s\n", _robotTypeName.c_str(), robotConfigStr);

        // // Set current position to be home (will be overridden when we do homing)
        // _motionHelper.setCurPositionAsHome(true, true, true);

        // // Get params specific to this robot
        // _maxHomingSecs = RdJson::getLong("maxHomingSecs", maxHomingSecs_default, robotConfigStr);

        // // Info
        // Log.notice("%s maxHome %ds\n", _robotTypeName.c_str(), _maxHomingSecs);

    //     return true;
    // }

    // void goHome(RobotCommandArgs& args)
    // {
    //   _motionHelper.goHome(args);
        // // Info
        // _homeX = args.isValid(0);
        // _homeY = args.isValid(1);
        // _homeZ = args.isValid(2);
        // if (!_homeX && !_homeY && !_homeZ)
        //     _homeX = _homeY = _homeZ = true;
        // Log.notice("%s goHome x%d, y%d, z%d\n", _robotTypeName.c_str(),
        //                 _homeX, _homeY, _homeZ);
        //
        // // Set homing state
        // homingSetNewState(HOMING_STATE_INIT);
    // }

    // void setHome(RobotCommandArgs& args)
    // {
    //     _motionHelper.setCurPositionAsHome(args.getPoint());
    // }

    // bool canAcceptCommand()
    // {
    //     // Check if homing
    //     // if (_homingState != HOMING_STATE_IDLE)
    //     //     return false;
    //
    //     // Check if motionHelper is can accept a command
    //     return _motionHelper.canAccept();
    // }

    // void homingSetNewState(HOMING_STATE newState)
    // {
    //     // Debug
    //     // if (_homingStepsDone != 0)
    //     Log.notice("Changing state to %d ... HomingSteps %d\n", newState, _homingStepsDone);
    //     // else
    //     //     Log.trace("Changing state to %d ... HomingSteps %d\n", _homingStepsDone);
    //
    //     // Reset homing vars
    //     _homingStepsDone = 0;
    //     _homingStepsLimit = 0;
    //     _homingApplyStepLimit = false;
    //     _homingSeekAxis1Endstop0 = HSEEK_NONE;
    //     _homingAxis1Step = HSTEP_NONE;
    //     _homingState = newState;
    //
    //     // Handle the specfics of the new homing state
    //     switch(newState)
    //     {
    //         case HOMING_STATE_IDLE:
    //         {
    //             break;
    //         }
    //         case HOMING_STATE_INIT:
    //         {
    //             // Check which axes we are homing
    //             if (_homeZ)
    //             {
    //                 _motionHelper.jumpHome(2);
    //             }
    //             if (_homeY)
    //             {
    //                 _homeReqMillis = millis();
    //                 // If we are at the endstop we need to move away from it first
    //                 _homingStateNext = HOMING_STATE_SEEK_ENDSTOP;
    //                 _homingSeekAxis1Endstop0 = HSEEK_OFF;
    //                 // Move away from endstop if needed
    //                 _homingAxis1Step = HSTEP_BACKWARDS;
    //                 _timeBetweenHomingStepsUs = _homingLinearFastStepTimeUs;
    //                 bool endstop1Val = _motionHelper.isAtEndStop(1,0);
    //                 Log.notice("Homing started%s\n", endstop1Val ? " moving from endstop" : "");
    //                 break;
    //             }
    //             else
    //             {
    //                 _homingState = HOMING_STATE_IDLE;
    //                 break;
    //             }
    //         }
    //         case HOMING_STATE_SEEK_ENDSTOP:
    //         {
    //             // Rotate to the rotation endstop
    //             _homingStateNext = HOMING_STATE_BACK_OFF;
    //             _homingSeekAxis1Endstop0 = HSEEK_ON;
    //             _homingAxis1Step = HSTEP_FORWARDS;
    //             _timeBetweenHomingStepsUs = _homingLinearSlowStepTimeUs;
    //             Log.notice("Homing to end stop\n");
    //             break;
    //         }
    //         case HOMING_STATE_BACK_OFF:
    //         {
    //             // Rotate to the rotation endstop
    //             _homingStateNext = HOMING_STATE_COMPLETE;
    //             _homingSeekAxis1Endstop0 = HSEEK_NONE;
    //             _homingAxis1Step = HSTEP_BACKWARDS;
    //             _timeBetweenHomingStepsUs = _homingLinearSlowStepTimeUs;
    //             _homingStepsLimit = 4000;
    //             _homingApplyStepLimit = true;
    //             Log.notice("Homing to end stop\n");
    //             break;
    //         }
    //         case HOMING_STATE_COMPLETE:
    //         {
    //             AxisFloats homeVals(0,0);
    //             _motionHelper.setCurPositionAsHome(homeVals);
    //             _homingState = HOMING_STATE_IDLE;
    //             Log.notice("Homing - complete\n");
    //             break;
    //         }
    //     }
    // }

    // bool homingService()
    // {
    //     // Check for idle
    //     if (_homingState == HOMING_STATE_IDLE)
    //         return false;
    //
    //     // Check for timeout
    //     if (millis() > _homeReqMillis + (_maxHomingSecs * 1000))
    //     {
    //         Log.notice("Homing Timed Out\n");
    //         homingSetNewState(HOMING_STATE_IDLE);
    //     }
    //
    //     // Check for endstop if seeking them
    //     bool endstop1Val = _motionHelper.isAtEndStop(1,0);
    //     if (((_homingSeekAxis1Endstop0 == HSEEK_ON) && endstop1Val) || ((_homingSeekAxis1Endstop0 == HSEEK_OFF) && !endstop1Val))
    //     {
    //         Log.notice("Mugbot at endstop setting new state %d\n", _homingStateNext);
    //         homingSetNewState(_homingStateNext);
    //     }
    //
    //     // Check if we are ready for the next step
    //     unsigned long lastStepMicros = _motionHelper.getAxisLastStepMicros(1);
    //     if (Utils::isTimeout(micros(), lastStepMicros, _timeBetweenHomingStepsUs))
    //     {
    //         // Axis 1
    //         if (_homingAxis1Step != HSTEP_NONE)
    //             _motionHelper.step(1, _homingAxis1Step == HSTEP_FORWARDS);
    //
    //         // Count homing steps in this stage
    //         _homingStepsDone++;
    //
    //         // Check for step limit in this stage
    //         if (_homingApplyStepLimit && (_homingStepsDone >= _homingStepsLimit))
    //         {
    //             Log.notice("Mugbot steps done setting new state %d\n", _homingStateNext);
    //             homingSetNewState(_homingStateNext);
    //         }
    //     }
    //
    //     return false;
    // }

    // // Pause (or un-pause) all motion
    // void pause(bool pauseIt)
    // {
    //     _motionHelper.pause(pauseIt);
    // }

    // Check if paused
    // bool isPaused()
    // {
    //     return _motionHelper.isPaused();
    // }
    //
    // Stop
    // void stop()
    // {
    //     _motionHelper.stop();
    // }

    // void service()
    // {
    //     // Service homing activity
    //     bool homingActive = homingService();
    //
    //     // Service the motion controller
    //     _motionHelper.service(!homingActive);
    // }

    // bool wasActiveInLastNSeconds(unsigned int nSeconds)
    // {
    //     if (_homingState != HOMING_STATE_IDLE)
    //         return true;
    //     return ((unsigned long)Time.now() < _motionHelper.getLastActiveUnixTime() + nSeconds);
    // }
};
