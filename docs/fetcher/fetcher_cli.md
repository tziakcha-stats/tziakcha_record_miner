# fetcher_cli 使用文档

## 概述

fetcher_cli 用于从 tziakcha.net 获取和管理对局记录数据。

## 命令列表

| 命令 | 功能 |
| --- | --- |
| history | 从服务器获取游戏历史 |
| sessions | 从历史记录中提取会话 |
| session | 获取单个会话 |
| record | 获取单个对局记录 |
| records | 批量获取对局记录 |
| local-history | 按日期过滤本地历史记录 |

## history - 获取历史记录

从服务器获取游戏历史，需要设置环境变量 `TZI_HISTORY_COOKIE`。

### 参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| -c, --config | string | config/fetcher_config.json | 配置文件路径 |
| -d, --data-dir | string | data | 数据存储目录 |
| -k, --key | string | history/history | 存储键 |
| --start-date | string | - | 起始日期 (YYYYMMDD) |
| --end-date | string | - | 结束日期 (YYYYMMDD) |
| -f, --filter | string | - | 按标题关键词过滤 |
| -p, --print | bool | false | 打印 JSON 到控制台 |

### 示例

```bash
# 获取指定日期范围的历史
export TZI_HISTORY_COOKIE="your_cookie_value"
./fetcher_cli history --start-date 20260101 --end-date 20260131

# 获取全部历史
./fetcher_cli history

# 过滤特定标题
./fetcher_cli history -f "竹花杯"
```

## sessions - 提取会话

从历史记录中提取会话并分组。

### 参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| -c, --config | string | config/fetcher_config.json | 配置文件路径 |
| -d, --data-dir | string | data | 数据存储目录 |
| -i, --input | string | history/history | 输入历史记录键 |
| -o, --output | string | sessions/all_record | 输出会话分组键 |
| -m, --map | string | sessions/record_parent_map | 输出记录父映射键 |
| -p, --print | bool | false | 打印 JSON 到控制台 |

### 示例

```bash
./fetcher_cli sessions -i history/history_20260101
```

## session - 获取单个会话

根据会话 ID 获取单个会话数据。

### 参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| -c, --config | string | config/fetcher_config.json | 配置文件路径 |
| -d, --data-dir | string | data | 数据存储目录 |
| -s, --session-id | string | - | 会话 ID（必填） |
| -o, --output | string | sessions/{session_id} | 输出键 |
| -p, --print | bool | false | 打印 JSON 到控制台 |

### 示例

```bash
./fetcher_cli session -s TszL5UsT
```

## record - 获取单个对局

根据对局 ID 获取单个对局记录。

### 参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| -c, --config | string | config/fetcher_config.json | 配置文件路径 |
| -d, --data-dir | string | data | 数据存储目录 |
| -r, --record-id | string | - | 对局 ID（必填） |
| -o, --output | string | record/{record_id} | 输出键 |
| -p, --print | bool | false | 打印 JSON 到控制台 |

### 示例

```bash
./fetcher_cli record -r im3TYGwi
```

## records - 批量获取对局

从会话 JSON 批量获取对局记录。

### 参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| -c, --config | string | config/fetcher_config.json | 配置文件路径 |
| -d, --data-dir | string | data | 数据存储目录 |
| -i, --input | string | sessions/all_record | 输入会话 JSON 键 |
| -o, --output-dir | string | record | 输出目录 |
| -l, --limit | int | 0 | 限制获取数量（0=无限制） |
| --delay | int | 500 | 请求间隔（毫秒） |
| --skip-existing | bool | true | 跳过已存在记录 |

### 示例

```bash
# 批量获取所有对局
./fetcher_cli records -i sessions/all_record

# 限制数量并设置延迟
./fetcher_cli records -l 100 --delay 1000

# 不跳过已存在记录
./fetcher_cli records --skip-existing false
```

## local-history - 过滤本地历史

按日期过滤本地 history_all.json 文件。

### 参数

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| -i, --input | string | data/history_all.json | 输入历史文件路径 |
| -d, --date | string | - | 过滤日期 (YYYYMMDD) |
| --start-date | string | - | 起始日期 (YYYYMMDD) |
| --end-date | string | - | 结束日期 (YYYYMMDD) |
| -o, --output | string | - | 输出文件路径（自动生成） |

### 示例

```bash
# 单日过滤
./fetcher_cli local-history -d 20260101

# 日期范围过滤
./fetcher_cli local-history --start-date 20260101 --end-date 20260131

# 指定输出路径
./fetcher_cli local-history -d 20260101 -o custom_output.json
```

## 工作流示例

```bash
# 1. 获取历史记录
export TZI_HISTORY_COOKIE="your_cookie"
./fetcher_cli history --start-date 20260101 --end-date 20260101

# 2. 提取会话
./fetcher_cli sessions -i history/history_20260101

# 3. 批量获取对局
./fetcher_cli records -i sessions/all_record -l 100
```

## 环境变量

| 变量名 | 说明 |
| --- | --- |
| TZI_HISTORY_COOKIE | tziakcha.net 历史记录页面的 Cookie 值 |

获取方式：登录 https://tziakcha.net/history/ 后从浏览器开发者工具复制 Cookie。
