/**
 * @file ClearBall.cpp
 *
 * This file defines an implementation of the ClearBall skill.
 *
 * @author Nico Holsten
 */

#include "SkillBehaviorControl.h"
#include "Platform/BHAssert.h"

// 详细的确定方式见ClearTargetProvider.cpp，实际上是在无敌方障碍物的扇形区域，用最快的踢球方式将球踢出
option((SkillBehaviorControl) ClearBall)
{
  initial_state(initial)
  {
    action
    {
      ASSERT(theClearTarget.getKickType() != KickInfo::numOfKickTypes);//  检查确保 theClearTarget 提供的踢球类型是有效的
      GoToBallAndKick({.targetDirection = theClearTarget.getAngle(),
                       .kickType = theClearTarget.getKickType(),
                       .preStepType = PreStepType::forced});
    }
  }
}
