// all rights reserved
#include "SUnicodeBlockRangeSelector.h"

#include "SSimpleButton.h"
#include "SlateOptMacros.h"

#include "UnicodeBrowser/UnicodeBrowserOptions.h"
#include "UnicodeBrowser/UnicodeBrowserStatic.h"
#include "UnicodeBrowser/UnicodeBrowserWidget.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SUnicodeBlockRangeSelector::Construct(FArguments const& InArgs)
{
	if (!InArgs._UnicodeBrowser.IsValid())
		return;

	UnicodeBrowser = InArgs._UnicodeBrowser;

	CheckboxIndices.Reset();
	CheckboxIndices.Reserve(UnicodeBrowser::GetUnicodeBlockRanges().Num());

	CheckBoxList = SNew(SUbCheckBoxList)
		.ItemHeaderLabel(FText::FromString(TEXT("Unicode Block Ranges")))
		.IncludeGlobalCheckBoxInHeaderRow(true);

	CheckBoxList->OnItemCheckStateChanged.BindSP(this, &SUnicodeBlockRangeSelector::UpdateRangeVisibility);

	for (FUnicodeBlockRange const& Range : UnicodeBrowser::GetUnicodeBlockRanges())
	{
		auto ItemWidget = SNew(SSimpleButton)
			.Text_Lambda(
				[UnicodeBrowser = UnicodeBrowser, Range = Range]()
				{
					if (!UnicodeBrowser.IsValid())
						return FText::GetEmpty();

					int32 const Num = UnicodeBrowser.Pin().Get()->Rows.Contains(Range.Index) ? UnicodeBrowser.Pin().Get()->Rows.FindChecked(Range.Index).Num() : 0;
					return FText::FromString(FString::Printf(TEXT("%s (%d)"), *Range.GetDisplayName().ToString(), Num));
				}
			)
			.ToolTipText(
				FText::FromString(
					FString::Printf(
						TEXT("%s: Range U+%-06.04X Codes U+%-06.04X - U+%-06.04X"),
						*Range.GetDisplayName().ToString(),
						Range.Index,
						Range.GetRange().GetLowerBoundValue(),
						Range.GetRange().GetUpperBoundValue()
					)
				)
			)
			.OnClicked(this, &SUnicodeBlockRangeSelector::RangeClicked, Range.Index);

		int32 Index = CheckBoxList->AddItem(ItemWidget, false);
		CheckboxIndices.Add(Range.Index, Index);
	}

	UnicodeBrowser.Pin()->OnFontChanged.AddSP(this, &SUnicodeBlockRangeSelector::UpdateRowVisibility);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SCheckBox)
			.IsChecked(UUnicodeBrowserOptions::Get()->bRangeSelector_HideEmptyRanges)
			.OnCheckStateChanged_Lambda(
				[this](ECheckBoxState const State)
				{
					UUnicodeBrowserOptions::Get()->bRangeSelector_HideEmptyRanges = State == ECheckBoxState::Checked;
					UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
					UpdateRowVisibility(nullptr);
				}
			)
			[
				SNew(STextBlock)
				.Text(INVTEXT("hide empty ranges"))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSpacer)
			.Size(FVector2D(1, 10))
		]
		+ SVerticalBox::Slot()
		[
			CheckBoxList.ToSharedRef()
		]
	];
}

void SUnicodeBlockRangeSelector::SetRanges(TArray<EUnicodeBlockRange> const& RangesToSet, bool const bExclusive)
{
	// update all checkboxes and keep the amount of state updates as low as possible to avoid redraw
	for (auto const& Range : UnicodeBrowser::GetUnicodeBlockRanges())
	{
		if (!CheckboxIndices.Contains(Range.Index))
		{
			UE_LOG(LogTemp, Warning, TEXT("No checkbox index found for range %d."), Range.Index);
			continue;
		}

		if (RangesToSet.Contains(Range.Index))
		{
			CheckBoxList->SetItemChecked(CheckboxIndices[Range.Index], ECheckBoxState::Checked);
		}
		else if (bExclusive)
		{
			CheckBoxList->SetItemChecked(CheckboxIndices[Range.Index], ECheckBoxState::Unchecked);
		}
	}
}

void SUnicodeBlockRangeSelector::Tick(FGeometry const& AllottedGeometry, double const InCurrentTime, float const InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (bSelectionChanged)
	{
		OnRangeSelectionChanged.ExecuteIfBound();
		bSelectionChanged = false;
	}

	SetCanTick(false);
}

void SUnicodeBlockRangeSelector::UpdateRangeVisibility(int32 const Index)
{
	auto const RangeFound = CheckboxIndices.FindKey(Index);
	if (!RangeFound) return;
	auto const Range = *RangeFound;
	OnRangeStateChanged.ExecuteIfBound(Range, CheckBoxList->IsItemChecked(Index));
	bSelectionChanged = true;
	SetCanTick(true);
}

void SUnicodeBlockRangeSelector::UpdateRowVisibility(FSlateFontInfo* FontInfo)
{
	if (!UnicodeBrowser.IsValid()) return;

	bool const bHideEmptyRanges = UUnicodeBrowserOptions::Get()->bRangeSelector_HideEmptyRanges;
	for (auto const& [Range, ItemIndex] : CheckboxIndices)
	{
		int32 const CharacterCount = UnicodeBrowser.Pin().Get()->Rows.Contains(Range) ? UnicodeBrowser.Pin().Get()->Rows.FindChecked(Range).Num() : 0;
		CheckBoxList->Items[ItemIndex].Get().bIsVisible = !bHideEmptyRanges || CharacterCount > 0;
	}

	CheckBoxList->UpdateItems();
}

FReply SUnicodeBlockRangeSelector::RangeClicked(EUnicodeBlockRange const BlockRange) const
{
	ensureAlways(CheckboxIndices.Contains(BlockRange));
	OnRangeClicked.ExecuteIfBound(BlockRange);
	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
