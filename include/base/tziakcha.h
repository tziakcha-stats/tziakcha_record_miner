#ifndef TZIAKCHA_BASE_TZIAKCHA_H_
#define TZIAKCHA_BASE_TZIAKCHA_H_

namespace tziakcha {
namespace base {

constexpr int kActionTypeNone          = 0;
constexpr int kActionTypeFlowerReplace = 1;
constexpr int kActionTypeDiscard       = 2;
constexpr int kActionTypeChi           = 3;
constexpr int kActionTypePeng          = 4;
constexpr int kActionTypeGang          = 5;
constexpr int kActionTypeWin           = 6;
constexpr int kActionTypeDraw          = 7;
constexpr int kActionTypePass          = 8;
constexpr int kActionTypeAbandon       = 9;

constexpr int kFlowerAutoMask = 0x1000;

constexpr int kDiscardHandPlayedMask = 0x0001;
constexpr int kDiscardPlayModeShift  = 9;
constexpr int kDiscardPlayModeMask   = 0x0003;

constexpr int kPengTileBaseMask        = 0x003F;
constexpr int kPengTileBaseShift       = 2;
constexpr int kPengTileOffsetShift     = 10;
constexpr int kPengTileOffsetMask      = 0x0003;
constexpr int kPengOfferDirectionShift = 6;
constexpr int kPengOfferDirectionMask  = 0x0003;

constexpr int kGangTileBaseMask        = 0x003F;
constexpr int kGangTileBaseShift       = 2;
constexpr int kGangTileOffsetShift     = 10;
constexpr int kGangTileOffsetMask      = 0x0003;
constexpr int kGangOfferDirectionShift = 6;
constexpr int kGangOfferDirectionMask  = 0x0003;
constexpr int kGangPromotedMask        = 0x0300;
constexpr int kGangPromotedValue       = 0x0300;

constexpr int kWinAutoMask      = 0x0001;
constexpr int kWinFanCountShift = 1;

constexpr int kDrawBackwardMask = 0x0100;

constexpr int kPassModeMask   = 0x0003;
constexpr int kPassModeManual = 0;
constexpr int kPassModeAuto   = 1;
constexpr int kPassModeForced = 2;

constexpr int kLowByteMask   = 0x00FF;
constexpr int kHighByteShift = 8;

constexpr int kActionPlayerShift = 4;
constexpr int kActionPlayerMask  = 0x0003;
constexpr int kActionTypeMask    = 0x000F;

constexpr int kFlowerTileOffset = 136;

} // namespace base
} // namespace tziakcha

#endif // TZIAKCHA_BASE_TZIAKCHA_H_
