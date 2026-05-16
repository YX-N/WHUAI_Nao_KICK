#include "SkillBehaviorControl.h"
#include "Tools/BehaviorControl/WalkSpeedConversion.h"

option((SkillBehaviorControl) PenaltyTaker)
{
  if(theGameState.isPushingFreeKick())
    Say({.text = "Taking Penalty"});

  const int timeSincePenaltyShootoutStarted = theFrameInfo.getTimeSince(theGameState.timeWhenStateStarted);
  const int timeUntilPenaltyShootoutEnds = -theFrameInfo.getTimeSince(theGameState.timeWhenStateEnds);

  common_transition
  {
    if(timeSincePenaltyShootoutStarted > 5000 && !theFieldBall.ballWasSeen(5000))
      goto goBehindPenaltyMark;
  }

  initial_state(initial)
  {
    transition
    {
      if(std::min(timeSincePenaltyShootoutStarted, state_time) > 3000 || timeUntilPenaltyShootoutEnds <= 10000)
        goto goToBallAndKickLeft;
    }
    action
    {
      LookLeftAndRight({.maxPan = 20_deg,
                        .tilt = 5.7_deg,
                        .speed = 30_deg});
      Stand({.energySavingWalk = false});
    }
  }

  state(goToBallAndKickLeft)
  {
    action
    {
      const Vector2f goalPostOnField(theFieldDimensions.xPosOpponentGoalPost, theFieldDimensions.yPosLeftGoal);
      const Vector2f ballPositionOnField = theRobotPose * theBallModel.estimate.position;
      const Angle angle = (goalPostOnField - ballPositionOnField).angle() -
                          theBehaviorParameters.penaltyStrikerAngleToLeftPostOffset;

      // Always use the right foot and aim at the left side of the goal.
      // The pass variant maps kickLength to a lower kick power than the normal fast kick.
      const KickInfo::KickType kickType = KickInfo::forwardFastRightPass;
      //修改踢球力度
      const float kickLength = 2200.f;
      

      const Pose2f walkingSpeedRatio = WalkSpeedConversion::convertSpeedRatio(ReduceWalkSpeedType::slow, Pose2f(1.f, 1.f, 1.f), Pose2f(1.f, 1.f, 1.f), theFrameInfo, theGameState, theFieldBall, theWalkingEngineOutput);

      if(theGameState.isPenaltyKick())
        GoToBallAndKick({.targetDirection = Angle::normalize(angle - theRobotPose.rotation),
                         .kickType = kickType,
                         .alignPrecisely = KickPrecision::precise,
                         .length = kickLength,
                         .speed = walkingSpeedRatio});
      else
      {
        Pose2f kickPoseOnField(angle, ballPositionOnField);
        kickPoseOnField.rotate(theKickInfo.kicks[kickType].rotationOffset);
        kickPoseOnField.translate(theKickInfo.kicks[kickType].ballOffset);
        PenaltyStrikerGoToBallAndKick({.kickPose = theRobotPose.inverse() * kickPoseOnField,
                                       .kickType = kickType,
                                       .walkSpeed = walkingSpeedRatio,
                                       .kickLength = kickLength});
      }
    }
  }

  state(goBehindPenaltyMark)
  {
    transition
    {
      if(theFieldBall.ballWasSeen())
      {
        if(theBallModel.estimate.position.squaredNorm() < sqr(500.f))
          goto goToBallAndKickLeft;
        else
          goto initial;
      }
    }
    action
    {
      LookActive({.withBall = true,
                  .onlyOwnBall = true});
      const Vector2f target = theRobotPose.inverse() * Vector2f(theFieldDimensions.xPosOpponentPenaltyMark - 300.f, 0.f);
      WalkToPoint({.target = {-theRobotPose.rotation, target},
                   .reduceWalkSpeedType = ReduceWalkSpeedType::slow,
                   .disableEnergySavingWalk = true,
                   .rough = true,
                   .disableObstacleAvoidance = true});
    }
  }
}
