/**
 * @file DirectKickOff.cpp
 *
 * This ...
 *
 * @author Arne Hasselbring (from former KickoffStrikerCard.cpp in 2019).
 */

#include "SkillBehaviorControl.h"
#include "Tools/BehaviorControl/SectorWheel.h"
#include "Debugging/DebugDrawings.h"

namespace DirectKickOff
{
  struct ObstacleSector
  {
    Rangea sector; /**< The angular range relative to the ball that the obstacle blocks. 障碍物角度范围*/
    float distance; /**< The distance of the obstacle to the ball.障碍物距离 */
    float x; /**< The x coordinate on field of the obstacle. 障碍物x坐标*/
  };
}
using namespace DirectKickOff;

option((SkillBehaviorControl) DirectKickOff,
       defs((float)(300.f) hysteresisNumber, /**< A number which is used in various places to define a hysteresis offset. 定义滞后偏移的数值*/
            (Angle)(45_deg) hysteresisAngle, /**< When sorting sectors by their opening angle, the one selected in the previous frame gets this as bonus. 当按扇形区域的开放角度对扇形进行排序时，上一帧选择的扇形会获得这个角度的奖励，有助于保持选择的稳定性*/
            (Angle)(60_deg) halfGoalSectorAngle, /**< Half of the goal opening angle around the positive x axis. 球门开放角度围绕正 x 轴的一半*/
            (Angle)(20_deg) minOpeningAngle, /**< The minimum opening angle a sector must have to be considered.扇形区域必须具备的最小开放角度，小于这个角度的扇形将不被考虑，确保开球有足够的空间 */
            (float)(500.f) ballDistanceForSlowWalk), /**< If the ball is close, walk slow.如果球与机器人的距离小于这个值，机器人将缓慢行走 */
       vars((bool)(false) wasActive, /**< Whether an in walk kick out of the center circle was already tried. 已经开球*/
            (KickInfo::KickType)(KickInfo::walkForwardsRightAlternative) kickType, /**< The kick type to try. 踢球类型*/
            (Angle)(0_deg) targetAngle)) /**< The target angle to which the kick-off should go (from the ball in field coordinates). 踢球角度*/
{
  initial_state(execute)
  {
    action
    {
      if(theFrameInfo.getTimeSince(theExtendedGameState.timeWhenStateStarted[GameState::ownKickOff]) < 2000)// 在开球状态开始后的前 2000 毫秒内，机器人执行 LookLeftAndRight 动作，并保持站立状态
      {
        LookLeftAndRight();
        Stand();
      }
      else
      {
        // Prepare obstacle sectors.
        std::vector<ObstacleSector> obstacleSectors;
        for(const Obstacle& obstacle : theObstacleModel.obstacles)
        {
          const Vector2f obstacleOnField = theRobotPose * obstacle.center;
          // 排除 x 坐标小于 0 或在中心圆内一定范围的障碍物
          if(obstacleOnField.x() < 0.f || obstacleOnField.squaredNorm() < sqr(theFieldDimensions.centerCircleRadius - 300.f))
            continue;

          const float width = (obstacle.left - obstacle.right).norm() + 4.f * theBallSpecification.radius;// 计算障碍物的宽度、与球的距离、占据的角度范围
          const float distance = std::sqrt(std::max((obstacleOnField - theFieldBall.positionOnField).squaredNorm() - sqr(width / 2.f), 1.f));
          if(distance < theBallSpecification.radius)
            continue;

          const float radius = std::atan(width / (2.f * distance));
          const Angle direction = (obstacleOnField - theFieldBall.positionOnField).angle();
          // Cull obstacles that are not in the goal sector anyway.
          if(direction - radius > halfGoalSectorAngle || direction + radius < -halfGoalSectorAngle)// 排除不在球门扇形区域内的障碍物
            continue;
          obstacleSectors.emplace_back();// 在容器末尾构造元素
          obstacleSectors.back().sector = Rangea(Angle::normalize(direction - radius), Angle::normalize(direction + radius));
          obstacleSectors.back().distance = distance;
          obstacleSectors.back().x = obstacleOnField.x();
          // If the previous target is inside this sector, artificially increase its x coordinate so it will potentially be culled before other sectors.
          if(wasActive && obstacleSectors.back().sector.isInside(targetAngle))
            obstacleSectors.back().x += hysteresisNumber;// 如果之前已经尝试过开球且目标角度在当前障碍物扇形区域内，增加该障碍物的 x 坐标？
        }
        // Obstacle to avoid passing straight forward and running after the ball with the same Robot
        if(!theIndirectKick.allowDirectKick)// 如果不允许直接开球（todo.如何传给队友），添加一个额外的障碍物扇形区域，范围为 -35° 到 35°，以避免直接向前传球并追球
        {
          obstacleSectors.emplace_back();
          obstacleSectors.back().sector = Rangea(-35_deg, 35_deg);
          obstacleSectors.back().distance = theFieldDimensions.centerCircleRadius;
          obstacleSectors.back().x = theFieldDimensions.centerCircleRadius;
        }


        // Sort obstacles according to their absolute x coordinate (to ease culling later on).按照障碍物的 x 坐标对障碍物扇形区域进行排序
        std::sort(obstacleSectors.begin(), obstacleSectors.end(), [](const ObstacleSector& s1, const ObstacleSector& s2) { return s1.x < s2.x; });
        // 构建一个扇形轮 wheel，添加球门扇形区域和障碍物扇形区域。筛选出类型为 goal 且开放角度足够大的扇形区域。如果没有满足条件的扇形区域，逐步剔除最后一个障碍物并重新构建扇形轮，直到找到合适的扇形区域或障碍物列表为空。
        SectorWheel wheel;
        std::list<SectorWheel::Sector> sectors;
        bool isLargeEnough = false;
        do
        {
          wheel.begin(theFieldBall.positionOnField);
          wheel.addSector(Rangea(-halfGoalSectorAngle, halfGoalSectorAngle), 2.f * theKickInfo[KickInfo::walkForwardsLeftAlternative].range.max, SectorWheel::Sector::goal);
          for(const ObstacleSector& obstacleSector : obstacleSectors)
            wheel.addSector(obstacleSector.sector, obstacleSector.distance, SectorWheel::Sector::obstacle);
          sectors = wheel.finish();

          for(const SectorWheel::Sector& sector : sectors)
            if(sector.type == SectorWheel::Sector::goal &&
               sector.angleRange.getSize() >= ((wasActive && sector.angleRange.isInside(targetAngle)) ? Angle(minOpeningAngle * 0.5f) : minOpeningAngle))
            {
              isLargeEnough = true;
              break;
            }
        }
        while(!isLargeEnough && !obstacleSectors.empty() && (obstacleSectors.pop_back(), true));

        DRAW_SECTOR_WHEEL("option:DirectKickOff:wheel", sectors, theFieldBall.endPositionOnField);

        // 遍历筛选后的扇形区域，选择开放角度最大的扇形区域作为最佳开球方向。根据最佳开球方向和之前的状态，选择合适的踢球类型（左踢或右踢）
        Angle bestOpeningAngle = 0_deg;
        Angle bestTargetAngle = 0_deg;
        for(const SectorWheel::Sector& sector : sectors)
        {
          if(sector.type != SectorWheel::Sector::goal)
            continue;
          const Angle openingAngle = sector.angleRange.getSize() + ((wasActive && sector.angleRange.isInside(targetAngle)) ? hysteresisAngle : 0_deg);
          if(openingAngle > bestOpeningAngle)
          {
            bestOpeningAngle = openingAngle;
            bestTargetAngle = sector.angleRange.getCenter();
          }
        }

        ASSERT(bestOpeningAngle > 0_deg);

        targetAngle = bestTargetAngle;
        if(targetAngle > (wasActive ? (kickType == KickInfo::walkForwardsRightAlternative ? -hysteresisAngle : hysteresisAngle) : 0_deg))
          kickType = KickInfo::walkForwardsRightAlternative;
        else
          kickType = KickInfo::walkForwardsLeftAlternative;

        GoToBallAndKick({.targetDirection = Angle::normalize(targetAngle - theRobotPose.rotation),
                         .kickType = kickType,
                         .lookActiveWithBall = true,
                         .preStepType = PreStepType::notAllowed,
                         .reduceWalkSpeedType = theFieldBall.positionRelative.squaredNorm() < sqr(ballDistanceForSlowWalk) ? ReduceWalkSpeedType::slow : ReduceWalkSpeedType::noChange });
        wasActive = true;// 表示已经尝试过开球
      }
    }
  }
}
