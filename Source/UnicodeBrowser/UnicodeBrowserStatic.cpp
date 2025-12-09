#include "UnicodeBrowserStatic.h"

#include "Fonts/UnicodeBlockRange.h"

THIRD_PARTY_INCLUDES_START
#include <unicode/uchar.h>
#include <unicode/unistr.h>
THIRD_PARTY_INCLUDES_END

TOptional<EUnicodeBlockRange> UnicodeBrowser::GetUnicodeBlockRangeFromChar(int32 const CharCode)
{
	for (auto const& BlockRange : FUnicodeBlockRange::GetUnicodeBlockRanges())
	{
		if (BlockRange.GetRange().Contains(CharCode))
		{
			return BlockRange.Index;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("No Unicode block range found for character code U+%-06.04X: %s"), CharCode, *FString::Chr(CharCode));
	return {};
}

TArrayView<FUnicodeBlockRange const> UnicodeBrowser::GetUnicodeBlockRanges()
{
	if (Ranges.IsEmpty())
	{
		Ranges = FUnicodeBlockRange::GetUnicodeBlockRanges();
		/*Ranges.Sort(
			[](FUnicodeBlockRange const& A, FUnicodeBlockRange const& B)
			{
				return A.GetDisplayName().CompareTo(B.GetDisplayName()) < 0;
			}
		);*/
	}
	return Ranges;
}

int32 UnicodeBrowser::GetRangeIndex(EUnicodeBlockRange BlockRange)
{
	return GetUnicodeBlockRanges().IndexOfByPredicate(
		[BlockRange](FUnicodeBlockRange const& Range)
		{
			return Range.Index == BlockRange;
		}
	);
}

FString UnicodeBrowser::GetUnicodeCharacterName(int32 const CharCode)
{
	UChar32 const uChar = static_cast<UChar32>(CharCode);
	UErrorCode errorCode = U_ZERO_ERROR;
	char* name = new char[256];

	// Get the Unicode character name using ICU
	int32_t const length = u_charName(uChar, U_CHAR_NAME_ALIAS, name, 256, &errorCode);
	FString Result;
	if (U_SUCCESS(errorCode) && length > 0)
	{
		Result = FString(name);
	}
	else
	{
		Result = FString::Printf(TEXT("Unknown %hs"), u_errorName(errorCode));
	}
	delete[] name;
	return Result;
}
