// SPDX-FileCopyrightText: 2025 NTY.studio
#include "UnicodeBrowser/UnicodeBrowserWidget.h"

#include "Editor.h"
#include "ISinglePropertyView.h"
#include "SlateOptMacros.h"
#include "ToolMenus.h"
#include "UnicodeBrowserOptions.h"

#include "Fonts/UnicodeBlockRange.h"

#include "Framework/Application/SlateApplication.h"

#include "Modules/ModuleManager.h"

#include "UnicodeBrowser/DataAsset_FontTags.h"
#include "UnicodeBrowser/UnicodeBrowserStatic.h"

#include "Widgets/SUnicodeBrowserSidePanel.h"
#include "Widgets/SUnicodeCharacterGridEntry.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STileView.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SUnicodeBrowserWidget::~SUnicodeBrowserWidget()
{
	UToolMenus::Get()->RemoveMenu("UnicodeBrowser.Settings");
	UToolMenus::Get()->RemoveMenu("UnicodeBrowser.Font");
	UUnicodeBrowserOptions::Get()->OnFontChanged.RemoveAll(this);
	CleanUpDisableCPUThrottlingDelegate();
}

FReply SUnicodeBrowserWidget::OnMouseMove(FGeometry const& MyGeometry, FPointerEvent const& MouseEvent)
{
	DisableThrottlingTemporarily();
	return SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);
}

void SUnicodeBrowserWidget::Construct(FArguments const& InArgs)
{
	SetUpDisableCPUThrottlingDelegate();
	CurrentFont = UUnicodeBrowserOptions::Get()->GetFontInfo();

	UUnicodeBrowserOptions::Get()->OnFontChanged.RemoveAll(this);
	UUnicodeBrowserOptions::Get()->OnFontChanged.AddLambda(
		[this, UnicodeBrowser = AsWeak()]()
		{
			if (UnicodeBrowser.IsValid())
			{
				if (CurrentFont.FontObject != UUnicodeBrowserOptions::Get()->GetFontInfo().FontObject
					|| CurrentFont.TypefaceFontName != UUnicodeBrowserOptions::Get()->GetFontInfo().TypefaceFontName)
				{
					static_cast<SUnicodeBrowserWidget*>(UnicodeBrowser.Pin().Get())->MarkDirty(static_cast<uint8>(EDirtyFlags::FONT));
				}
				else
				{
					static_cast<SUnicodeBrowserWidget*>(UnicodeBrowser.Pin().Get())->MarkDirty(static_cast<uint8>(EDirtyFlags::FONT_STYLE));
				}
			}
		}
	);

	// create a dummy for the preview until the user highlights a character
	CurrentRow = MakeShared<FUnicodeBrowserRow>(UnicodeBrowser::InvalidSubChar, EUnicodeBlockRange::Specials, &CurrentFont);

	SearchBar = SNew(SUbSearchBar)
		.OnTextChanged(this, &SUnicodeBrowserWidget::FilterByString);

	// generate the settings context menu
	UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("UnicodeBrowser.Settings");
	UToolMenus::Get()->AssembleMenuHierarchy(
		Menu,
		{
			this->CreateMenuSection_Settings(),
			SearchBar->CreateMenuSection_Settings()
		}
	);

	TSharedPtr<SComboButton> SettingsButton = SNew(SComboButton)
		.HasDownArrow(false)
		.ContentPadding(0.0f)
		.ForegroundColor(FSlateColor::UseForeground())
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
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

		FToolMenuSection& PresetSettingsSection = MenuFont->AddSection(TEXT("PresetSettings"), INVTEXT("preset"));
		{
			FSinglePropertyParams SinglePropertyParams;
			SinglePropertyParams.NamePlacement = EPropertyNamePlacement::Type::Hidden;
			TSharedRef<SWidget> Widget = SNew(SBox)
				.Padding(15, 0, 0, 0)
				[
					PropertyEditor.CreateSingleProperty(UUnicodeBrowserOptions::Get(), "Preset", SinglePropertyParams).ToSharedRef()
				];

			PresetSettingsSection.AddEntry(FToolMenuEntry::InitWidget("FontInfo", Widget, FText::GetEmpty()));
		}

		FToolMenuSection& FontSettingsSection = MenuFont->AddSection(TEXT("FontSettings"), INVTEXT("font"));
		{
			FSinglePropertyParams SinglePropertyParams;
			SinglePropertyParams.NamePlacement = EPropertyNamePlacement::Type::Hidden;
			TSharedRef<SWidget> Widget = SNew(SBox)
				.Padding(15, 0, 0, 0)
				[
					PropertyEditor.CreateSingleProperty(UUnicodeBrowserOptions::Get(), "Font", SinglePropertyParams).ToSharedRef()
				];
			FontSettingsSection.AddEntry(FToolMenuEntry::InitWidget("FontInfo", Widget, FText::GetEmpty()));
		}

		{
			FSinglePropertyParams SinglePropertyParams;
			SinglePropertyParams.NameOverride = INVTEXT("Typeface");
			TSharedRef<SWidget> Widget = SNew(SBox)
				.Padding(15, 0, 0, 0)
				[
					PropertyEditor.CreateSingleProperty(UUnicodeBrowserOptions::Get(), "FontTypeFace", SinglePropertyParams).ToSharedRef()
				];
			FontSettingsSection.AddEntry(FToolMenuEntry::InitWidget("FontTypeFace", Widget, FText::GetEmpty()));
		}

		{
			TSharedRef<SWidget> Widget = SNew(SBox)
				.Padding(15, 0, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(INVTEXT("font size"))
					]
					+ SHorizontalBox::Slot()
					.Padding(10, 0, 10, 0)
					.VAlign(VAlign_Center)
					[
						SNew(SSpinBox<float>)
						.Delta(1)
						.Justification(ETextJustify::Left)
						.MinValue(6)
						.MinDesiredWidth(150)
						.MaxValue(200)
						.Value_Lambda([this]() { return CurrentFont.Size; })
						.OnValueChanged_Lambda(
							[this](float const CurrentValue)
							{
								CurrentFont.Size = CurrentValue;
								UUnicodeBrowserOptions::Get()->SetFontInfo(CurrentFont);
								CharactersTileView->RebuildList();
							}
						)
					]
				];

			FontSettingsSection.AddEntry(
				FToolMenuEntry::InitWidget("BrowserFontSize", Widget, FText::GetEmpty(), false, false, false, INVTEXT("you can use CTRL + MouseWheel to adjust font size in the browser"))
			);
		}
	}

	TSharedPtr<SComboButton> SettingsButtonFont = SNew(SComboButton)
		.HasDownArrow(true)
		.ContentPadding(0.0f)
		.ForegroundColor(FSlateColor::UseForeground())
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
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
				+ SHorizontalBox::Slot()
				[
					SearchBar.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(10, 1))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(10, 1))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SettingsButtonFont.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(10, 1))
				]
				+ SHorizontalBox::Slot()
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
				.OnTileViewScrolled(this, &SUnicodeBrowserWidget::OnCharactersTileViewScrolled)
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

	MarkDirty(static_cast<uint8>(EDirtyFlags::INIT));
	MarkDirty(static_cast<uint8>(EDirtyFlags::FONT));
}

UToolMenu* SUnicodeBrowserWidget::CreateMenuSection_Settings()
{
	UToolMenu* Menu = UToolMenus::Get()->GenerateMenu("UnicodeBrowser.Settings.General_Display", FToolMenuContext());

	FToolMenuSection& GeneralSettingsSection = Menu->AddSection(TEXT("GeneralSettings"), INVTEXT("General Settings"));
	{
		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda(
					[this]()
					{
						UUnicodeBrowserOptions::Get()->bShowSidePanel = !UUnicodeBrowserOptions::Get()->bShowSidePanel;
						UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
						SetSidePanelVisibility(UUnicodeBrowserOptions::Get()->bShowSidePanel);
						//RangeScrollbox->RebuildList();
					}
				),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return UUnicodeBrowserOptions::Get()->bShowSidePanel; })
			);

			GeneralSettingsSection.AddMenuEntry("ShowSidePanel", INVTEXT("Show Sidepanel"), FText::GetEmpty(), FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton);
		}

		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda(
					[this]()
					{
						UUnicodeBrowserOptions::Get()->bShowMissing = !UUnicodeBrowserOptions::Get()->bShowMissing;
						UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
						UpdateCharacters();
					}
				),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return UUnicodeBrowserOptions::Get()->bShowMissing; })
			);

			GeneralSettingsSection.AddMenuEntry(
				"ShowMissingCharacters",
				INVTEXT("Show Missing Characters"),
				INVTEXT("Should characters which are missing in the font be shown?"),
				FSlateIcon(),
				Action,
				EUserInterfaceActionType::ToggleButton
			);
		}

		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda(
					[this]()
					{
						UUnicodeBrowserOptions::Get()->bShowZeroSize = !UUnicodeBrowserOptions::Get()->bShowZeroSize;
						UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
						UpdateCharacters();
					}
				),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return UUnicodeBrowserOptions::Get()->bShowZeroSize; })
			);

			GeneralSettingsSection.AddMenuEntry(
				"ShowZeroSizeCharacters",
				INVTEXT("Show Zero Size"),
				INVTEXT("Show characters which have a measurement of 0x0 (primarily for debug purposes)"),
				FSlateIcon(),
				Action,
				EUserInterfaceActionType::ToggleButton
			);
		}

		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda(
					[this]()
					{
						UUnicodeBrowserOptions::Get()->bCacheCharacterMetaOnLoad = !UUnicodeBrowserOptions::Get()->bCacheCharacterMetaOnLoad;
						UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
					}
				),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return UUnicodeBrowserOptions::Get()->bCacheCharacterMetaOnLoad; })
			);

			GeneralSettingsSection.AddMenuEntry(
				"CacheCharacterMeta",
				INVTEXT("Cache Character Metadata"),
				INVTEXT("Cache the Character meta information while loading the font, this is slower while changing fonts, but may reduce delay for displaying character previews"),
				FSlateIcon(),
				Action,
				EUserInterfaceActionType::ToggleButton
			);
		}

		{
			FUIAction Action = FUIAction(
				FExecuteAction::CreateLambda(
					[this]()
					{
						UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange = !UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange;
						UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
						if (UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange)
						{
							SidePanel->SelectAllRangesWithCharacters(RowsRaw);
						}
					}
				),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange; })
			);

			GeneralSettingsSection.AddMenuEntry(
				"AutoSetRangeOnFontChange",
				INVTEXT("Auto Set Ranges on Font Change"),
				INVTEXT("Automatic enable all ranges which are covered by the current font"),
				FSlateIcon(),
				Action,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}

	FToolMenuSection& DisplaySettingsSection = Menu->AddSection(TEXT("DisplaySettings"), INVTEXT("display settings"));
	{
		{
			TSharedRef<SWidget> Widget = SNew(SBox)
				.Padding(15, 0, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(INVTEXT("cell padding"))
					]
					+ SHorizontalBox::Slot()
					.Padding(10, 0, 10, 0)
					.VAlign(VAlign_Center)
					[
						SNew(SSpinBox<int32>)
						.Delta(1)
						.MinValue(0)
						.MaxValue(25)
						.Value_Lambda([this]() { return UUnicodeBrowserOptions::Get()->GridCellPadding; })
						.OnValueChanged_Lambda(
							[this](int32 const CurrentValue)
							{
								UUnicodeBrowserOptions::Get()->GridCellPadding = CurrentValue;
								UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();
								CharactersTileView->RebuildList();
								MarkDirty(static_cast<uint8>(EDirtyFlags::FONT_STYLE));
							}
						)
					]
				];

			DisplaySettingsSection.AddEntry(
				FToolMenuEntry::InitWidget(
					"BrowserGridCellPadding",
					Widget,
					FText::GetEmpty(),
					false,
					false,
					false,
					INVTEXT("Use CTRL + Shift + MouseWheel to adjust cell padding")
				)
			);
		}
	}

	return Menu;
}

void SUnicodeBrowserWidget::SetSidePanelVisibility(bool const bVisible)
{
	SidePanel->SetVisibility(bVisible ? EVisibility::Visible : EVisibility::Collapsed);
}

void SUnicodeBrowserWidget::MarkDirty(uint8 const Flags)
{
	DirtyFlags |= Flags;
	// schedule an update on the next tick
	SetCanTick(true);
}

TSharedRef<ITableRow> SUnicodeBrowserWidget::GenerateItemRow(TSharedPtr<FUnicodeBrowserRow> CharacterData, TSharedRef<STableViewBase> const& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FUnicodeBrowserRow>>, OwnerTable)
		[
			SNew(SUnicodeCharacterGridEntry)
			.FontInfo(CurrentFont)
			.UnicodeCharacter(CharacterData)
			.OnMouseMove(this, &SUnicodeBrowserWidget::OnCharacterMouseMove, CharacterData)
			.OnZoomFontSize(this, &SUnicodeBrowserWidget::HandleZoomFont)
			.OnZoomCellPadding(this, &SUnicodeBrowserWidget::HandleZoomPadding)
		];
}

void SUnicodeBrowserWidget::Tick(FGeometry const& AllottedGeometry, double const InCurrentTime, float const InDeltaTime)
{
	if (DirtyFlags && IsConstructed()) // wait for the widget to be constructed before updating
	{
		// grid size MUST be evaluated before the font, because FONT flag may set it dirty for the next tick
		if (DirtyFlags & static_cast<uint8>(EDirtyFlags::TILEVIEW_GRID_SIZE))
		{
			double CalcDesiredSize = 0;
			for (int ChildIdx = 0; ChildIdx < CharactersTileView->GetNumGeneratedChildren(); ChildIdx++)
			{
				if (!CharactersTileView->GetGeneratedChildAt(ChildIdx).Get()->NeedsPrepass())
				{
					FVector2D const ChildDesiredSize = CharactersTileView->GetGeneratedChildAt(ChildIdx).Get()->GetDesiredSize();
					CalcDesiredSize = FMath::Max(CalcDesiredSize, FMath::Max(ChildDesiredSize.X, ChildDesiredSize.X));
				}
			}

			if (CalcDesiredSize > 0)
			{
				CharactersTileView->SetItemWidth(FMath::Min(CalcDesiredSize, CharactersTileView->GetCachedGeometry().Size.X / 2.0));
				CharactersTileView->SetItemHeight(FMath::Min(CalcDesiredSize, CharactersTileView->GetCachedGeometry().Size.Y / 2.0));
				CharactersTileView->RebuildList();
			}

			DirtyFlags &= ~static_cast<uint8>(EDirtyFlags::TILEVIEW_GRID_SIZE);
		}

		if (DirtyFlags & static_cast<uint8>(EDirtyFlags::FONT))
		{
			CurrentFont = UUnicodeBrowserOptions::Get()->GetFontInfo();

			if (DirtyFlags & static_cast<uint8>(EDirtyFlags::FONT_FACE))
			{
				// rebuild the character list

				// ensure all ranges are checked when auto set ranges is enabled
				if (UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange)
				{
					for (FUnicodeBlockRange const& Range : UnicodeBrowser::GetUnicodeBlockRanges())
					{
						if (SidePanel.IsValid() && SidePanel->RangeSelector.IsValid())
						{
							SidePanel->RangeSelector->SetRanges({Range.Index}, false);
						}
					}
				}

				PopulateSupportedCharacters();
				if (UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange)
				{
					SidePanel->SelectAllRangesWithCharacters(RowsRaw);
				}

				OnFontChanged.Broadcast(&CurrentFont);

				DirtyFlags &= ~static_cast<uint8>(EDirtyFlags::FONT_FACE);
			}

			if (DirtyFlags & static_cast<uint8>(EDirtyFlags::FONT_STYLE))
			{
				CharactersTileView->RebuildList();
				MarkDirty(static_cast<uint8>(EDirtyFlags::TILEVIEW_GRID_SIZE)); // set grid size dirty
				DirtyFlags &= ~static_cast<uint8>(EDirtyFlags::FONT_STYLE);
			}
		}

		if (DirtyFlags & static_cast<uint8>(EDirtyFlags::INIT))
		{
			if (SidePanel.IsValid() && SidePanel->RangeSelector.IsValid())
			{
				// only use a default preset if this option is disabled, otherwise it causes trouble when reopening the Tab while a valid font is still set in UUnicodeBrowserOptions 
				if (!UUnicodeBrowserOptions::Get()->bAutoSetRangeOnFontChange)
				{
					// default preset		
					SidePanel->RangeSelector->SetRanges(UnicodeBrowser::SymbolRanges);
				}

				DirtyFlags &= ~static_cast<uint8>(EDirtyFlags::INIT);
			}
		}
	}

	SetCanTick(DirtyFlags != 0);
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

		for (int CharCode = Range.GetRange().GetLowerBound().GetValue(); CharCode <= Range.GetRange().GetUpperBound().GetValue(); ++CharCode)
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
	Rows.Empty(RowsRaw.Num());

	for (auto const& [Range, RawRangeRows] : RowsRaw)
	{
		// we can skip the whole range if it's not selected
		if (!SidePanel->RangeSelector->IsRangeChecked(Range))
			continue;

		TArray<TSharedPtr<FUnicodeBrowserRow>> RowsFiltered;
		RowsFiltered = RawRangeRows.FilterByPredicate(
			[](TSharedPtr<FUnicodeBrowserRow> const& RawRow)
			{
				if (RawRow->bFilteredByTag)
					return false;

				if (!UUnicodeBrowserOptions::Get()->bShowMissing && !RawRow->CanLoadCodepoint())
					return false;

				if (!UUnicodeBrowserOptions::Get()->bShowZeroSize && RawRow->GetMeasurements().IsZero())
					return false;

				return true;
			}
		);

		if (!RowsFiltered.IsEmpty())
		{
			Rows.Add(Range, RowsFiltered);
		}
	}

	UpdateCharactersArray();
}

void SUnicodeBrowserWidget::UpdateCharactersArray()
{
	int CharacterCount = 0;
	for (auto const& [Range, RangeRows] : Rows)
	{
		CharacterCount += RangeRows.Num();
	}

	CharacterWidgetsArray.Empty();
	CharacterWidgetsArray.Reserve(CharacterCount);

	for (auto const& [Range, FilteredRangeRows] : Rows)
	{
		for (auto& Character : FilteredRangeRows)
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
	for (int Idx = CharacterNeedles.Num() - 1; Idx >= 0; Idx--)
	{
		CharacterNeedles[Idx].TrimStartAndEndInline();
		if (CharacterNeedles[Idx].Len() > 1)
		{
			CharacterNeedles.RemoveAtSwap(Idx);
		}
	}

	bool const bFilterByCharacter = CharacterNeedles.Num() > 0;

	bool const bCaseSensitive = UUnicodeBrowserOptions::Get()->bSearch_CaseSensitive;

	bool bNeedUpdate = false;

	TArray<int32> Whitelist;
	if (bFilterTags)
	{
		Whitelist = UUnicodeBrowserOptions::Get()->Preset->GetCharactersByNeedle(Needle);
	}

	for (auto& [Range, RawRangeRows] : RowsRaw)
	{
		bool bRangeHasMatch = false;
		for (TSharedPtr<FUnicodeBrowserRow>& RowRaw : RawRangeRows)
		{
			bool bToggleRowState = Needle.IsEmpty() && RowRaw->bFilteredByTag;

			if (!bToggleRowState && bFilterTags)
			{
				bToggleRowState |= !Whitelist.Contains(RowRaw->Codepoint) != RowRaw->bFilteredByTag;
			}

			if (!bToggleRowState && bFilterByCharacter)
			{
				bool const bMatchesCharacter = CharacterNeedles.ContainsByPredicate(
					[bCaseSensitive, RowRaw](FString const& CharacterNeedle)
					{
						return CharacterNeedle.Equals(RowRaw->Character, bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase);
					}
				);

				bToggleRowState |= !bMatchesCharacter != RowRaw->bFilteredByTag;
			}

			if (bToggleRowState)
			{
				RowRaw->bFilteredByTag = !RowRaw->bFilteredByTag;
				bNeedUpdate = true;
				bRangeHasMatch = true;
			}
		}

		// ensure that the necessary ranges are selected before UpdateCharacters() is updating the filtered list
		if (bRangeHasMatch && UUnicodeBrowserOptions::Get()->bSearch_AutoSetRange)
		{
			if (!SidePanel->RangeSelector->IsRangeChecked(Range))
			{
				SidePanel->RangeSelector->SetRanges({Range}, false);
			}
		}
	}

	if (bNeedUpdate)
	{
		UpdateCharacters();
		CharactersTileView->RebuildList();
	}
}

FReply SUnicodeBrowserWidget::OnCharacterMouseMove(FGeometry const& Geometry, FPointerEvent const& PointerEvent, TSharedPtr<FUnicodeBrowserRow> Row)
{
	if (CurrentRow == Row) return FReply::Unhandled();
	CurrentRow = Row;

	OnCharacterHighlight.ExecuteIfBound(Row.Get());
	return FReply::Handled();
}

void SUnicodeBrowserWidget::OnCharactersTileViewScrolled(double X)
{
	DisableThrottlingTemporarily();
}

void SUnicodeBrowserWidget::DisableThrottlingTemporarily()
{
	// disable the CPU throttle for a short time so the user can scroll through the list without lag, even if the window isn't in the foreground
	// this can be an issue if the user has the editor preference "Use Less CPU when in Background" enabled.
	// i.e. if we allow the user to scroll, we should allow them to do so smoothly
	// We only disable throttling for 1s, which should be time enough for scrolling/interaction
	bShouldDisableThrottle = true;
	TWeakPtr<SUnicodeBrowserWidget> LocalWeakThis = SharedThis(this);
	GEditor->GetTimerManager()->SetTimer(
		ReenableThrottleHandle,
		[LocalWeakThis]()
		{
			if (LocalWeakThis.IsValid())
			{
				LocalWeakThis.Pin()->bShouldDisableThrottle = false;
			}
		},
		1.0f,
		false
	);
}

void SUnicodeBrowserWidget::HandleZoomFont(float const Offset)
{
	CurrentFont.Size = FMath::Max(1.0f, CurrentFont.Size + Offset);
	UUnicodeBrowserOptions::Get()->SetFontInfo(CurrentFont);
	CharactersTileView->RebuildList();
}

void SUnicodeBrowserWidget::HandleZoomPadding(float const Offset)
{
	// we want inverted behavior for cell padding
	UUnicodeBrowserOptions::Get()->GridCellPadding = FMath::Max(0, FMath::RoundToInt(UUnicodeBrowserOptions::Get()->GridCellPadding - Offset));
	UUnicodeBrowserOptions::Get()->TryUpdateDefaultConfigFile();

	CharactersTileView->RebuildList();
	MarkDirty(static_cast<uint8>(EDirtyFlags::FONT_STYLE));
}

bool SUnicodeBrowserWidget::ShouldDisableCPUThrottling() const
{
	return bShouldDisableThrottle;
}

void SUnicodeBrowserWidget::SetUpDisableCPUThrottlingDelegate()
{
	if (GEditor && !DisableCPUThrottleHandle.IsValid())
	{
		GEditor->ShouldDisableCPUThrottlingDelegates.Add(UEditorEngine::FShouldDisableCPUThrottling::CreateSP(this, &SUnicodeBrowserWidget::ShouldDisableCPUThrottling));
		DisableCPUThrottleHandle = GEditor->ShouldDisableCPUThrottlingDelegates.Last().GetHandle();
	}
}

void SUnicodeBrowserWidget::CleanUpDisableCPUThrottlingDelegate()
{
	if (GEditor && DisableCPUThrottleHandle.IsValid())
	{
		GEditor->ShouldDisableCPUThrottlingDelegates.RemoveAll(
			[this](UEditorEngine::FShouldDisableCPUThrottling const& Delegate)
			{
				return Delegate.GetHandle() == DisableCPUThrottleHandle;
			}
		);
		DisableCPUThrottleHandle.Reset();
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
