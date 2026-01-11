<p align="center">
<h2 align="center">tziakcha_record_miner</h2>
<p align="center"><a href="https://github.com/tziakcha-stats/tziakcha_record_miner"><img src="https://img.shields.io/badge/Github-181717?style=for-the-badge&logo=github&logoColor=white"><img src="https://img.shields.io/github/stars/tziakcha-stats/tziakcha_record_miner?style=for-the-badge"></a></p>
<p align="center"><a href="https://github.com/tziakcha-stats/tziakcha_record_miner"><img src="https://img.shields.io/github/license/tziakcha-stats/tziakcha_record_miner?style=for-the-badge"> </a> <a href="https://choimoe.github.io/chaga-reviewer-script/"><img src="https://img.shields.io/github/actions/workflow/status/tziakcha-stats/tziakcha_record_miner/deploy-pages.yml?label=deploy%20pages&style=for-the-badge"></a></p>
</p>

**tziakcha_record_miner** 是一个专注于国标麻将的数据挖掘与分析工具集。它旨在为开发者和麻将爱好者提供从数据获取、牌谱解析到高精度的番数计算的一站式解决方案。

本项目包含三个核心模块，分别对应数据链路中的不同环节：获取、分析与计算。

完整的说明文档已部署至 GitHub Pages，请访问：
👉 **[tziakcha_record_miner 文档](https://miner.choimoe.com/)**

## 核心模块

### 1. 数据获取器 (Fetcher)
`fetcher_cli` 用于从 [tziakcha.net](https://tziakcha.net) 下载牌谱数据，支持增量更新和按日期筛选，方便建立本地数据库。

### 2. 牌谱分析器 (Analyzer)
`analyzer_cli` 负责解析 JSON 牌谱，重放对局过程，提取和牌番数、玩家行为等详细数据，用于统计分析。

### 3. 核心计算器 (Calc)
`calc_cli` 包含国标麻将算法核心，提供算番（Fan Calculation）和向听数（Shanten Analysis）计算功能，支持 81 种番型判断。

## 快速开始

### 编译构建

本项目使用 CMake 进行构建。请确保您的环境中已安装 CMake 和 C++17 兼容的编译器。

```bash
# 1. 克隆仓库
git clone https://github.com/choimoe/tziakcha_record_miner.git
cd tziakcha_record_miner

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置与编译
cmake ..
make -j4
```

编译完成后，所有可执行文件（`fetcher_cli`, `analyzer_cli`, `calc_cli`）将位于 `src/` 对应的子目录中。

### 运行测试

单元测试：

```bash
cd build
ctest --output-on-failure
```