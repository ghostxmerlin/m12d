# Third-party code and models

| What | Where it is used | License | In this repo? |
|---|---|---|---|
| [M5Stack UnitV2Framework](https://github.com/m5stack/UnitV2Framework) @0e48be6, `main/face_detector.cpp` | `tools/ref/ref_stock_fd.cpp` 中有逐字复制的片段；`src/vision/retinaface.cpp` 是移植后修改的版本，改动写在文件头 | Apache-2.0 | 只有上述片段 |
| [Tencent ncnn](https://github.com/Tencent/ncnn) @6afc736 | 推理引擎，静态链接 | BSD-3-Clause | 否，由 `scripts/fetch-deps.sh` 拉取 |
| [doctest](https://github.com/doctest/doctest) 2.4.11 | 单元测试 | MIT | 否，由 `scripts/fetch-deps.sh` 拉取 |
| RetinaFace mnet.25 模型（M5Stack 设备自带） | 人脸检测 | 随设备 | 否，部署时从设备的 payload 目录复制 |
| Yolo-Fastest `yolo_20class` 模型（M5Stack 设备自带，VOC 20 类） | 人形检测（阶段 1b） | 随设备 | 否，部署时从设备的 payload 目录复制 |
| [insightface](https://github.com/deepinsight/insightface) buffalo_s `genderage.onnx` | 性别和年龄段，转换成 ncnn 后使用 | 见 insightface 的模型许可（非商业研究用途） | 否 |
| [deepinx/age-gender-estimation](https://github.com/deepinx/age-gender-estimation) | 只用于评估，已放弃 | 见原仓库 | 否 |

模型文件都不在仓库里，仓库也不再分发它们。
