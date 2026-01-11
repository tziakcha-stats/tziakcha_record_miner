# analyzer_cli 使用文档

## 概述

`analyzer_cli` 是本项目中用于分析国标麻将（GB Mahjong）牌谱记录的命令行工具。它基于 `RecordSimulator` 核心引擎，能够完整重放牌局过程，提取和牌信息、番数详情以及玩家的各项统计数据。无论是单局深入分析还是批量数据挖掘，此工具都能提供支持。

## 基本用法

```bash
./analyzer_cli <command> [选项]
```

支持的子命令：
*   `analyze`: 分析单个牌谱文件。
*   `batch`: 批量分析指定目录下的多个牌谱。

---

## 子命令：analyze (单局分析)

用于对单一的 JSON 格式牌谱进行详细解析。

### 参数说明

| 参数 | 缩写 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| `--file` | `-f` | String | **(二选一)** 牌谱 JSON 文件的路径。 |
| `--json` | `-j` | String | **(二选一)** 直接传入牌谱 JSON 字符串。 |
| `--output` | `-o` | String | 指定输出文件的路径（输出为 JSON 格式）。 |
| `--print` | `-p` | Boolean | 是否在控制台打印详细的和牌分析结果（默认为 `true`）。 |
| `--detailed` | `-d` | Boolean | 是否打印详细的牌局步骤日志（默认为 `false`）。 |
| `--verbose` | `-v` | Flag | 启用详细调试日志。 |
| `--help` | `-h` | Flag | 显示帮助信息。 |

### 输出格式 (JSON)

当指定 `-o` 参数时，工具会生成包含以下字段的 JSON 文件：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `winner_name` | String | 和牌玩家的名称（或 ID）。 |
| `winner_wind` | String | 和牌玩家的门风。 |
| `total_fan` | Number | 总番数。 |
| `base_fan` | Number | 基础番数（不含花牌等奖励）。 |
| `flower_count` | Number | 花牌数量。 |
| `env_flag` | String | 比赛环境标志（如圈风、门风等）。 |
| `hand_string_for_gb` | String | 用于番数计算的标准手牌字符串。 |
| `fan_details` | Array | 具体的番种列表，包含 `fan_name`, `fan_points`, `count`。 |

---

## 子命令：batch (批量分析)

用于对指定目录下的牌谱进行大规模批量处理，适合数据统计和挖掘。

### 参数说明

| 参数 | 缩写 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| `--directory` | `-d` | String | **(必填)** 包含牌谱文件的目录路径。 |
| `--pattern` | `-p` | String | 文件匹配模式，默认为 `*.json`。 |
| `--summary` | `-s` | String | 输出汇总文件的路径（TSV 格式）。 |
| `--verbose` | `-v` | Flag | 启用详细调试日志。 |
| `--help` | `-h` | Flag | 显示帮助信息。 |

### 汇总输出格式 (TSV)

`--summary` 指定的文件将以 Tab 分隔符（TSV）存储汇总数据，每行一条记录：

| 列顺序 | 内容 | 说明 |
| :--- | :--- | :--- |
| 1 | Filepath | 牌谱文件路径。 |
| 2 | Winner | 和牌玩家名称。 |
| 3 | Total Fan | 总番数。 |
| 4 | Base Fan | 基础番数。 |
| 5 | Flower Count | 花牌数。 |

---

## 示例命令

| 场景 | 命令示例 |
| :--- | :--- |
| **分析单个文件并打印结果** | `./analyzer_cli analyze -f data/record/example.json` |
| **分析并将结果存为 JSON** | `./analyzer_cli analyze -f data/record/example.json -o result.json` |
| **查看详细牌局步骤** | `./analyzer_cli analyze -f data/record/example.json --detailed` |
| **批量分析目录** | `./analyzer_cli batch -d data/records -s summary.tsv` |

---

## 单元测试与验证

`analyzer` 模块的准确性通过以下单元测试进行保障：

| 测试文件 | 覆盖内容 |
| :--- | :--- |
| `test/unit/starting_hand_test.cpp` | 验证 `RecordSimulator` 对起手配牌的捕获和重放逻辑。 |
| `test/unit/fan_calculator_test.cpp` | 间接验证分析器依赖的算番逻辑。 |
| `test/unit/player_stats_test.cpp` | 验证基于分析结果生成的玩家统计数据准确性。 |

**运行命令**:
```bash
# 在 build 目录下
./test/unit/starting_hand_test
# 或
ctest -R starting_hand_test
```
