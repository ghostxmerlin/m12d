# 工作状态（2026-11-10 晚更新）

## 当前状态

- m12d 0.1.0 已经装在 M12 上，开机自启。M12 装进 StackChan 后，通过 Grove 串口和 StackChan 对接，**StackChan 端已接入成功**（2026-11-10）。
- 协议：`docs/protocol.md`。StackChan 的驱动按这份协议实现。以后改协议，只能追加字段。
- 已完成的阶段：
  - 0–1：人脸检测，与原厂一致，但更快。
  - 1b：远距离人形检测。
  - 2：按需人脸识别。
  - 3：年龄和性别。
  - 5：开机自启。
- 已放弃的阶段：
  - 4（场景识别）：改由 StackChan 自己的摄像头和图像识别模型来做。
  - 阶段 5 里的温控分档、频率策略、守护进程：用户决定先不做，只在心跳里报告温度。
- 结果记录在 `results/`，各阶段的设计和计划在 `docs/superpowers/`。

## 主要数字

| 项目 | 结果 | 出处 |
|---|---|---|
| 人脸检测，近距离模式（192×144） | 约 18 fps；原厂 6.95 fps | `results/2026-11-09-input-size-sweep.md` |
| 人脸 + 人形，有脸时 | 15.3–15.6 fps，77°C（裸机） | `results/2026-11-10-phase1b.md` |
| `who` 往返时间 | p50 265 ms，p95 445 ms | `results/2026-11-10-phase2.md` |
| 开机到开始输出 | 约 21 s | 下文"开机自启" |

## 还没做的事

- **人脸识别阈值**：还没有其他人的人脸样本，默认阈值暂用原厂的 0.5。
  - 用户安排其他人按 `scripts/shots/others.txt` 拍照后，用 `scripts/faceid_eval.py` 来定阈值。
  - 阈值的默认值在 `RuntimeCfg::id_th`。
- **识别速度**：MobileFaceNet 单次推理 218 ms，达不到设计要求的 300 ms 延迟、帧率下降不超过 10%。可以选择：放宽目标、试 int8 量化、换更小的模型。还没决定。
- 可选的测试：
  - 阶段 0–1 的 D 层 5 分钟对比；
  - 阶段 1b 的语音验收（`scripts/accept1b.py`）；
  - 60 s 连续 `who` 时的帧率；
  - 装在 StackChan 机壳里的长时间温度观察。
- **最终审查时推迟处理的小问题**：
  - 人形检测：
    - 人形先配对再截取前 5 个，脸可能和被丢掉的第 6 个人形配对。
    - 检测或过热暂停恢复后，可能有 1–4 帧旧的人形数据。
    - 一次复核会让一行检测行拖到约 265 ms。
    - `--no-person` 启动时，`cfg person:true` 会回复 true，但实际不输出人形（`attr` 也一样）。
  - 人脸识别：
    - `learn` 指定的脸 id 一直没有出现时，回复 `timeout`，而不是 `noface`。
    - 摄像头卡住后：`who` 可能用几秒前的脸来回答；`forget` 和 `faces` 会超时。
    - 重叠规则晚一帧：两张脸开始重叠的那一帧，名字还会带着。
    - 名字没有做 UTF-8 合法性检查。
  - 人脸库文件：
    - 掉电后残留的 `faces.bin.tmpXXXXXX` 不会被清理。
    - 目录 fsync 失败时报 `io`，但新文件其实已经写入。
  - 其他：
    - `snap` 在只做了人形检测的帧上报 `num:0`。
    - helloworld 页面的名字里有 `$&` 时显示会乱。
    - `m12d.cpp` 里的识别调度和阶段 1b 的部分逻辑没有单元测试；没有 `tests/data` 时，基于图片的测试会被跳过。

## 开机自启（阶段 5）

- 安装、卸载、查看状态：`scripts/boot.sh install|remove|status`。
- SD 卡上有 `/media/sdcard/m12-m12d` 时：
  - `S85runpayload` 跳过原厂 payload；
  - `/etc/init.d/S99m12d` 启动 `./bin/m12d`，日志写到 `log/m12d.log`，超过 1 MB 时保留一份旧日志；
  - 没有 USB 网络地址时，自动加 `--tcp-port 0`，只用串口。
- 原厂温控 `S110thermal` 不受影响。`S85runpayload` 的原件备份在 `/media/sdcard/m12test/S85runpayload.orig`。
- **回到原厂**：删除 SD 卡上的标记文件再重启，或者运行 `scripts/boot.sh remove`。
- 没有守护进程：m12d 崩溃后要等下一次上电才会重新运行。
- 上电验证：开机第 21 s 自动启动，人脸库完好，`who` 认出了用户。

## 设备上的改动（M12）

- `/etc/init.d/S90wifi-conf`：SD 卡上有 `m12-nowifi` 时跳过 Wi-Fi。备份在 `results/2026-11-09-baseline/S90wifi-conf.orig`。
- `S85runpayload` 和 `S99m12d`：见上一节。
- 应用目录：`/home/m5stack/m12d/{bin,models,log,data}`。人脸库在 `data/faces.bin`，里面有用户学过的人脸，不提交。

## 供电和接线（装在 StackChan 里）

- M12 由 StackChan 的 Grove 口供电，这时**不要再插 M12 的 USB-C**：两路 5 V 会接在一起。
- 串口：StackChan 的 Port C（UART0），115200 8N1，TX 和 RX 交叉连接。
- 要调试时，取下 M12 插 USB-C，可以用 TCP 5555 或者 helloworld 页面。

## Mac 端工具

- helloworld 页面：
  - 启动：`/usr/bin/python3 tools/helloworld/server.py [--serial /dev/cu.usbserial-0001]`，打开 http://127.0.0.1:8765/
  - 页面显示人脸框、人形框、心跳，提供开关和人脸识别面板。
  - 也可以用 `--replay tools/helloworld/sample.jsonl` 回放录好的数据。
- USB-TTL：用 CP2102，设备名 `/dev/cu.usbserial-0001`。只接 GND 和 TX/RX，不接 VCC。
- 命令脚本：`scripts/cmdtest.py --tcp|--serial`；`--watch 60` 查看推送。
- 采集：
  - `scripts/capset.sh` 连拍；
  - `scripts/capsession.sh` 用语音逐个报机位，清单在 `scripts/shots/`；
  - 图片放在 `tests/data/`，不提交。
- 编译：`scripts/env.sh scripts/build.sh host|target`。部署：`scripts/deploy.sh`。测量：`scripts/measure.sh`。
- 设备密码在 `.superpowers/m12.env`，已被 git 忽略。用法：`. .superpowers/m12.env`。
- **Mac 上的 Python 一律用 `/usr/bin/python3`**。Homebrew 的 Python 会被"本地网络"权限拦住。

## 访问设备的注意事项

- 设备上的 shell 是 zsh：
  - 普通用户不能写 `/dev/null`；
  - 没有 `pkill` 和 `timeout`；
  - scp 和 sftp 都不能用，传文件用 `scripts/m12push.sh`。
- 停原厂服务一律用 `scripts/stock.sh stop`，严禁使用 `killall python3`。
- 按名字结束进程时，不要匹配 `m12cmd`：`m12ssh.sh --root` 自己的临时脚本就叫这个名字。
- 用户喜欢短时间的测试，一般 2–5 分钟。
