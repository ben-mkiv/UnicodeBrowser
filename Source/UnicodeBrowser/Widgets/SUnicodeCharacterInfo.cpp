#include "SUnicodeCharacterInfo.h"

#include "SlateOptMacros.h"
#include "UnicodeBrowser/DataAsset_FontTags.h"
#include "UnicodeBrowser/UnicodeBrowserOptions.h"

#include "UnicodeBrowser/UnicodeBrowserRow.h"
#include "UnicodeBrowser/UnicodeBrowserStatic.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SUnicodeCharacterInfo::Construct(FArguments const& InArgs)
{
	SInvalidationPanel::Construct(SInvalidationPanel::FArguments());
	SetCanCache(true);
	
	if(InArgs._Row.IsValid())
	{
		SetRow(InArgs._Row);
	}
}

void SUnicodeCharacterInfo::SetRow(TSharedPtr<FUnicodeBrowserRow> InRow)
{
	if(!InRow.IsValid() || !InRow->bHasValidCharacter)
		return;

	FText TagsText = FText::GetEmpty();
	
	if(UUnicodeBrowserOptions::Get()->Preset && UUnicodeBrowserOptions::Get()->Preset->SupportsFont(*InRow->FontInfo))
	{
		TagsText = FText::FromString(TEXT("Tags: ") + FString::Join(UUnicodeBrowserOptions::Get()->Preset->GetCodepointTags(InRow->Codepoint), TEXT(", ")));			
	}

	FString BlockRangeName = "";
	if(InRow->BlockRange){
		if(FUnicodeBlockRange const *Range = UnicodeBrowser::GetUnicodeBlockRanges().FindByPredicate([Needle = InRow->BlockRange.Get(EUnicodeBlockRange::ControlCharacter)](FUnicodeBlockRange const &Range){ return Range.Index == Needle; }))
		{
			BlockRangeName = *Range->GetDisplayName().ToString();	
		}
	}
	
	SetContent(
		SNew(SVerticalBox)
			// codepoint
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Codepoint: 0x%04X"), InRow->Codepoint)))
			]
			// can load
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Can Load: %s"), *LexToString(InRow->CanLoadCodepoint()))))
			]
			// dimensions
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Size: %dx%d"), FMath::FloorToInt(InRow->GetMeasurements().X), FMath::FloorToInt(InRow->GetMeasurements().Y) )))
			]
			// font object
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(InRow->GetFontData() ? FText::FromString(FString::Printf(TEXT("Font: %s"), *InRow->GetFontData()->GetFontFilename())) : FText::GetEmpty())
			]
			// subface index
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(InRow->GetFontData() ? FText::FromString(FString::Printf(TEXT("SubFace Index: %d"), InRow->GetFontData()->GetSubFaceIndex())) : FText::GetEmpty())
			]
			// scaling factor
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Scaling Factor: %3.3f"), InRow->GetScaling())))
			]
			// range
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Unicode Range: %s"), *BlockRangeName)))
			]
			// tags
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(TagsText)
			]
		);
	
	Invalidate(EInvalidateWidgetReason::PaintAndVolatility);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
