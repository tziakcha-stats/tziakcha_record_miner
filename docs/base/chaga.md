???+ warning "注意事项"

  本文档中所描述的字段名称、数据结构及语义解释，均基于对 chaga api 的逆向分析与推测。这些字段名并非来自官方公开文档，仅为社区根据当前行为合理猜测所得。

  因此，请注意：

  - 字段命名、嵌套结构或值的含义可能随 CHAGA 后端接口更新而发生变化；
  - 本文档内容不具备长期稳定性保证，仅反映截至撰写时的观察结果；
  - 若用于生产环境或关键逻辑，强烈建议自行验证最新响应格式，并做好兼容性与错误处理。
  - 本说明仅供参考，不构成对 CHAGA API 的官方定义或承诺。

# CHAGA 评测响应数据格式说明

本文档详细说明 CHAGA 评测接口返回数据（如 `pydemo/response.json`）的结构。该说明基于项目 [chaga-reviewer-script](https://github.com/Choimoe/chaga-reviewer-script)

- `chaga_reviewer.user.js` 中对评测数据的加载与使用逻辑（`loadReviewData` / `show_cands` / `act2str`）
- `pydemo/response.json` 的实际样本统计

---

## 概述

评测响应用于给出某个 `seat` 在牌局各决策点的 AI 候选动作与权重（分值）。

接口示例：

- `https://tc-api.pesiu.org/review/?id=<gameId>&seat=<seat>`

其中：

- `id`: 牌谱 ID
- `seat`: 玩家座位（0~3）

### 根对象结构

接口返回可能是两种形态：

1. 直接返回数组 `Array<Row>`
2. 返回对象 `{ data: Array<Row>, ... }`

当前项目脚本按以下方式兼容读取：

- JS: `(Array.isArray(r) ? r : r.data)`
- Python: `response if list else response['data']`

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `data` | Array | 当根为对象时，评测行数组。 |
| `code` | Number | （错误场景）接口错误码，非 0 表示失败。 |
| `message` | String | （错误场景）错误信息。 |

---

## 评测行数据 (Row)

每一条 Row 对应一个评测动作节点，核心字段如下。

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `m` | Integer | 模式/模型标识。样本中恒为 `2`。 |
| `r` | Integer | 动作类型编码（与对局动作语义一致）。 |
| `v` | Integer | 动作值（具体含义由 `r` 决定）。 |
| `rr` | Integer | 小局索引（round index）。 |
| `ri` | Integer | 步索引（review index，和牌谱步骤对齐）。 |
| `extra` | Object | 扩展信息（候选动作、推理信息）。 |
| `_meta` | Object | 元信息（非每条都有）。 |

> 在 `chaga_reviewer.user.js` 中，仅当 `ri` 为真值时写入索引：`__reviews["${rr}-${ri}"] = row`。

---

## rr / ri 对齐规则

JS 端将评测数据按 `rr-ri` 作为键存储：

- 键格式：`${rr}-${ri}`
- `rr`：来自 `round` 显示文本解析（`parseRound`）
- `ri`：来自牌局当前步骤 `tzInstance.stp - 18`

因此评测命中逻辑是：

1. 先得到当前 `(round, ri)`
2. 查找 `__reviews_filled["round-ri"]`，若无则查 `__reviews["round-ri"]`

### 缺失 ri 的补位行为

`fillEmptyValues()` 会在同一 `rr` 内进行一次“单步前向补位”：

- 若某个 `ri` 缺失且前一个 `ri` 有值，会把前一个值补到该 `ri`
- 每次补一次后会将 `lastValue` 清空，避免无限连续传播

---

## 动作类型与 v 字段说明

`r` 的语义与牌局动作一致（Discard/Chi/Peng/Gang/Hu/Pass/Abandon 等），当前项目中可确认的解释如下。

| `r` | 语义 | `v` 说明 |
| :--- | :--- | :--- |
| `1` | Abandon / 弃 | 样本中固定为 `1`。 |
| `2` | Play / 出牌 | `v` 为牌 ID（0~135），牌型索引 = `v // 4`。 |
| `3` | Chi / 吃（或 Pass） | `v=0` 视为 Pass；`v>0` 视为 Chi。 |
| `4` | Peng / 碰（或 Pass） | `v=0` 视为 Pass；`v>0` 视为 Peng。 |
| `5` | Gang / 杠（或 Pass） | `v=0` 视为 Pass；`v>0` 视为 Gang。 |
| `6` | Hu / 和牌 | `v` 为和牌附加值（样本中出现 27/45/50/100）。 |

> 说明：在评测展示层，最终动作文案主要使用 `extra.candidates[*][1]`（例如 `Play J3` / `Chi W2`）进行展示和比对。

---

## extra 扩展字段

`extra` 常见字段：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `candidates` | Array | 候选动作列表，按权重从高到低。 |
| `aid` | Integer | 附加分析 ID（样本中少量出现）。 |
| `oid` | Integer | 附加对象 ID（样本中少量出现）。 |
| `md` | Integer | 附加模式标记（样本中少量出现）。 |

### candidates 结构

`candidates` 的每个元素为二元数组：

- `[weight, action]`

| 索引 | 类型 | 说明 |
| :--- | :--- | :--- |
| `0` | Number | 动作权重/评分（浮点）。 |
| `1` | String | 动作字符串（如 `Play J3`、`Chi W2`、`Pass`、`Hu`）。 |

示例：

```json
{
  "extra": {
    "candidates": [
      [10.785551071166992, "Play J3"],
      [0.34959113597869873, "Play W1"]
    ]
  },
  "rr": 0,
  "ri": 7
}
```

---

## 动作字符串编码

候选动作字符串常见格式：

- `Play <TileCode>`
- `Chi <TileCode>`
- `Peng <TileCode>`
- `Gang <TileCode>` / `BuGang <TileCode>`
- `Hu`
- `Pass`
- `Abandon`

其中 `<TileCode>` 使用 bz 编码：

- 数牌：`W1..W9`、`T1..T9`、`B1..B9`
- 风牌：`F1..F4`
- 字牌：`J1..J3`
- 花牌：`H1..H8`

映射到牌类型索引（tc）的规则：

- `W` -> `0..8`
- `T` -> `9..17`
- `B` -> `18..26`
- `F` -> `27..30`
- `J` -> `31..33`
- `H` -> `34..41`

并通过页面 `TILE[i * 4]` 转为显示字符串。

---

## _meta 元信息

样本中 `_meta` 仅出现：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| `d` | Boolean | 样本中均为 `false`。具体业务含义未在现有代码中消费。 |

另外，部分 `r=1` 行仅有 `_meta`，且缺失 `rr/ri`，这些记录不会进入 `__reviews` 主索引。

---

## 示例解析（基于 `pydemo/response.json`）

**Row**:

```json
{
  "m": 2,
  "r": 3,
  "v": 4,
  "_meta": {"d": false},
  "extra": {
    "candidates": [
      [6.094849109649658, "Chi W2"],
      [0.7086477279663086, "Pass"]
    ]
  },
  "rr": 0,
  "ri": 24
}
```

1. `r=3`：表示 Chi/Pass 决策节点。  
2. `v=4`：非 0，因此主语义为 Chi。  
3. 候选首选为 `Chi W2`（权重 6.09），次选为 `Pass`。  
4. 该记录定位于 `rr=0` 小局、`ri=24` 步，可通过键 `0-24` 与牌谱步骤对齐。

---

???+ info

  下面信息来自 [chaga-reviewer-script](https://github.com/Choimoe/chaga-reviewer-script)
  
  ## 与牌谱 step 的关系（用于一致率计算）
  
  在本项目比对逻辑中：
  
  1. 从 `step.json` 提取玩家选择动作（Play/Chi/Peng/Gang/Hu/Pass/Abandon）
  2. 从 `response.json` 读取同一 `(rr,ri)` 位置的 `extra.candidates[0]` 作为 AI 首选
  3. 比较动作类型与（若是 Play）牌值是否一致
  4. 对 `Pass/Abandon` 使用“候选中不存在相关操作”规则判断一致性
  
  这样可计算“选手操作 vs CHAGA 首选”一致率。
