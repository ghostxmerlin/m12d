# m12d 阶段 2：按需人脸识别 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal：** m12d 支持 `learn` / `who` / `forget` / `faces` 四条指令。识别只在被问到时算，结果写回检测行，人脸库掉电安全。

**Architecture：**
- 新增四个单元：
  - `FaceEmbedder`：对齐裁剪，跑 MobileFaceNet 得到 128 维特征。
  - `FaceDb`：内存中的人脸库，负责序列化和原子写盘。
  - `FaceIdWorker`：识别任务队列。每帧调度一次，负责采样、超时，以及名字写回检测行。
  - `IoLoop::reply`：跨线程把延迟回复发到指定通道。
- `Dispatcher` 收到 learn / who / forget / faces 时不当场回复，先把请求放进队列，由检测线程在帧间处理后再回复。

**Tech Stack：** C++17、ncnn 6afc736、doctest；Mac 端用 stdlib Python。

**Spec：** `docs/superpowers/specs/2026-11-10-m12d-phase2-face-id-design.md`（简称"设计"）。上位设计是 `docs/superpowers/specs/2026-11-09-m12-vision-design.md`。

## Global Constraints

- 模型：原厂 `mobilefacenet.{param,bin}`。
  - 输入节点 `data`，内容是 112×112、**BGR 顺序**、0–255 原值，不做归一化；输出节点 `fc1`，128 维，再做 L2 归一化。依据是原厂 `src/arcface.cpp`，原厂用 `from_pixels(PIXEL_BGR)` 喂入。
  - 对齐用现有的 `align_face`，它输出 RGB，喂给模型前要换成 BGR。
- 学习：每次在同一个跟踪 id 上采 5 个样本，两个样本至少相隔 150 ms。每个名字最多 20 个样本，超出时丢掉最旧的；库里最多 16 个名字。名字是 UTF-8，1–32 字节，不能含控制字符。
- 识别：每个名字取其样本中的最大余弦相似度，再在名字之间取最大值。低于 `id_th` 时返回 `name:null`。`id_th` 默认 0.5，等 Task 6 有了数据再定，`cfg` 可以设，范围 0.3–0.9。
- 队列：最多 4 条，满了回复 `busy`。每个请求从入队起计时，期限 3 s。每帧最多做一次 MobileFaceNet 推理，`who` 优先于 `learn`。
- 错误码：现有的那些，加上新增的 `full`、`conflict`、`standby`。
- 人脸库文件 `data/faces.bin`：
  - 格式：魔数 `M12F`、版本 1，每个名字依次是名字长度、名字、样本数、float32 特征，文件末尾 CRC32。
  - 写入：先写 tmp 并 fsync，再 rename，再对目录 fsync。
  - 加载失败时，把坏文件改名为 `faces.bad.<秒>`，以空库启动。
- 隐私：`data/` 写进 `.gitignore`。任何人脸图片和特征都不进仓库。
- 原有检测行字段不变。`name` 只在 `id` 之后追加。
- Mac 端 Python 用 `/usr/bin/python3` 运行；设备上停原厂服务用 `scripts/stock.sh stop`；用户偏好 2–5 分钟的短测试。

## Review Focus

1. **无脸模式下的人形帧**（这一帧没跑人脸检测）：识别任务不能在这一帧处理，否则会误报 `noface`。只在跑了人脸检测的帧上处理（Task 3 测试）。
2. **learn 进行中，被采样的那个人离开、另一个人进入**：样本必须全部来自同一个跟踪 id；id 变了就不再采。到期时回复 `noface`，不存任何样本（Task 3 测试）。
3. **人脸库文件被截断或 CRC 不对**：以空库启动，坏文件保留为 `.bad`。下一次 learn 写出的新库可以正常加载（Task 2 测试）。
4. **待机（det:false）或过热时发来 learn / who**：立即回复 `standby` / `hot`，不进队列。forget / faces 照常处理（Task 4、5 测试）。
5. **forget 正被写回检测行的名字**：所有挂着这个名字的跟踪立即不再输出 `name`（Task 3 测试）。

---

### Task 1：FaceEmbedder 与原厂参照

**Files：**
- 新建：`src/vision/face_embed.{h,cpp}`、`tests/test_face_embed.cpp`、`tools/ref/ref_stock_arc.cpp`（只在 HOST 时编译）
- 修改：`CMakeLists.txt`

**Interfaces（Produces）：**
```cpp
using Embedding = std::array<float, 128>;  // L2-normalised
float cosine(const Embedding& a, const Embedding& b);  // dot product
class FaceEmbedder {
 public:
  bool load(const std::string& model_base, int num_threads);
  // rgb: 320x240 packed RGB; face in the same coordinates (with 5 landmarks).
  Embedding embed(const uint8_t* rgb, int w, int h, const FaceBox& face);
};
```

- [ ] **Step 1：写参照程序**
  - 从 `../upstream/UnitV2Framework/src/arcface.cpp` **逐字复制** `Arcface::getFeature`、`normalize`、`preprocess`，以及它们依赖的 `getAffineMatrix`、`warpAffineMatrix`。
  - 文件头注明来源提交 `0e48be6` 和原文件行号。
  - 用法：`ref_stock_arc <model_base> <in.yuyv> <lm10...>`。程序读入 640×480 帧，换算成 320×240 的 BGR，按给定的五点（320×240 坐标）调用 `preprocess` 和 `getFeature`，打印 128 个数。
- [ ] **Step 2：写失败的测试** `test_face_embed.cpp`
  - `embedding is unit length`：取 `tests/data/faceid/id_front_00.yuyv`，先用 RetinaFace 找到脸，再 embed，检查 |e| 在 1±1e-4。
  - `same person different frames is similar`：`id_front_00` 和 `id_front_01` 的 cosine 大于 0.6。
  - 没有 `tests/data` 或模型时，按 `test_retinaface.cpp` 的方式打印 SKIPPED。
  - 模型路径：`../research/2026-11-09-app-readiness/official-recovery-files/home/m5stack/payload/models/mobilefacenet`。
- [ ] **Step 3：** 运行 `scripts/env.sh scripts/build.sh host`，期望失败（还没有实现）。
- [ ] **Step 4：** 实现：`align_face` → RGB 换成 BGR → `ncnn::Mat::from_pixels(…, PIXEL_RGB, 112, 112)`（数据已是 BGR 顺序）→ 抽取 `fc1` → L2 归一化。
- [ ] **Step 5：** 运行，期望 `100% tests passed`。
- [ ] **Step 6：A 层一致性**
  - 新建 `scripts/run-arc-parity.sh`：对 `tests/data/faceid/*.yuyv` 的每一帧，分别用 `ref_stock_arc` 和新工具 `tools/embed_file.cpp`（同样只在 HOST 时编译，打印 `FaceEmbedder` 的 128 个数）计算特征，再求 cosine。
  - 期望：每一帧都 ≥ 0.999。打印最小值。
- [ ] **Step 7：** 提交，信息为 `vision: FaceEmbedder (stock mobilefacenet, BGR) + stock arcface reference parity`。

### Task 2：FaceDb（内存库、序列化、原子写盘）

**Files：** 新建 `src/vision/face_db.{h,cpp}`、`tests/test_face_db.cpp`；修改 `CMakeLists.txt`、`.gitignore`（加入 `data/`）。

**Interfaces（Produces）：**
```cpp
struct Match { std::string name; float sim; };  // name "" = none above threshold
enum class AddResult { kOk, kFull, kConflict, kBadName };
class FaceDb {
 public:
  static constexpr size_t kMaxNames = 16, kMaxSamples = 20, kMaxNameBytes = 32;
  static bool valid_name(const std::string& n);
  // Rejects (kConflict, *conflict_name set) when any sample matches ANOTHER name at >= th.
  AddResult add(const std::string& name, const std::vector<Embedding>& samples, float th, std::string* conflict_name);
  Match best(const Embedding& e, float th) const;  // max over samples per name, then over names
  bool forget(const std::string& name);
  std::vector<std::pair<std::string, size_t>> list() const;  // name, samples
  std::string serialize() const;
  static std::optional<FaceDb> parse(const std::string& bytes);  // nullopt on any corruption
  bool save(const std::string& path) const;  // tmp + fsync + rename + dir fsync
  // Missing -> empty, ok. Corrupt -> renamed to <path>.bad.<unix_s>, empty; *note says which.
  static FaceDb load(const std::string& path, std::string* note);
};
```

- [ ] **Step 1：写失败的测试**（用合成的单位向量，不需要模型）
  - `add then best finds the name above threshold`
  - `best returns empty below threshold`，并且仍然给出 sim
  - `samples capped at 20, oldest dropped`
  - `16 names max -> kFull`
  - `conflict with another name is rejected and nothing stored`
  - `valid_name`：空串、33 字节、含 `\n` 都判为无效；"小明"（6 字节）有效。
  - `serialize/parse round trip`
  - `parse rejects bad magic, bad CRC, truncated file`（截掉 1 字节、改 1 个字节各一个用例）
  - `load corrupt file renames it to .bad and starts empty`（对应 Review Focus 3）：用 `mkdtemp` 建临时目录；改名之后再 save，再 load，期望得到新库。
  - `save is atomic`：保存后目录里不残留 `.tmp`。
- [ ] **Step 2：** 运行，期望失败。
- [ ] **Step 3：** 实现。CRC32 用标准多项式 0xEDB88320，自己实现，不引入依赖。
- [ ] **Step 4：** 运行，期望全部通过。
- [ ] **Step 5：** 提交，信息为 `vision: FaceDb (cosine library, CRC'd atomic file, corrupt-file quarantine)`。

### Task 3：FaceIdWorker（队列、调度、采样、写回）

**Files：** 新建 `src/vision/face_id_worker.{h,cpp}`、`tests/test_face_id_worker.cpp`；修改 `CMakeLists.txt`。

**Interfaces（Produces）：**
```cpp
struct FaceJob {
  enum Kind { kLearn, kWho, kForget, kList } kind;
  int chan; long long req_id;
  std::string name;   // learn / forget
  int face_id = 0;    // 0 = largest face
  int64_t enq_ms;
};
struct JobReply { int chan; std::string line; };   // full reply line
class FaceJobQueue {                                // IO thread -> detect thread
 public:
  static constexpr size_t kMax = 4;
  bool push(FaceJob j);                 // false when full
  std::vector<FaceJob> take_all();      // detect thread
};
class FaceIdWorker {
 public:
  using EmbedFn = std::function<Embedding(const FaceBox&)>;
  static constexpr int kLearnSamples = 5;
  static constexpr int64_t kSampleGapMs = 150, kDeadlineMs = 3000;
  FaceIdWorker(FaceDb& db, std::function<bool(const FaceDb&)> persist);
  void enqueue(std::vector<FaceJob> jobs);
  // Call once per frame ON WHICH FACE DETECTION RAN. faces/ids as from FaceTracker. th = cfg id_th.
  // At most one embed() call. Returns replies to send.
  std::vector<JobReply> step(const std::vector<FaceBox>& faces, const std::vector<int>& ids, uint64_t frame,
                             int64_t now_ms, float th, const EmbedFn& embed);
  // Per-frame name write-back for the det line ("" = none).
  std::string name_for(int track_id) const;
  // Call every frame (also frames without face detection) so expired deadlines are answered.
  std::vector<JobReply> expire(int64_t now_ms, bool face_present);
};
```
- 回复格式照设计 §4，例如 `{"id":2,"ok":true,"f":123,"face":4,"name":"x","sim":0.734}`，sim 保留 3 位小数。
- 选脸：`face_id == 0` 时取面积最大的那张脸；指定的 id 不在画面里时，`who` 立即回复 `noface`。
- `forget` 和 `kList` 在 step 中直接处理，不占用推理名额。
- learn 存库时调用 `persist(db)`；persist 返回 false 时，回复 `{"ok":false,"err":"io"}`，并撤销这次添加。`io` 是 Task 3 新增的错误码。
- 名字写回的规则照设计 §4"识别结果写回检测行"那一段。其中"重叠"指两张脸的框 IoU > 0。

- [ ] **Step 1：写失败的测试**（embed 用假函数：按 face.x 返回固定的向量，并统计调用次数）
  - `who on empty db returns null name sim 0`
  - `who finds learned name and writes it back`
  - `who has priority over an in-progress learn`：先放 learn 再放 who，第一次 step 必须调 who 的 embed。
  - `learn takes 5 samples >= 150 ms apart from the same id then stores`
  - `learn aborts with noface when the sampled id disappears and nothing is stored`（对应 Review Focus 2）
  - `learn conflict / full replies and nothing persisted`
  - `deadline: who unanswered after 3 s -> timeout; learn with face present -> timeout, absent -> noface`
  - `at most one embed per step`
  - `forget clears write-back on all tracks with that name`（对应 Review Focus 5）
  - `overlapping faces drop write-back`；`who returning another name replaces, null clears`
  - `persist failure -> io error and db unchanged`
  - `step is only called on face frames`：这一条是 m12d 的契约，在 Task 5 验证（对应 Review Focus 1）。
- [ ] **Step 2：** 运行，期望失败。
- [ ] **Step 3：** 实现。
- [ ] **Step 4：** 运行，期望全部通过。
- [ ] **Step 5：** 提交，信息为 `vision: FaceIdWorker (who-first scheduling, learn sampling, deadlines, name write-back)`。

### Task 4：延迟回复与指令

**Files：** 修改 `src/io/io_loop.{h,cpp}`、`src/proto/commands.{h,cpp}`、`tests/test_io_loop.cpp`、`tests/test_commands.cpp`、`tests/cmd_script.jsonl`、`tests/cmd_expected.jsonl`。

**Interfaces（Produces）：**
- `void IoLoop::reply(ChanId c, std::string line);`
  - 任何线程都可以调用。实现时用加锁的队列，再写 wake_fd 唤醒 IO 循环。
  - IO 循环在 `drain_push_queue` 旁边把回复发给指定通道。通道已经关闭时直接丢弃。
- `RuntimeCfg` 新增 `std::atomic<float> id_th{0.5f}`。`cfg` 接受 `id_th`（0.3–0.9），回复里在 `"person"` 之后加 `"id_th":0.50`（2 位小数）。
- `StatusSource` 新增：
  - `std::function<bool(FaceJob)> face_job`：返回 false 表示队列满；
  - `std::function<bool()> paused`；
  - `bool faceid_enabled`。
- `Dispatcher::handle` 处理这四条指令：
  - `learn {name, face?}`：名字无效时回复 `badarg`。
  - `who {face?}`
  - `forget {name}`
  - `faces {}`
  - 判定顺序：模型没有加载时回复 `nomodel`；learn / who 在 det_on 为 false 时回复 `standby`，paused 时回复 `hot`；`face_job` 返回 false 时回复 `busy`。以上都通过时，返回空串，表示稍后再回复。
  - `face` 必须是正整数，否则回复 `badarg`。多余的键也回复 `badarg`。

- [ ] **Step 1：写失败的测试**
  - io_loop：`reply from another thread reaches only that channel`、`reply to a closed channel is dropped`。
  - commands：每条指令的成功路径都返回空串，并收到对应的 FaceJob；`standby`、`hot`、`busy`、`nomodel`、`badarg`（名字为空、名字超长、face 为 0 或为字符串）各一个用例；`cfg id_th` 的读、写、越界。
  - 更新 `cmd_expected.jsonl` 里所有 cfg 回复，加上 `"id_th":0.50`。命令脚本里不放 learn / who，因为它们的结果依赖实时画面。
- [ ] **Step 2：** 运行，期望失败。
- [ ] **Step 3：** 实现。
- [ ] **Step 4：** 运行，期望全部通过。
- [ ] **Step 5：** 提交，信息为 `io/proto: deferred replies; learn/who/forget/faces commands; cfg id_th`。

### Task 5：接入 m12d 与部署

**Files：** 修改 `src/app/m12d.cpp`、`src/app/options.{h,cpp}`（`--no-faceid`、`--data DIR`，默认 `data`）、`src/proto/det_json.{h,cpp}`（`FaceExtra` 加 `std::string name`，非空时输出在 `id` 之后）、`tests/test_det_json.cpp`、`tests/test_options.cpp`、`scripts/deploy.sh`（复制 `mobilefacenet.*`，并 `mkdir data`）。

- [ ] **Step 1：写失败的测试**
  - det_json：`name appended after id when set`、`no name key when empty`。
  - options：`--no-faceid`、`--data`。
- [ ] **Step 2：** 运行，期望失败。
- [ ] **Step 3：** 实现接入：
  - 启动时 `FaceDb::load(opt.data + "/faces.bin")`，有 note 时记到 stderr。
  - 检测线程每帧先调用 `take_all()`，再 `enqueue`。
  - `plan.face` 为真的帧调用 `step`，否则调用 `expire`（对应 Review Focus 1）。
  - 在 det_on 为 false 或 paused 时，也要调用 `expire` 和 `step` 来处理 forget / faces：在 `continue` 之前，用空的 faces 调用一次 `step`，并把 th 传进去。learn / who 在这种状态下已经被 Dispatcher 挡掉了（对应 Review Focus 4）。
  - 回复用 `io->reply`。
  - `extra[i].name = worker.name_for(ids[i])`。
  - ver 的 models 列表加入 `"mobilefacenet"`。
  - embed 用的 rgb 是本帧的 320×240 RGB。
- [ ] **Step 4：** 运行 `scripts/env.sh scripts/build.sh host`，期望 100% 通过；运行 `run-parity.sh`，期望 56 PASS。
- [ ] **Step 5：** 运行 `scripts/deploy.sh`，在设备上重启 m12d。用 `cmdtest.py --tcp` 跑命令脚本，期望 16/16。
  - 再手动发 `{"do":"faces"}`，期望 `faces:[]`；
  - 发 `{"do":"who"}`：画面里没人时期望 `noface`；
  - 发 `cfg det:false` 再发 `who`，期望 `standby`。
- [ ] **Step 6：** 提交，信息为 `app: on-demand face ID in the detect loop; det line "name"; deploy mobilefacenet`。

### Task 6：离线评估、**用户采集检查点**、定阈值

**Files：** 新建 `tools/faceid_eval.cpp`（只在 HOST 时编译）、`scripts/faceid_eval.py`、`scripts/shots/others.txt`、`results/2026-11-xx-faceid.md`。

- [ ] **Step 1：写 `faceid_eval`**
  - 对每个 yuyv：检测所有人脸，逐个 embed。
  - 每张脸输出一行：`<file> <face_idx> <x> <y> <w> <h> <128 floats>`。
  - 输出只写到 scratchpad，不进仓库，因为特征属于生物特征数据。
- [ ] **Step 2：写机位清单 `scripts/shots/others.txt`**
  - 机位：正脸、左转约 30°、右转约 30°、约 1 m、笑，每个机位 4 帧。
  - 拍法：用 `scripts/capsession.sh others_<代号> scripts/shots/others.txt`，每人用一个代号目录（例如 `others_a`）。
- [ ] **Step 3：检查点，停下来喊用户**
  - 告诉用户：架构已经就绪，请安排其他人来拍。
  - 每人约 1 分 30 秒；拍之前先停 m12d，拍完重新启动 m12d。
  - 用户准备好之后再拍，逐个人检查预览图，确认每张脸都清楚。
- [ ] **Step 4：评估 `scripts/faceid_eval.py`**
  1. 用用户的 `id_front` 作学习样本：5 帧，模拟一次 learn。
  2. 本人测试集：`faceid/` 里的其余帧，加上 `set/` 里检到的用户脸，按机位分组。
  3. 非本人测试集：`others_*/` 的全部人脸，加上 `faceid/` 背景里的同事脸。同事脸要排除用户自己，按框的位置区分。
  4. 输出两组相似度的分布，并按阈值 0.30–0.80（步长 0.05）列出：各机位的识别率、非本人的最高相似度和误认数。
  5. 同时用设计 §7 描述的"分次学习"再做一遍：正脸一次加转头一次。
- [ ] **Step 5：定阈值**
  - 按设计 §7：误认为 0，并留出余量（阈值 ≥ 非本人最高相似度 + 0.05）。
  - 在此前提下，正脸、左右约 30°、约 1 m 的识别率都要 ≥ 90%。
  - 达不到时：先试 640×480 对齐，给 `faceid_eval` 加 `--full-res`，从 640×480 帧上按五点乘以 2 对齐；还达不到，就停下来报告，不硬调阈值。
  - 把选定的值写成 `RuntimeCfg::id_th` 的默认值，同时修改 `test_commands` 的默认 cfg 字符串。
- [ ] **Step 6：** 写 `results/2026-11-xx-faceid.md`，内容包括表格、阈值、局限。不写人名，参与者用代号。提交，信息为 `results: face ID offline evaluation and threshold`。

### Task 7：设备验收与页面

**Files：** 修改 `tools/helloworld/helloworld.html`（人脸标签显示 `name`）、`tools/helloworld/sample.jsonl`；新建 `scripts/accept2.py`、`results/2026-11-xx-phase2.md`。

- [ ] **Step 1：页面改动**：人脸框标签和右侧列表在有 `name` 时显示名字。用 CDP 截图检查，方法见 Task 5（阶段 1b）的 `cdpshot.py`。
- [ ] **Step 2：写 `scripts/accept2.py`**：语音引导，通过 TCP 读数据。
  1. `learn` 用户的名字（名字由命令行参数 `--name` 传入，不写进仓库）。
  2. 在正脸、左转、右转、约 1 m 各做 5 次 `who`，记录识别结果、延迟（请求到回复）和 sim。
  3. 每秒一次 `who`、持续 60 s，同时统计心跳 fps，与开始前 30 s 的 fps 对比。
  4. 打印汇总。
- [ ] **Step 3：设备验收（需要用户配合，约 5 分钟）**
  - 运行 accept2。期望：延迟 p95 ≤ 300 ms；正脸、左右、1 m 的识别率 ≥ 90%；连续 `who` 期间 fps 下降 ≤ 10%。
  - **掉电测试**：learn 回复后，请用户拔掉 M12 电源再插上，等约 30 s 设备启动；按 STATUS 的方法重新停原厂服务、启动 m12d，然后 `faces` 里应该还有这个名字。
  - 最后 `forget` 这个名字，之后 `who` 回复 `null`。
- [ ] **Step 4：** 写 `results/2026-11-xx-phase2.md`，更新 `docs/STATUS.md` 和上位设计 §10 第 2 项（标注已实现）。提交，信息为 `tools/results: helloworld shows names; phase 2 device acceptance`。
