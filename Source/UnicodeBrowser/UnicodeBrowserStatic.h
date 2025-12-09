#pragma once

#include "CoreMinimal.h"

#include "Fonts/UnicodeBlockRange.h"
namespace UnicodeBrowser
{
       TCHAR constexpr InvalidSubChar = TEXT('\uFFFD');
       TOptional<EUnicodeBlockRange> GetUnicodeBlockRangeFromChar(int32 const CharCode);

       static TConstArrayView<FUnicodeBlockRange const> Ranges; // all known Unicode ranges
       TConstArrayView<FUnicodeBlockRange const> GetUnicodeBlockRanges();

       static FString GetUnicodeCharacterName(int32 CharCode);

       int32 GetRangeIndex(EUnicodeBlockRange BlockRange);

       static TArray<EUnicodeBlockRange> SymbolRanges = {
               EUnicodeBlockRange::Arrows,
               EUnicodeBlockRange::BlockElements,
               EUnicodeBlockRange::BoxDrawing,
               EUnicodeBlockRange::CurrencySymbols,
               EUnicodeBlockRange::Dingbats,
               EUnicodeBlockRange::EmoticonsEmoji,
               EUnicodeBlockRange::EnclosedAlphanumericSupplement,
               EUnicodeBlockRange::EnclosedAlphanumerics,
               EUnicodeBlockRange::GeneralPunctuation,
               EUnicodeBlockRange::GeometricShapes,
               EUnicodeBlockRange::Latin1Supplement,
               EUnicodeBlockRange::LatinExtendedB,
               EUnicodeBlockRange::MathematicalAlphanumericSymbols,
               EUnicodeBlockRange::MathematicalOperators,
               EUnicodeBlockRange::MiscellaneousMathematicalSymbolsB,
               EUnicodeBlockRange::MiscellaneousSymbols,
               EUnicodeBlockRange::MiscellaneousSymbolsAndArrows,
               EUnicodeBlockRange::MiscellaneousSymbolsAndPictographs,
               EUnicodeBlockRange::MiscellaneousTechnical,
               EUnicodeBlockRange::NumberForms,
               EUnicodeBlockRange::SupplementalSymbolsAndPictographs,
               EUnicodeBlockRange::TransportAndMapSymbols
       };
}