# m12d 阶段 1b：远距离人形检测与融合 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal：** 在 m12d 里加入人形检测（Yolo-Fastest 160，新目标用 320 整帧复核），让 1.5–5 米站立的人能被检出并与人脸配对，输出到检测行的 `person` 列表。

**Architecture：**
- 新增三个纯逻辑单元和一个模型封装，都在 `src/vision/`，主机单测覆盖：
  - `PersonDetector`：模型封装和输出解码。
  - `PersonTracker`：IoU 跟踪，以及"待复核 / 已确认 / 已否决"三种状态。
  - `DetSchedule`：每帧决定跑人脸还是人形。
  - `pair_faces`：把人脸和人形配对。
- `m12d.cpp` 的检测线程按 `DetSchedule` 调度，并把结果交给 `det_json`。
- 统一指令集的 `cfg` 新增 `person` 开关，helloworld 页面画出人形框。

**Tech Stack：** C++17、ncnn 6afc736（静态库）、doctest，Mac 端用 stdlib Python 和原生 JS。

**Spec：** `docs/superpowers/specs/2026-11-09-m12-vision-design.md` §5"远距离：人形检测"、§6；实验依据见 `results/2026-11-10-person-detection.md`。

## Global Constraints

- 用户选定方案 ①（2026-11-10）：平时 160×160 检测，阈值 **0.60**；新出现的人形用 320×320 对整帧复核，阈值 **0.55**。
- 模型：原厂 `yolo_20class` 的 `yolo-fastest.{param,bin}`，原样使用，param 里写死阈值 0.55。
  - 输入节点 `data`，输出节点 `output`；person 的 label 为 **15**。
  - 预处理：320×240 RGB 直接拉伸到 S×S，不加黑边，像素值 ×1/255。
- 调度：
  - 有脸（距上次检到脸不超过 1000 ms）时，每帧做人脸检测，每 5 帧做一次人形检测。
  - 无脸时，人脸检测和人形检测逐帧交替。
- 融合：脸中心的 x 落在人形框内，且 y 落在人形框上部 40% 以内，就算配对。多个人形都符合时，取面积最小的。
- 输出：检测行原有字段和顺序都不变，只在末尾 `"running":"Face Detector"` 之后追加 `"person":[...]`。坐标与人脸一样按 640×480。
- 测试用的 Mac 端 Python 一律用 `/usr/bin/python3` 运行。设备上停原厂服务只用 `scripts/stock.sh stop`。
- 用户偏好短测试：设备上的验收每组 2–5 分钟。
- 仓库是公开的：提交里不能出现 IP（`10.254.239.1` 是 USB 链路固定地址，已在仓库中）、MAC、密码或人脸图片。

## Review Focus

1. **静止物体被否决后，真人站到它前面**：否决的轨迹每 5 s 重新复核一次，真人随后会被确认（Task 2 测试）。
2. **160 和 320 给出的框形状不同**：真人的复核不能因为 IoU 偏低被误否决。复核按 IoU ≥ 0.3 **或** 160 框中心落在 320 框内来判定（Task 2 测试）。
3. **运行中用 `cfg` 关掉 person**：检测行不再带 `person` 键，跟踪器清空，重新打开后从头开始（Task 4 测试）。
4. **人形很多**：输出最多 5 个，按 prob 从高到低排列（Task 3 测试）。
5. **无脸模式下的人形帧**：这一帧没有跑人脸，检测行仍要输出，`num` 为 0、`face` 为空，不能复用旧的人脸结果（Task 4 测试）。

---

### Task 0：提交上午的采集与实验工具

**Files：** 当前未提交的 `scripts/capset.sh`、`scripts/capsession.sh`、`scripts/shots/`、`scripts/person_eval.py`、`tools/person_file.cpp`、`CMakeLists.txt`、`scripts/cmdtest.py`、`results/2026-11-10-person-detection.md`、`docs/STATUS.md`。

- [ ] **Step 1：** 从 `phase-0-1` 切出新分支 `phase-1b`。
- [ ] **Step 2：** 运行 `scripts/env.sh scripts/build.sh host`，期望 `100% tests passed`。
- [ ] **Step 3：** 提交前检查，以下命令在暂存区中查不到任何结果：`git diff --cached | grep -E '192\.168|([0-9A-F]{2}:){5}'`。
- [ ] **Step 4：** 提交，信息为 `tools: voice-guided frame capture, person-detection experiment (yolo-fastest 160/224/320)`。

### Task 1：PersonDetector（模型封装与解码）

**Files：**
- 新建：`src/vision/person_det.{h,cpp}`、`tests/test_person_det.cpp`
- 修改：`CMakeLists.txt`，把它加入 `M12CORE_SOURCES` 和 `M12TEST_SOURCES`

**Interfaces（Produces）：**
```cpp
struct PersonBox { float x, y, w, h, prob; };  // 坐标在输入图像空间（320x240）
// rows: Yolov3DetectionOutput 的 n 行，每行 [label, prob, x0, y0, x1, y1]（归一化）
std::vector<PersonBox> decode_person_rows(const float* rows, int n, int img_w, int img_h, float prob_th);
class PersonDetector {
 public:
  static constexpr int kLabelPerson = 15;
  bool load(const std::string& model_base, int num_threads);
  // rgb: 320x240 packed RGB; size: 160 或 320。结果按 prob 降序。
  std::vector<PersonBox> detect(const uint8_t* rgb, int w, int h, int size, float prob_th);
};
```

- [ ] **Step 1：写失败的测试**
  - `decode keeps only label 15 above threshold`：输入三行，分别为 `{15,0.9,0.1,0.2,0.3,0.8}`、`{9,0.95,...}`、`{15,0.5,...}`，`prob_th=0.6`。期望只剩 1 个框，`x=32,y=48,w=64,h=144`（img 320×240）。
  - `decode sorts by prob desc`。
  - 用 body_full 帧测 `detect`（没有 `tests/data/body_full/` 或模型时，按 `test_retinaface.cpp` 的方式打印 SKIPPED）：
    - `stand25_00` 在 160、阈值 0.6 下至少有 1 个框，且与 `{139.6,31.6,48.5,169.7}`（320×240 空间，即实验真值除以 2）的 IoU ≥ 0.5。实验中 160 的框是 `{141.6,42.7,43.7,162.8}`，prob 0.945。
    - `clothes_00` 在 320、阈值 0.55 下检不到 person。
  - 模型路径：`../research/2026-11-10-person/yolo-fastest`。
- [ ] **Step 2：** 运行 `scripts/env.sh scripts/build.sh host`，期望编译失败或测试失败。
- [ ] **Step 3：实现**
  - 预处理用 `ncnn::Mat::from_pixels_resize(rgb, PIXEL_RGB, w, h, size, size)`，再做 `substract_mean_normalize({0,0,0},{1/255.f×3})`。
  - 抽取 `output` 后交给 `decode_person_rows`。
- [ ] **Step 4：** 运行同一命令，期望 `100% tests passed`。
- [ ] **Step 5：** 提交，信息为 `vision: PersonDetector (stock yolo-fastest, person label 15)`。

### Task 2：PersonTracker（跟踪与复核状态）

**Files：** 新建 `src/vision/person_tracker.{h,cpp}`、`tests/test_person_tracker.cpp`

**Interfaces（Produces）：**
```cpp
struct PersonTrack { int id; PersonBox box; enum State { kPending, kConfirmed, kRejected } state; };
class PersonTracker {
 public:
  static constexpr float kMatchIou = 0.3f;
  static constexpr int64_t kForgetMs = 1500, kRecheckMs = 5000;
  void update(const std::vector<PersonBox>& dets160, int64_t now_ms);  // 新框 -> kPending
  bool needs_verify(int64_t now_ms) const;  // 有 kPending，或有 kRejected 已超过 kRecheckMs
  void apply_verify(const std::vector<PersonBox>& dets320, int64_t now_ms);
  std::vector<PersonTrack> confirmed() const;  // 只返回 kConfirmed
  void clear();
};
```
- 复核规则：一条轨迹满足"与某个 320 框 IoU ≥ 0.3，或其框中心落在某个 320 框内"就改为 kConfirmed，否则改为 kRejected，并记下复核时间。
- `apply_verify` 只改变 kPending，以及已到期的 kRejected 轨迹。

- [ ] **Step 1：写失败的测试**
  - `new detection is pending and not reported`
  - `verify confirms when center inside 320 box even with low IoU`：160 框 `{100,50,30,100}`，320 框 `{90,40,60,140}`。
  - `verify rejects coat-like track and does not recheck before 5 s`：`needs_verify` 在 4999 ms 时为 false，在 5000 ms 时为 true。
  - `rejected track becomes confirmed on recheck`（对应 Review Focus 1）。
  - `track forgotten after 1500 ms unseen`
  - `ids stable across updates`
- [ ] **Step 2：** 运行，期望失败。
- [ ] **Step 3：** 实现。匹配沿用 `FaceTracker` 的贪心最大 IoU 做法。
- [ ] **Step 4：** 运行，期望全部通过。
- [ ] **Step 5：** 提交，信息为 `vision: PersonTracker with 320 verify (pending/confirmed/rejected)`。

### Task 3：调度、配对与检测行输出

**Files：**
- 新建：`src/vision/det_schedule.{h,cpp}`、`src/vision/pairing.{h,cpp}`、`tests/test_det_schedule.cpp`、`tests/test_pairing.cpp`
- 修改：`src/proto/det_json.{h,cpp}`、`tests/test_det_json.cpp`

**Interfaces（Produces）：**
```cpp
struct DetPlan { bool face, person; };
// n: 本次检测循环的序号（从 0 起），face_recent: 距上次检到脸 <= 1000 ms
DetPlan plan_frame(uint64_t n, bool face_recent);
// 返回每个人形配对到的人脸下标（faces 下标，没有为 -1）
std::vector<int> pair_faces(const std::vector<FaceBox>& faces, const std::vector<PersonBox>& persons);
struct PersonOut { int id; PersonBox box; int face_id; };  // face_id = -1 表示无
// det_json 新增最后一个参数；persons == nullptr 时不输出 "person" 键
std::string det_json(..., const std::vector<FaceExtra>* extra = nullptr,
                     const std::vector<PersonOut>* persons = nullptr);
```
- 人形对象格式：`{"x":..,"y":..,"w":..,"h":..,"prob":..,"id":N,"face":<face_id 或 -1>}`。坐标乘以 scale，1 位小数；prob 3 位小数。最多 5 个，按 prob 降序。

- [ ] **Step 1：写失败的测试**
  - `plan_frame`：
    - 有脸时：n=0..9 每帧 face=true，person 只在 n%5==0 时为 true。
    - 无脸时：偶数帧 face=true、person=false，奇数帧 face=false、person=true。
  - `pair_faces`：
    - 脸中心在框上部 40% 内则配对；在下部则不配对。
    - 两个嵌套框都符合时，选面积小的那个。
  - `det_json`：
    - persons 为 nullptr 时，输出与现有测试逐字相同。
    - 有 1 个人形时，末尾是 `,"running":"Face Detector","person":[{"x":200.0,...,"id":3,"face":7}]}`。
    - 有 7 个人形时只输出 5 个，且按降序排列（对应 Review Focus 4）。
- [ ] **Step 2：** 运行，期望失败。
- [ ] **Step 3：** 实现。
- [ ] **Step 4：** 运行，期望全部通过，原有 det_json 测试不变。
- [ ] **Step 5：** 提交，信息为 `vision/proto: frame schedule, face-person pairing, "person" list in det line`。

### Task 4：接入 m12d、指令集和部署

**Files：**
- 修改：
  - `src/app/m12d.cpp`
  - `src/app/options.{h,cpp}`：新增 `--no-person`，对应 `use_person`
  - `src/proto/commands.{h,cpp}`：`RuntimeCfg::person_on`；`cfg` 接受 `person`，回复里在 `"attr"` 之后加 `"person"`
  - `tests/test_commands.cpp`、`tests/test_options.cpp`、`tests/cmd_expected.jsonl`
  - `scripts/deploy.sh`：模型不存在时，从 `/home/m5stack/payload/uploads/models/yolo_20class/` 复制 `yolo-fastest.{param,bin}`
- 新建：`src/vision/person_stage.{h,cpp}`、`tests/test_person_stage.cpp`

**Interfaces：**
```cpp
// 每帧调用一次；内部持有 PersonTracker。返回要输出的人形（已确认的，带配对）。
class PersonStage {
 public:
  explicit PersonStage(PersonDetector& det);
  // ran_person: 本帧是否按计划跑人形；faces/face_ids 是本帧的人脸及其 tracker id
  std::vector<PersonOut> step(const uint8_t* rgb320, bool ran_person, const std::vector<FaceBox>& faces,
                              const std::vector<int>& face_ids, int64_t now_ms);
  void reset();
};
```
- 帧内顺序：先跑 160 并更新跟踪器；如果 `needs_verify`，**同一帧**接着跑 320 并调用 `apply_verify`；最后对已确认的人形做配对。
- 跟踪 id 改为始终计算：`FaceTracker::update` 移出 `use_attr` 判断。
- `cfg person:false` 时调用 `stage.reset()`，并给 `det_json` 传 nullptr（对应 Review Focus 3）。
- 无脸模式下的人形帧：`faces` 为空，检测行照常输出（对应 Review Focus 5）。
- ver 的 models 列表在启用时加入 `"yolo-fastest"`。

- [ ] **Step 1：写失败的测试**
  - `test_person_stage`（用 body_full 帧，缺数据时 SKIPPED）：
    - 把 `clothes_00` 连续送 10 帧（每帧 ran_person=true，时间每次加 100 ms），结果始终为空。
    - 把 `stand25_00` 连续送 3 帧，第 1 帧复核之后就返回 1 个人形。
    - `reset()` 之后再送 `stand25_00`，重新经过 pending → 复核。
  - `test_commands`：默认 cfg 字符串里包含 `"person":true`；`{"do":"cfg","person":false}` 生效；`person:1` 返回 badarg。
  - `test_options`：`--no-person` 把 `use_person` 设为 false。
  - 更新 `cmd_expected.jsonl` 里所有 cfg 回复，在 `"attr":true` 之后加 `"person":true`。
- [ ] **Step 2：** 运行，期望失败。
- [ ] **Step 3：** 实现 `PersonStage`，并在 `m12d.cpp` 的检测线程里按 `plan_frame` 调度：
  - `face_recent` 取最近一次检到脸的时间。
  - 计数 `n` 每次循环加 1。
  - `face=false` 的帧不调用 `rf.detect`。
- [ ] **Step 4：** 运行 `scripts/env.sh scripts/build.sh host`，期望 `100% tests passed`；再运行 `scripts/run-parity.sh`，期望 B 层结果不变（56 PASS）。
- [ ] **Step 5：** 运行 `scripts/deploy.sh`，在设备上启动 m12d（默认参数，带串口）。再运行 `cmdtest.py --serial /dev/cu.usbserial-0001` 和 `--tcp`，期望都是 `PASS 16/16`。
- [ ] **Step 6：** 提交，信息为 `app: person detection stage (160 + 320 verify), cfg person, deploy yolo-fastest`。

### Task 5：helloworld 页面与设备验收

**Files：** 修改 `tools/helloworld/server.py`（`allowed_command` 加入 `person`）、`tools/helloworld/helloworld.html`、`tools/helloworld/test_server.py`、`tools/helloworld/sample.jsonl`；新建 `results/2026-11-10-phase1b.md`。

- [ ] **Step 1：写失败的测试** `test_server`：`{"do":"cfg","person":true}` 允许通过；`{"do":"cfg","person":"x"}` 被拒绝。
- [ ] **Step 2：** 在 `tools/helloworld` 目录运行 `/usr/bin/python3 -m unittest test_server`，期望失败；实现后再运行，期望通过。
- [ ] **Step 3：页面改动**
  - 人形框用新的颜色变量 `--person`，在亮色和暗色主题下都要定义。
  - 框上标注 `人 #id`，配对时加上 `· 脸 #face`。
  - 加一个"人形"开关按钮。
  - 右侧列表新增人形一栏。
  - 用带 `person` 的回放数据截图，检查显示效果。
- [ ] **Step 4：设备验收（需要用户配合，约 5 分钟）**
  - 用户坐在镜头前约 1 分钟，用 `measure.sh` 记录：有脸时的检测 fps 应 ≥ 15，温度不超过 85°C。
  - 用户依次站到 1.5 米、2.5 米、3.5 米、最远处，在页面上看到人形框。走近后人形框与人脸配对。
  - 椅子挂外套、人离开画面，持续 1 分钟，页面上不出现人形框。
- [ ] **Step 5：** 把数字和结论写进 `results/2026-11-10-phase1b.md`，更新 `docs/STATUS.md` 和设计 §5，标注已实现。提交，信息为 `tools/results: helloworld person boxes; phase 1b device acceptance`。
