# 行为控制：争球与团队协作（概览 + 详细）

本文围绕 **争球（Duel/Contest）** 与 **争球时的团队配合**，给出可直接用于 Wiki 的详细说明。内容以 `BehaviorControl` 与 `StrategyBehaviorControl` 相关代码为主，并包含定位路径、函数名与具有区分度的代码片段，便于 `Ctrl+F` 快速定位。本文覆盖的逻辑链条完整贯穿：争球触发 → 争球评分 → 踢法执行 → 团队角色分配 → 传接球协同。

---

## 阅读顺序（目录）

1. 争球入口与退出（PlayBall）
2. 争球对象与方向/扇区（Zweikampf）
3. forwardSteal / sidewards 触发与限制
4. getDuelRating 评分机制与优先级
5. 执行阶段（踢球 or 走位）
6. 避障与近距离控球
7. 团队协作（TTRB 与 TeamMessage）
8. 附录 A：Zweikampf.cpp 逐段详解（函数名 + 行号）

---

## 概览（言简意赅）

- **争球入口与退出**：`PlayBall::activateDuel` 以“球近 + 障碍近 + 朝向允许”触发；`PlayBall::stopDuel` 以“球远或障碍不再接近”退出（搜索关键词：`activateDuel`、`ballWasSeen(300)`、`positionRelative.x() > 1000.f`）。
- **争球决策主链路**：`Zweikampf::calculateDuel` 生成方向候选 → `calculateSectorWheel` 形成扇区 → `getDuelRating` 评分 → 以 `Goal > Steal > Pass > Other` 选最优，并用 `kickForcedUpTime` 保持 100ms（搜索关键词：`GoalShots > StealBall > Pass > Others`、`kickForcedUpTime`）。
- **争球踢法策略名称**：forwardSteal / sidewards（踢法枚举为 `walkForwardStealBallLeft/Right`、`walkSidewardsLeftFootToLeft/RightFootToRight`），其触发条件由 `StealBallParameters` 与 `ObstacleHandling` 共同约束（搜索关键词：`forwardStealBallOpponentPositionNormal`、`maxObstacleDistanceForWalkStealBallKick`、`forbiddingKickAngle`）。
- **近距离控球与避障**：`GoToBallAndDribble` / `GoToBallAndKick` 在远距用 `PathPlanner.plan`，近距用 `LibWalk.calcObstacleAvoidance`，最终由 `WalkToBallAndKick` 或 `Dribble` 执行动作（搜索关键词：`PathPlanner.plan`、`calcObstacleAvoidance`、`WalkToBallAndKick`）。
- **团队协作**：`StrategyBehaviorControl::updateAgentByTeamMessage` 融合队友 `BehaviorStatus/StrategyStatus/BallModel`，`Behavior::calcTTRB` 决定 `ActiveRole::playBall`，`BehaviorStatus.passTarget` 驱动 `ReceivePass`（搜索关键词：`disagreeOnBall`、`calcTTRB`、`passTarget`）。

下面进入详细分析。

---

## 1. 争球入口与近战状态机（PlayBall）

### 1.1 概览
- 这是“Close combat/近战”触发条件：球近 + 障碍近 + 朝向允许。
- 不是只看“球近”，而是 **球位置 + 障碍物角度/距离 + 朝向球的可执行性** 的组合。

### 1.2 详细分析（触发条件与退出条件）
**文件**：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/PlayBall.cpp`  
**函数位置**：`PlayBall.cpp:17`（`activateDuel`）、`PlayBall.cpp:55`（`stopDuel`）

触发条件函数：`activateDuel()`
**触发逻辑（位置：`PlayBall.cpp:17`）**：
- 先统计“最近障碍物”的距离、角度与最小正前方角度（基于球的相对坐标）。  
- 判定 `obstacleClose` 与 `obstacleAngleClose`：障碍越偏侧，距离阈值越严格。  
- 判定球状态：`ballWasSeen(300)`、`positionRelative.x()` 在前方近距、`positionRelative.y()` 不偏离太大。  
- 判定朝向：`abs(angleToBall) < 80_deg`，确保当前转身角度可执行。  
- 上述条件同时满足时进入争球。  

关键代码片段（`activateDuel`，`PlayBall.cpp:17`）：  
```cpp
const bool obstacleClose = distanceToClosestObstacle < (duelMinDistanceToClosest * (1.f - (std::abs(angleToClosestObstacle) / duelMinAngleToClosest / 3.f)));
const bool obstacleAngleClose = smallestAngleToCloseObstacle <= duelMinAngleToClosest;
const bool ballSeen = theFieldBall.ballWasSeen(300);
const bool ballPosXClose = between<float>(theFieldBall.positionRelative.x(), 0.f, 700.f);
const bool ballPosYClose = std::abs(theFieldBall.positionRelative.y()) < 500.f;
const bool rotationToBallOk = std::abs(theFieldBall.positionRelative.angle()) < 80_deg;
return obstacleClose && obstacleAngleClose && ballSeen && ballPosXClose && ballPosYClose && rotationToBallOk;
```

退出条件函数：`stopDuel()`
**退出逻辑（位置：`PlayBall.cpp:55`）**：  
- 球相对 X 超过 1000mm 直接退出。  
- 若仍存在“距离近 + 角度允许”的障碍，则保持争球；否则退出争球。  

关键代码片段（`stopDuel`，`PlayBall.cpp:55`）：  
```cpp
if(theFieldBall.positionRelative.x() > 1000.f)
  return true;
for(const Obstacle& omo : theObstacleModel.obstacles)
{
  const float distanceToObstacle = (omo.center - theFieldBall.positionRelative).norm();
  const float angleToObstacle = std::abs(omo.center.angle());
  if(distanceToObstacle < (duelMinDistanceToClosest * 1.2f * (1.f - (std::abs(angleToObstacle) / pi / 2.f))))
    return false;
}
return true;
```

---

## 2. Zweikampf：数据结构与全局变量（理解“争球对象”）

### 2.1 关键结构
**文件**：`Zweikampf.cpp`
- `DuelPose`：存放“要执行的踢法”的所有结果（角度、踢法、精度、评分）。
- `RatingMap` / `RatingMapVector`：把不同踢程映射到“场地评分”，避免重复计算。
- `TargetType`：`goalShot > goalDribbleShot > stealBall > pass > other`，用于 **类型优先级**。

### 2.2 争球对象与障碍修正
**函数定位**：`calculateClosestObstacle`（位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:436`）与 `shiftObstacleBackward`（位置：`Zweikampf.cpp:418`）。  
**解释**：  
- **争球对手 = 最近障碍物**（不区分队友/对手时以障碍模型为准）。  
- `shiftObstacleBackward` 会把离球过近、且可能“被误感知在球前方”的障碍物后移，避免错误策略导致踢向对手脚。  

---

## 3. 争球动作空间：方向采样与扇区构建

### 3.1 方向采样（directionPossibilities）
**函数定位**：`calculateDuel`（位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1279`），方向列表初始化在 `Zweikampf.cpp:1292`。  
**关键逻辑位置**：  
- 局部搜索：`Zweikampf.cpp:1406` 至 `Zweikampf.cpp:1415`。  
- 全局搜索：`Zweikampf.cpp:1422` 至 `Zweikampf.cpp:1423`。  
- sidewards/forwardSteal 角度加入：`Zweikampf.cpp:1438` 至 `Zweikampf.cpp:1443`。  
**解释**：方向搜索以“上一帧踢向”为中心做局部扩展，再做全局补充，确保既稳定又能跳出局部最优；同时显式加入 sidewards 与 forwardSteal 的角度候选，避免争球踢法被遗漏。  

### 3.2 扇区构建（sector wheel）
**函数定位**：`calculateSectorWheel`（位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:478`），在 `calculateDuel` 内调用（`Zweikampf.cpp:1457`）。  
**解释**：扇区会记录障碍物角度范围与距离，用于计算 `maxKickRange` 与 `isGoalAngle`，是 Goal/Pass 可行性的基础过滤器。  

---

## 4. forwardSteal / sidewards（争球踢法策略）的详细逻辑

### 4.1 sidewards（侧向踢）触发限制
**函数定位**：`calculateDuel` 内的 sidewards 过滤（位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1643`）。  
**解释**：  
- 只有当对手**比自己更接近球**且踢向角度处于允许扇区时，sidewards 才可用。  
- 若当前必须“站在球与本方球门之间”，侧踢角度会被限制（`forbiddingKickAngle`），并受 `sidewardRestrictionTime` 延迟释放，避免禁区附近频繁切换。  
- 相关变量位置：`mustStandBetweenBallAndGoal` 与 `forbiddingKickAngle` 计算在 `Zweikampf.cpp:1318` 附近。  

关键代码片段（sidewards 过滤，`Zweikampf.cpp:1643`）：  
```cpp
if((kickType == KickInfo::walkSidewardsLeftFootToLeft || kickType == KickInfo::walkSidewardsRightFootToRight) &&
   ((opponentAndSelfDistanceToBallDiff < 100.f) ||
    !(forbiddingKickAngle.isInside(kickAngle) || forbiddingKickAngleExtra.isInside(kickAngle))))
  continue;
```

### 4.2 forwardSteal（争球踢法）触发条件
**函数定位**：`calculateDuel` 内 forwardSteal 过滤（位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1649`）。  
**解释**：  
- forwardSteal 只有在 **对手在球后方角度锥内**、**对手更接近球**时允许。  
- `leftAngleRangeBallToOpponent` 来自 `calculateForwardStealObstacleAngleRange`（位置：`Zweikampf.cpp:1340`）。  
- `obstacleWallBehindBall` 在扇区轮后计算（位置：`Zweikampf.cpp:1460`），若判定为“障碍墙”，允许 forwardSteal 继续评估。  

关键代码片段（forwardSteal 过滤，`Zweikampf.cpp:1649`）：  
```cpp
if((kickType == KickInfo::walkForwardStealBallLeft || kickType == KickInfo::walkForwardStealBallRight) &&
   (stealBallMin != stealBallMax ||
    (!leftAngleRangeBallToOpponent.isInside(angleFromBallToOpponent) && !obstacleWallBehindBall) ||
    !(rightForwardStealRange.isInside(Angle::normalize(kickAngle + theRobotPose.rotation)) ||
      leftForwardStealRange.isInside(Angle::normalize(kickAngle + theRobotPose.rotation))) ||
    opponentAndSelfDistanceToBallDiff > obstacleHandling.maxObstacleDistanceForWalkStealBallKick))
  continue;
```

### 4.3 Steal 类型加分（评分）
**函数定位**：`getDuelRating` 内 forwardSteal 评分（位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:876`）。  
**解释**：  
- forwardSteal 会被直接标记为 `stealBall`，其类型优先级高于 `pass/other`。  
- 若落点距离边线较安全，则根据对手相对角度选择“好侧”加分；若落点靠近边线则削减奖励以避免出界风险。  

关键代码片段（forwardSteal 加分，`Zweikampf.cpp:876`）：  
```cpp
if(kickType == KickInfo::walkForwardStealBallLeft || kickType == KickInfo::walkForwardStealBallRight)
{
  pose.rating += duelRatings.ratingStealBallKick;
  pose.type = TargetType::stealBall;
  float outOfFieldMalus = std::min(0.f, std::abs(std::abs(pose.lastFieldEndPoint.y()) - (theFieldDimensions.yPosLeftTouchline - 500.f)) / 500.f);
  if(outOfFieldMalus == 0)
  {
    if((angleFromBallToOpponent > forwardStealPreferenceRange && kickType == KickInfo::walkForwardStealBallLeft) ||
       (angleFromBallToOpponent < -forwardStealPreferenceRange && kickType == KickInfo::walkForwardStealBallRight))
      pose.rating += duelRatings.ratingStealBallKickBetterSide;
  }
  else
    pose.rating -= duelRatings.ratingStealBallKickBetterSide * outOfFieldMalus;
}
```

---

## 5. getDuelRating：争球评分的完整路径

### 5.1 关键步骤（按代码注释 8.x 顺序）
说明：`8.x` 是 `getDuelRating` 内部注释编号，其中 “8” 对应 `calculateDuel` 的第 8 步（对每个方向、每种踢法评分），`8.4.*` 是该步骤的子流程编号。
- **8.4.1** 前踢/转踢插值：保证相同踢法在不同角度下可执行。
- **8.4.2** 预判 steal 类型：steal 踢法会降低一些惩罚。
- **8.4.6** 生成踢球姿态（pose）。
- **8.4.7** 旋转限制：若转身过大则淘汰，**但传球角度可例外**。
- **8.4.8** forwardSteal “站位侧”检查，防止站在错误侧踢。
- **8.4.3** 使用 FieldRating 选择踢程（range）。
- **8.4.9** GoalShot 评分与类型判断。
- **8.4.10** 禁区保护（避免回传进己方禁区）。
- **8.4.11** 自己半场惩罚。
- **8.4.12** forwardSteal 左右侧偏好加分。
- **8.4.13** 落脚点阻挡检查（避免踩到对手脚）。

关键代码片段（旋转限制 + 传球例外，`Zweikampf.cpp:769`）：  
```cpp
if(!maxAllowedRotationRangeOne.isInside(useRotationOfInterest) && !maxAllowedRotationRangeTwo.isInside(useRotationOfInterest) &&
   !(isGoalAngle && rangeForGoal < dribbleRange + useKickRange.max + ((theDuelPose.type == TargetType::goalShot || theDuelPose.type == TargetType::goalDribbleShot) ? rangeHysteresis : 0.f)) &&
   !(kickType == KickInfo::walkForwardStealBallLeft || kickType == KickInfo::walkForwardStealBallRight))
{
  if(theSkillRequestPose.passTarget != -1 &&
     (theSkillRequestPose.passRangeOne.isInside(kickAngle) || theSkillRequestPose.passRangeTwo.isInside(kickAngle)))
    tooMuchRotationForNonePass = true;
  else
    return false;
}
```

关键代码片段（选择最佳踢程，`Zweikampf.cpp:807`）：  
```cpp
for(size_t index = 0; index < ratingMapVector.ratingMap.size(); index++)
{
  if(sqr(ratingMapVector.ratingMap[index].range + useFieldBorderSafeDistance) > distanceToOutOfFieldSquared ||
     ratingMapVector.ratingMap[index].range > useKickRange.max + rangeHysteresis)
    break;
  if(ratingMapVector.ratingMap[index].range >= useKickRange.min - rangeHysteresis &&
     ratingMapVector.ratingFunc(theFieldRating, ratingMapVector.ratingMap[index]) < bestRating + (useStrongForwardKick ? 0.1f : 0.f))
  {
    bestIndex = index;
    bestRating = ratingMapVector.ratingFunc(theFieldRating, ratingMapVector.ratingMap[index]);
  }
}
```

关键代码片段（GoalShot 加分与类型判定，`Zweikampf.cpp:826`）：  
```cpp
if(isGoalAngle && rangeForGoal < dribbleRange + useKickRange.max + (theDuelPose.type == TargetType::goalShot ? rangeHysteresis : 0.f))
{
  pose.rating += duelRatings.ratingGoalShot;
  pose.type = rangeForGoal > useKickRange.max + (theDuelPose.type == TargetType::goalShot ? rangeHysteresis : 0.f) ? TargetType::goalDribbleShot : TargetType::goalShot;
}
```

这些步骤共同决定“Goal/Steal/Pass/Other”的最终选择。

**补充细节（关键判断点）**：
- **传球例外**：当旋转过大时，如果踢向角度落入 `passRangeOne/passRangeTwo`，会保留为“可传球角度”（搜索关键词：`tooMuchRotationForNonePass`、`passRangeOne`）。  
- **Pass 类型标记**：评分时若 `ratingMapVector.ratingMap[bestIndex].isTeammatePass` 为真，则 `pose.type` 变为 `TargetType::pass`（搜索关键词：`isTeammatePass`）。  
- **Goal 类型标记**：满足射门距离与角度时，直接设置 `goalShot/goalDribbleShot` 并加 `ratingGoalShot`（搜索关键词：`ratingGoalShot`、`goalDribbleShot`）。  
- **优先级保持**：`kickForcedUpTime` 对高优先级动作保持 100ms，避免频繁抖动（搜索关键词：`kickForcedUpTime`）。

---

更完整的逐段行号说明见 **附录 A**（`Zweikampf.cpp` 逐段详解）。

## 6. 争球执行（踢球 or 走位）

**执行位置**：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1945`（`GoToBallAndKick`），`Zweikampf.cpp:1957`（`WalkToPoint`）。  
**说明**：  
- 若 `theDuelPose.noKick` 为假，调用 `GoToBallAndKick`，并传入 `targetDirection`、`kickType`、`length`、`directionPrecision` 等参数，保证踢向与精度与评分结果一致。  
- 若 `theDuelPose.noKick` 为真，改为 `WalkToPoint` 占位移动，并持续施压，防止对手轻松推进。  

---

## 7. 障碍物避让（Obstacle Avoidance）

### 7.1 走位避障（WalkToPointObstacle）
**文件**：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Walk/WalkToPointObstacle.cpp`  
**函数**：`getObstacleAtMyPositionCircle`（位置：`WalkToPointObstacle.cpp:62`）  
**说明**：当目标点落入障碍物半径内时，生成“占位圆”并返回，用于后续偏移目标点，避免直接走向被占用位置。

### 7.2 带球/踢球避障
**文件**：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndDribble.cpp`、`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndKick.cpp`  
**关键位置**：  
- 远距离路径规划：`GoToBallAndDribble.cpp:41`、`GoToBallAndKick.cpp:58` 使用 `PathPlanner.plan`。  
- 近距离局部避障：`GoToBallAndDribble.cpp:63`、`GoToBallAndKick.cpp:102` 使用 `LibWalk.calcObstacleAvoidance`（`toBall=true`）。  
**说明**：远距更强调全局避障与路径可行性，近距更强调贴球稳定与脚步安全。

---

## 8. 近距离控球（Close Ball）

- **进入条件**：`PlayBall::activateDuel`（位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/PlayBall.cpp:17`）综合判断“球相对位置 + 障碍接近 + 朝向可执行”，当球前方近距且对手靠近时进入近战控球模式。  
- **执行方式**：  
  - `GoToBallAndDribble`（位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndDribble.cpp:19`）使用 `switchToPathPlannerDistance` 决定远距/近距方案：远距用 `PathPlanner.plan`（`GoToBallAndDribble.cpp:41`），近距用 `LibWalk.calcObstacleAvoidance`（`GoToBallAndDribble.cpp:63`）。  
  - `GoToBallAndKick`（位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndKick.cpp:24`）同样以 `switchToPathPlannerDistance` 切换：远距用 `PathPlanner.plan`（`GoToBallAndKick.cpp:58`），近距用 `LibWalk.calcObstacleAvoidance`（`GoToBallAndKick.cpp:102`）。  
- **核心点**：近距离阶段优先使用 `LibWalk` 的局部避障与精确步态参数（`toBall=true`），保证贴球场景下的姿态稳定与障碍安全距离。

---

## 9. 团队协作：谁去抢球（TTRB）

### 9.1 队友消息融合
**文件**：`WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/StrategyBehaviorControl.cpp`  
**函数**：`updateAgentByTeamMessage`（位置：`StrategyBehaviorControl.cpp:206`）  
**说明**：同步队友的 `BehaviorStatus` 与 `BallModel`，更新 `agent.lastKnownTarget/Speed/ballPosition`；并在 `StrategyBehaviorControl.cpp:245` 计算 `disagreeOnBall`，当球位置分歧过大时剔除该队友以避免“多人追不同的球”。

**解释**：
- `disagreeOnBall` 用于过滤“对球认知不一致”的队友，避免多名机器人同时追不同的球。
- 阈值由球距离动态调整，典型搜索词为 `mapToRange` 与 `baseThreshold`（同文件内的球一致性判断逻辑）。

### 9.2 抢球者选择
**文件**：`WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/Behavior.cpp`  
**函数**：`calcTTRB`（位置：`Behavior.cpp:1011`）用于计算“到球时间”，包含“曾经是主动球员”与“球丢失惩罚”等因子。  
**分配逻辑**：  
- 在 `Behavior.cpp:1111` 计算自身 `minTTRB`。  
- 在 `Behavior.cpp:1120` 遍历队友比较 `itsTTRB`。  
- 在 `Behavior.cpp:1131` 将 `minAgent->nextRole` 设为 `ActiveRole::playBall`（或对方任意球时设为 `freeKickWall`）。  
**说明**：TTRB 是“谁去抢球”的核心指标，且会考虑 `disagreeOnBall`、是否起身、是否为守门员等条件（见 `canBeActive` 附近逻辑）。

**解释**：
- `calcTTRB` 计算“到球时间”，不仅看距离，还包含“是否刚刚是主动球员”的惩罚/奖励项（同文件内 `calcTTRB` 定义处）。
- 当自身不可见球但 `TeamBallModel` 有效时，逻辑会转向 `closestToTeamBall` 分配（搜索关键词：`closestToTeamBall`、`TeamBallModel`）。

---

## 10. 关键文件列表（争球 + 团队协作完整覆盖）

- `SkillBehaviorControl/Skills/Ball/PlayBall.cpp`：争球入口/退出。
- `SkillBehaviorControl/Skills/Ball/Zweikampf.cpp`：争球评分与踢法选择。
- `StrategyBehaviorControl/Behavior.cpp`：TTRB 抢球者判定。
- `StrategyBehaviorControl/StrategyBehaviorControl.cpp`：融合队友消息。
- `Representations/BehaviorControl/BehaviorStatus.h`：传球/速度/目标共享。
- `Representations/Communication/ReceivedTeamMessages.h`：队友消息结构。

---

## 11. Wiki 可直接使用的结论

- **争球入口**由 PlayBall 的 `activateDuel()` 决定，依据球位置、障碍物角度/距离、朝向可执行性组合判断。
- **争球策略**由 `Zweikampf` 评分系统决定，目标优先级为 `Goal > Steal > Pass > Other`。
- **争球踢法策略**由 `walkForwardStealBallLeft/Right` 与 `walkSidewards*` 实现，对手相对角度与距离是触发关键。
- **团队协作**通过 `TTRB` 分配抢球者（ActiveRole::playBall），并用 `BehaviorStatus.passTarget` 协调传接球。

---

## 附录 A. Zweikampf.cpp 逐段详解（函数名 + 行号 + 详细解释）

说明：以下按 `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp` 源码顺序整理；行号以当前仓库版本为准。

### A.1 参数与关键结构（数据含义与作用范围）
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:32`，结构：`SearchParameters`。作用：定义方向采样的密度、范围与射门缓冲角等，决定“要搜索哪些踢向”。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:46`，结构：`DuelRatings`。作用：所有“评分加减项”的系数集合，直接影响 Goal/Steal/Pass 的倾向。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:66`，结构：`DuelTimings`。作用：时间滞回与失活条件，用于防止争球抖动。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:76`，结构：`ObstacleHandling`。作用：脚边缘安全、障碍阻挡阈值、障碍后移修正、forwardSteal 强制触发距离等。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:95`，结构：`StealBallParameters`。作用：sidewards/forwardSteal 的角度锥与转身阈值；决定“何时侧抢/前抢合理”。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:106`，结构：`LastKickHysteresis`。作用：同踢法/同方向滞回，避免频繁切换。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:116`，枚举：`TargetType`。作用：Goal > Steal > Pass > Other 的优先级顺序，是最终选择的“类型门槛”。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:125`，结构：`DuelPose`。作用：记录一个候选踢法的“评分、姿态、踢法类型、精度与落点”。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:157`，结构：`RatingMap`。作用：缓存“某一踢程”的潜力场评分。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:213`，结构：`RatingMapVector`。作用：把“同一踢向的多个踢程评分”组织起来，作为 `getDuelRating` 输入。

### A.2 行为请求与踢向更新
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:325`，函数：`updateDuelPose`。作用：基于“上次落点 + 里程计变化”更新 `kickAngle`，保持踢向连续性。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:338`，函数：`updatePassEndPosition`。作用：确认 `passTarget`，并生成 `passRangeOne/passRangeTwo`，用于跨 ±180° 角度的传球限制。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:372`，函数：`updateSkillRequest`。作用：仅在请求变化或忽略时间过期时刷新行为请求；`pass`/`dribble` 会改变后续踢向搜索基准。

### A.3 障碍修正与“争球对手”选择
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:418`，函数：`shiftObstacleBackward`。作用：当障碍与球重叠或过近时，将障碍“后移”，避免误判在球前。
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:436`，函数：`calculateClosestObstacle`。作用：选最近障碍作为 `duelObstacle`，并做后移修正，作为争球关键对象。

### A.4 传球请求的临时忽略
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:458`，函数：`handleIgnoreSkillRequest`。作用：当传球方向不在允许扇区、对手距离近且机器人未对准传球方向时，暂时把 `pass` 转为 `shoot`。

### A.5 扇区轮构建（障碍扇区 + 球门扇区）
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:478`，函数：`calculateSectorWheel`。作用：将障碍与球门转化为“角度扇区”，并做障碍扇区合并与平滑；用于后续 `maxKickRange` 与 `isGoalAngle`。

### A.6 评分主逻辑：`getDuelRating`
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:686`，函数：`getDuelRating`。作用：对某一踢向/踢法进行完整评分。
- 8.x 的含义：这是作者在 `getDuelRating` 内部的注释编号；“8”对应 `calculateDuel` 的第 8 步（对每个方向、每种踢法进行评分），而 “8.4.*”表示该评分步骤的子流程。
- 评分子流程（按 8.4.* 注释顺序）：
  - 8.4.1：前踢与转踢插值，保证在不同角度仍可生成可执行姿态。
  - 8.4.2：steal 类型基础加分，弱化部分惩罚。
  - 8.4.6：生成踢球姿态（`pose`），作为碰撞/旋转判断基础。
  - 8.4.7：旋转过大淘汰，但若踢向落入 `passRangeOne/Two` 则允许作为传球。
  - 8.4.8：forwardSteal 站位侧检查，防止站在“错误侧”导致踢法失效。
  - 8.4.3：在评分缓存中挑选“最佳踢程”，并决定是否标记为 `pass`。
  - 8.4.9：若属于球门扇区且距离可达，则标记为 `goalShot/goalDribbleShot` 并加分。
  - 8.4.10/8.4.11：禁止踢回己方禁区或己方半场深处。
  - 8.4.12：forwardSteal 结合边线距离与对手角度决定“好侧奖励/贴边惩罚”。
  - 8.4.13：脚边缘碰撞检查，必要时禁用预步并加惩罚。
  - 8.4.14：估算到位时间，若对手更快则加惩罚。
  - 8.4.15：同踢法滞回奖励，防止频繁切换。

### A.7 调试绘制：`drawRating`
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1033`，函数：`drawRating`。作用：把候选落点绘制成热力块，用于调试评分分布。

### A.8 精度裁剪：`calculateSectorUntilFieldBorder`
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1088`，函数：`calculateSectorUntilFieldBorder`。作用：将踢向精度范围裁剪到安全场地边界内，避免出界方向。

### A.9 主流程：`calculateDuel`
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1279`，函数：`calculateDuel`。
- 主要流程：
  1) 更新计时与“无踢法”状态，决定是否处于强制失活。
  2) 计算对手/自身到球距离差，用于 forwardSteal/sidewards 触发阈值。
  3) 处理 sidewards 受限条件（必须站在球与己方球门之间时禁止侧踢）。
  4) 计算 forwardSteal 的角度锥与方向插值范围。
  5) 生成方向候选（局部 + 全局 + forwardSteal + sidewards + pass）。
  6) 执行 `calculateSectorWheel` 得到扇区。
  7) 对每个候选角度、每种踢法调用 `getDuelRating`，按优先级选最优。
  8) 若选择 goalShot，可能强制切换为 forwardLong；随后计算踢球精度。
  9) 若长时间无踢法，进入“占位阻挡”。
  10) 对 forwardSteal 做短时滞回，避免感知抖动。

### A.10 球位置插值：`calculateUseBallPosition`
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1839`，函数：`calculateUseBallPosition`。作用：在近球场景融合当前感知位置与拦截终点，降低噪声带来的踢向波动，并裁剪到场地边界。

### A.11 初始化：`reset`
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1862`，函数：`reset`。作用：初始化 `lastFieldEndPoint` 与 `kickAngle`，并生成 `checkKickDistancesForFR` 供评分缓存使用。

### A.12 执行阶段：`initial_state(execute)`
- 位置：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:1909`，状态：`initial_state(execute)`。
- 作用：每帧更新球位置与最近障碍，调用 `calculateDuel` 生成动作；若有踢法则执行 `GoToBallAndKick`，否则 `WalkToPoint` 占位并持续观察。

---