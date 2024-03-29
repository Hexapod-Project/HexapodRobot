#include "Hexapod.h"
#include "Tools.h"
#include "../includes/Enums.h"
#include "../includes/HexapodConstants.h"
#include "HardwareSerial.h"

String LEG_NAMES[] = {"FRONTRIGHT", "MIDRIGHT", "BACKRIGHT", "BACKLEFT", "MIDLEFT", "FRONTLEFT"};
String MOVESTATE_STRINGS[] = {"MOVING", "STOPSTARTED", "STOPPING", "STOPPED"};

extern HardwareSerial Serial;

uint8_t legIdx;

void Hexapod::setup()
{
    delay(500);

    initGaits();
    initLegs();

    delay(1000);

    setLegsPos();
    // updateJyStkPos(degreesToJoyStickPos(90));
}

void Hexapod::initLegs()
{
    mServoDriverL.begin();
    mServoDriverL.setOscillatorFrequency(DRIVER_FREQ);
    mServoDriverL.setPWMFreq(SERVO_FREQ);

    mServoDriverR.begin();
    mServoDriverR.setOscillatorFrequency(DRIVER_FREQ);
    mServoDriverR.setPWMFreq(SERVO_FREQ);

    mLegs[FRONTLEFT].setIsLeft();
    mLegs[MIDLEFT].setIsLeft();
    mLegs[BACKLEFT].setIsLeft();

    // Set servos
    mLegs[FRONTLEFT].setServos(6, 12, 11);
    mLegs[MIDLEFT].setServos(5, 8, 7);
    mLegs[BACKLEFT].setServos(4, 18, 17);

    mLegs[FRONTRIGHT].setServos(1, 16, 13);
    mLegs[MIDRIGHT].setServos(2, 9, 10);
    mLegs[BACKRIGHT].setServos(3, 15, 14);

    // Attach servos to servo driver
    mLegs[FRONTLEFT].attach(13, 14, 15, &mServoDriverL);
    mLegs[MIDLEFT].attach(12, 4, 3, &mServoDriverL);
    mLegs[BACKLEFT].attach(2, 1, 0, &mServoDriverL);

    mLegs[FRONTRIGHT].attach(2, 1, 0, &mServoDriverR);
    mLegs[MIDRIGHT].attach(3, 11, 12, &mServoDriverR);
    mLegs[BACKRIGHT].attach(13, 14, 15, &mServoDriverR);
}

void Hexapod::setLegsPos()
{
    // Leg offsets from the body's origin
    Vec3 legOffsets[LEG_COUNT];
    legOffsets[LEG::FRONTLEFT] = Vec3(-X_FB_LEG_OFFSET, Y_LEG_OFFSET, Z_FB_LEG_OFFSET);
    legOffsets[LEG::FRONTRIGHT] = Vec3(X_FB_LEG_OFFSET, Y_LEG_OFFSET, Z_FB_LEG_OFFSET);
    legOffsets[LEG::MIDLEFT] = Vec3(-X_MID_LEG_OFFSET, Y_LEG_OFFSET, 0);
    legOffsets[LEG::MIDRIGHT] = Vec3(X_MID_LEG_OFFSET, Y_LEG_OFFSET, 0);
    legOffsets[LEG::BACKLEFT] = Vec3(-X_FB_LEG_OFFSET, Y_LEG_OFFSET, -Z_FB_LEG_OFFSET);
    legOffsets[LEG::BACKRIGHT] = Vec3(X_FB_LEG_OFFSET, Y_LEG_OFFSET, -Z_FB_LEG_OFFSET);

    float legAngles[LEG_COUNT];
    legAngles[LEG::FRONTLEFT] = LEG_ANGLE_FRONTLEFT;
    legAngles[LEG::FRONTRIGHT] = LEG_ANGLE_FRONTRIGHT;
    legAngles[LEG::MIDLEFT] = LEG_ANGLE_MIDLEFT;
    legAngles[LEG::MIDRIGHT] = LEG_ANGLE_MIDRIGHT;
    legAngles[LEG::BACKLEFT] = LEG_ANGLE_BACKLEFT;
    legAngles[LEG::BACKRIGHT] = LEG_ANGLE_BACKRIGHT;

    Vec3 legStartPos[LEG_COUNT];
    legStartPos[FRONTRIGHT] = START_FOOT_POS_FRONTRIGHT;
    legStartPos[MIDRIGHT] = START_FOOT_POS_MIDRIGHT;
    legStartPos[BACKRIGHT] = START_FOOT_POS_BACKRIGHT;
    legStartPos[FRONTLEFT] = START_FOOT_POS_FRONTLEFT;
    legStartPos[MIDLEFT] = START_FOOT_POS_MIDLEFT;
    legStartPos[BACKLEFT] = START_FOOT_POS_BACKLEFT;

    mBodyStartPos = Vec3(0, BODY_Y_OFFSET, 0);

    mBaseMatrix = mBaseMatrix.translate(mBodyStartPos);

    mBodyPos.mY = START_HEIGHT;
    mBodyMatrix = mBaseMatrix.translate(mBodyPos);
    mBaseStepHeight = mBodyPos.mY * BODY_TO_STEP_Y_RATIO;

    Leg *currLeg;
    // Calculate the angles first
    for (legIdx = 0; legIdx < LEG_COUNT; legIdx++)
    {
        currLeg = &mLegs[legIdx];
        currLeg->setBase(legOffsets[legIdx], legAngles[legIdx]);
        currLeg->setStartFootPos(legStartPos[legIdx]);
        currLeg->setRoot(&mBodyMatrix);
        currLeg->calculateJointAngles();
    }

    delay(1000);

    // Then update all at once
    updateLegServoAngles();

    delay(1000);
}

void Hexapod::initGaits()
{
    mGaits[GAITTYPE::TRIPOD] = Gait(new GaitGroup[2]{GaitGroup(new LEG[3]{LEG::FRONTLEFT, LEG::MIDRIGHT, LEG::BACKLEFT}, 3),
                                                     GaitGroup(new LEG[3]{LEG::FRONTRIGHT, LEG::MIDLEFT, LEG::BACKRIGHT}, 3)},
                                    2);

    mGaits[GAITTYPE::TRIPLE].mGaitGroups = new GaitGroup[2]{GaitGroup(new LEG[3]{LEG::FRONTLEFT, LEG::MIDRIGHT, LEG::BACKLEFT}, 3, 0.3),
                                                            GaitGroup(new LEG[3]{LEG::FRONTRIGHT, LEG::MIDLEFT, LEG::BACKRIGHT}, 3, 0.3)};
    mGaits[GAITTYPE::TRIPLE].mGaitGroupsSize = 2;

    mGaits[GAITTYPE::WAVE].mGaitGroups = new GaitGroup[2]{GaitGroup(new LEG[3]{LEG::FRONTRIGHT, LEG::MIDRIGHT, LEG::BACKRIGHT}, 3, 0.7),
                                                          GaitGroup(new LEG[3]{LEG::FRONTLEFT, LEG::MIDLEFT, LEG::BACKLEFT}, 3, 0.7)};
    mGaits[GAITTYPE::WAVE].mGaitGroupsSize = 2;

    mGaits[GAITTYPE::RIPPLE].mGaitGroups = new GaitGroup[2]{GaitGroup(new LEG[3]{LEG::FRONTRIGHT, LEG::MIDRIGHT, LEG::BACKRIGHT}, 3, 0.8),
                                                            GaitGroup(new LEG[3]{LEG::BACKLEFT, LEG::MIDLEFT, LEG::FRONTLEFT}, 3, 0.8)};
    mGaits[GAITTYPE::RIPPLE].mGaitGroupsSize = 2;

    mCurrGaitGrpSize = mGaits[mGaitType].mGaitGroupsSize;
    mCurrGait = &mGaits[mGaitType];
}

void Hexapod::calibrateMode(uint16_t hipAngle, uint16_t femurAngle, uint16_t tibiaAngle)
{
    mCalibrateMode = true;

    initLegs();

    for (legIdx = 0; legIdx < LEG_COUNT; legIdx++)
        mLegs[legIdx].setAngles(hipAngle, femurAngle, tibiaAngle);
}

void Hexapod::calculateLegJointAngles()
{    
    for (legIdx = 0; legIdx < LEG_COUNT; legIdx++)
        mLegs[legIdx].calculateJointAngles();
}

void Hexapod::updateLegServoAngles()
{    
    for (legIdx = 0; legIdx < LEG_COUNT; legIdx++)
        mLegs[legIdx].updateServoAngles();
}

void Hexapod::update()
{
    if (mCalibrateMode)
        return;

    if (mMoveState != MOVESTATE::STOPPED)
    {
        calculateLegJointAngles();
        walk();
    }
    else if (mGaitType != mInputGaitType)
    {
        mGaitType = (GAITTYPE)mInputGaitType;
        mCurrGait = &mGaits[mGaitType];
        mCurrGaitGrpSize = mCurrGait->mGaitGroupsSize;
    }

    updateLegServoAngles();
}

void Hexapod::setStance(float height, BTDATA_MISC stance)
{
    mBodyPos.mY = height;
    Serial.println(mBodyPos.toString());
    mBodyMatrix = mBaseMatrix.translate(mBodyPos).rotate(mBodyRot);
    mBaseStepHeight = height * BODY_TO_STEP_Y_RATIO;

    mStance = stance;

    calculateLegJointAngles();
    updateLegServoAngles();
}

void Hexapod::transRotBody(float transDir, float rotDir)
{
    Vec3 tempPos = mBodyPos;
    Vec3 tempRot = mBodyRot;

    if (transDir > -1)
    {
        tempPos.mX += cos(transDir) * BODY_TRANS_DIST;
        tempPos.mZ += sin(transDir) * BODY_TRANS_DIST;
    }

    if (rotDir > -1)
    {
        tempRot.mZ -= cos(rotDir) * MAX_ROLL;
        tempRot.mX -= sin(rotDir) * MAX_PITCH;
    }

    mBodyMatrix = mBaseMatrix.translate(tempPos).rotate(tempRot);

    calculateLegJointAngles();
    updateLegServoAngles();
}

void Hexapod::updateDirs(float moveDir, float turnDir, float transDir, float rotDir)
{
    if (mMoveState == MOVESTATE::STOPPED)
        transRotBody(transDir, rotDir);

    // Walk
    if (moveDir <= -1)
    {
        if (mMoveState == MOVESTATE::MOVING && mStepDistMulti > 0)
        {
            if (compareFloats(mTargetFaceDir, mFaceDir))
                mMoveState = MOVESTATE::STOPSTARTED;
            else
                mStepDistMulti = 0;
        }
    }
    else
    {
        mMoveDir = moveDir + mFaceDirDiff;

        if (mStepDistMulti <= 0)
        {
            calcMoveDir();

            // mStepDistMulti = pos.magnitude();
            mStepDistMulti = 1;

            if (compareFloats(mTargetFaceDir, mFaceDir))
            {
                initStep();
                setNextStep();
                setNextStepRot();
            }
        }
        else if (!compareFloats(mMoveDir, moveDir))
        {
            calcMoveDir();

            // setNextStep();
            // setNextStepRot();
        }
    }

    // Rotate
    mTurnDir = turnDir;

    if (turnDir > -1)
    {
        mFaceDir = toPositiveRad(mFaceDir);

        if (mMoveState == MOVESTATE::STOPPED)
        {
            initStep();
            setNextStep();
            setNextStepRot();
        }
    }
}

void Hexapod::calcMoveDir()
{
    mCosMoveDir = cos(mMoveDir);
    mSinMoveDir = sin(mMoveDir);
}

void Hexapod::initStep()
{
    mGaitGrpIdx = mCurrGaitGrpSize - 1;
    mMoveState = MOVESTATE::MOVING;
    mStepHeight = mBaseStepHeight;
}

void Hexapod::setNextStepRot()
{
    // Store the values after being rotated
    mFaceDir = clampTo360Rad(toPositiveRad(FORWARD - mBodyRot.mY));
    mFaceDirDiff = mFaceDir - M_PI_2;

    if (mTurnDir > -1)
        mTargetFaceDir = mFaceDir - cos(mTurnDir) * MAXRAD_PERSTEP;

    if (compareFloats(mFaceDir, mTargetFaceDir))
    {
        if (mMoveState == MOVESTATE::MOVING && compareFloats(mTargetFaceDir, mFaceDir) && mStepDistMulti == 0)
            mMoveState = MOVESTATE::STOPSTARTED;

        return;
    }

    float diffAngle = getSmallestRad(mTargetFaceDir - mFaceDir);
    mStepRotAngle = fabs(diffAngle) > MAXRAD_PERSTEP ? copysign(MAXRAD_PERSTEP, diffAngle) : diffAngle;
    mBodyStepStartYaw = toPositiveRad(mBodyRot.mY);

    if (mStepDistMulti == 0)
    {
        mStepHeight = fabs(mStepRotAngle) * mBaseStepHeight / MAXRAD_PERSTEP;
        if (mStepHeight < MIN_STEP_HEIGHT)
            mStepHeight = MIN_STEP_HEIGHT;
    }
}

void Hexapod::setNextStep()
{
    // Go to the next leg sequence group
    mGaitGrpIdx++;
    if (mGaitGrpIdx >= mCurrGaitGrpSize)
        mGaitGrpIdx = 0;

    mStepStartTime = millis();
    GaitGroup *currGroup = &mCurrGait->mGaitGroups[mGaitGrpIdx];
    mLegIndices = currGroup->mLegIndices;
    mLegIndicesSize = currGroup->mLegIndicesSize;
    mStepTimeOffset = currGroup->mStepTimeOffset;
    mStepDuration = currGroup->mStepDuration;

    float baseStepDist = STEP_DIST * mStepDistMulti;

    // Serial.println("------------- Group " + (String)mGaitGrpIdx + "----------------------------");
    // Serial.println("CosMove: " + (String)mCosMoveDir + ", SinMove: " + (String)mSinMoveDir);

    for (int i = 0; i < mLegIndicesSize; i++)
    {
        legIdx = mLegIndices[i];
        Leg *currLeg = &mLegs[legIdx];

        // Store the current position of the leg as the step starting position
        mLegStepStartPos[legIdx] = currLeg->getTargetFootPos();
        // Store the current leg height to offset the step lifting position
        mPrevStepHeight[legIdx] = mLegStepStartPos[legIdx].mY;
        // Set the step start Y position to 0 to prevent the step Y position from getting skewed
        mLegStepStartPos[legIdx].mY = 0;

        Vec3 bodyPosXZ = Vec3(mBodyPos.mX, 0, mBodyPos.mZ);
        // Calculate the step distance
        Vec3 footStartPos = rotateAroundY(currLeg->getStartFootPos(), -mBodyRot.mY) + bodyPosXZ;
        Vec3 footDiff = mLegStepStartPos[legIdx] - footStartPos;

        // Include the foot differences if it is still moving
        if (mMoveState == MOVESTATE::MOVING)
            mStepDist[legIdx] = Vec3(mCosMoveDir * baseStepDist, 0, mSinMoveDir * baseStepDist) - footDiff;
        else
            mStepDist[legIdx] = -footDiff;

        // Serial.println(LEG_NAMES[i] + ": " + mStepDist[legIdx].toString());
    }

    // Serial.println("Move State: " + MOVESTATE_STRINGS[mMoveState]);
    // Serial.println("-----------------------------------------------");

    // Update the body position vector to match the body matrix
    mBodyStepStartPos.mX = mBodyPos.mX;
    mBodyStepStartPos.mZ = mBodyPos.mZ;

    if (mMoveState == MOVESTATE::STOPSTARTED)
        mMoveState = MOVESTATE::STOPPING;
}

void Hexapod::walk()
{
    float stepNormTimeLapsed = normalizeTimelapsed(mStepStartTime, mStepDuration);

    if (stepNormTimeLapsed >= 1)
    {
        if (mGroupStoppedCount < mCurrGaitGrpSize - 1)
        {
            setNextStep();
            setNextStepRot();
            stepNormTimeLapsed = 0;

            if (mMoveState == MOVESTATE::STOPPING)
                mGroupStoppedCount++;
        }
        else // Stops the walk cycle
        {
            mMoveState = MOVESTATE::STOPPED;
            mStepDistMulti = 0;
            mStepRotAngle = 0;
            mGroupStoppedCount = 0;

            // Temporary fix for gimbal lock after turning
            mFaceDir = FORWARD;
            mTargetFaceDir = mFaceDir;
            mFaceDirDiff = 0;

            for (legIdx = 0; legIdx < LEG_COUNT; legIdx++)
                mLegs[legIdx].setTargetFootPos(mLegs[legIdx].getStartFootPos());

            mBodyPos.mX = 0;
            mBodyPos.mZ = 0;
            mBodyRot.mY = 0;
            mBodyStepStartYaw = 0;
            mBodyMatrix = mBaseMatrix.translate(mBodyPos);

            updateLegServoAngles();

            return;
        }
    }

    if (stepNormTimeLapsed >= 0)
    {
        // Set the new foot target position
        for (int i = 0; i < mLegIndicesSize; i++)
        {
            float legNormTimeLapsed = normalizeTimelapsed(mStepStartTime + BASE_STEP_DURATION * mStepTimeOffset * i, BASE_STEP_DURATION);

            if (legNormTimeLapsed < 0 || legNormTimeLapsed > 1)
                continue;

            legIdx = mLegIndices[i];

            float footYOffset = sin(M_PI * legNormTimeLapsed) * mStepHeight + mPrevStepHeight[legIdx] * (1 - legNormTimeLapsed);
            float footXOffset = mStepDist[legIdx].mX * legNormTimeLapsed;
            float footZOffset = mStepDist[legIdx].mZ * legNormTimeLapsed;

            Vec3 offset = Vec3(footXOffset, footYOffset, footZOffset);

            Vec3 rootPos = Vec3(mBodyPos.mX, 0, mBodyPos.mZ);
            Vec3 stepStartPos = rotateAroundY(mLegStepStartPos[legIdx] - rootPos, mStepRotAngle * legNormTimeLapsed) + rootPos;
            Vec3 newPos = stepStartPos + offset;

            mLegs[legIdx].setTargetFootPos(newPos);
        }

        // Move the body forward
        if (mMoveState != MOVESTATE::STOPPING)
        {
            float baseBodyOffset = STEP_DIST * stepNormTimeLapsed * mStepDistMulti;
            mBodyPos.mX = mBodyStepStartPos.mX + mCosMoveDir * baseBodyOffset;
            mBodyPos.mZ = mBodyStepStartPos.mZ + mSinMoveDir * baseBodyOffset;

            mBodyRot.mY = mBodyStepStartYaw - mStepRotAngle * stepNormTimeLapsed;

            mBodyMatrix = mBaseMatrix.translate(mBodyPos).rotate(mBodyRot);
        }
    }
}

void Hexapod::setMisc(uint8_t miscState)
{
    switch (miscState)
    {
    case BTDATA_MISC::TRIPOD_GAIT:
        mInputGaitType = GAITTYPE::TRIPOD;
        break;
    case BTDATA_MISC::TRIPLE_GAIT:
        mInputGaitType = GAITTYPE::TRIPLE;
        break;
    case BTDATA_MISC::WAVE_GAIT:
        mInputGaitType = GAITTYPE::WAVE;
        break;
    case BTDATA_MISC::RIPPLE_GAIT:
        mInputGaitType = GAITTYPE::RIPPLE;
        break;
    // case BTDATA_MISC::AUTOROT_ON:
    //     mNaturalWalkMode = true;
    //     Serial.println("Natural Walk ON");
    //     break;
    // case BTDATA_MISC::AUTOROT_OFF:
    //     mNaturalWalkMode = false;
    //     Serial.println("Natural Walk OFF");
    //     break;
    case BTDATA_MISC::CROUCH:
        setStance(CROUCH_HEIGHT, CROUCH);
        break;
    case BTDATA_MISC::RISE:
        setStance(RISE_HEIGHT, RISE);
        break;
    default:
        break;
    }
}

uint8_t Hexapod::getStanceMisc()
{
    return mStance;
}

uint8_t Hexapod::getGaitTypeMisc()
{
    switch (mGaitType)
    {
    default:
    case TRIPOD:
        return TRIPOD_GAIT;
    case TRIPLE:
        return TRIPLE_GAIT;
    case WAVE:
        return WAVE_GAIT;
    case RIPPLE:
        return RIPPLE_GAIT;
    }
}