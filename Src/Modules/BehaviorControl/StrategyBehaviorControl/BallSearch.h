/**
 * @file BallSearch.h
 *
 * This file declares a ball search behavior.
 *
 * @author Arne Hasselbring
 */

#pragma once

#include "Debugging/Annotation.h"
#include "Tools/BehaviorControl/Strategy/Agent.h"
#include "Tools/BehaviorControl/Strategy/BehaviorBase.h"
#include "Representations/BehaviorControl/SkillRequest.h"
#include "Math/Geometry.h"
#include <array>
#include <regex> // not needed in header, but would otherwise be broken by CABSL

class BallSearch;

#ifdef __INTELLISENSE__
#define INTELLISENSE_PREFIX BallSearch::
#endif
#include "Tools/BehaviorControl/Cabsl.h"

class BallSearch : public cabsl::Cabsl<BallSearch>, public BehaviorBase
{
public:
  BallSearch();

  void preProcess() override;

  void postProcess() override;

  SkillRequest execute(const Agent& agent, const Agents& otherAgents);

private:
  ActivationGraph activationGraph;
  SkillRequest skillRequest; /**< Skill request for ballSearch behavior */
  Agents agents;
  const float goalLineXOffset = 50.f;
  const float minRadius = 20.f;//用于守门员避球
  float initialRadius;
  const Agent* agent;

  const int lastBallPositionThreshold = 6000; /**< if the last known ball position was longer that this not in view at the beginning of the search check it first. */
  const float ignoreVoronoiThreshold = 1500.f; /**< if the last known ball position is closer that this check it first even if it is outside the Voronoi cell */

  option(Root)//找球函数逻辑，TODO
  {
    const Pose2f goalCenterOnFieldWithOffset = Pose2f(0.f, theFieldDimensions.xPosOwnGoalLine + goalLineXOffset, 0.f);//守门员移动目标位置
    const Pose2f goalCenterRelativeWithOffset = theRobotPose.inverse() * goalCenterOnFieldWithOffset;

    //更新找球搜索网格（BallSearchAreas.grid），并找到离球最后一次被发现时所在的位置最近的网格区域
    const auto getBallCell = [&]   
    {
      // Todo: refactor the grid to find positions by indices
      // find the cell next to the ball
      auto cellsToSearch = theBallSearchAreas.grid;
      return *std::min_element(cellsToSearch.cbegin(), cellsToSearch.cend(), [&](const BallSearchAreas::Cell& a, const BallSearchAreas::Cell& b)
      {
        return (a.positionOnField - theFieldBall.recentBallPositionOnField()).squaredNorm() <
               (b.positionOnField - theFieldBall.recentBallPositionOnField()).squaredNorm();
      });
    };

    /**
     * return true if the condition to first check the last ball position is met
     */

    /*
    判断球是否有较大可能在原位置，若有可能在原位置则优先检查球最后的位置(关键条件)
    条件：（同时满足）
        1.是否未被检查时间超过阈值(lastBallPositionThreshold)
        2.最后位置是否离机器人较近(小于阈值ignoreVoronoiThreshold)，或处于该机器人的基础姿势在战术中的Voronoi图区域(?)。
        3.不处于任意球阶段或处于推任意球阶段（？）TODO
    */
    const auto checkNearLastBallCondition = [&](const BallSearchAreas::Cell nextCellToBall)
    {
      const bool longerNotChecked = theFrameInfo.getTimeSince(nextCellToBall.timestamp) > lastBallPositionThreshold;
      const bool notToFar = (theFieldBall.recentBallPositionOnField() - agent->pose.translation).norm() < ignoreVoronoiThreshold ||
          Geometry::isPointInsideConvexPolygon(agent->baseArea.data(), static_cast<int>(agent->baseArea.size()), theFieldBall.recentBallPositionOnField());
      const bool notMovedByGameState = !theGameState.isFreeKick() || theGameState.isPushingFreeKick();

      return longerNotChecked && notToFar && notMovedByGameState;
    };

    //the initial ballSearch
    initial_state(initial)//状态转移见wiki
    {
      transition
      {
        ANNOTATION("BallSearch", "is active");
        if(agent->isGoalkeeper)
          goto goalkeeper;
        else
        {
          if(!theBallSearchAreas.grid.empty() && checkNearLastBallCondition(getBallCell()))
            goto checkNearLastBall;
          else
            goto gridSearch;
        }
      }
    }

    // first look near the last position of the ball
    state(checkNearLastBall)
    {
      BallSearchAreas::Cell nextCellToBall = getBallCell();
      transition
      {
        if(!checkNearLastBallCondition(nextCellToBall))//条件如上所述（Line 72）
        {
          if(agent->isGoalkeeper)
            goto goalkeeper;
          else
            goto gridSearch;
        }
      }
      action
      {
        skillRequest = SkillRequest::Builder::observe(nextCellToBall.positionOnField);//查看离球最后位置最近的cell
      }
    }

    // if the ball is lost in a non-standard situation, the robot will use the ballSearchAreas Grid to search the ball.
    state(gridSearch)//TODO
    {
      transition
      {
        if(agent->isGoalkeeper)
        {
          goto goalkeeper;
        }
      }
      action
      {
        skillRequest = SkillRequest::Builder::observe(theBallSearchAreas.cellToSearchNext(*agent));//按cell的优先级，时间戳遍历搜索网格
      }
    }

    //BallSearch Behavior for the Goalkeeper
    //守门员子状态机，详见Wiki状态图
    state(goalkeeper)
    {
      transition
      {
        {
          if((theGameState.isCornerKick() || theGameState.isGoalKick()) && theFrameInfo.getTimeSince(theGameState.timeWhenStateStarted) < 10000)
            goto goalkeeperStandardSituation;
          else if(theFieldBall.timeSinceBallWasSeen > 10000 && theRobotPose.translation.x() < theFieldDimensions.xPosOwnGoalLine)
            goto goalkeeperWalkToTarget;
          else if(theFieldBall.timeSinceBallWasSeen > 10000 && (std::abs(goalCenterRelativeWithOffset.translation.angle()) > 45_deg))
          {
            initialRadius = (theRobotPose.translation - Vector2f(theFieldDimensions.xPosOwnGoalLine, 0.f)).norm();;
            goto goalkeeperAvoidBall;
          }
          else if(theFieldBall.timeSinceBallWasSeen > 10000)
            goto goalkeeperTurnToTarget;
        }
      }
      action
      {
        skillRequest = SkillRequest::Builder::walkTo(agent->basePose);
      }
    }

    // during a standard situation the goalkeeper will remain at the current position
    state(goalkeeperStandardSituation)
    {
      transition
      {
        if((!theGameState.isCornerKick() && !theGameState.isGoalKick()) || theFrameInfo.getTimeSince(theGameState.timeWhenStateStarted) > 10000)
        {
          goto goalkeeper;
        }
      }
      action
      {
        skillRequest = SkillRequest::Builder::stand();
      }
    }

    // the goalkeeper will walk to a given position
    state(goalkeeperWalkToTarget)
    {
      action
      {
        skillRequest = SkillRequest::Builder::walkTo(goalCenterOnFieldWithOffset);
      }
    }

    // the goalkeeper will turn to the given position
    state(goalkeeperTurnToTarget)
    {
      transition
      {
        if((std::abs(goalCenterRelativeWithOffset.translation.angle()) < 10_deg))//允许误差，防止机器一直原地转向
          goto goalkeeperWalkToTarget;
      }
      action
      {
        skillRequest = SkillRequest::Builder::observe((theRobotPose * goalCenterRelativeWithOffset).translation);
      }
    }

    // while searching for the ball the goalkeeper should avoid touching the ball to avoid scoring an own goal.
    //防止乌龙球，实现方法为相对球门中心向外侧移动至少minRadius距离
    state(goalkeeperAvoidBall)
    {
      transition
      {
        const float GoalkeeperToGoalLineCenter = (theRobotPose.translation - Vector2f(theFieldDimensions.xPosOwnGoalLine, 0.f)).norm();
        if(GoalkeeperToGoalLineCenter > initialRadius + minRadius)
          goto goalkeeperTurnToTarget;//完成避球逻辑后尝试返回原位置
      }
      action
      {
        const Vector2f goalCenterRelative(theRobotPose.inverse() * Vector2f(theFieldDimensions.xPosOwnGoalLine, 0.f));
        const Vector2f offsetRelative(goalCenterRelative.normalized(-200.f));
        const Vector2f offsetAbsolute(theRobotPose * offsetRelative);
        skillRequest = SkillRequest::Builder::walkTo(offsetAbsolute);
      }
    }
  }
};
#undef action
#undef transition
