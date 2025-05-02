/**
 * @file PlayBall.cpp
 *
 * This file defines an implementation of a skill that plays the ball (under consideration of the skill request).根据技能请求和当前游戏状态来控制机器人处理球的行为
 *
 * @author Arne Hasselbring
 */

#include "SkillBehaviorControl.h"

option((SkillBehaviorControl) PlayBall,
       load((float)(500.f) goalPostHandlingAreaRadius,//球门柱处理区域的半径，用于判断球是否在球门柱附近
            (float)(1.2f) goalPostHandlingAreaHysteresisMultiplier,//球门柱处理区域的滞后乘数，用于状态转移时避免频繁切换
            (float)(1000.f) duelMinDistanceToClosest,//进入一对一对抗的最小距离阈值
            (Angle)(100_deg) duelMinAngleToClosest,//进入一对一对抗的最小角度阈值
            (float) oneAndOnlyTeammateDistance))//唯一队友的距离阈值，用于排除特定情况下的障碍物
{
  // 辅助函数一，判断是否激活一对一对抗状态。
  const auto activateDuel = [&] 
  {
    if((theGameState.isKickOff() || theGameState.isFreeKick()) && theGameState.isForOwnTeam()) // 如果当前是（开球或任意球)且是己方的，则不激活。
      return false;

    const auto [distanceToClosestObstacle, angleToClosestObstacle, smallestAngleToCloseObstacle] = [&]() -> std::tuple<float, Angle, Angle> // 离球最近障碍物的距离、角度以及与近距离障碍物的最小角度
    {
      std::optional<Vector2f> theOneAndOnlyTeammate;//存储可能的唯一队友的位置
      if(Global::getSettings().scenario.starts_with("SharedAutonomyAttacker") && !theTeamData.teammates.empty())// 如果当前场景是以 "SharedAutonomyAttacker" 开头，并且队友列表不为空（注意队友列表为当前与‘我’通信的所有队友的无序列表）
      {
        theOneAndOnlyTeammate = theRobotPose.inverse() * theTeamData.teammates[0].getEstimatedPosition(theFrameInfo.getTimeSince(theTeamData.teammates[0].theFrameInfo.time));// 则计算该队友相对于机器人的位置，并存储在 theOneAndOnlyTeammate 中
      }
      float x = std::numeric_limits<float>::max();
      Angle y = 0_deg;
      Angle z = 180_deg;
      for(const auto& o : theObstacleModel.obstacles)
      {
        if(theOneAndOnlyTeammate.has_value() && (o.center - theOneAndOnlyTeammate.value()).squaredNorm() < sqr(oneAndOnlyTeammateDistance))// oneAndOnlyTeammateDistance该参数与我跟障碍物的距离有关？
          continue;
        const float d = (o.center - theFieldBall.positionRelative).squaredNorm();// 障碍物中心与球的相对位置的距离平方 d
        if(d < x)
        {
          x = d;
          y = o.center.angle();//该向量相对于机器人坐标系原点的极角
          if(x < sqr(duelMinDistanceToClosest))
            z = std::min(Angle(std::abs(y)), z);// z为遍历角度的最小值
        }
      }
      return
      {
        std::sqrt(x), y, z
      };
    }();

    const bool obstacleClose = distanceToClosestObstacle < (duelMinDistanceToClosest * (1.f - (std::abs(angleToClosestObstacle) / duelMinAngleToClosest / 3.f)));// 判断离球最近的障碍物是否足够近
    const bool obstacleAngleClose = smallestAngleToCloseObstacle <= duelMinAngleToClosest;// 判断与近距离障碍物的最小角度是否小于等于 duelMinAngleToClosest
    const bool ballSeen = theFieldBall.ballWasSeen(300);// 判断球在最近 300 个时间单位内是否被看到
    const bool ballPosXClose = between<float>(theFieldBall.positionRelative.x(), 0.f, 700.f);// 判断球在机器人相对坐标系下的 x 坐标是否在 0 到 700 之间
    const bool ballPosYClose = std::abs(theFieldBall.positionRelative.y()) < 500.f;// 判断球在机器人相对坐标系下的 y 坐标的绝对值是否小于 500
    const bool rotationToBallOk = std::abs(theFieldBall.positionRelative.angle()) < 80_deg;// 判断机器人与球的相对角度的绝对值是否小于 80 度

    return obstacleClose && obstacleAngleClose &&
           ballSeen && ballPosXClose && ballPosYClose && rotationToBallOk;
  };
// 辅助函数二，判断是否应该停止当前的一对一对抗（duel）状态
  const auto stopDuel = [&]
  {
    if((theGameState.isKickOff() || theGameState.isFreeKick()) && theGameState.isForOwnTeam())// 如果处于开球（isKickOff()）或者任意球（isFreeKick()）状态，并且是己方的开球或任意球，那么直接返回 true，表示停止对抗状态
      return true;

    if(theFieldBall.positionRelative.x() > 1000.f)//球较远，停止对抗
      return true;

    for(const Obstacle& omo : theObstacleModel.obstacles)
    {
      const float distanceToObstacle = (omo.center - theFieldBall.positionRelative).norm();// 计算当前障碍物中心与球的相对位置的距离
      const float angleToObstacle = std::abs(omo.center.angle());// 计算当前障碍物中心位置向量相对于机器人自身坐标系的角度的绝对值

      if(distanceToObstacle < (duelMinDistanceToClosest * 1.2f * (1.f - (std::abs(angleToObstacle) / pi / 2.f))))// 当前障碍物与球的距离小于这个动态阈值（障碍物角度越偏离正前方（角度接近 0），这个阈值就越大），说明障碍物离球较近，可能还需要继续进行对抗，此时返回 false，表示不停止对抗状态
        return false;
    }
    return true;
  };

  const bool isLeft = theFieldBall.positionOnField.y() > 0.f;// 根据球在场地中的 y 坐标（绝对）判断球是否在场地左侧
  const Vector2f usedGoalPost(std::min(theFieldDimensions.xPosOwnGoalPost, theFieldBall.positionOnField.x() - theFieldDimensions.goalPostRadius), isLeft ? theFieldDimensions.yPosLeftGoal : theFieldDimensions.yPosRightGoal);// 根据球的位置确定使用的球门柱位置

  initial_state(executeSkillRequest)// 初始状态
  {
    transition
    {
      if(theGameState.isKickOff()) // 如果当前是开球状态，则转移到 kickOff 状态
        goto kickOff;
      else if((theFieldBall.positionOnField - usedGoalPost).squaredNorm() < sqr(goalPostHandlingAreaRadius))// 如果球的位置与使用的球门柱的距离的平方小于 goalPostHandlingAreaRadius 的平方，说明球在球门柱处理区域内，转移到 atOwnGoalPost 状态
        goto atOwnGoalPost;
      if(activateDuel())//根据函数一，进入争球状态
        goto zweikampf;
    }
    action
    {
      switch(theSkillRequest.skill)
      {
        case SkillRequest::pass:
          PassToTeammate({.playerNumber = theSkillRequest.passTarget});
          break;
        case SkillRequest::dribble:
          GoToBallAndDribble({.targetDirection = Angle::normalize(theSkillRequest.target.rotation - theRobotPose.rotation)});
          break;
        case SkillRequest::clear:
          ClearBall();
          break;
        case SkillRequest::shoot:
        default:
          KickAtGoal();//没有请求默认射门
          break;
      }
    }
  }

  state(atOwnGoalPost)
  {
    transition
    {
      if((theFieldBall.positionOnField - usedGoalPost).squaredNorm() > sqr(goalPostHandlingAreaRadius * goalPostHandlingAreaHysteresisMultiplier))// 如果球的位置与使用的球门柱的距离的平方大于阈值，说明球已经离开球门柱处理区域（考虑了滞后）
      {
        if(activateDuel())
          goto zweikampf;
        goto executeSkillRequest;//回到初始
      }
    }
    action
    {
      HandleBallAtOwnGoalPost();//专门的在己方区域的处理方式，见ball.h
    }
  }

  state(zweikampf)
  {
    transition
    {
      if((theFieldBall.positionOnField - usedGoalPost).squaredNorm() < sqr(goalPostHandlingAreaRadius))
        goto atOwnGoalPost;
      if(stopDuel())
        goto executeSkillRequest;
    }
    action
    {
      Zweikampf();
    }
  }

  state(kickOff)
  {
    transition
    {
      if(!theGameState.isKickOff())
        goto executeSkillRequest;
    }
    action
    {
      //DirectKickOff();
      const auto seekTeammate = [&](int athlete)
      {
        for(const auto& t : theGlobalTeammatesModel.teammates)
        {
          if(t.playerNumber == athlete)
          {
            return true;
          }
        }
        return false;
      };
      if(seekTeammate(5))
      {
        PassTarget({.passTarget = 5,
          .ballTarget = theRobotPose.inverse() * Vector2f(-1200, 0)});

        GoToBallAndKick({.targetDirection = Angle::normalize((Vector2f(-1200, 0) - theFieldInterceptBall.interceptedEndPositionOnField).angle() - theRobotPose.rotation),
          .alignPrecisely = KickPrecision::notPrecise,
          .length = (Vector2f(-1200, 0) - theFieldInterceptBall.interceptedEndPositionOnField).norm(),
          .turnKickAllowed = false,
          .reduceWalkSpeedType = theGameState.isFreeKick() && theFieldBall.positionRelative.squaredNorm() < sqr(500) ? ReduceWalkSpeedType::slow : ReduceWalkSpeedType::noChange});
        
      }
      else
      {
        PassTarget({.passTarget = 3,
          .ballTarget = theRobotPose.inverse() * Vector2f(-750, 750/2 + 750)});

        GoToBallAndKick({.targetDirection = Angle::normalize((Vector2f(-750, 750/2 + 750) - theFieldInterceptBall.interceptedEndPositionOnField).angle() - theRobotPose.rotation),
          .alignPrecisely = KickPrecision::notPrecise,
          .length = (Vector2f(-750, 750/2 + 750) - theFieldInterceptBall.interceptedEndPositionOnField).norm(),
          .turnKickAllowed = false,
          .reduceWalkSpeedType = theGameState.isFreeKick() && theFieldBall.positionRelative.squaredNorm() < sqr(500) ? ReduceWalkSpeedType::slow : ReduceWalkSpeedType::noChange});
      }
    }
  }
}
