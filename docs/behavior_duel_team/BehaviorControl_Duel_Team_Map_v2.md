# 行为控制：争球与团队协作（新版��理）

本稿聚焦“与对手争球”时的动作细节与“团队配合”链路，强调可读性：先解释 forwardSteal/sidewards/getDuelRating，再串起策略→技能→执行，再放自然嵌入的源码片段。旧版 10. 关键文件列表移至附录 A，`Zweikampf.cpp` 逐段详解放在附录 B（按行号格式），通信字段放在附录 C。

## 目录
1. 阅读指引与范围
2. 术语速查：forwardSteal / sidewards / getDuelRating
3. 行为链路总览（策略 → 技能 → 争球入口）
4. 争球评分与踢法选择（Zweikampf）
5. 执行阶段：接近、避障与落地
6. 团队协作：球一致性、角色分配、传接/清球
7. 流程小结
8. 附录 A. 关键文件列表（争球 + 团队协作完整覆盖）
9. 附录 B. `Zweikampf.cpp` 逐段详解（行号注释）
10. 附录 C. 团队协作与通信共享变量索引

---

## 1. 阅读指引与范围
- 目标：搞懂争球动作细节 + 团队配合如何让正确的机器人去抢球、如何配合传/清球。
- 范围：`StrategyBehaviorControl`（角色/消息）、`SkillBehaviorControl`（PlayBall 入口 + ReceivePass）、`Zweikampf`（评分/踢法）、`GoToBallAndKick|Dribble` + `WalkToPointObstacle`（执行/避障）。
- 深入：关键文件列表 → 附录 A；`Zweikampf.cpp` 行号走读 → 附录 B；通信字段 → 附录 C。

## 2. 术语速查：forwardSteal / sidewards / getDuelRating
- `getDuelRating`：`Zweikampf` 的评分核心（`Zweikampf.cpp:686-1027`）。输入“踢向 + 踢法”，输出评分与类型（Goal/Steal/Pass/Other），并决定是否淘汰。
- `forwardSteal`：正向抢断类 InWalkKick（枚举 `walkForwardStealBallLeft/Right`）。要求对手更接近球且在球后方角锥内，允许较大旋转与贴边加分，用来快速抢下球权。
- `sidewards`：侧向挡/抢（`walkSidewards*`）。当对手更近或必须站在球与己方门之间时启用，用窄角锥把球拨向安全侧，避免被穿门。

## 3. 行为链路总览（策略 → 技能 → 争球入口）
- 策略入口：`StrategyBehaviorControl::update` 聚合 TeamMessage 并调用 `Behavior::update` 输出 `SkillRequest`。文件：`WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/StrategyBehaviorControl.cpp:L29-L259`。
- 谁去抢球：`Behavior::calcTTRB` 结合球一致性/起身/视野，指派 `ActiveRole::playBall`，`closestToTeamBall` 做兜底。文件：`StrategyBehaviorControl/Behavior.cpp:L1008-L1181`。
- 动作选择：`ActiveRoles/PlayBall::smashOrPass` 在射门/传球/带球/解围间选择。文件：`StrategyBehaviorControl/ActiveRoles/PlayBall.cpp:L35-L132`。
- 技能层桥接：`SkillBehaviorControl::executeRequest` 读取 `SkillRequest`；若自己是 `passTarget` 直接切入 `ReceivePass`，否则进入 `PlayBall`。文件：`SkillBehaviorControl/SkillBehaviorControl.cpp:L131-L179`。

片段（`PlayBall.cpp:17-53`，为什么在这里放：直观看到“球近 + 障碍近 + 朝向允许”何时切入争球）：
```cpp
const bool obstacleClose = distanceToClosestObstacle < (duelMinDistanceToClosest * (1.f - (std::abs(angleToClosestObstacle) / duelMinAngleToClosest / 3.f)));
const bool obstacleAngleClose = smallestAngleToCloseObstacle <= duelMinAngleToClosest;
const bool ballSeen = theFieldBall.ballWasSeen(300);
const bool ballPosXClose = between<float>(theFieldBall.positionRelative.x(), 0.f, 700.f);
const bool ballPosYClose = std::abs(theFieldBall.positionRelative.y()) < 500.f;
const bool rotationToBallOk = std::abs(theFieldBall.positionRelative.angle()) < 80_deg;
return obstacleClose && obstacleAngleClose && ballSeen && ballPosXClose && ballPosYClose && rotationToBallOk;
```
- 退出争球：`PlayBall::stopDuel` 在球远或“近障碍消失”时回退到常规技能。文件：`PlayBall.cpp:L55-L72`。

## 4. 争球评分与踢法选择（Zweikampf）
### 4.1 评分主流程（calculateDuel → getDuelRating）
- 方向采样：围绕上次踢向做局部 + 全局搜索，并显式加入 pass/sidewards/forwardSteal 角度。入口：`Zweikampf.cpp:L1396-L1455`。
- 扇区构建：`calculateSectorWheel` 把障碍/球门转成扇区，决定 `maxKickRange` 与 `isGoalAngle`。入口：`Zweikampf.cpp:L478-L680`。
- 评分：对每个“踢向 + 踢法”执行 `getDuelRating`，完成旋转限制、踢程筛选、Goal/Steal/Pass 判定与评分加减。核心：`Zweikampf.cpp:L686-L1027`。
- 选择：按类型优先级 Goal > Steal > Pass > Other 取最优，保持 100ms 防抖（`kickForcedUpTime`）。入口：`Zweikampf.cpp:L1672-L1703`。
- 精度：对选中踢法裁剪精度到安全场地内（`calculateSectorUntilFieldBorder`），防止出界。入口：`Zweikampf.cpp:L1762-L1800`。
- 兜底：若无踢法，退化为站位挡线（`WalkToPoint`）防守。入口：`Zweikampf.cpp:L1804-L1834`。

片段（`Zweikampf.cpp:769-787`，放在这里强调“旋转限制 + 传球例外”是淘汰/保留的关键）：
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

### 4.2 forwardSteal / sidewards 启停逻辑
- sidewards：只在“对手更近”或“必须挡在球与己方门之间”时启用，且踢向必须落在允许角锥内。判定位置：`Zweikampf.cpp:L1318-L1344`（角锥）与过滤 `Zweikampf.cpp:L1643`。
- forwardSteal：要求对手在球后方角锥内、对手更近且踢向落在前向抢断窄锥。角锥计算：`Zweikampf.cpp:L1334-L1368`；过滤：`Zweikampf.cpp:L1649`。

片段（`Zweikampf.cpp:1640-1655`，放在这里展示“抢断踢法何时直接跳过”）：
```cpp
// sidewards：对手更接近球且踢向在允许锥内
if((kickType == KickInfo::walkSidewardsLeftFootToLeft || kickType == KickInfo::walkSidewardsRightFootToRight) &&
   ((opponentAndSelfDistanceToBallDiff < 100.f) ||
    !(forbiddingKickAngle.isInside(kickAngle) || forbiddingKickAngleExtra.isInside(kickAngle))))
  continue;

// forwardSteal：对手在球后方角锥内，且确实更接近球
if((kickType == KickInfo::walkForwardStealBallLeft || kickType == KickInfo::walkForwardStealBallRight) &&
   (stealBallMin != stealBallMax ||
    (!leftAngleRangeBallToOpponent.isInside(angleFromBallToOpponent) && !obstacleWallBehindBall) ||
    !(rightForwardStealRange.isInside(Angle::normalize(kickAngle + theRobotPose.rotation)) ||
      leftForwardStealRange.isInside(Angle::normalize(kickAngle + theRobotPose.rotation))) ||
    opponentAndSelfDistanceToBallDiff > obstacleHandling.maxObstacleDistanceForWalkStealBallKick))
  continue;
```

### 4.3 精度裁剪与防守兜底
- 精度裁剪：`calculateSectorUntilFieldBorder` 根据边线/禁区削减踢向精度，避免把球送出界或回传到己方禁区。入口：`Zweikampf.cpp:L1088-L1267`。
- 站位兜底：长时间无踢法时，`calculateDuel` 设置 `noKick` 并让机器人站在球与己方门之间，拖住对手。入口：`Zweikampf.cpp:L1804-L1834`。

## 5. 执行阶段：接近、避障与落地
- 有踢法：`GoToBallAndKick` 执行近战踢球，传入评分阶段确定的踢向/踢程/精度。入口：`Zweikampf.cpp:L1909-L1953`。
- 无踢法：`WalkToPoint` 占位阻挡，并保持头部关注球。入口：`Zweikampf.cpp:L1955-L1963`。

### 5.1 远距离接近 + 近距离控球
- 远距路径：`GoToBallAndKick.cpp:L35-L76`、`GoToBallAndDribble.cpp:L30-L49` 使用 `PathPlanner.plan` 绕障快速接近。
- 近距避障：两者在近距改用 `LibWalk.calcObstacleAvoidance`，参数 `toBall=true`，确保贴球安全。位置：`GoToBallAndKick.cpp:L79-L120`、`GoToBallAndDribble.cpp:L52-L71`。

### 5.2 目标点占用偏移
- 若占位点被障碍/对手堵住，`WalkToPointObstacle` 计算占位圆并偏移目标点，避免踩入对手脚下。入口：`WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Walk/WalkToPointObstacle.cpp:L61-L150`。

## 6. 团队协作：球一致性、角色分配、传接/清球
- 球一致性：`updateAgentByTeamMessage` 融合队友 `BallModel/RobotPose`，计算 `disagreeOnBall` 过滤错误球信息。位置：`StrategyBehaviorControl.cpp:L206-L246`。
- 角色分配：`calcTTRB` + `canBeActive` 结合球一致性/起身/守门员等条件，设置 `ActiveRole::playBall` 或 `closestToTeamBall`。位置：`Behavior.cpp:L1008-L1181`。
- 传接球：`BehaviorStatus.passTarget` 经 TeamMessage 共享；若自己是目标，技能层直接进入 `ReceivePass`。位置：`SkillBehaviorControl.cpp:L131-L179`。

片段（`SkillBehaviorControl.cpp:131-179`，放这里说明“队友传球时如何自动切换到接球”）：
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
- 清球方向：`ClearTargetProvider` 比较队友/对手距离，防止把球清到对手脚下或制造新争球。位置：`WHUAI_Nao/Src/Modules/BehaviorControl/ActionRatingProvider/ClearTargetProvider.cpp:L69-L126`。

## 7. 流程小结
- 策略层：融合队友消息 → 过滤球一致性 → TTRB 分配主动抢球者 → 输出 `SkillRequest`（射门/传球/带球/解围/观察）。
- 技能层：常规执行；当“球近 + 障碍近 + 朝向允许”时切入 `Zweikampf` 近战链路。
- 评分：`Zweikampf` 基于扇区与潜力场对“踢向 + 踢法”打分，强优先级顺序 `Goal > Steal > Pass > Other`，forwardSteal/sidewards 仅在对手威胁确凿时启用。
- 执行：有踢法 → `GoToBallAndKick`（含精度）；无踢法 → `WalkToPoint` 占位。精度按场地边界裁剪，降低出界/回传风险。
- 协作：`passTarget/passOrigin` 与 `ClearTargetProvider` 保证传接/清球与队友位置一致，不让对手轻易继续争球。

---

## 附录 A. 关键文件列表（争球 + 团队协作完整覆盖）
### A.1 BehaviorControl（技能层）
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/PlayBall.cpp`：争球入口/退出与状态切换。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp`：争球评分、方向采样、踢法选择与执行。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndKick.cpp`：近战踢球执行与远/近距避障。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/GoToBallAndDribble.cpp`：近战带球执行与避障。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Walk/WalkToPointObstacle.cpp`：占位绕障处理。
- `WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/SkillBehaviorControl.cpp`：技能层入口、状态发布与接球触发。

### A.2 StrategyBehaviorControl（策略层）
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/StrategyBehaviorControl.cpp`：Agent 列表与 TeamMessage 融合、球一致性判断。
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/Behavior.cpp`：set play 记忆、角色分配、TTRB 计算。
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/ActiveRoles/PlayBall.cpp`：射门/传球/带球/解围评分与输出。
- `WHUAI_Nao/Src/Modules/BehaviorControl/StrategyBehaviorControl/ActiveRoles/ClosestToTeamBall.cpp`：无球观察。

### A.3 Representations 与 Communication
- `WHUAI_Nao/Src/Representations/BehaviorControl/SkillRequest.h`：策略到技能的请求载体。
- `WHUAI_Nao/Src/Representations/BehaviorControl/BehaviorStatus.h`：passTarget/passOrigin 等共享字段。
- `WHUAI_Nao/Src/Representations/BehaviorControl/StrategyStatus.h`：role/position/setPlay 状态。
- `WHUAI_Nao/Src/Representations/Communication/ReceivedTeamMessages.h`：TeamMessage 载荷结构。

---

## 附录 B：`Zweikampf.cpp` 逐段详解（行号注释）
文件：WHUAI_Nao/Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Ball/Zweikampf.cpp

参数与结构
L32-L152 SearchParameters / DuelRatings / DuelTimings：采样密度、评分权重、时间窗口。
L157-L245 RatingMap / RatingMapVector / ObstacleSector：评分缓存、惩罚接口与扇区结构。

行为请求与踢向更新
L325-L333 updateDuelPose()：按里程计更新踢向，保持连续性。
L338-L366 updatePassEndPosition()：生成传球角限制（passRangeOne/Two）。
L372-L412 updateSkillRequest()：同步 SkillRequest/Pass 目标并刷新踢向。

障碍修正与争球对象
L418-L453 shiftObstacleBackward()/calculateClosestObstacle()：把贴球障碍后移并选择最近对手作为 duelObstacle。

忽略传球请求的条件
L458-L472 handleIgnoreSkillRequest()：传球角不合法且对手逼近时暂时改为 shoot。

扇区轮构建
L478-L680 calculateSectorWheel()：障碍/球门扇区构建与合并，生成 goalAngleWithBuffer。

getDuelRating 评分主流程
L686-L1027 getDuelRating()：旋转限制、踢程裁剪、Goal/Pass/Steal 判定与评分累加（对应 calculateDuel 第 8 步）。

调试绘制
L1033-L1083 drawRating()：绘制候选落点热区便于调试。

精度裁剪
L1088-L1267 calculateSectorUntilFieldBorder()：依据边界裁剪踢向精度。

calculateDuel 主流程
L1279-L1333 初始化计时与对手距离。
L1334-L1371 forwardSteal 角锥与 mustStandBetweenBallAndGoal 判定。
L1396-L1504 方向搜索（局部/全局/pass/sidewards/forwardSteal）。
L1506-L1563 forwardSteal/sidewards 角锥绘制调试。
L1570-L1670 踢法遍历 + getDuelRating 评分 + 目标类型优先级选择。
L1672-L1801 最优踢法确定、精度估计、forward/long 替换与 buffer 处理。
L1804-L1834 无可行踢法时站位兜底与 forwardSteal 滞回。

球位置插值
L1839-L1857 calculateUseBallPosition()：融合当前与预测球位并裁剪场地。

reset 初始化
L1862-L1907 reset()：重置踢向、预计算踢程列表并套用 SkillRequest。

execute 执行
L1909-L1969 execute()：更新球/障碍，调用 calculateDuel，执行踢球或站位。

---

## 附录 C. 团队协作与通信共享变量索引
- TeamMessage 载荷：`ReceivedTeamMessages.h:L21-L34`（RobotPose、BallModel、BehaviorStatus、StrategyStatus）。
- 传接球字段：`BehaviorStatus.h:L17-L25`（passTarget/passOrigin）。
- 角色同步：`StrategyStatus.h:L18-L30`（role/position/setPlay）。
