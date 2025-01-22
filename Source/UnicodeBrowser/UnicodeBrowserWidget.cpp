﻿// SPDX-FileCopyrightText: 2025 NTY.studio
#include "UnicodeBrowser/UnicodeBrowserWidget.h"

#include "ISinglePropertyView.h"
#include "SlateOptMacros.h"
#include "UnicodeBrowserOptions.h"
#include "Fonts/UnicodeBlockRange.h"

#include "Framework/Application/SlateApplication.h"

#include "Modules/ModuleManager.h"

#include "Widgets/SUnicodeBrowserSidePanel.h"
#include "Widgets/SUnicodeCharacterGridEntry.h"
#include "Widgets/Images/SLayeredImage.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STileView.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SUnicodeBrowserWidget::~SUnicodeBrowserWidget()
{
	UToolMenus::Get()->RemoveMenu("UnicodeBrowser.Settings");
	UToolMenus::Get()->RemoveMenu("UnicodeBrowser.Font");
}

void SUnicodeBrowserWidget::Construct(FArguments const& InArgs)
{
	CurrentFont = UUnicodeBrowserOptions::Get()->GetFontInfo();
	
	UUnicodeBrowserOptions::Get()->OnFontChanged.RemoveAll(this);
	UUnicodeBrowserOptions::Get()->OnFontChanged.AddLambda([UnicodeBrowser = AsWeak()]()
	{
		if(UnicodeBrowser.IsValid()){
			static_cast<SUnicodeBrowserWidget*>(UnicodeBrowser.Pin().Get())->MarkDirty(static_cast<uint8>(EDirtyFlags::FONT));
		}
	});

	// create a dummy for the preview until the user highlights a character
	CurrentRow = MakeShared<FUnicodeBrowserRow>(UnicodeBrowser::InvalidSubChar, EUnicodeBlockRange::Specials, &CurrentFont);
	

	SearchBar = SNew(SUbSearchBar)
		.OnTextChanged(this, &SUnicodeBrowserWidget::FilterByString);
	
	
	// generate the settings context menu
	UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("UnicodeBrowser.Settings");	
	UToolMenus::Get()->AssembleMenuHierarchy(Menu,
	{
		this->CreateMenuSection_Settings(),
		SearchBar->CreateMenuSection_Settings()
	});

	TSharedPtr<SComboButton> SettingsButton = SNew( SComboButton )
		.HasDownArrow(false)
		.ContentPadding(0.0f)
		.ForegroundColor( FSlateColor::UseForeground() )
		.ButtonStyle( FAppStyle::Get(), "SimpleButton" )
		.MenuContent()
		[			
			UToolMenus::Get()->GenerateWidget(Menu->GetMenuName(), Menu)			
		]
		.ButtonContent()
		[
			SNew(SLayeredImage)
			.Image(FAppStyle::Get().GetBrush("DetailsView.ViewOptions"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		];


	// generate the preset/font settings context menu
	UToolMenu* MenuFont = UToolMenus::Get()->RegisterMenu("UnicodeBrowser.Font");
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		
		FToolMenuSection &PresetSettingsSection = MenuFont->AddSection(TEXT("PresetSettings"), INVTEXT("preset"));
		{
			FSinglePropertyParams SinglePropertyParams;
			SinglePropertyParams.NamePlacement = EPropertyNamePlacement::Type::Hidden;
			TSharedRef<SWidget> Widget = SNew(SBox)
				.Padding(15, 0, 0, 0)
				[
					PropertyEditor.CreateSingleProperty(UUnicodeBrowserOptions::Get(), "Preset", SinglePropertyParams).ToSharedRef()
				];
					
			PresetSettingsSection.AddEntry(FToolMenuEntry::InitWidget("FontInfo",  Widget, FText::GetEmpty()));
		}
		
		FToolMenuSection &FontSettingsSection = MenuFont->AddSection(TEXT("FontSettings"), INVTEXT("font"));
		{		
			FSinglePropertyParams SinglePropertyParams;
			SinglePropertyParams.NamePlacement = EPropertyNamePlacement::Type::Hidden;
			TSharedRef<SWidget> Widget = SNew(SBox)
				.Padding(15, 0, 0, 0)
				[
					PropertyEditor.CreateSingleProperty(UUnicodeBrowserOptions::Get(), "Font", SinglePropertyParams).ToSharedRef()
				];
			FontSettingsSection.AddEntry(FToolMenuEntry::InitWidget("FontInfo",  Widget, FText::GetEmpty()));
		}

		{		
			FSinglePropertyParams SinglePropertyParams;
			SinglePropertyParams.NameOverride = INVTEXT("Typeface");
			TSharedRef<SWidget> Widget = SNew(SBox)
				.Padding(15, 0, 0, 0)
				[
					PropertyEditor.CreateSingleProperty(UUnicodeBrowserOptions::Get(), "FontTypeFace", SinglePropertyParams).ToSharedRef()
				];
			FontSettingsSection.AddEntry(FToolMenuEntry::InitWidget("FontTypeFace",  Widget, FText::GetEmpty()));
		}
		
		{
			TSharedRef<SWidget> Widget = SNew(SBox)
			.Padding(15, 0, 0, 0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(INVTEXT("font size"))
				]
				+SHorizontalBox::Slot()
				.Padding(10, 0, 10, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SSpinBox<float>)
					.Delta(1)
					.Justification(EHorizontalAlignment::HAlign_Left)
					.MinValue(6)
					.MinDesiredWidth(150)
					.MaxValue(200)
					.Value_Lambda([this](){ return CurrentFont.Size; })
					.OnValueChanged_Lambda([this](float CurrentValue)
					{
						CurrentFont.Size = CurrentValue;
						UUnicodeBrowserOptions::Get()->SetFontInfo(CurrentFont);
						CharactersTileView->RebuildList();
					})
				]
			];
				
			FontSettingsSection.AddEntry(FToolMenuEntry::InitWidget("BrowserFontSize",  Widget, FText::GetEmpty(), false, false, false, INVTEXT("you can use CTRL + MouseWheel to adjust font size in the browser")));
		}
			
	}
	
	TSharedPtr<SComboButton> SettingsButtonFont = SNew( SComboButton )
			.HasDownArrow(true)
			.ContentPadding(0.0f)
			.ForegroundColor( FSlateColor::UseForeground() )
			.ButtonStyle( FAppStyle::Get(), "SimpleButton" )
			.MenuContent()
			[			
				UToolMenus::Get()->GenerateWidget(MenuFont->GetMenuName(), MenuFont)			
			]
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(INVTEXT("Preset / Font"))
			];

	
	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		.ResizeMode(ESplitterResizeMode::Fill)
		+ SSplitter::Slot()
			.SizeRule(SSplitter::SizeToContent)
			[
				SNew(SBox)
				.Padding(10)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SearchBar.ToSharedRef()
					]
					+SHorizontalBox::Slot()
					.AutoWidth()			
					[
						SNew(SSpacer)
						.Size(FVector2D(10, 1))
					]					
					+SHorizontalBox::Slot()
					.AutoWidth()			
					[
						SNew(SSpacer)
						.Size(FVector2D(10, 1))
					]					
					+SHorizontalBox::Slot()
					.AutoWidth()			
					[
						SettingsButtonFont.ToSharedRef()
					]
					+SHorizontalBox::Slot()
					.AutoWidth()			
					[
						SNew(SSpacer)
						.Size(FVector2D(10, 1))
					]					
					+SHorizontalBox::Slot()
					.AutoWidth()			
					[
						SettingsButton.ToSharedRef()
					]
				]
			]
			+ SSplitter::Slot()
			.SizeRule(SSplitter::FractionOfParent)
			.Value(1.0)
			[
			SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				.ResizeMode(ESplitterResizeMode::Fill)
				+ SSplitter::Slot()
				.SizeRule(SSplitter::FractionOfParent)
				.Value(0.7)
				[
					SAssignNew(CharactersTileView, STileView<TSharedPtr<FUnicodeBrowserRow>>)
					.ListItemsSource(&CharacterWidgetsArray)
					.ItemAlignment(EListItemAlignment::EvenlySize)
					.SelectionMode(ESelectionMode::None)
					.OnGenerateTile(this, &SUnicodeBrowserWidget::GenerateItemRow)
				]
				+ SSplitter::Slot()
				.SizeRule(SSplitter::FractionOfParent)
				.Value(0.3)
				[
					SAssignNew(SidePanel, SUnicodeBrowserSidePanel).UnicodeBrowser(SharedThis(this))
				]				
			]
	];

	SetSidePanelVisibility(UUnicodeBrowserOptions::Get()->bShowSidePanel);
	
	MarkDirty(static_cast<uint8>(EDirtyFlags::INIT) | static_cast<uint8>(EDirtyFlags::FONT));
}

UToolMenu* SUnicodeBrowserWidget::CreateMenuSection_Settings()
{
	UToolMenu *Menu = UToolMenus::Get()->GenerateMenu("UnicodeBrowser.Settings.General_Display", FToolMenuContext());
	
	FToolMenuSection &GeneralSettingsSection = Menu->AddSection(TEXT("GeneralSettings"), INVTEXT("general settings"));
	{
		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					UUnicodeBrowserOptions::Get()->bShowSidePanel = !UUnicodeBrowserOptions::Get()->bShowSidePanel;
					UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
					SetSidePanelVisibility(UUnicodeBrowserOptions::Get()->bShowSidePanel);
					//RangeScrollbox->RebuildList();
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return UUnicodeBrowserOptions::Get()->bShowSidePanel; })
			);
			
			GeneralSettingsSection.AddMenuEntry("ShowSidePanel", INVTEXT("show sidepanel"), FText::GetEmpty(), FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton);
		}
		
		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					UUnicodeBrowserOptions::Get()->bShowMissing = !UUnicodeBrowserOptions::Get()->bShowMissing;
					UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
					UpdateCharacters();
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return UUnicodeBrowserOptions::Get()->bShowMissing; })
			);
			
			GeneralSettingsSection.AddMenuEntry("ShowMissingCharacters", INVTEXT("show missing characters"), INVTEXT("should characters which are missing in the font be shown?"), FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton);
		}

		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					UUnicodeBrowserOptions::Get()->bShowZeroSize = !UUnicodeBrowserOptions::Get()->bShowZeroSize;	
					UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
					UpdateCharacters();
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()	{ return UUnicodeBrowserOptions::Get()->bShowZeroSize; })
			);
			
			GeneralSettingsSection.AddMenuEntry("ShowZeroSizeCharacters", INVTEXT("show zero size"), INVTEXT("show Characters which have a measurement of 0x0 (primary for debug purposes)"), FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton);
		}

		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					UUnicodeBrowserOptions::Get()->bCacheCharacterMetaOnLoad = !UUnicodeBrowserOptions::Get()->bCacheCharacterMetaOnLoad;
					UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()	{ return UUnicodeBrowserOptions::Get()->bCacheCharacterMetaOnLoad; })
			);
			
			GeneralSettingsSection.AddMenuEntry("CacheCharacterMeta", INVTEXT("cache character meta data"), INVTEXT("Cache the Character meta information while loading the font, this is slower while changing fonts, but may reduce delay for displaying character previews"), FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton);
		}

		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange = !UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange;
					UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
					if(UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange)
					{
						SidePanel->SelectAllRangesWithCharacters(RowsRaw);
					}
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()	{ return UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange; })
			);
			
			GeneralSettingsSection.AddMenuEntry("AutoSetRangeOnFontChange", INVTEXT("auto set ranges on font change"), INVTEXT("automatic enable all ranges which are covered by the current font"), FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton);
		}
	}

	FToolMenuSection &DisplaySettingsSection = Menu->AddSection(TEXT("DisplaySettings"), INVTEXT("display settings"));
	{
		{
			TSharedRef<SWidget> Widget = SNew(SBox)
			.Padding(15, 0, 0, 0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(INVTEXT("columns"))
				]
				+SHorizontalBox::Slot()
				.Padding(10, 0, 10, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SSpinBox<int32>)
					.Delta(1)
					.MinValue(1)
					.MaxValue(32)
					.Value_Lambda([this]()  { return UUnicodeBrowserOptions::Get()->NumCols; })
					.OnValueChanged_Lambda([this](int32 CurrentValue)
					{
						UUnicodeBrowserOptions::Get()->NumCols = CurrentValue;						
						UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
						CharactersTileView->RebuildList();
					})
				]
			];
			
			DisplaySettingsSection.AddEntry(FToolMenuEntry::InitWidget("BrowserNumColumns",  Widget, FText::GetEmpty(), false, false, false, INVTEXT("you can use CTRL + Shift + MouseWheel to adjust column count in the browser")));
		}
		
		
	}

	return Menu;
}

void SUnicodeBrowserWidget::SetSidePanelVisibility(bool bVisible)
{
	SidePanel->SetVisibility(bVisible ? EVisibility::Visible : EVisibility::Collapsed);
}


void SUnicodeBrowserWidget::MarkDirty(uint8 Flags)
{
	DirtyFlags |= Flags;
	// schedule an update on the next tick
	SetCanTick(true);
}

TSharedRef<ITableRow> SUnicodeBrowserWidget::GenerateItemRow(TSharedPtr<FUnicodeBrowserRow> CharacterData, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FUnicodeBrowserRow>>, OwnerTable)
	[
		SNew(SUnicodeCharacterGridEntry)
			.FontInfo(CurrentFont)
			.UnicodeCharacter(CharacterData)
			.OnMouseMove(this, &SUnicodeBrowserWidget::OnCharacterMouseMove, CharacterData)
			.OnZoomFontSize(this, &SUnicodeBrowserWidget::HandleZoomFont)
	];
}


void SUnicodeBrowserWidget::Tick(FGeometry const& AllottedGeometry, double const InCurrentTime, float const InDeltaTime)
{
	if (DirtyFlags && IsConstructed())  // wait for the widget to be constructed before updating
	{
		if(DirtyFlags & static_cast<uint8>(EDirtyFlags::FONT))
		{
			CurrentFont = UUnicodeBrowserOptions::Get()->GetFontInfo();

			if(DirtyFlags & static_cast<uint8>(EDirtyFlags::FONT_FACE))
			{
				// rebuild the character list


				// ensure all ranges are checked when auto set ranges is enabled
				if(UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange)
				{
					for(FUnicodeBlockRange const &Range : UnicodeBrowser::GetUnicodeBlockRanges())
					{
						if(SidePanel.IsValid() && SidePanel->RangeSelector.IsValid())
						{
							SidePanel->RangeSelector->SetRanges({ Range.Index }, false);
						}
					}
				}
				
				PopulateSupportedCharacters();
				if(UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange)
				{
					SidePanel->SelectAllRangesWithCharacters(RowsRaw);
				}
			
				OnFontChanged.Broadcast(&CurrentFont);
			
				DirtyFlags &= ~static_cast<uint8>(EDirtyFlags::FONT_FACE);
			}

			if(DirtyFlags & static_cast<uint8>(EDirtyFlags::FONT_STYLE))
			{
				CharactersTileView->RebuildList();
				DirtyFlags &= ~static_cast<uint8>(EDirtyFlags::FONT_STYLE);
			}		
		}

		if(DirtyFlags & static_cast<uint8>(EDirtyFlags::INIT)){
			if(SidePanel.IsValid() && SidePanel->RangeSelector.IsValid())
			{
				// only use a default preset if this option is disabled, otherwise it causes trouble when reopening the Tab while a valid font is still set in UUnicodeBrowserOptions 
				if(!UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange)
				{
					// default preset		
					SidePanel->RangeSelector->SetRanges(UnicodeBrowser::SymbolRanges);
				}

				DirtyFlags &= ~static_cast<uint8>(EDirtyFlags::INIT);
			}
		}
	}

	SetCanTick(DirtyFlags);
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}



void SUnicodeBrowserWidget::PopulateSupportedCharacters()
{
	RowsRaw.Empty();
	RowsRaw.Reserve(UnicodeBrowser::GetUnicodeBlockRanges().Num());

	for (auto const& Range : UnicodeBrowser::GetUnicodeBlockRanges())
	{
		RowsRaw.Add(Range.Index);
		TArray<TSharedPtr<FUnicodeBrowserRow>>& RangeArray = RowsRaw.FindChecked(Range.Index);

		for (int CharCode = Range.Range.GetLowerBound().GetValue(); CharCode <= Range.Range.GetUpperBound().GetValue(); ++CharCode)
		{
			auto Row = MakeShared<FUnicodeBrowserRow>(CharCode, Range.Index, &CurrentFont);

			if (UUnicodeBrowserOptions::Get()->bCacheCharacterMetaOnLoad)
			{
				Row->Preload();
			}

			RangeArray.Add(Row);
		}
	}

	UpdateCharacters();
}

void SUnicodeBrowserWidget::UpdateCharacters()
{
	Rows.Empty();
	Rows.Reserve(RowsRaw.Num());

	for(const auto &[Range, RawRangeRows] : RowsRaw)
	{
		// we can skip the whole range if it's not selected
		if(!SidePanel->RangeSelector->IsRangeChecked(Range))
			continue;	
		
		TArray<TSharedPtr<FUnicodeBrowserRow>> RowsFiltered;
		RowsFiltered = RawRangeRows.FilterByPredicate([](TSharedPtr<FUnicodeBrowserRow> const &RawRow)
		{
			if(RawRow->bFilteredByTag)
				return false;
			
			if (!UUnicodeBrowserOptions::Get()->bShowMissing && !RawRow->CanLoadCodepoint())
				return false;

			if(!UUnicodeBrowserOptions::Get()->bShowZeroSize && RawRow->GetMeasurements().IsZero())
				return false;

			return true;
		});

		if(!RowsFiltered.IsEmpty())
		{
			Rows.Add(Range, RowsFiltered);
		}
	}

	UpdateCharactersArray();
}

void SUnicodeBrowserWidget::UpdateCharactersArray()
{
	int CharacterCount = 0;
	for(const auto &[Range, RangeRows] : Rows)
	{
		CharacterCount += RangeRows.Num();
	}
	
	CharacterWidgetsArray.Empty();	
	CharacterWidgetsArray.Reserve(CharacterCount);

	for(const auto &[Range, FilteredRangeRows] : Rows){
		for(auto &Character : FilteredRangeRows)
		{
			CharacterWidgetsArray.Add(Character);
		}
	}
}


void SUnicodeBrowserWidget::FilterByString(FString Needle)
{
	bool const bFilterTags = Needle.Len() > 0 && UUnicodeBrowserOptions::Get()->Preset && UUnicodeBrowserOptions::Get()->Preset->SupportsFont(CurrentFont);

	// build an array which include all single character search terms
	TArray<FString> CharacterNeedles;
	Needle.ParseIntoArray(CharacterNeedles, TEXT(","));
	for(int Idx=CharacterNeedles.Num() - 1; Idx >= 0; Idx--)
	{
		CharacterNeedles[Idx].TrimStartAndEndInline();
		if(CharacterNeedles[Idx].Len() > 1)
		{
			CharacterNeedles.RemoveAtSwap(Idx);
		}
	}
	
	bool const bFilterByCharacter = CharacterNeedles.Num() > 0;

	bool const bCaseSensitive = UUnicodeBrowserOptions::Get()->bSearch_CaseSensitive;
	
	bool bNeedUpdate = false;
	
	TArray<int32> Whitelist;
	if(bFilterTags){
		Whitelist = UUnicodeBrowserOptions::Get()->Preset->GetCharactersByNeedle(Needle);
	}

	for(auto &[Range, RawRangeRows] : RowsRaw)
	{
		bool bRangeHasMatch = false;
		for(TSharedPtr<FUnicodeBrowserRow> &RowRaw : RawRangeRows)
		{
			bool bToggleRowState = Needle.IsEmpty() && RowRaw->bFilteredByTag;
			
			if(!bToggleRowState && bFilterTags)
			{
				bToggleRowState |= !Whitelist.Contains(RowRaw->Codepoint) != RowRaw->bFilteredByTag;
			}

			if(!bToggleRowState && bFilterByCharacter)
			{
				bool const bMatchesCharacter = CharacterNeedles.ContainsByPredicate([bCaseSensitive, RowRaw](FString const &CharacterNeedle)
				{
					return CharacterNeedle.Equals(RowRaw->Character, bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase);
				});
				
				bToggleRowState |= !bMatchesCharacter != RowRaw->bFilteredByTag;				
			}

			if(bToggleRowState)
			{
				RowRaw->bFilteredByTag = !RowRaw->bFilteredByTag;
				bNeedUpdate = true;
				bRangeHasMatch = true;
			}
		}

		// ensure that the necessary ranges are selected before UpdateCharacters() is updating the filtered list
		if(bRangeHasMatch && UUnicodeBrowserOptions::Get()->bSearch_AutoSetRange){
			if(!SidePanel->RangeSelector->IsRangeChecked(Range))
			{
				SidePanel->RangeSelector->SetRanges({ Range }, false);
			}
		}			
	}

	if(bNeedUpdate){
		UpdateCharacters();
		CharactersTileView->RebuildList();
	}
}


FReply SUnicodeBrowserWidget::OnCharacterMouseMove(FGeometry const& Geometry, FPointerEvent const& PointerEvent, TSharedPtr<FUnicodeBrowserRow> Row) const
{
	if (CurrentRow == Row) return FReply::Unhandled();
	CurrentRow = Row;

	OnCharacterHighlight.ExecuteIfBound(Row.Get());	
	return FReply::Handled();
}


void SUnicodeBrowserWidget::HandleZoomFont(float Offset)
{	
	CurrentFont.Size = FMath::Max(1.0f, CurrentFont.Size + Offset);
	UUnicodeBrowserOptions::Get()->SetFontInfo(CurrentFont);
	CharactersTileView->RebuildList();
}


void SUnicodeBrowserWidget::HandleZoomColumns(float Offset)
{
	// we want inverted behavior for columns
	UUnicodeBrowserOptions::Get()->NumCols = FMath::Max(1, FMath::RoundToInt(UUnicodeBrowserOptions::Get()->NumCols - Offset));
	
	CharactersTileView->RebuildList();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
