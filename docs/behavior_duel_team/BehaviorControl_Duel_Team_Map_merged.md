# 行为控制：争球与团队协作（融合版：层次 + 细节，优化源码呈现）

本稿融合原稿的细节密度与新版的层次/可读性：先用术语速查和主线把“策略 → 技能 → 争球入口 → 评分 → 执行 → 协作”走通，再补系统/策略总览与阶段走读；源码仅在解释更清晰处内联，先给出文件与大致位置，并说明为何在此放片段，避免“为了放源码而放”。关键文件列表移至附录 A，`Zweikampf.cpp` 逐段行号走读（函数名使用代码格式并分层）在附录 B，通信索引在附录 C。

## 目录
1. 概览与范围
2. 术语速查：forwardSteal / sidewards / getDuelRating
3. 行为主线（策略 → 技能 → 争球入口）
4. 系统结构总览（融合）
5. 策略总览（融合）
6. 争球执行链路与协作机制（融合）
7. 争球评分与踢法选择（Zweikampf）
8. 执行阶段：接近、避障与落地
9. 团队协作：球一致性、角色、传接/清球
10. 流程小结
11. 附录 A. 关键文件列表（争球 + 团队协作完整覆盖）
12. 附录 B. `Zweikampf.cpp` 逐段详解（行号注释）
13. 附录 C. 团队协作与通信共享变量索引

---

## 1. 概览与范围
- 目标：掌握与对手争球时的动作细节，以及团队如何在争球时协作（角色分配、传接/清球）。
- 范围：`StrategyBehaviorControl`（Agent/角色/TTRB/消息）、`SkillBehaviorControl`（PlayBall/ReceivePass）、`Zweikampf`（评分/踢法）、`GoToBallAndKick|Dribble`/`WalkToPointObstacle`（执行/避障）。
- 深入：附录 A（关键文件全表）、附录 B（Zweikampf 行号走读）、附录 C（通信字段）。

## 2. 术语速查：forwardSteal / sidewards / getDuelRating
- `getDuelRating`：Zweikampf 的评分核心（`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:686-1027`）。针对“某个踢向 + 踢法”计算评分与类型（Goal/Steal/Pass/Other），并可能直接淘汰。
- `forwardSteal`：正向抢断类 InWalkKick（`walkForwardStealBallLeft/Right`），要求对手更接近球且处于球后方角锥；允许更大旋转，快速出脚抢球。
- `sidewards`：侧向挡/抢（`walkSidewards*`），常用于“必须站在球与己方门之间/对手更近”场景，用窄锥把球拨向安全侧。

## 3. 行为主线（策略 → 技能 → 争球入口）
- 策略入口：`StrategyBehaviorControl::update` 聚合 TeamMessage，调用 `Behavior::update` 输出 `SkillRequest`。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/StrategyBehaviorControl.cpp:L29-L259`。
- 抢球者选择：`Behavior::calcTTRB` + `canBeActive` 决定谁去抢球（`ActiveRole::playBall`），`closestToTeamBall` 兜底。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/Behavior.cpp:L1008-L1181`。
- 动作选择：`ActiveRoles/PlayBall::smashOrPass` 在射门/传球/带球/解围间输出请求。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/ActiveRoles/PlayBall.cpp:L35-L132`。
- 技能桥接：`SkillBehaviorControl::executeRequest` 读取请求；若自己是 `passTarget`，直接 `ReceivePass`，否则进入 `PlayBall`。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/SkillBehaviorControl.cpp:L131-L179`。
- 争球入口：`PlayBall::activateDuel` 检查“球近 + 障碍近 + 朝向允许”触发，`stopDuel` 解除。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/PlayBall.cpp:L17-L139`。

片段（文件：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/PlayBall.cpp`，位置：`L17-L53`；放在这里是因为解释“何时切入近战”最直接）：
```cpp
const bool obstacleClose = distanceToClosestObstacle < (duelMinDistanceToClosest * (1.f - (std::abs(angleToClosestObstacle) / duelMinAngleToClosest / 3.f)));
const bool obstacleAngleClose = smallestAngleToCloseObstacle <= duelMinAngleToClosest;
const bool ballSeen = theFieldBall.ballWasSeen(300);
const bool ballPosXClose = between<float>(theFieldBall.positionRelative.x(), 0.f, 700.f);
const bool ballPosYClose = std::abs(theFieldBall.positionRelative.y()) < 500.f;
const bool rotationToBallOk = std::abs(theFieldBall.positionRelative.angle()) < 80_deg;
return obstacleClose && obstacleAngleClose && ballSeen && ballPosXClose && ballPosYClose && rotationToBallOk;
```

---

## 4. 系统结构总览（融合）
- 技能层入口：`SkillBehaviorControl::update` 设置状态并执行根选项，驱动技能树。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/SkillBehaviorControl.cpp:L21-L127`。
- 策略层入口：`StrategyBehaviorControl::update` 更新 Agent，调用 `Behavior::update` 生成 `SkillRequest` 并同步 `StrategyStatus`。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/StrategyBehaviorControl.cpp:L29-L69`、`.../Behavior.cpp:L248-L460`。
- 团队消息融合：`updateAgents`/`updateAgentByTeamMessage` 使用 `ReceivedTeamMessages` 更新队友与球信息，并计算 `disagreeOnBall`。文件：`StrategyBehaviorControl/StrategyBehaviorControl.cpp:L74-L259`。
- 执行桥梁：`SkillRequest` 连接策略与技能。文件：`WHUAI_Nao/Src/Representations/BehaviorControl/SkillRequest.h:L15-L55`。
- 共享变量：`BehaviorStatus` 和 `StrategyStatus` 通过 TeamMessage 传播。文件：`.../BehaviorStatus.h:L17-L25`、`.../StrategyStatus.h:L18-L30`。

## 5. 策略总览（融合）
- 角色分配与“谁去抢球”：`Behavior::calcTTRB` 与 `canBeActive` 决定 `ActiveRole::playBall`，结合 `closestToTeamBall` 处理“看不到球”的协作场景。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/Behavior.cpp:L1008-L1205`。
- 行为决策：`ActiveRoles/PlayBall::smashOrPass` 根据射门/传球/带球/解围评分输出 `SkillRequest`，含切换惩罚（防抖）。文件：`.../ActiveRoles/PlayBall.cpp:L35-L133`。
- 技能执行：`SkillBehaviorControl::executeRequest` 接收请求，进入 `PlayBall`，再由 `PlayBall` 判断是否进入 `Zweikampf`。文件：`SkillBehaviorControl/SkillBehaviorControl.cpp:L131-L179`、`Skills/Ball/PlayBall.cpp:L77-L139`。

片段（文件：`WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/ActiveRoles/PlayBall.cpp`，位置：`L35-L72`；这里放出以直观看到“射门优先 + 切换惩罚”的入口条件）：
```cpp
float bestActionRating = !theIndirectKick.allowDirectKick ? p.minRating : theExpectedGoals.getRating(ballPosition, false);
if(lastPassTarget > 0)
  bestActionRating -= decisionPenalty; // 防抖：切换惩罚
if(bestActionRating > p.shootThreshold && theIndirectKick.allowDirectKick)
  return SkillRequest::Builder::shoot();
```

---

## 6. 争球执行链路与协作机制（融合）
### 6.1 争球评分与踢向选择（`getDuelRating` 入口）
- 概述：`calculateDuel` 构造方向集合与扇区轮，`getDuelRating` 评估并选出最终踢向。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp:L1279-L1702`。
- 评分边界：旋转限制、踢程选择、Goal/Pass/Steal 判定影响评分路径。文件：同文件 `L686-L1027`。
- 放片段的原因：这里的“旋转限制 + 传球例外”是理解评分淘汰/保留的关键（片段见第 7 节）。

### 6.2 执行阶段：避障与近距离控球
- 远距避障：`PathPlanner.plan` 先全局绕障安全接近。文件：`Skills/Ball/GoToBallAndKick.cpp:L35-L76`、`.../GoToBallAndDribble.cpp:L30-L49`。
- 近距控球：`LibWalk.calcObstacleAvoidance` 进行贴球微调与避障。文件：`GoToBallAndKick.cpp:L79-L120`、`GoToBallAndDribble.cpp:L52-L71`。
- 站位阻挡：若无可执行踢法，退化为 `WalkToPoint`，目标点被占用时由 `WalkToPointObstacle` 偏移。文件：`Zweikampf.cpp:L1909-L1969`、`Skills/Walk/WalkToPointObstacle.cpp:L61-L150`。

### 6.3 团队协作：球一致性与角色分配
- 球信息融合：`updateAgentByTeamMessage` 汇总队友 BallModel 计算 `disagreeOnBall`，作为活跃角色过滤条件。文件：`StrategyBehaviorControl.cpp:L206-L259`。
- 角色协作：`calcTTRB` 与 `canBeActive` 决定 `ActiveRole::playBall`，并结合 `closestToTeamBall`。文件：`Behavior.cpp:L1008-L1205`。
- 定位统一：队友消息携带 `RobotPose/BallModel`，坐标统一后判断球一致性。文件：`.../ReceivedTeamMessages.h:L21-L34`、`StrategyBehaviorControl.cpp:L206-L245`。

### 6.4 团队协作：传接球与清球方向
- 传接球：`BehaviorStatus.passTarget` 与 `ReceivePass` 配合完成接力（片段见第 9 节）。
- 清球方向：`ClearTargetProvider` 比较队友与对手距离，避免把球清给对手或制造争球。文件：`ActionRatingProvider/ClearTargetProvider.cpp:L69-L126`。

---

## 7. 争球评分与踢法选择（Zweikampf）
### 7.1 主流程（`calculateDuel` → `getDuelRating`）
- 方向采样：围绕上次踢向做局部 + 全局搜索，并显式加入 pass/sidewards/forwardSteal 角度。`Zweikampf.cpp:L1396-L1455`。
- 扇区构建：`calculateSectorWheel` 将障碍/球门转为扇区，给出 `maxKickRange` 与 `isGoalAngle`。`Zweikampf.cpp:L478-L680`。
- 评分：对每个“踢向 + 踢法”执行 `getDuelRating`，完成旋转限制、踢程筛选、Goal/Steal/Pass 判定与评分。`Zweikampf.cpp:L686-L1027`。
- 选择：按类型优先级 Goal > Steal > Pass > Other 选最优，并 100ms 防抖。`Zweikampf.cpp:L1672-L1703`。
- 精度：`calculateSectorUntilFieldBorder` 将精度裁剪在安全边界内。`Zweikampf.cpp:L1762-L1800`。
- 兜底：无踢法退化为 `WalkToPoint` 站位防守。`Zweikampf.cpp:L1804-L1834`。

片段（文件：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp`，位置：`L769-L787`；放在这里是因为“旋转限制 + 传球例外”是保留/淘汰关键）：
```cpp
if(!maxAllowedRotationRangeOne.isInside(useRotationOfInterest) && !maxAllowedRotationRangeTwo.isInside(useRotationOfInterest) &&
   !(isGoalAngle && rangeForGoal < dribbleRange + useKickRange.max + ((theDuelPose.type == TargetType::goalShot || theDuelPose.type == TargetType::goalDribbleShot) ? rangeHysteresis : 0.f)) &&
   !(kickType == KickInfo::walkForwardStealBallLeft || kickType == KickInfo::walkForwardStealBallRight))
{
  if(theSkillRequestPose.passTarget != -1 &&
     (theSkillRequestPose.passRangeOne.isInside(kickAngle) || theSkillRequestPose.passRangeTwo.isInside(kickAngle)))
    tooMuchRotationForNonePass = true; // 允许作为传球角
  else
    return false; // 非传球且转身过大直接淘汰
}
```

### 7.2 forwardSteal / sidewards 启停逻辑
- sidewards：仅在“对手更近”或“必须挡在球与己门之间”时启用，且踢向需在允许锥内。角锥/限制：`Zweikampf.cpp:L1318-L1333`；过滤：`Zweikampf.cpp:L1643`。
- forwardSteal：要求对手处于球后方角锥、对手更近且踢向落在前向抢断窄锥。角锥：`Zweikampf.cpp:L1334-L1368`；过滤：`Zweikampf.cpp:L1649`。

片段（文件：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp`，位置：`L1640-L1655`；放在这里为了直观看到“何时直接跳过抢断/侧踢”）：
```cpp
// sidewards 过滤
if((kickType == KickInfo::walkSidewardsLeftFootToLeft || kickType == KickInfo::walkSidewardsRightFootToRight) &&
   ((opponentAndSelfDistanceToBallDiff < 100.f) ||
    !(forbiddingKickAngle.isInside(kickAngle) || forbiddingKickAngleExtra.isInside(kickAngle))))
  continue;

// forwardSteal 过滤
if((kickType == KickInfo::walkForwardStealBallLeft || kickType == KickInfo::walkForwardStealBallRight) &&
   (stealBallMin != stealBallMax ||
    (!leftAngleRangeBallToOpponent.isInside(angleFromBallToOpponent) && !obstacleWallBehindBall) ||
    !(rightForwardStealRange.isInside(Angle::normalize(kickAngle + theRobotPose.rotation)) ||
      leftForwardStealRange.isInside(Angle::normalize(kickAngle + theRobotPose.rotation))) ||
    opponentAndSelfDistanceToBallDiff > obstacleHandling.maxObstacleDistanceForWalkStealBallKick))
  continue;
```

---

## 8. 执行阶段：接近、避障与落地
- 有踢法：`GoToBallAndKick` 执行近战踢球（含精度/踢程/预步/转踢）。`Zweikampf.cpp:L1909-L1953`。
- 无踢法：`WalkToPoint` 占位阻挡，保持 `LookActive`。`Zweikampf.cpp:L1955-L1963`。

### 8.1 远距离接近 + 近距离控球
- 远距路径：`GoToBallAndKick.cpp:L35-L76`、`GoToBallAndDribble.cpp:L30-L49` 使用 `PathPlanner.plan` 绕障快速接近。
- 近距避障：两者在近距改用 `LibWalk.calcObstacleAvoidance`，参数 `toBall=true`，确保贴球安全。位置：`GoToBallAndKick.cpp:L79-L120`、`GoToBallAndDribble.cpp:L52-L71`。

### 8.2 目标点占用偏移
- 若占位点被障碍/对手堵住，`WalkToPointObstacle` 计算占位圆并偏移目标点，避免踩入对手脚下。入口：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Walk/WalkToPointObstacle.cpp:L61-L150`。

---

## 9. 团队协作：球一致性、角色、传接/清球
- 球一致性：`updateAgentByTeamMessage` 融合队友 `BallModel/RobotPose`，计算 `disagreeOnBall` 过滤异常。位置：`StrategyBehaviorControl.cpp:L206-L246`。
- 角色分配：`calcTTRB` + `canBeActive` 决定 `ActiveRole::playBall` / `closestToTeamBall`。位置：`Behavior.cpp:L1008-L1181`。
- 传接球：`BehaviorStatus.passTarget` 经 TeamMessage 共享；若自己是目标，技能层直接进入 `ReceivePass`。位置：`SkillBehaviorControl.cpp:L131-L179`。

片段（文件：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/SkillBehaviorControl.cpp`，位置：`L131-L179`；放在这里说明“如何自动切换到接球”）：
```cpp
for(const Teammate& teammate : theTeamData.teammates)
{
  if(teammate.theBehaviorStatus.passTarget == theGameState.playerNumber)
  {
    if(theFrameInfo.getTimeSince(teammate.theFrameInfo.time) < ignoreReceivePassAfterTime)
    {
      theBehaviorStatus.passOrigin = teammate.number;
      ReceivePass({.playerNumber = teammate.number});
      return;
    }
  }
}
```

---

## 10. 流程小结
- 策略层：融合队友消息 → 过滤球一致性 → 角色分配（TTRB） → 输出 `SkillRequest`。
- 技能层：按请求执行；当“球近 + 障碍近 + 朝向允许”时进入 `Zweikampf`。
- 评分：方向/扇区/潜力场综合打分；类型优先级 Goal > Steal > Pass > Other；旋转过大在非传球时淘汰。
- 执行：有踢法 → `GoToBallAndKick`（含精度）；无踢法 → `WalkToPoint` 防守；精度按场地边界裁剪，减少出界风险。
- 协作：`passTarget/passOrigin` + `ClearTargetProvider` 确保传接/清球与队友位置一致，不让对手轻易延续争球。

---

## 附录 A. 关键文件列表（争球 + 团队协作完整覆盖）
### A.1 BehaviorControl（技能层）
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/PlayBall.cpp`：近战入口/退出与状态切换。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp`：争球评分、方向采样、踢法选择与执行。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndKick.cpp`：近战踢球执行与远/近距避障。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndDribble.cpp`：近战带球执行与避障。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Walk/WalkToPointObstacle.cpp`：占位绕障处理。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/SkillBehaviorControl.cpp`：技能层入口、状态发布与接球触发。

### A.2 StrategyBehaviorControl（策略层）
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/StrategyBehaviorControl.cpp`：Agent/TeamMessage 融合、球一致性判断。
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/Behavior.cpp`：set play 记忆、角色分配、TTRB。
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/ActiveRoles/PlayBall.cpp`：射门/传球/带球/解围决策。
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/ActiveRoles/ClosestToTeamBall.cpp`：无球观察。

### A.3 Representations 与 Communication
- `WHUAI_Nao/Src/Representations/BehaviorControl/SkillRequest.h`：策略到技能的请求。
- `WHUAI_Nao/Src/Representations/BehaviorControl/BehaviorStatus.h`：passTarget/passOrigin 等共享字段。
- `WHUAI_Nao/Src/Representations/BehaviorControl/StrategyStatus.h`：role/position/setPlay 状态。
- `WHUAI_Nao/Src/Representations/Communication/ReceivedTeamMessages.h`：TeamMessage 载荷结构。

---

## 附录 B：`Zweikampf.cpp` 逐段详解（行号注释）
文件：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp`

### 参数与结构
- L32-L152 `SearchParameters` / `DuelRatings` / `DuelTimings`：采样密度、评分权重、时间窗口。
- L157-L245 `RatingMap` / `RatingMapVector` / `ObstacleSector`：评分缓存与扇区结构。
- L116 `enum TargetType`：类型优先级（`goalShot > goalDribbleShot > stealBall > pass > other`）。
- L125 `struct DuelPose`：评分、踢向、姿态、踢法、精度与落点。

### 行为请求与踢向更新
- L325-L333 `updateDuelPose()`：按里程计变化更新 `kickAngle`。
- L338-L366 `updatePassEndPosition()`：确认 `passTarget` 与 `passRangeOne/Two`。
- L372-L412 `updateSkillRequest()`：在 pass/dribble 时刷新踢向基准并保持一致性。

### 障碍修正与争球对象
- L418-L431 `shiftObstacleBackward()`：将贴球障碍后移，避免误判挡在球前。
- L436-L453 `calculateClosestObstacle()`：选最近障碍为 `duelObstacle` 并记录距离。

### 忽略传球请求的条件
- L458-L472 `handleIgnoreSkillRequest()`：传球角度不合法且对手逼近时暂时改为 `shoot`。

### 扇区轮构建
- L478-L680 `calculateSectorWheel()`：构造障碍/球门扇区，计算 `goalAngleWithBuffer`。

### 评分主流程（getDuelRating）
- L686-L1027 `getDuelRating(...)`：
  - 8.4.1 前踢/转踢插值；8.4.2 识别 steal；8.4.6 生成踢姿；
  - 8.4.7 旋转限制（传球角度例外）；8.4.8 forwardSteal 站位侧检查；
  - 8.4.3 选择踢程；8.4.9 Goal 判定与加分；8.4.10/11 自家半场/禁区保护；
  - 8.4.12 forwardSteal 偏侧奖励/贴边惩罚；8.4.13 落脚阻挡检查；8.4.14 到位时长惩罚；8.4.15 同踢法滞回奖励。

### 调试绘制
- L1033-L1083 `drawRating(...)`：在场地上绘制候选落点热区。

### 精度裁剪
- L1088-L1267 `calculateSectorUntilFieldBorder(...)`：按边界裁剪精度，防止出界/回传禁区。

### 主流程（calculateDuel）
- L1279-L1333 `calculateDuel()`：初始化计时与对手距离，处理 `forcedInactive`。
- L1334-L1371 `calculateDuel()`：forwardSteal 角锥与必须挡门判定。
- L1396-L1504 `calculateDuel()`：方向搜索（局部/全局/pass/sidewards/forwardSteal）。
- L1506-L1563 `calculateDuel()`：forwardSteal/sidewards 角锥调试绘制。
- L1570-L1670 `calculateDuel()`：踢法遍历 + `getDuelRating` 评分 + 目标类型优先级选择。
- L1672-L1801 `calculateDuel()`：最优踢法、精度估计、forward/long 替换与 buffer 处理。
- L1804-L1834 `calculateDuel()`：无踢法时站位兜底与 forwardSteal 滞回。

### 球位置插值 / 初始化 / 执行
- L1839-L1857 `calculateUseBallPosition()`：融合当前与预测球位并裁剪场地。
- L1862-L1907 `reset()`：重置踢向、预计算踢程列表并套用 `SkillRequest`。
- L1909-L1969 `initial_state(execute)`：更新球/障碍，调用 `calculateDuel`，执行踢球或站位。

---

## 附录 C. 团队协作与通信共享变量索引
- TeamMessage 载荷：`WHUAI_Nao/Src/Representations/Communication/ReceivedTeamMessages.h:L21-L34`（`RobotPose`、`BallModel`、`BehaviorStatus`、`StrategyStatus`）。
- 传接球字段：`WHUAI_Nao/Src/Representations/BehaviorControl/BehaviorStatus.h:L17-L25`（`passTarget`/`passOrigin`）。
- 角色同步：`WHUAI_Nao/Src/Representations/BehaviorControl/StrategyStatus.h:L18-L30`（`role`/`position`/`setPlay`）。
