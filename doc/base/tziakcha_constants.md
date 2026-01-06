# tziakcha 基础常量

## 动作类型
| 常量                     | 值  | 含义      |
| ------------------------ | --- | --------- |
| kActionTypeNone          | 0   | 空操作    |
| kActionTypeFlowerReplace | 1   | 补花      |
| kActionTypeDiscard       | 2   | 出牌/打牌 |
| kActionTypeChi           | 3   | 吃        |
| kActionTypePeng          | 4   | 碰        |
| kActionTypeGang          | 5   | 杠        |
| kActionTypeWin           | 6   | 和牌      |
| kActionTypeDraw          | 7   | 摸牌      |
| kActionTypePass          | 8   | 过        |
| kActionTypeAbandon       | 9   | 弃和      |

## 位掩码与位移
| 常量                     | 值/写法          | 含义                                 |
| ------------------------ | ---------------- | ------------------------------------ |
| kFlowerAutoMask          | 0x1000 (1 << 12) | 自动补花标志位（action data）        |
| kDiscardHandPlayedMask   | 0x0001           | hi_byte & 1，手打标志                |
| kDiscardPlayModeShift    | 9                | 打牌模式起始位（action data >> 9）   |
| kDiscardPlayModeMask     | 0x0003           | 打牌模式掩码（两位）                 |
| kPengTileBaseMask        | 0x003F           | 牌基值掩码（取低 6 位）              |
| kPengTileBaseShift       | 2                | 牌基值左移量                         |
| kPengTileOffsetShift     | 10               | 牌偏移起始位                         |
| kPengTileOffsetMask      | 0x0003           | 牌偏移掩码（两位）                   |
| kPengOfferDirectionShift | 6                | 来自谁的方向位起始                   |
| kPengOfferDirectionMask  | 0x0003           | 方向掩码（两位）                     |
| kGangTileBaseMask        | 0x003F           | 杠牌基值掩码                         |
| kGangTileBaseShift       | 2                | 杠牌基值左移量                       |
| kGangTileOffsetShift     | 10               | 杠牌偏移起始位                       |
| kGangTileOffsetMask      | 0x0003           | 杠牌偏移掩码                         |
| kGangOfferDirectionShift | 6                | 杠牌来源方向起始位                   |
| kGangOfferDirectionMask  | 0x0003           | 杠牌来源方向掩码                     |
| kGangPromotedMask        | 0x0300           | 加杠标志位掩码                       |
| kGangPromotedValue       | 0x0300           | 判断加杠的匹配值                     |
| kWinAutoMask             | 0x0001           | 自动和标志位                         |
| kWinFanCountShift        | 1                | 番数起始位（data >> 1）              |
| kDrawBackwardMask        | 0x0100           | 逆向摸牌标志（hi_byte != 0）         |
| kPassModeMask            | 0x0003           | 过牌模式掩码：0 手动，1 自动，2 强制 |

## 通用位操作
| 常量               | 值/写法 | 含义                 |
| ------------------ | ------- | -------------------- |
| kLowByteMask       | 0x00FF  | 低字节掩码           |
| kHighByteShift     | 8       | 高字节位移           |
| kActionPlayerShift | 4       | 玩家索引起始位       |
| kActionPlayerMask  | 0x0003  | 玩家索引掩码（两位） |
| kActionTypeMask    | 0x000F  | 动作类型掩码         |

## 牌偏移
| 常量              | 值  | 含义                            |
| ----------------- | --- | ------------------------------- |
| kFlowerTileOffset | 136 | 花牌偏移（高字节低 4 位 + 136） |
