// all rights reserved
#include "SUnicodeBrowserSidePanel.h"

#include "SUnicodeCharacterInfo.h"
#include "SlateOptMacros.h"

#include "HAL/PlatformApplicationMisc.h"
#include "UnicodeBrowser/UnicodeBrowserStatic.h"

#include "UnicodeBrowser/UnicodeBrowserWidget.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SUnicodeBrowserSidePanel::Construct(FArguments const& InArgs)
{
	if (!InArgs._UnicodeBrowser.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[SUnicodeBrowserSidePanel::Construct] Invalid UnicodeBrowser pointer"));
		return;
	}

	UnicodeBrowser = InArgs._UnicodeBrowser;

	SSplitter::Construct(
		SSplitter::FArguments()
		.Orientation(Orient_Vertical)
		.ResizeMode(ESplitterResizeMode::Fill)
		+ SSplitter::Slot()
		  .SizeRule(SSplitter::FractionOfParent)
		  .Value(0.25)
		  .MinSize(50)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SAssignNew(CurrentCharacterView, STextBlock)
				.Visibility(EVisibility::Visible)
				.OnDoubleClicked_Lambda(
					[UnicodeBrowser = UnicodeBrowser](FGeometry const& Geometry, FPointerEvent const& PointerEvent)
					{
						if (!UnicodeBrowser.IsValid())
							return FReply::Unhandled();

						FPlatformApplicationMisc::ClipboardCopy(*UnicodeBrowser.Pin().Get()->CurrentRow->Character);
						return FReply::Handled();
					}
				)
				.Font(UnicodeBrowser.Pin().Get()->CurrentFont)
				.Justification(ETextJustify::Center)
				.IsEnabled(true)
				.ToolTipText(
					FText::FromString(
						FString::Printf(TEXT("Char Code: U+%-06.04X. Double-Click to copy: %s."), UnicodeBrowser.Pin().Get()->CurrentRow->Codepoint, *UnicodeBrowser.Pin().Get()->CurrentRow->Character)
					)
				)
				.Text(FText::FromString(FString::Printf(TEXT("%s"), *UnicodeBrowser.Pin().Get()->CurrentRow->Character)))
			]
		]
		+ SSplitter::Slot()
		  .SizeRule(SSplitter::FractionOfParent)
		  .Value(0.75)
		[
			SNew(SScrollBox)
			.Orientation(Orient_Vertical)
			+ SScrollBox::Slot()
			.FillSize(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SNew(SExpandableArea)
					.HeaderPadding(FMargin(2, 4))
					.HeaderContent()
					[
						SNew(STextBlock)
						.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
						.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
						.Text(INVTEXT("Character Info"))
					]
					.BodyContent()
					[
						SAssignNew(CurrentCharacterDetails, SUnicodeCharacterInfo)
						.Row(UnicodeBrowser.Pin().Get()->CurrentRow)
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1)
				.FillContentHeight(1)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					MakeBlockRangesSidebar()
				]
			]
		]
	);

	UnicodeBrowser.Pin().Get()->OnCharacterHighlight.BindSPLambda(
		CurrentCharacterView.Get(),
		[this](FUnicodeBrowserRow* CharacterInfo)
		{
			// update the preview glyph/tooltip
			CurrentCharacterView->SetText(FText::FromString(CharacterInfo->Character));
			CurrentCharacterView->SetToolTipText(FText::FromString(FString::Printf(TEXT("Char Code: U+%-06.04X. Double-Click to copy: %s."), CharacterInfo->Codepoint, *CharacterInfo->Character)));

			// update the glyph details
			CurrentCharacterDetails->SetRow(CharacterInfo->AsShared());
		}
	);

	UnicodeBrowser.Pin().Get()->OnFontChanged.AddSPLambda(
		CurrentCharacterView.Get(),
		[CurrentCharacterView = CurrentCharacterView](FSlateFontInfo* FontInfo)
		{
			CurrentCharacterView->SetFont(*FontInfo);
		}
	);
}

TSharedRef<SExpandableArea> SUnicodeBrowserSidePanel::MakeBlockRangesSidebar()
{
	RangeSelector = SNew(SUnicodeBlockRangeSelector).UnicodeBrowser(UnicodeBrowser);

	RangeSelector->OnRangeClicked.BindSPLambda(
		this,
		[this](EUnicodeBlockRange const BlockRange)
		{
			if (UnicodeBrowser.IsValid() && UnicodeBrowser.Pin().Get()->CharactersTileView.IsValid())
			{
				if (TArray<TSharedPtr<FUnicodeBrowserRow>>* RangeCharacters = UnicodeBrowser.Pin().Get()->Rows.Find(BlockRange))
				{
					if (RangeCharacters->Num() > 0)
					{
						// we scroll to the first character within that range
						TSharedPtr<FUnicodeBrowserRow> const& Character = (*RangeCharacters)[0];

						// we can't use animated scroll as the layout invalidation of the RangeWidgets would be to early
						// see PR: https://github.com/EpicGames/UnrealEngine/pull/12580
						UnicodeBrowser.Pin().Get()->CharactersTileView->RequestScrollIntoView(Character);
					}
				}
			}
		}
	);

	// this method gets called a tick late, so that a preset can modify multiple states and the character list will only be updated once
	RangeSelector->OnRangeSelectionChanged.BindSPLambda(
		this,
		[this]()
		{
			UnicodeBrowser.Pin()->UpdateCharacters();
			UnicodeBrowser.Pin()->CharactersTileView->RebuildList();
		}
	);

	return SNew(SExpandableArea)
		.HeaderPadding(FMargin(2, 4))
		.HeaderContent()
		[
			SNew(STextBlock)
			.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
			.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
			.Text(FText::FromString(TEXT("Unicode Block Ranges")))
		]
		.BodyContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(0, 4)
			.HAlign(HAlign_Fill)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Preset: Blocks with Characters")))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda(
					[this]()
					{
						if (UnicodeBrowser.IsValid())
						{
							SelectAllRangesWithCharacters(UnicodeBrowser.Pin()->RowsRaw);
						}
						return FReply::Handled();
					}
				)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(0, 4)
			.HAlign(HAlign_Fill)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Preset: Symbols")))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda(
					[RangeSelector = RangeSelector]()
					{
						RangeSelector->SetRanges(UnicodeBrowser::SymbolRanges);
						return FReply::Handled();
					}
				)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1)
			.FillContentHeight(1)
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				RangeSelector.ToSharedRef()
			]
		];
}

void SUnicodeBrowserSidePanel::SelectAllRangesWithCharacters(TMap<EUnicodeBlockRange, TArray<TSharedPtr<FUnicodeBrowserRow>>>& Rows, bool const bExclusive) const
{
	TArray<EUnicodeBlockRange> Ranges;
	for (auto const& [Range, Characters] : Rows)
	{
		if (!Characters.IsEmpty()) Ranges.Add(Range);
	}
	RangeSelector->SetRanges(Ranges, bExclusive);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
