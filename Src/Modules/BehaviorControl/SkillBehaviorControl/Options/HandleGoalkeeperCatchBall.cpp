#include "SkillBehaviorControl.h"
#include "Tools/BehaviorControl/Interception.h"


//TODO:守门员扑球逻辑，这里可以去看一下去年的守门员代码和这个比较一下
option((SkillBehaviorControl) HandleGoalkeeperCatchBall)
{
  initial_state(notCatching)
  {
    transition
    {
      if(theGameState.isGoalkeeper() &&
         between<float>(theFieldInterceptBall.timeUntilIntersectsOwnYAxis, 0.3f, 3.f) &&
         theFieldBall.ballWasSeen(100) &&
         theFieldBall.isRollingTowardsOwnGoal &&
         theFieldBall.positionRelative.squaredNorm() < sqr(3000.f))
        goto preparingCatch;
    }
  }

  state(preparingCatch)
  {
    transition
    {
      if(!(between<float>(theFieldInterceptBall.timeUntilIntersectsOwnYAxis, 0.1f, 4.f) &&
           theFieldBall.ballWasSeen(100) &&
           theFieldBall.isRollingTowardsOwnGoal &&
           theFieldBall.positionRelative.squaredNorm() < sqr(3000.f)))
        goto notCatching;
      if(state_time >= 100)
        goto doingCatch;
    }
    action
    {
      LookAtBall();
      KeyFrameArms({.motion = ArmKeyFrameRequest::keeperStand});
      Stand();
    }
  }

  state(doingCatch)
  {
    transition
    {
      if(action_done || !theFieldInterceptBall.interceptBall)
        goto notCatching;
    }
    action
    {
      //calculate isNearPost
      const auto [isNearLeftPost, isNearRightPost] = theLibPosition.isNearPost(theRobotPose);

      unsigned interceptionMethods = bit(Interception::stand) | bit(Interception::walk);
      interceptionMethods |= bit(Interception::genuflectStandDefender);
      if(theLibPosition.isInOwnPenaltyArea(theRobotPose.translation))//在罚球区（包含球门区）
      {
        if(!isNearLeftPost)//如果不靠近左就往左
          interceptionMethods |= bit(Interception::jumpLeft);
        if(!isNearRightPost)
          interceptionMethods |= bit(Interception::jumpRight);
      }
      //TODO:往哪边扑和扑不扑应当要结合球和机器人y轴的交点intersectionPositionWithOwnYAxis；有一些球可以移动阻挡不需要扑

      InterceptBall({.interceptionMethods = interceptionMethods,
                     .allowDive = theBehaviorParameters.keeperJumpingOn});
    }
  }
}
