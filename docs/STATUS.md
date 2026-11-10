# 工作状态（2026-11-09 深夜更新）

## 当前所处阶段

- 阶段 0–1 计划：`docs/superpowers/plans/2026-11-09-m12d-phase0-1.md`
  - 执行方式：在会话里直接执行，每个任务提交一次，分支 `phase-0-1`。
  - 进度记录在 `.superpowers/sdd/2026-11-09-m12d-phase0-1/progress.md`，该目录已被 git 忽略。
- **Task 1–12 已完成。Task 13（验收）只剩需要用户配合的几步。**

### 已完成的验收

- **A 层**：静态二进制可以在设备上运行；主机和设备的推理结果逐位一致；单帧推理 118 ms。
- **B 层**：28 帧上，B1 与原厂完全一致，B2 最大偏差 0.139 px，设备复核偏差 0.001 px。见 `results/2026-11-09-parity.md`。
- **C 层**：
  - 统一指令集在 pty 和 TCP 上的测试已通过；
  - 在设备上通过 TCP 跑命令脚本，16/16 通过。

### 这一阶段里用户做的决定（2026-11-09）

- 检测采集改用 320×240。实验数据见 `results/2026-11-09-capture-experiments.md`。
- 默认使用近距离模式：检测输入 192×144，摄像头 30 fps。
  - 30 秒试跑：17.95 fps、CPU 84%；原厂是 6.95 fps、97%。
  - 实验数据见 `results/2026-11-09-input-size-sweep.md`。
- 新增阶段 1b：远距离人形检测（Yolo-Fastest 160，约 37 ms/次），并与人脸检测融合。排在人脸识别之前。见设计 §5、§10。

## 明天上午（需要用户配合）

1. **Task 13 Step 2：画框确认**
   - 用户在 2–3 个位置各停一下，从 Mac 发 `snap`，拉回 `/tmp/m12d_snap.yuyv`，用 `draw_boxes.py` 叠图。
   - 快照帧现在是 320×240，`draw_boxes` 会自动缩放。
2. **Task 13 Step 3–5：D 层对比，每组 5 分钟**
   - 共四组：原厂、m12d 默认（近距离模式）、m12d 320×240、m12d 限到原厂同帧率。
   - 每组开始前，设备要冷却到 66°C 以下，期间用户要坐在镜头前。
3. **Task 13 Step 6：真实串口**
   - 用 USB-TTL 线，只接 GND 和 TX/RX（交叉），**不接 VCC**。接线前先对照 M5 官方文档确认 Grove 线序。
   - 用 `cmdtest.py --serial` 跑命令脚本，再和 `--tcp` 的结果对比；最后用 `--watch 60` 查看推送。
4. 写 `results/2026-11-xx-phase1.md`。然后做整个分支的最终代码审查（最好交给新的审查者），再向用户汇报。
5. 写阶段 1b（人形检测）的计划。

## 图片采集（2026-11-10 上午）

- 工具：`scripts/capset.sh <目录> <名字> [张数] [间隔] [尺寸]` 连拍并拉回（丢弃前约 20 帧等白平衡稳定）；`scripts/capsession.sh <目录> scripts/shots/<清单>.txt [从哪个机位开始]` 用 Mac 中文语音逐个报机位，88°C 自动暂停。
- 已采（640×480 YUYV，在 `tests/data/` 下，不提交）：
  - `body/`：48 帧，阶段 1b 用。1.5/2.5/3.5/4+ 米正面、侧身、背对、坐、横走、画面边缘半身、空房间、椅子挂外套。**镜头平放朝上，没有一帧拍到脚**；房间纵深约 3 米多，3.5 米和 4 米以外差别不大。用户决定先凑合用。walk_00–02 背景有第二个人。
  - `faceid/`：32 帧，阶段 2 用。正脸、左右转、抬头（幅度很小）、低头、笑、约 1 米、暗光。背景里有同事，部分帧有同事正脸，可作"非本人"样本。
  - `objects/`：32 帧，阶段 4 用。杯子、瓶子、手机、小册子、剪刀、鼠标、手提袋，以及桌面多物体。
  - `body_full/`：36 帧，重拍的全身（M12 垫高约 30 cm、镜头放平），每帧都能看到脚。1.5 米时头顶贴着画面上边；2.5、3.5 米和最远处（约 5 米）；侧身、横走 8 帧、空房间、椅子挂深色外套。阶段 1b 以这组为主。
- 还没采：猫、各个房间（用户决定先不拍）。

## 2026-11-10 已完成

- **Task 13 Step 6 真实串口：通过。**
  - USB-TTL 是 Mac 上的 CP2102，设备名 `/dev/cu.usbserial-0001`。另一个 FTDI（`usbserial-XXXXXXXX`）不是 M12 的。
  - 命令脚本 16/16 通过，串口和 TCP 的回复只在实时数值上不同。
  - `--watch 60`：1088 行检测、29 次心跳，0 行乱码。打开串口的瞬间会收到半行残缺数据，属正常。
  - 修正 `cmdtest.py`：ver/status 的结构比较不再要求列表长度一致。模型列表多了 genderage，旧的比较方式误报。
- **阶段 1b 离线实验**：见 `results/2026-11-10-person-detection.md`。
  - 160 输入时，椅子上挂的外套被误认成人，得分 0.906。
  - 调度方案（160 加 320 二次确认、224、320）待用户决定。
  - 工具：`tools/person_file.cpp`（主机和设备都编译）、`scripts/person_eval.py`。
  - 模型副本放在 `../research/2026-11-10-person/`，设备上是 `models/yolo-fastest-th020.*`。
- m12d 正在设备上运行（默认参数，带串口），由 nohup 启动，重启后不会自动运行。

## 实时查看页面 helloworld（2026-11-09 新增）

- 在 Mac 上运行 `/usr/bin/python3 tools/helloworld/server.py`，然后打开 http://127.0.0.1:8765/
- 走串口（2026-11-10 验证通过，检测、心跳、开关按钮都正常）：`/usr/bin/python3 tools/helloworld/server.py --serial /dev/cu.usbserial-0001`
- 数据源：默认是 m12d 的 TCP 5555。`--serial /dev/tty.xxx` 改从串口读，`--replay tools/helloworld/sample.jsonl` 回放录好的数据。
- 页面显示人脸框和五点、心跳信息（帧率、温度、内存等），可以切换检测开关和五点开关。
- 测试：在 `tools/helloworld` 目录下运行 `/usr/bin/python3 -m unittest test_server`。

## 设备当前状态（M12，通过 USB 访问，地址 10.254.239.1）

- 持久改动只有一处：`/etc/init.d/S90wifi-conf`，开机时如果 SD 卡上有 `m12-nowifi` 就跳过 Wi-Fi。备份在 `results/2026-11-09-baseline/S90wifi-conf.orig`。
- 新增应用目录 `/home/m5stack/m12d/{bin,models,log}`，里面有 m12d、m12bench、m12cap、detect_file，以及几个实验用的二进制。
- 当前原厂服务处于停止状态，是按 PID 停的；温控程序在运行。**重启设备就会回到原厂状态。**
- `/tmp/set_*.yuyv` 是采集的固定图片集，重启后会丢失；本地 `tests/data/set/` 里有一份。

## Mac 端注意事项

- 设备密码在 `.superpowers/m12.env`，已被 git 忽略。使用方法：`. .superpowers/m12.env`。
- **Mac 上的 Python 脚本必须用 `/usr/bin/python3` 运行**。Homebrew 的 Python 被 macOS 的"本地网络"权限拦截，会报 "No route to host"。
- 编译：`scripts/env.sh scripts/build.sh host|target`。部署：`scripts/deploy.sh`。测量：`scripts/measure.sh`。

## 访问设备的注意事项

- 设备上的 shell 是 zsh：普通用户不能写 `/dev/null`，没有 `pkill`、`timeout`，scp 和 sftp 都不可用。
- 停原厂服务一律用 `scripts/stock.sh stop`，它只按 PID 结束进程。严禁使用 `killall python3`。
- 在设备上按名字结束进程时，不要匹配 `m12cmd`：`m12ssh.sh --root` 自己的临时脚本就叫这个名字，会把自己也杀掉。
- 用户喜欢短时间的测试，一般 2–5 分钟。
