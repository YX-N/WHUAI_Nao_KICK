# 争球策略及团队协作
本文围绕 **争球（Duel/Contest）** 与 **争球时的团队配合**，给出详细说明。内容以 `BehaviorControl` 与 `StrategyBehaviorControl` 相关代码为主，并包含定位路径、函数名与具有区分度的代码片段，便于查找时快速定位。本文覆盖的逻辑链条完整贯穿：争球触发 → 争球评分 → 踢法执行 → 团队角色分配 → 传接球协同。


**说明**：为便于阅读与核对，本文采用了行号注释与函数名注释混合的标记，并在必要时列出了可供查找的关键词。行号以本地文档为准。

---

## 目录

- 概要
1. 争球入口与近战状态机（PlayBall）
2. Zweikampf，争球对象
3. 争球动作空间：方向采样与扇区构建
4. forwardSteal / sidewards（争球踢法策略）
5. getDuelRating：争球评分的完整路径
6. 争球执行（踢球 or 走位）
7. 障碍物避让
8. 近距离控球
9. 团队协作：谁去抢球（TTRB）
10. 小结
附录 A：Zweikampf.cpp 行号注释
附录 B：关键引用文件列表

---

## 概要

本文档旨在分析“跟对手抢球”时机器人的动作和队友配合，通俗地概括就是：先判断“球够近、对手也靠近、身体朝向能动”才进入近战；把最近的对手当作主要威胁，修正可能的感知误差；生成若干可踢方向，既考虑正面抢、侧踢也考虑传球，按“能射门 > 能断球 > 能传球 > 其他”去打分；如果能踢就执行合适力度的踢球，不能踢就贴身干扰并绕开障碍；同时融合队友的球感和角色分配，选择谁去抢球、谁去支援，并把传球意图传到底层动作，形成完整的抢球协同。

- **入口与争球对象**：`PlayBall::activateDuel/stopDuel` 以“球近 + 障碍近 + 朝向可执行”判定进入/退出；`calculateClosestObstacle` 选最近障碍为对手，`shiftObstacleBackward` 纠正误感知，并标记近距/障碍墙以决定是否启用激进 forwardSteal。
- **动作空间与踢法**：`calculateDuel` 先局部再全局采样方向，显式补充 forwardSteal/sidewards/传球方向；sidewards/forwardSteal 受对手距离、角度锥、站位侧与禁踢角限制。
- **评分链路**：`calculateSectorWheel` 生成扇区 → `getDuelRating` 按 Goal > Steal > Pass > Other 评分，包含旋转/踢程/禁区/落脚点/到位时间等过滤，并用 `kickForcedUpTime` 防抖 100ms。
- **执行与近距控球**：`initial_state(execute)` 每帧插值并裁剪球位，若有踢法则 `GoToBallAndKick`（goalShot 用最强版本），否则 `WalkToPoint` 占位压迫；远距用 `PathPlanner.plan`，近距统一用 `LibWalk.calcObstacleAvoidance(toBall=true)` 精细绕障。
- **团队协作**：`updateAgentByTeamMessage` 融合并过滤分歧球感 (`disagreeOnBall`)，`calcTTRB` 结合距离与惩罚/奖励、守门员/罚时过滤选出 `ActiveRole::playBall`，传球意图由 `passTarget` 下推到技能层参与踢向搜索与评分。

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

片段（`activateDuel`，`PlayBall.cpp:17`）：  
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

片段（`stopDuel`，`PlayBall.cpp:55`）：  
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

## 2. Zweikampf，争球对象

### 2.1 概览
**文件**：`Zweikampf.cpp`
- `DuelPose`：存放“要执行的踢法”的所有结果（角度、踢法、精度、评分）。
- `RatingMap` / `RatingMapVector`：把不同踢程映射到“场地评分”，避免重复计算。
- `TargetType`：`goalShot > goalDribbleShot > stealBall > pass > other`，用于 **类型优先级**。

### 2.2 争球对象与障碍修正
**定位**：`shiftObstacleBackward()`（L418-L431）、`calculateClosestObstacle()`（L436-L453），相关状态变量在 `calculateDuel` 开头（L1292 起）。  
**细节**：  
- **最近障碍即争球对手**：遍历 `ObstacleModel` 选欧氏距离最近者为 `duelObstacle`，保留其后移版本 `duelObstacleMovedNearBall` 用于脚边阻挡判定。  
- **误感知纠正**：`shiftObstacleBackward` 会把“球前但离球过近”的障碍沿球径向后移到球后，避免误判导致直踢对手脚（核心在 x 坐标与球半径的双阈值判断）。  
- **阻挡半径放大**：若最近障碍本身就是 `duelObstacle`，在扇区生成前会放大距球距离（最小 300mm）以免因数厘米误差导致角度抖动。  
- **近距危险标记**：`soonCloseRangeDuel`（L1299）在“障碍距球小于 `maxObstacleDistanceToBallForRiskyKicks`”时置真，后续允许更激进的 forwardSteal。  
- **障碍墙识别**：扇区轮计算后检查 `duelObstacle` 是否形成“球后障碍墙”（L1459-L1472），若是则 forwardSteal 过滤放宽，认为对手确实挡在球后。  

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

片段（sidewards 过滤，`Zweikampf.cpp:1643`）：  
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

片段（forwardSteal 过滤，`Zweikampf.cpp:1649`）：  
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

片段（forwardSteal 加分，`Zweikampf.cpp:876`）：  
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

片段（旋转限制 + 传球例外，`Zweikampf.cpp:769`）：  
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

片段（选择最佳踢程，`Zweikampf.cpp:807`）：  
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

片段（GoalShot 加分与类型判定，`Zweikampf.cpp:826`）：  
```cpp
if(isGoalAngle && rangeForGoal < dribbleRange + useKickRange.max + (theDuelPose.type == TargetType::goalShot ? rangeHysteresis : 0.f))
{
  pose.rating += duelRatings.ratingGoalShot;
  pose.type = rangeForGoal > useKickRange.max + (theDuelPose.type == TargetType::goalShot ? rangeHysteresis : 0.f) ? TargetType::goalDribbleShot : TargetType::goalShot;
}
```

这些步骤共同决定“Goal/Steal/Pass/Other”的最终选择。

**补充一部分细节（一些关键判断点）**：
- **传球例外**：当旋转过大时，如果踢向角度落入 `passRangeOne/passRangeTwo`，会保留为“可传球角度”（关键词：`tooMuchRotationForNonePass`、`passRangeOne`）。  
- **Pass 类型标记**：评分时若 `ratingMapVector.ratingMap[bestIndex].isTeammatePass` 为真，则 `pose.type` 变为 `TargetType::pass`（关键词：`isTeammatePass`）。  
- **Goal 类型标记**：满足射门距离与角度时，直接设置 `goalShot/goalDribbleShot` 并加 `ratingGoalShot`（关键词：`ratingGoalShot`、`goalDribbleShot`）。  
- **优先级保持**：`kickForcedUpTime` 对高优先级动作保持 100ms，避免频繁抖动（搜索关键词：`kickForcedUpTime`）。

---

更完整的逐段行号说明见 **附录 A**（`Zweikampf.cpp` 逐段详解）。

## 6. 争球执行（踢球 or 走位）

**执行主线**：`initial_state(execute)`（L1909-L1969）。  
- 每帧先 `calculateUseBallPosition()` → `calculateClosestObstacle()`，并在球速度为零且距离 < 250mm 时启用速度插值，>500mm 关闭插值。  
- `reset()` 只在进入选项或 `option_time` 重置时运行：刷新球/拦截点、更新 `SkillRequest`、设置 `lastFieldEndPoint` 与预计算踢程。  
- 若被标记为 `forcedInactive`（长时间无踢法且球在对方半场），则直接 `DribbleToGoal()`，直到失活时间窗结束。  
- 选中踢法后调用 `GoToBallAndKick`：goalShot 强制将 `length` 设为 10000mm（用最强版本），并带上 `preStepAllowed/turnKickAllowed/shiftTurnKickPose` 与 `directionPrecision`，保持与评分一致。  
- 若无可踢法：`WalkToPoint` 以 `rough=true`、`disableStanding=true` 占位压迫，并 `LookActive` 保持抬头跟球。  
- 帧尾更新 `forcedInactive` 标记（超时且在对方半场），为下一帧决定是否进入压迫式失活。  

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
- **球位插值与裁剪**：`calculateUseBallPosition()`（L1839-L1857）会在“球近且速度为零”时混合当前感知与拦截终点，降低定位噪声；并用 `ballClipX/ballClipY` 把球位置限制在场地边界内，避免评分跑出界。  
- **执行方式**：  
  - `GoToBallAndDribble`（`GoToBallAndDribble.cpp:19`）与 `GoToBallAndKick`（`GoToBallAndKick.cpp:24`）都用 `switchToPathPlannerDistance` 划分远/近：远距 → `PathPlanner.plan` 规划全局路径；近距 → `LibWalk.calcObstacleAvoidance(toBall=true)` 局部绕障并精细步态。  
  - 近距阶段传入 `toBall=true`，会收紧脚步与安全距离，优先稳定贴球与避障。  
- **失活与占位**：若长时间无踢法且球在对方半场，会进入 `forcedInactive`，仍以 Dribble/Walk 占位压迫，防止对手从容出球。  

---

## 9. 团队协作：谁去抢球（TTRB）

### 9.1 队友消息融合
**文件**：`StrategyBehaviorControl.cpp`，函数：`updateAgentByTeamMessage`（约 L206）。  
**核心流程**：  
- 合并队友 `BehaviorStatus/StrategyStatus/BallModel`，填充 `agent.lastKnownTarget/Speed/ballPosition` 等字段。  
- 计算 `disagreeOnBall`（约 L245）：对比本机与队友的 `TeamBallModel`，基于距离动态阈值（`mapToRange` + `baseThreshold`）判定是否“球感知严重分歧”，分歧则标记该队友不可用。  
- 跳过失效或超时队友：若消息时间戳过旧、机器人被罚、或球丢失时间过长，会直接过滤，避免用过期信息。  
- 同步 `BehaviorStatus.passTarget/passOrigin`、`StrategyStatus.role` 等共享状态，为后续抢球/传球决策提供统一视图。

### 9.2 抢球者选择
**文件**：`Behavior.cpp`，函数：`calcTTRB`（约 L1011）与角色分配逻辑（L1111-L1131）。  
**流程细化**：  
- `calcTTRB` = 距离/速度估计 + 惩罚/奖励项：刚担任主动球员会有惩罚（防止刷角色），球刚丢失会有惩罚，若当前已起身或未倒地则奖励。守门员与罚时机器人按 `canBeActive` 过滤。  
- 本机先算 `minTTRB`，再遍历队友 `itsTTRB`（跳过 `disagreeOnBall`、倒地、罚时、守门员特殊约束）。  
- 选最小者设为 `ActiveRole::playBall`；若对方任意球则改为 `freeKickWall`。其余机器人根据策略保持或切换辅助角色。  
- 球不可见但 `TeamBallModel` 有效时，退化为“谁离 TeamBall 最近”分配（`closestToTeamBall`），保证有人去压迫。  
- 传球与接球协同：`BehaviorStatus.passTarget` 从策略层同步到技能层，配合 `updateSkillRequest` 影响 `Zweikampf` 的方向搜索与踢法评分。

---

## 10. 小结

- **争球入口**：`PlayBall::activateDuel/stopDuel` 以“球近 + 障碍近 + 朝向可执行”判定进入/退出近战。
- **争球对象**：`calculateClosestObstacle` 选最近障碍为对手，并用 `shiftObstacleBackward` 纠正感知；近距/障碍墙标记决定是否启用激进 forwardSteal。
- **评分链路**：`calculateDuel` 生成方向候选 → `calculateSectorWheel` 得扇区 → `getDuelRating` 按 Goal > Steal > Pass > Other 评分，包含旋转/踢程/禁区/落脚点/到位时间等多重过滤。
- **执行与近距**：选到踢法即 `GoToBallAndKick`（goalShot 用最强），否则 `WalkToPoint` 占位；`calculateUseBallPosition` 在球近时插值与裁剪，近距动作统一走 `LibWalk.calcObstacleAvoidance(toBall=true)`。
- **团队协作**：`updateAgentByTeamMessage` 融合队友状态并过滤分歧球感；`calcTTRB` 结合距离、惩罚/奖励、守门员/罚时过滤，选出 `ActiveRole::playBall`，传球请求通过 `BehaviorStatus.passTarget` 下推到技能层。

---

## 附录 A. Zweikampf.cpp 行号注释

说明：按 `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp` 当前版本行号整理，格式与示例一致。

### 参数与关键结构
- L32-L44 `SearchParameters`：方向采样密度/范围、射门缓冲角、局部/全局搜索设置。
- L46-L64 `DuelRatings`：评分加减项系数，决定 Goal/Steal/Pass 倾向。
- L66-L74 `DuelTimings`：失活/滞回时间窗口，防抖与限制侧踢。
- L76-L93 `ObstacleHandling`：脚边缘/踢后脚边缘、安全距离、障碍后移和 forwardSteal 触发距离。
- L95-L104 `StealBallParameters`：sidewards/forwardSteal 角度锥、转身阈值与偏好。
- L106-L110 `LastKickHysteresis`：同踢法/方向滞回阈值。
- L116-L123 `TargetType`：goalShot > goalDribbleShot > stealBall > pass > other 的优先级枚举。
- L125-L139 `DuelPose`：单个候选踢法的评分、姿态、踢法类型与落点记录。
- L157-L207 `RatingMap`：按踢程缓存潜力场评分（含队友传球判定）。
- L213-L235 `RatingMapVector`：封装 `RatingMap` 列表与调用接口。

### 行为请求与踢向更新
- L325-L333 `updateDuelPose()`：基于里程计校正 `kickAngle`，保持踢向连续。
- L338-L366 `updatePassEndPosition()`：确认 `passTarget`，生成 `passRangeOne/Two` 的允许传球角。
- L372-L412 `updateSkillRequest()`：处理技能请求刷新/忽略，更新 pass/dribble 方向并回写 `kickAngle`。

### 障碍修正与争球对象
- L418-L431 `shiftObstacleBackward()`：将贴球但不在前方的障碍后移，避免误判。
- L436-L453 `calculateClosestObstacle()`：选最近障碍为 `duelObstacle`，并应用后移修正。
- L458-L473 `handleIgnoreSkillRequest()`：传球角受限且对手近时临时改为射门。

### 扇区与扇区轮
- L478-L680 `calculateSectorWheel()`：生成障碍/球门扇区，计算 `goalAngleWithBuffer` 供评分与可视化。

### 评分主逻辑
- L686-L1028 `getDuelRating()`：逐踢法评分，涵盖旋转/踢程裁剪、Goal/Steal/Pass 类型判定、禁区/半场约束、脚边阻挡/预步、到位时间与同踢法滞回；`8.x` 编号对应 `calculateDuel` 的第 8 步，`8.4.*` 为评分子流程：
  - 8.4.1：前踢/转踢插值，保证不同角度的可执行性。
  - 8.4.2：steal 类型基础加分，弱化部分惩罚。
  - 8.4.6：生成踢球姿态（`pose`），作为碰撞与旋转判断基础。
  - 8.4.7：旋转过大淘汰；若落入 `passRangeOne/Two` 则允许作为传球。
  - 8.4.8：forwardSteal 站位侧检查，防止站错侧。
  - 8.4.3：在缓存中选“最佳踢程”，并决定是否标记为 `pass`。
  - 8.4.9：GoalShot 评分与类型判断。
  - 8.4.10/8.4.11：禁止踢回己方禁区或己方半场深处。
  - 8.4.12：forwardSteal 边线惩罚/好侧奖励。
  - 8.4.13：落脚点阻挡检查，必要时禁用预步。
  - 8.4.14：到位时间惩罚，若对手更快则扣分。
  - 8.4.15：同踢法滞回奖励，防止频繁切换。

### 评分可视化与精度裁剪
- L1033-L1083 `drawRating()`：调试绘制评分热力块。
- L1088-L1267 `calculateSectorUntilFieldBorder()`：按场地边界裁剪踢向精度范围，避免出界/回传禁区。

### 主流程：踢向搜索与选择
- L1279-L1834 `calculateDuel()`：
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
  11) 设置踢球精度/强制 forwardLong、必要时进入占位压迫。  
- L1839-L1857 `calculateUseBallPosition()`：融合球速/拦截点并裁剪到场地边界。
- L1862-L1907 `reset()`：初始化踢向、拦截位置与预计算踢程。
- L1909-L1969 `initial_state(execute)`：每帧更新球/障碍，若有踢法则 `GoToBallAndKick`，否则 `WalkToPoint` 占位并根据超时切换失活状态。

---
## 附录 B. 关键引用文件列表（争球 + 团队协作）
### B.1 BehaviorControl（技能层）
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/PlayBall.cpp`：争球入口/退出与状态切换。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp`：争球评分、方向采样、踢法选择与执行。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndKick.cpp`：近战踢球执行与远/近距避障。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndDribble.cpp`：近战带球执行与避障。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Walk/WalkToPointObstacle.cpp`：占位绕障处理。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/SkillBehaviorControl.cpp`：技能层入口、状态发布与接球触发。

### B.2 StrategyBehaviorControl（策略层）
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/StrategyBehaviorControl.cpp`：Agent 列表与 TeamMessage 融合、球一致性判断。
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/Behavior.cpp`：set play 记忆、角色分配、TTRB 计算。
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/ActiveRoles/PlayBall.cpp`：射门/传球/带球/解围评分与输出。
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/ActiveRoles/ClosestToTeamBall.cpp`：无球观察。

### B.3 Representations 与 Communication
- `WHUAI_Nao/Src/Representations/BehaviorControl/SkillRequest.h`：策略到技能的请求载体。
- `WHUAI_Nao/Src/Representations/BehaviorControl/BehaviorStatus.h`：passTarget/passOrigin 等共享字段。
- `WHUAI_Nao/Src/Representations/BehaviorControl/StrategyStatus.h`：role/position/setPlay 状态。
- `WHUAI_Nao/Src/Representations/Communication/ReceivedTeamMessages.h`：TeamMessage 载荷结构。
